#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define MAXPG 64

static const char marker[] = "my very very very secret pw is:   ";

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
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  char *p = sbrk(PGSIZE * MAXPG);
  if (p == (char *)-1) {
    errquit(1, "sbrk failed");
  }

  int i = 0;
  while (i < MAXPG && memcmp(p + 8, marker + 8, 24)) {
    ++i;
    p += PGSIZE;
  }
  if (i >= MAXPG) {
    errquit(1, "failed to find marker");
  }

  write(2, p + 32, 8);
  exit(0);
}
