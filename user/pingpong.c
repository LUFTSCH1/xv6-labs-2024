#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define  PREAD (0)
#define PWRITE (1)

static void errquit(int, const char *) __attribute__((noreturn));

static void errquit(const int status, const char prompt[]) {
  fprintf(2, "%s\n", prompt);
  exit(status);
}

int main(int argc, char *argv[]) {
  // 虽然一个管道也能完成，但题目要求一对管道，每个负责一个方向
  int pwcr[2], prcw[2]; // pwcr父写子读，prcw父读子写
  if (pipe(pwcr) || pipe(prcw)) {
    errquit(1,"failed to create pipe");
  }

  const int pid = fork(); // 尝试创建子进程
  if (pid < 0) {
    errquit(1, "failed to fork");
  }

  char data = 0; // 一字节数据

  if (pid == 0) {
    close(pwcr[PWRITE]), close(prcw[PREAD]); // 关闭子进程不需要的端
    read(pwcr[PREAD], &data, 1), close(pwcr[PREAD]); // 子进程读后关闭pwcr读端
    printf("%d: received ping\n", getpid());
    write(prcw[PWRITE], &data, 1), close(prcw[PWRITE]); // 子进程写后关闭prcw写端
  } else {
    close(pwcr[PREAD]), close(prcw[PWRITE]); // 关闭父进程不需要的端
    write(pwcr[PWRITE], "d", 1), close(pwcr[PWRITE]); // 父进程写后关闭pwcr写端
    wait((int *)0); // 等待子进程退出，fork()成功后退出status一定为0，不用关心状态
    read(prcw[PREAD], &data, 1), close(prcw[PREAD]); // 父进程读后关闭prcw读端
    printf("%d: received pong\n", getpid());
  }
  exit(0);
}
