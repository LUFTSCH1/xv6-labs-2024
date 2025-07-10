#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define  PREAD (0)
#define PWRITE (1)
#define INT_SIZE sizeof(int)
#define UPPER_REALM (280)

static void errquit(int, const char *) __attribute__((noreturn));
static void prime(int) __attribute__((noreturn));

static const char S_EXCE_PIPE[] = "failed to create pipe";
static const char S_EXCE_FORK[] = "failed to fork";

static void
errquit(const int status, const char prompt[])
{
  fprintf(2, "%s\n", prompt);
  exit(status);
}

static void
prime(const int rfd)
{
  int pri;
  if (read(rfd, &pri, INT_SIZE) != INT_SIZE) {
    close(rfd);
    exit(0);
  }
  printf("prime %d\n", pri);

  int nxtpipe[2];
  if (pipe(nxtpipe)) {
    errquit(1, S_EXCE_PIPE);
  }
  const int pid = fork();
  if (pid < 0) {
    errquit(1, S_EXCE_FORK);
  }

  if (pid == 0) {
    close(rfd), close(nxtpipe[PWRITE]);
    prime(nxtpipe[PREAD]);
  } else {
    close(nxtpipe[PREAD]);
    for (int n; read(rfd, &n, INT_SIZE) == INT_SIZE; ) {
      if (n % pri) {
        write(nxtpipe[PWRITE], &n, INT_SIZE);
      }
    }
    close(rfd), close(nxtpipe[PWRITE]);
    int cstatus;
    wait(&cstatus);
    exit(cstatus);
  }
}

int
main(int argc, char *argv[])
{
  int fds[2];
  if (pipe(fds)) {
    errquit(1, S_EXCE_PIPE);
  }

  const int pid = fork();
  if (pid < 0) {
    errquit(1, S_EXCE_FORK);
  }

  if (pid == 0) {
    close(fds[PWRITE]);
    prime(fds[PREAD]);
  }
  close(fds[PREAD]);
  for (int i = 2; i <= UPPER_REALM; ++i) {
    write(fds[PWRITE], &i, INT_SIZE);
  }
  close(fds[PWRITE]);
  int cstatus;
  wait(&cstatus);
  exit(cstatus);
}
