#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define stderr (2)

static char buf[4] = "";

static void
closepipe(int p[]) {
  close(p[0]);
  close(p[1]);
  p[0] = 0;
  p[1] = 0;
}

int
main(void) {
  // pwcr父写子读，prcw父读子写
  int pwcr[2], prcw[2];
  if (pipe(pwcr)) {
    fprintf(stderr, "failed to create pipe\n");
    exit(1);
  }
  if (pipe(prcw)) {
    fprintf(stderr, "failed to create pipe\n");
    closepipe(pwcr);
    exit(1);
  }

  const int pid = fork();
  if (pid < 0) {
    fprintf(stderr, "failed to fork\n");
    exit(1);
  }
  if (pid == 0) {
    read(pwcr[0], buf, 1);
    printf("%d: received ping\n", getpid());
    write(prcw[1], buf, 1);
    exit(0);
  }
  write(pwcr[1], "d", 1);
  {
    int child_status;
    wait(&child_status);
    if (child_status) {
      fprintf(stderr, "child exit error\n");
      closepipe(pwcr), closepipe(prcw);
      exit(1);
    }
  }
  read(prcw[0], buf, 1);
  printf("%d: received pong\n", getpid());

  closepipe(pwcr), closepipe(prcw);
  exit(0);
}
