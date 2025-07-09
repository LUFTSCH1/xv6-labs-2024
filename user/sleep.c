#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define stderr (2)

int
main(int argc, char *argv[]) {
  int interval;
  if (argc != 2 || (interval = atoi(argv[1])) <= 0) {
    fprintf(stderr, "Usage: %s <positive_integer_interval>\n", argv[0]);
    exit(1);
  }

  sleep(interval);

  exit(0);
}
