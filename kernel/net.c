#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;

// ---- UDP RX per-port bounded queues ----
#define UDP_Q_LIMIT    16 // 每端口最多缓存16个包（实验要求）
#define UDP_PORT_SLOTS 32 // 同时允许绑定的端口槽位数（够本实验用）

struct udp_pkt {
  uint32 sip;   // 源IP（host序）
  uint16 sport; // 源端口（host序）
  int len;      // 负载长度
  char *buf;    // 指向整帧缓冲（ETH+IP+UDP+payload）
};

struct udp_portq {
  int used;
  uint16 port;   // host序
  int owner_pid; // 绑定者
  struct spinlock lock;
  struct udp_pkt q[UDP_Q_LIMIT];
  int r, w, n;
};

static struct udp_portq upq[UDP_PORT_SLOTS];
static struct spinlock upqmap_lock;

static struct udp_portq*
find_portq(uint16 port)
{
  for (int i = 0; i < UDP_PORT_SLOTS; ++i) {
    if(upq[i].used && upq[i].port == port) {
      return &upq[i];
    }
  }
  return 0;
}

void
netinit(void)
{
  initlock(&netlock, "netlock");
  initlock(&upqmap_lock, "udp-port-map");
  for (struct udp_portq *up = upq, *end = upq + UDP_PORT_SLOTS;
       up < end; ++up) {
    initlock(&up->lock, "udp-portq");
    up->used = 0;
    up->r = 0;
    up->w = 0;
    up->n = 0;
  }
}

//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  //
  // Your code here.
  //
  int port_s;
  argint(0, &port_s);
  uint16 port = (uint16)port_s;
  struct proc *p = myproc();

  acquire(&upqmap_lock);
  struct udp_portq *pq = find_portq(port);
  if (pq) {
    int ok = pq->owner_pid == p->pid;
    release(&upqmap_lock);
    return ok ? 0 : -1; // 已被本进程绑定则幂等成功，否则冲突
  }
  // 分配空槽
  for (struct udp_portq *up = upq, *end = upq + UDP_PORT_SLOTS;
       up < end; ++up) {
    if (!up->used) {
      acquire(&up->lock);
      up->used = 1;
      up->port = port;
      up->owner_pid = p->pid;
      up->r = 0;
      up->w = 0;
      up->n = 0;
      release(&up->lock);
      release(&upqmap_lock);
      return 0;
    }
  }
  release(&upqmap_lock);
  return -1; // 没有可用槽位
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //
  int port_s;
  argint(0, &port_s);
  uint16 port = (uint16)port_s;
  struct proc *p = myproc();

  acquire(&upqmap_lock);
  struct udp_portq *pq = find_portq(port);
  if (!pq || pq->owner_pid != p->pid) {
    release(&upqmap_lock);
    return -1;
  }
  acquire(&pq->lock);
  while (pq->n > 0) {
    struct udp_pkt *slot = &pq->q[pq->r];
    if (slot->buf) {
      kfree(slot->buf);
    }
    slot->buf = 0;
    slot->len = 0;
    pq->r = (pq->r + 1) % UDP_Q_LIMIT;
    --pq->n;
  }
  pq->used = 0;
  release(&pq->lock);
  release(&upqmap_lock);
  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  //
  // Your code here.
  //
  int dport_s;
  int maxlen;
  uint64 u_src;
  uint64 u_sport;
  uint64 u_buf;
  argint(0, &dport_s);
  argaddr(1, &u_src);
  argaddr(2, &u_sport);
  argaddr(3, &u_buf);
  argint(4, &maxlen);

  uint16 dport = (uint16)dport_s;
  acquire(&upqmap_lock);
  struct udp_portq *pq = find_portq(dport);
  release(&upqmap_lock);
  if (!pq) {
    return -1; // 未bind
  }

  struct proc *p = myproc();
  acquire(&pq->lock);
  while (pq->n == 0) {
    if (p->killed) {
      release(&pq->lock);
      return -1;
    }
    sleep(pq, &pq->lock); // ip_rx() 会 wakeup(pq)
  }

  struct udp_pkt *slot = &pq->q[pq->r];
  int n = slot->len;
  if (n > maxlen) {
    n=maxlen;
  }

  // 源信息与负载 copyout（host 序）
  int rc = 0;
  if (copyout(p->pagetable, u_src, (char*)&slot->sip, sizeof(uint32)) < 0 ||
      copyout(p->pagetable, u_sport, (char*)&slot->sport, sizeof(uint16)) < 0) {
    rc = -1;
  }

  char *payload = slot->buf + sizeof(struct eth)
                + sizeof(struct ip) + sizeof(struct udp);
  if (n > 0 && rc == 0 && copyout(p->pagetable, u_buf, payload, n) < 0) {
    rc = -1;
  }

  // 出队并释放内核缓冲
  kfree(slot->buf);
  slot->buf = 0;
  slot->len = 0;
  pq->r = (pq->r + 1) % UDP_Q_LIMIT;
  --pq->n;

  release(&pq->lock);
  return rc < 0 ? -1 : n;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  //
  // Your code here.
  //
  // 基本越界检查
  if (len < sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp)) {
    kfree(buf);
    return;
  }
  // 保险
  if (ntohs(((struct eth *)buf)->type) != ETHTYPE_IP) {
    kfree(buf);
    return;
  }
  // 只处理UDP
  struct ip *iph = (struct ip *)(buf + sizeof(struct eth));
  if (iph->ip_p != IPPROTO_UDP) {
    kfree(buf);
    return;
  }

  // 本实验无IP选项，header固定20字节
  struct udp *uh = (struct udp *)((char *)iph + sizeof(struct ip));
  int ulen = ntohs(uh->ulen); // 含UDP头
  if (ulen < (int)sizeof(struct udp)) {
    kfree(buf);
    return;
  }

  int hdrs = sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  int payload_len = ulen - (int)sizeof(struct udp);
  if (len < hdrs + payload_len) { // 越界丢弃
    kfree(buf);
    return;
  }

  uint16 dport = ntohs(uh->dport);
  uint16 sport = ntohs(uh->sport);
  uint32 sip   = ntohl(iph->ip_src);

  // 找到端口队列
  acquire(&upqmap_lock);
  struct udp_portq *pq = find_portq(dport);
  release(&upqmap_lock);
  if (!pq) { // 未绑定直接丢
    kfree(buf);
    return;
  }

  // 入队（满则丢），并唤醒等待的recv()
  acquire(&pq->lock);
  if(pq->n >= UDP_Q_LIMIT){
    release(&pq->lock);
    kfree(buf);
    return;
  }
  struct udp_pkt *slot = &pq->q[pq->w];
  slot->sip = sip;
  slot->sport = sport;
  slot->len = payload_len;
  slot->buf = buf;
  pq->w = (pq->w + 1) % UDP_Q_LIMIT;
  ++pq->n;
  wakeup(pq);
  release(&pq->lock);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
