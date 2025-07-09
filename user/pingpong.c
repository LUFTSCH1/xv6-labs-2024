#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define stderr (2)

int
main(void) {
  // 虽然一个管道也能完成，但题目要求一对管道，每个负责一个方向
  int pwcr[2], prcw[2]; // pwcr父写子读，prcw父读子写
  if (pipe(pwcr)) {
    fprintf(stderr, "failed to create pipe\n");
    exit(1);
  }
  if (pipe(prcw)) {
    fprintf(stderr, "failed to create pipe\n");
    close(pwcr[0]), close(pwcr[1]);
    exit(1);
  }

  const int pid = fork(); // 尝试创建子进程
  if (pid < 0) {
    fprintf(stderr, "failed to fork\n");
    close(pwcr[0]), close(pwcr[1]);
    close(pwcr[0]), close(pwcr[1]);
    exit(1);
  }

  char data = 0; // 一字节数据

  if (pid == 0) {
    close(pwcr[1]), close(prcw[0]); // 关闭子进程不需要的端
    read(pwcr[0], &data, 1), close(pwcr[0]); // 子进程读后关闭pwcr读端
    printf("%d: received ping\n", getpid());
    write(prcw[1], &data, 1), close(prcw[1]); // 子进程写后关闭prcw写端
    exit(0);
  }
  close(pwcr[0]), close(prcw[1]); // 关闭父进程不需要的端
  write(pwcr[1], "d", 1), close(pwcr[1]); // 父进程写后关闭pwcr写端
  wait((int *)0); // 等待子进程退出，fork()成功后退出status一定为0，不用关心状态
  read(prcw[0], &data, 1), close(prcw[0]); // 父进程读后关闭prcw读端
  printf("%d: received pong\n", getpid());

  exit(0);
}
