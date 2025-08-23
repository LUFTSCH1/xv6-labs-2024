#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define  PREAD (0)
#define PWRITE (1)

static void errquit(int, const char *) __attribute__((noreturn));

static void
errquit(const int status, const char *const prompt)
{
  write(2, prompt, strlen(prompt));
  write(2, "\n", 1);
  exit(status);
}

int
main(int argc, char *argv[])
{
  // 虽然一个管道也能完成，但题目要求一对管道，每个负责一个方向
  int pwcr[2], prcw[2]; // pwcr父写子读，prcw父读子写
  if (pipe(pwcr) || pipe(prcw)) {
    errquit(1,"failed to create pipe");
  }

  const int pid = fork(); // 尝试创建子进程
  if (pid < 0) {
    errquit(1, "failed to fork");
  }

  uint8 data = 0; // 一字节数据

  if (pid == 0) {
    close(pwcr[PWRITE]), close(prcw[PREAD]); // 关闭子进程不需要的端
    read(pwcr[PREAD], &data, sizeof(data)), close(pwcr[PREAD]); // 子进程读后关闭pwcr读端
    printf("%d: received ping\n", getpid());
    write(prcw[PWRITE], &data, sizeof(data)), close(prcw[PWRITE]); // 子进程写后关闭prcw写端
  } else {
    close(pwcr[PREAD]), close(prcw[PWRITE]); // 关闭父进程不需要的端
    write(pwcr[PWRITE], "d", sizeof(data)), close(pwcr[PWRITE]); // 父进程写后关闭pwcr写端
    wait((int *)0); // 等待子进程退出，fork()成功后退出status一定为0，不用关心状态
    read(prcw[PREAD], &data, sizeof(data)), close(prcw[PREAD]); // 父进程读后关闭prcw读端
    printf("%d: received pong\n", getpid());
  }
  exit(0);
}
