#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "user/user.h"

static void errquit(int, const char *) __attribute__((noreturn));

static char buf[512];
static const char *dst;

static void
errquit(const int status, const char *const prompt)
{
  write(2, prompt, strlen(prompt));
  write(2, "\n", 1);
  exit(status);
}

static void
checkroot(const char *const root)
{
  const int fd = open(root, O_RDONLY);
  if (fd < 0) {
    errquit(1, "failed to open root_directory");
  }
  struct stat st;
  if (fstat(fd, &st) < 0) {
    errquit(1, "failed to stat root_directory");
  }
  if (st.type != T_DIR) {
    errquit(1, "root_directory must be a directory");
  }
  close(fd);
}

static void
find(char *cur_end, int depth)
{
  if (cur_end - buf + 1 + DIRSIZ + 1 > sizeof(buf)) {
    fprintf(2, "path too long\n");
    return;
  }
  const int fd = open(buf, O_RDONLY);
  if (fd < 0) {
    fprintf(2, "cannot open\n");
    return;
  }
  
  int not_found = 1;
  *cur_end++ = '/';
  for (struct dirent de; read(fd, &de, sizeof(de)) == sizeof(de); ) {
    // 排除空目录项、当前目录、父目录
    if (de.inum == 0 || !strcmp(de.name, ".") || !strcmp(de.name, "..")) {
      continue;
    }
    strcpy(cur_end, de.name);
    struct stat st;
    if (stat(buf, &st) < 0) {
      fprintf(2, "cannot stat\n");
      continue;
    }
    if (not_found && st.type == T_FILE) {
      if (!(not_found = strcmp(de.name, dst))) {
        printf("%s\n", buf);
      }
    } else if (st.type == T_DIR) {
      find(cur_end + strlen(de.name), depth + 1);
    }
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc != 2 && argc != 3) {
    fprintf(
      2,
      "Usage: %s [root_directory] <file_name>\n"
      "  Default root_directory is \".\"\n",
      argv[0]
    );
    exit(1);
  }

  if (argc == 2) {
    strcpy(buf, ".");
    dst = argv[1];
  } else {
    strcpy(buf, argv[1]);
    dst = argv[2];
  }
  checkroot(buf);
  find(buf + strlen(buf), 0);
  exit(0);
}
