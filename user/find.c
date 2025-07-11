#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

#define  PREAD (0)
#define PWRITE (1)

static void errquit(int, const char *) __attribute__((noreturn));

static void errquit(const int status, const char prompt[]) {
  fprintf(2, "%s\n", prompt);
  exit(status);
}



static void find(const char *const file_name) {

}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(2, "Usage: %s <file_name>\n", argv[0]);
    exit(1);
  }

  find(argv[1]);

  exit(0);
}