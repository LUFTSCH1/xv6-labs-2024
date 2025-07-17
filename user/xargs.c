#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAXLEN (1024)

enum argst {
  ARGST_CUT,
  ARGST_NORMAL,
  ARGST_LAST
};

static void errquit(int, const char *) __attribute__((noreturn));

static void errquit(const int status, const char prompt[]) {
  fprintf(2, "%s\n", prompt);
  exit(status);
}

static int getchar(void) {
  char ch;
  if (read(0, &ch, sizeof(ch)) != sizeof(ch)) {
    return 0;
  }
  return ch;
}

static int readarg(char *const buf, int maxsize, enum argst *const st) {
  --maxsize;
  *st = ARGST_NORMAL;
  int ch;
  do {
    ch = getchar();
  } while (ch == ' ' || ch == '\t');
  if (ch == 0) {
    return 0;
  }
  char *p = buf;
  do {
    if (ch == '\n') {
      *st = ARGST_LAST;
      break;
    }
    if (p - buf >= maxsize) {
      *st = ARGST_CUT;
      return 0;
    }
    *p++ = (char)ch;
    ch = getchar();
  } while (ch != 0 && ch != ' ' && ch != '\t');
  *p = '\0';
  return p - buf;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "Usage: xargs <command> [initial-args...]\n");
    exit(1);
  }

  char *exec_argv[MAXARG];
  for (int i = 1; i < argc; ++i) {
    exec_argv[i - 1] = argv[i];
  }
  const int base = argc - 1;

  while (1) {
    char buf[MAXLEN];
    {
      char *p = buf;
      enum argst st;
      int len, rest = MAXLEN, pos = base;
      while ((len = readarg(p, rest, &st)) && st != ARGST_CUT && pos < MAXARG - 1) {
        exec_argv[pos++] = p;
        p += len + 1;
        rest -= len + 1;
        if (st == ARGST_LAST) {
          break;
        }
      }
      if (p == buf) {
        break;
      }
      if (st == ARGST_CUT) {
        errquit(1, "line too long, exec ended");
      }
      exec_argv[pos] = (char *)0;
    }

    const int pid = fork();
    if (pid < 0) {
      errquit(1, "failed to fork");
    }
    if (pid == 0) {
      exec(exec_argv[0], exec_argv);
      errquit(1, "failed to exec");
    }
    int cstatus;
    wait(&cstatus);
    if (cstatus) {
      errquit(cstatus, "exec line failed");
    }
  }
  exit(0);
}
