# xv6-riscv实验报告

> 2253713 戴金瓯

实验版本：[2024](https://pdos.csail.mit.edu/6.828/2024/)  
实验仓库：[https://github.com/LUFTSCH1/xv6-labs-2024](https://github.com/LUFTSCH1/xv6-labs-2024)  

## 配置实验环境

物理机环境：`Windows 11 (x64)`  
[官方文档-安装说明部分](https://pdos.csail.mit.edu/6.828/2024/tools.html) 给出了许多环境的配置方式，这里选择使用`WSL2`进行实验环境配置，代码编辑器选用`Visual Studio Code`。  

### 安装`WSL发行版`

打开`Microsoft Store`应用，搜索`Debian`并安装即可，本次实验所使用的版本为`Debian 12.9`，安装完成可以直接点击“打开”、在“开始”菜单旁的“搜索”功能输入`Debian`或是直接在`CMD`/`Power Shell`中输入`bash`启动（若安装了多个`WSL发行版`，则应注意该方式启动的是默认发行版）  

### 配置`WSL发行版`

#### 准备编辑器智能提示支持

```bash
sudo apt install -y clangd bear
```
简单解释一下安装内容：  
  * `clangd`：语言服务器核心  
  * `bear`：编译指令捕获工具   

#### 官方要求的项目环境配置

按照官网说明，需要安装一些工具，命令如下：
```bash
sudo apt-get update && sudo apt-get upgrade
sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```
简单解释一下安装内容：  
  * `git`：版本控制工具  
  * `build-essential`：开发工具组合，主要使用其中的`make`（自动化构建工具）  
  * `gdb-multiarch`：GNU调试器  
  * `qemu-system-misc`：QEMU全系统模拟器  
  * `gcc-riscv64-linux-gnu`：`RISC-V 64`交叉编译器  
  * `binutils-riscv64-linux-gnu`：`RISC-V 64`二进制工具  

**使用如下命令检验安装是否成功**：  

所有发行版下均一致的命令（成功则显示版本号）：  
```bash
qemu-system-riscv64 --version
```
接着以下三条有一条执行成功即可：  
```bash
riscv64-linux-gnu-gcc --version
riscv64-unknown-elf-gcc --version
riscv64-unknown-linux-gnu-gcc --version
```

### 编辑器（`Visual Studio Code`）配置

`Visual Studio Code`安装在物理机中。在插件栏搜索并安装`WSL`插件，完成后左边栏将多出一个图标，为`远程资源管理器`。点击`远程资源管理器`，选择`WSL目标`中`Debian发行版`对应的选项进行连接。连接完成后若没有显示终端，则在左上角`查看`菜单中打开`终端`，现在可以在编辑器的终端中操作Linux子系统了。  
另外，需要智能提示和自动代码补全，在编辑器的插件栏中搜索并安装`Clangd`插件。  

### 拉取实验仓库

按照官方给出的仓库地址，执行：  
```bash
git clone git://g.csail.mit.edu/xv6-labs-2024
```
**不要**拉取`GitHub`上的`mit-pdos/xv6-riscv`仓库，最简单直接的理由就是它没有提供实验测试脚本。  
现在，实验目录就在`~/xv6-labs-2024`，在编辑器左边的`资源管理器`中使用`打开文件夹`功能，打开这个目录。  

### 继续配置项目

如果要使用`git`进行版本控制，建议在`.gitignore`文件中加上如下配置：  
```.gitignore
.vscode/
.cache/
__pycache__/
compile_commands.json
*.pdf
```
推送到远程仓库时，如需特殊网络环境，直接使用物理机代理软件的`虚拟网卡`（`TUN`）模式即可。  

在项目根目录（`~/xv6-labs-2024`）中新建`.vscode`目录，在里面新建`settings.json`文件，内容如下（需要修改对应部分）：  
```json
{
  "editor.tabSize": 2,
  "editor.insertSpaces": true,
  "files.encoding" : "utf8",
  "files.eol" : "\n",
  "editor.formatOnSave": false,
  "clangd.path": "/usr/bin/clangd",
  "clangd.arguments": [
    "--background-index",
    "--compile-commands-dir=${Linux子系统中你的项目路径}",
    "--query-driver=/usr/bin/gcc",
    "--header-insertion=never"
  ]
}
``` 

### 第一次构建

#### 第一次的例外

```bash
bear -- make qemu  # 构建命令
bear -- make clean # 清除命令
```
第一次构建使用以上命令中的构建命令，让`Clangd`知晓`-I.`参数，正确解析`kernel`和`user`目录。这样编辑器的自动缩进和`Clangd` Server就配置好了，打开一个C源程序，应当没有报错标红。  

#### 实验指导使用的命令

```bash
make qemu
```
依照实验指导，在项目根目录下输入以上命令来构建。完成构建则会自动启动`xv6`内核，按`Ctrl+A`后按`X`可以退出`QEMU`，回到Linux系统中。  

## 实验系统调用（`user/user.h`）

| 系统调用 | 解释 |
| :-- | :-- |
| `int fork(void)` | 创建一个子进程（当前进程的完整副本）；返回其PID（若本身为`fork()`产生的子进程，返回0；错误时返回-1） |
| `int exit(int status) __attribute__((noreturn))` | 终止当前进程；`status`报告给`wait()`；返回指令不会被执行，故实际无返回值 |
| `int wait(int *status)` | 等待子进程退出；退出状态存在`*status`（`status`为`(int *)0`表示不关心子进程退出状态）；返回值为子进程PID |
| `int kill(int pid)` | 根据传入的PID终止对应进程；返回值为0或-1（错误） |
| `int getpid(void)` | 返回值为当前进程的PID |
| `int sleep(int n)` | 暂停n个时钟节拍；总是返回0 |
| `int exec(const char *file, char **argv)` | 加载`file`路径对应文件并用给定的`argv`参数表执行；成功时当前进程空间被覆盖，无返回，失败返回-1 |
| `char *sbrk(int n)` | `n`为0时仅查询堆顶，返回当前堆结束地址；`n`为正时扩展堆空间，返回原堆顶地址；`n`为负时收缩堆空间，返回原堆顶地址；执行失败返回`(char *)-1` |
| `int open(const char *file, int flags)` | 打开`file`路径对应文件，`flags`为打开模式，有`O_RDONLY`、`O_WRONLY`、`O_RDWR`、`O_CREATE`和`O_TRUNC`及其组合（按位或进行组合）；成功返回文件描述符（正数），失败返回-1 |
| `int write(int fd, const void *buf, int n)` | 向文件描述符`fd`写入`buf`开始的`n`字节数据；返回n |
| `int read(int fd, void *buf, int n)` | 从文件描述符`fd`读取`n`字节数据存入`buf`；返回读取字节数，如已到文件末尾返回0 |
| `int close(int fd)` | 释放打开的文件`fd`；成功返回0，失败返回-1 |
| `int dup(int fd)` | 返回和`fd`指向相同文件的新文件描述符 |
| `int pipe(int *p)` | 创建一个管道，将读、写文件描述符分别放入`p[0]`、`p[1]`；创建成功返回0，失败返回-1 |
| `int chdir(const char *dir)` | 修改当前进程的工作目录；成功返回0，失败返回-1 |
| `int mkdir(const char *dir)` | 新建目录；成功返回0，失败返回-1 |
| `int mknod(const char *file, short major, short minor)` | 创建设备文件，`file`为设备文件路径，`major`为主设备号（标识设备类型），`minor`为次设备号（标识具体设备实例）；成功返回0，失败返回-1 |
| `int fstat(int fd, struct stat *st)` | 查询`fd`对应文件元数据存入`*st`；成功返回0，失败返回-1 |
| `int link(const char *file1, const char *file2)` | 为`file1`创建别名（硬链接）`file2`；成功返回0，失败返回-1 |
| `int unlink(const char *file)` | 删除文件链接；成功返回0，失败返回-1 |

## Lab: Xv6 and Unix utilities（git checkout util）

### sleep（`user/sleep.c` 难度：easy）

#### 题目

为`xv6`提供一个用户级工具`sleep`，类似`UNIX`中的`sleep`命令。你的`sleep`工具应该暂停用户指定的时钟节拍数（节拍是`xv6`内核定义的时间概念，即定时器芯片两次中断之间的时间间隔）你的解决方案应位于文件`user/sleep.c`中。  

#### 简要分析

思路上只需要从参数中拿到用户指定的时钟节拍数，直接传给`sleep`系统调用即可。实现上有几个注意点：  
  * `xv6`中没有`标准C runtime初始化系统`（包含于`libc`中），其进程的退出必须调用`int exit(int status)`，而不是在主函数中直接`return`（实际上平时写的`return 0;`后还有`libc`调用`exit`）。  
  * 提示中说明从数字字符串转换为整型变量可以用`int atoi(const char *s)`（定义位于`user/ulib.c`），从其实现可以看出：转换失败返回0，成功返回一个正数。  

#### 代码

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int interval;
  if (argc != 2 || (interval = atoi(argv[1])) <= 0) {
    fprintf(2, "Usage: %s <positive_integer_interval>\n", argv[0]);
    exit(1);
  }

  sleep(interval);

  exit(0);
}
```

#### 本题额外说明

接着在项目根目录`Makefile`文件第180行的`UPROGS`项下追加：  
```makefile
$U/_sleep\ # 即 $U/_{不带.c后缀的文件名}\
```
保存后使用前面提到的`make qemu`命令构建即可。  

除了在`xv6`内核的命令行界面手动输入`sleep 10`之类的命令进行测试之外，还需要通过测试脚本，测试的方法是在项目根目录下输入命令：  
```bash
./grade-lab-util sleep
```
如无执行权限，则`chmod`追加执行权限后再尝试：  
```bash
chmod +x ./grade-lab-util
```

以上构建和测试方法后续通用，无特殊情况将不重复说明。  

### pingpong（`user/pingpong.c` 难度：easy）

#### 题目

编写一个用户级程序，使用`xv6`系统调用通过一对管道在两个进程之间“ping pong”发送一个字节。父进程应向子进程发送一个字节；子进程应打印“`<pid>: received ping`”（其中`<pid>`是子进程PID），并将该字节通过管道写入父进程，然后退出；父进程应从子进程读取该字节，打印“`<pid>: received pong`”（其中`<pid>`是父进程PID），然后退出。你的解决方案应位于文件`user/pingpong.c`中。  

#### 简要分析

先来了解`xv6`的`fork`、`pipe`、`wait`机制： 
  * `fork`：从含`fork()`的语句处对进程进行“分身”，将当前进程作为父进程，完整拷贝一份父进程的内存数据生成子进程，这两个进程会被挂起等待调度。父进程将从刚刚提到的含`fork()`的那句继续执行，其`fork()`函数返回子进程的PID（一个正数）；子进程也将从同一句（仅代码角度的同一句）开始执行，其`fork()`调用返回0，表示它是子进程。
  * `文件描述符`：`文件描述符`指向一个`打开的文件对象`，但其并非一对一的关系，而是一对多的关系。就像房卡与房间，可能有多名客人入住酒店的一间多床房间，每名客人都有一张可以开启房门的房卡，只有当所有客人都归还房卡（客人视角的“退房”操作）时，才可以让工作人员前往“释放”这个房间。对于文件描述符和打开的文件对象而言，这就是在底层增加了一个`引用计数`，其数值就是使用这个文件的进程数。`fork()`复制进程时会增加这个引用计数。可以手动`close(文件描述符)`来减少引用计数，也可以让操作系统自动在进程退出后减少对应的引用计数。打开的文件对象将在引用计数归零时释放。  
  * `pipe`：管道是一种虚拟文件，打开管道`int pipe(int p[])`将生成一对读、写文件描述符分别存入`&p[0]`、`&p[1]`处。在打开的读、写文件对象的引用计数均归零后管道会被释放。  
  * `wait`：子进程执行完后不会完全消失，而是进入`僵尸进程`状态，需要父进程调用`int wait(int *status)`进行回收，这是`wait`的根本作用。另外，由于父子进程执行顺序完全取决于操作系统的调度，尽管调度算法是确定的，但实际情况相当复杂，故可以认为执行顺序不确定。调整父进程中`wait`调用的位置可以保证其之后的语句一定在子进程退出后执行，在必要时可以通过`status`确定子进程执行是否成功。  

#### 代码

```c
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
```

### primes（`user/primes.c` 难度：hard）

#### 题目

用管道为`xv6`编写一个并行素数筛程序。使用`pipe`和`fork`来设置管道，第一个进程将2到280间的数送入管道。对于每个素数，你需要创建一个进程，该进程通过一个管道从其左侧相邻进程读取数据，并通过另一个管道写入其右侧相邻的进程。由于`xv6`的文件描述符和进程数量有限，因此第一个进程可以在280处停止。你的解决方案应位于文件`user/primes.c`中。  

#### 简要分析

本题可以采用埃氏筛进行实现。每个管道视为一个序列，顺序为数写入的先后（从小到大写入）。主进程显然有一个2-280的序列，然后进行筛除。每次取出并打印序列首的数，这一定是当前序列中最小的质数，然后将序列中不能被该数整除的数按顺序复制进新的序列（新管道）；重复上述过程，直到序列为空。  

#### 代码

```c
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
errquit(const int status, const char *const prompt)
{
  write(2, prompt, strlen(prompt));
  write(2, "\n", 1);
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
  }
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
```

### find（`user/find.c` 难度：中等）

#### 题目

为`xv6`写一个简化的`UNIX` `find`程序：查找目录树中所有指定名称的文件，你的解决方案应位于文件`user/find.c`中。  

#### 简要分析

需要阅读`user/ls.h`来了解如何读取目录。阅读源码并查看函数定义后发现，`xv6`中，有三种文件类型，分别由宏`T_DEVICE`、`T_FILE`和`T_DIR`标识。`目录`也是一种特殊的文件，`目录文件`存储着目录中的`目录项`。目录项由结构体`dirent`定义，其成员是`short inum`和`char name[DIRSIZ]`。前者标识目录项是否被使用（0表示空目录项）；后者是目录项对应目录中的的文件名。读取目录时，以只读模式打开目录文件并获取其文件描述符，使用`read`将该文件逐项读入一个`dirent`中，就可以获取目录内的目录项的名字。如果需要这个目录项对应的详细信息（如它到底是设备、文件、还是目录等），则需要将这个名字拼接到关于当前进程工作目录的相对路径中，使用`stat`（相较于`fstat`，`stat`在获取文件元数据时不需要手动打开文件，适合后续不需要文件描述符的操作，便于管理）将文件元数据读取到`stat`结构体中。`find`递归实现中，由于保证先遍历完当前目录中的文件再进入下级目录需要额外开销，故直接先按默认顺序遍历（遇到文件比较是不是目标，遇到目录直接先进去），这种方式直接通过了测试。  

#### 代码

```c
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
```

### xargs（`user/xargs.c` 难度：中等）

#### 题目

为 xv6 编写一个 UNIX xargs 程序的简单版本：它的参数描述一个要运行的命令，它从标准输入读取一行，然后逐行运行该命令，并将该行附加到命令的参数中。你的解决方案应位于文件`user/xargs.c`中。  

#### 简要分析

创建一个缓冲数组，将标准输入中的参数逐个读入该数组中，并将其首地址追加入`argv`数组，每个参数以尾零分隔，读完一行则`fork`一个子进程去执行（因为`exec`调用成功会覆盖当前进程，故必须让进程副本去执行）。  

#### 代码

```c
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

static void
errquit(const int status, const char *const prompt)
{
  write(2, prompt, strlen(prompt));
  write(2, "\n", 1);
  exit(status);
}

static int
getchar(void)
{
  char ch;
  if (read(0, &ch, sizeof(ch)) != sizeof(ch)) {
    return 0;
  }
  return ch;
}

static int
readarg(char *const buf, int maxsize, enum argst *const st)
{
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

int
main(int argc, char *argv[])
{
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
      while ((len = readarg(p, rest, &st)) &&
             st != ARGST_CUT &&
             pos < MAXARG - 1) {
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
```
### 测试

在根目录上创建一个`time.txt`，内容为一个整型，记录实验用时（小时数）。然后测试目录结构和所有题目：  

```bash
make grade
```

![util测试结果](./img/util.png)

## Lab: System calls（git checkout syscall）

### gdb

```c
struct proc {
  lock = {
    locked = 0x0,
    name = 0x800071c8,
    cpu = 0x0
  },
  state = 0x4,
  chan = 0x0,
  killed = 0x0,
  xstate = 0x0,
  pid = 0x1,
  parent = 0x0,
  kstack = 0x3fffffd000,
  sz = 0x1000,
  pagetable = 0x87f55000,
  trapframe = 0x87f56000,
  context = {
    ra = 0x8000124e,
    sp = 0x3fffffde80,
    s0 = 0x3fffffdeb0,
    s1 = 0x80007d80,
    s2 = 0x80007950,
    s3 = 0x1,
    s4 = 0x8000dc08,
    s5 = 0x3,
    s6 = 0x80018a20,
    s7 = 0x1,
    s8 = 0x80018b48,
    s9 = 0x4,
    s10 = 0x0,
    s11 = 0x0
  },
  ofile = {
    0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0
  },
  cwd = 0x80015e90,
  name = {
    0x69, 0x6e, 0x69, 0x74,
    0x63, 0x6f, 0x64, 0x65,
    0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0
  }
}
```

#### Looking at the backtrace output, which function called syscall?

  - usertrap  

#### What is the value of p->trapframe->a7 and what does that value represent? (Hint: look user/initcode.S, the first user program xv6 starts.)

  - `0x2`，为一个系统调用号，代表`SYS_exit`  

#### What was the previous mode that the CPU was in?

  - $sstatus = `0x200000022`，SPP (Bit 8) = 0，User Mode

#### Write down the assembly instruction the kernel is panicing at. Which register corresponds to the variable num?

```assembly
// num = p->trapframe->a7;
num = *(int *)0;
  80001c1e:	00002683          	lw	a3,0(zero) # 0 <_entry-0x80000000>
```
  - 寄存器: a3  

#### Why does the kernel crash? Hint: look at figure 3-3 in the text; is address 0 mapped in the kernel address space? Is that confirmed by the value in scause above? (See description of scause in RISC-V privileged instructions)

  - 内核地址空间从`0x80000000`开始，`0x0`为用户地址空间起始，不在内核地址空间范围内。
  - RISC-V的CPU检测到内存非法访问后会触发load page fault异常，并自动设置scause寄存器记录原因。内核恐慌时scause寄存器内的值为`0xd`，含义为加载地址错误，证实了结论。

#### What is the name of the process that was running when the kernel paniced? What is its process id (pid)?

  - 进程名：initcode，pid：1  

### System call tracing（`user/trace.c` 难度：中等）

#### 题目

在本作业中，你将添加一个系统调用跟踪功能，该功能可能有助于你调试后续的实验。你将创建一个新的trace系统调用来控制跟踪。它应该接受一个参数，即一个整数“掩码”，其位指定要跟踪的系统调用。例如，要跟踪`fork`系统调用，程序会调用`trace(1 << SYS_fork)`，其中`SYS_fork`是`kernel/syscall.h`中的系统调用编号。如果系统调用的编号在掩码中设置，则你必须修改`xv6`内核，使其在每个系统调用即将返回时打印一行。该行应包含进程 ID、系统调用名称和返回值；你无需打印系统调用参数。trace系统调用应该启用对调用它的进程及其后续分叉的任何子进程的跟踪，但不应影响其他进程。  

#### 步骤

- `Makefile`中添加`trace`
```makefile
UPROGS=\
	...
	$U/_trace\
```

- `user/user.h`中添加函数声明
```c
// system calls
int fork(void);
...
int trace(int);
```

- `user/usys.pl`中添加系统调用存根
```pl
entry("trace");
```

- `kernel/syscall.h`中添加系统调用号
```c
#define SYS_trace 22
```

- `kernel/proc.h`中添加`proc`结构体成员储存掩码
```c
struct proc {
  ...
  int tmask; // Trace mask
  ...
};
```

- `kernel/sysproc.c`末尾定义`sys_trace`函数
```c
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->tmask = mask;
  return 0;
}
```

- `kernel/proc.c`中修改`fork`函数定义
```c
int
fork(void)
{
  ...
  np->cwd = idup(p->cwd);
  np->tmask = p->tmask;

  safestrcpy(np->name, p->name, sizeof(p->name));
  ...
}
```

- `kernel/syscall.c`中修改如下标注的四处
```c
...
// Prototypes for the functions that handle system calls.
extern uint64 sys_fork(void);
...
extern uint64 sys_trace(void); // 1 添加外部声明 +1行
...
static uint64 (*syscalls[])(void) = {
...
[SYS_trace] sys_trace          // 2 添加入系统调用函数指针数组 +1行
};

static char *syscall_name[] = {
[SYS_fork]    "fork",
[SYS_exit]    "exit",
[SYS_wait]    "wait",
[SYS_pipe]    "pipe",
[SYS_read]    "read",
[SYS_kill]    "kill",
[SYS_exec]    "exec",
[SYS_fstat]   "fstat",
[SYS_chdir]   "chdir",
[SYS_dup]     "dup",
[SYS_getpid]  "getpid",
[SYS_sbrk]    "sbrk",
[SYS_sleep]   "sleep",
[SYS_uptime]  "uptime",
[SYS_open]    "open",
[SYS_write]   "write",
[SYS_mknod]   "mknod",
[SYS_unlink]  "unlink",
[SYS_link]    "link",
[SYS_mkdir]   "mkdir",
[SYS_close]   "close",
[SYS_trace]   "trace"
};                             // 3 添加系统调用名数组 +23行
...

void
syscall(void)
{
  ...
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    p->trapframe->a0 = syscalls[num]();
    // processing trace
    if ((p->tmask >> num) & 1) {
      printf("%d: syscall %s -> %d\n",
             p->pid, syscall_name[num], (int)p->trapframe->a0);
    }                          // 4 添加符合条件时打印追踪信息 +4行
    ...
}
```

### Attack xv6（`user/attack.c` 难度：中等）

#### 题目

`user/secret.c`在其内存中写入一个 8 字节的密钥，然后退出（这将释放其内存）。您的目标是向`user/attack.c`添加几行代码，以查找上次执行`secret.c`时写入内存的密钥，并将这 8 个密钥字节写入文件描述符 2。如果attacktest打印出`OK: secret is ebb.ebb`，您将获得满分。（注意： attacktest 每次运行的密钥可能不同。）。  

#### 简要分析

扫描每页前第8-31字节的特征前缀找到目标页，直接输出第32字节开始的8字节即可（前第0-7字节会被用于指针被覆写）。  

#### 代码

```c
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
```

### 测试

![syscall测试结果](./img/syscall.png)

## Lab: page tables（git checkout pgtbl）

### Inspect a user-process page table（`answers-pgtbl.txt` 难度：简单）

#### 题目

对于print_pgtbl输出 中的每个页表项，解释其逻辑上包含的内容以及其权限位。xv6 手册中的图 3.4 可能会有所帮助，但请注意，该图的页面集合可能与此处检查的进程略有不同。需要注意的是，xv6 不会将虚拟页面连续地放置在物理内存中。  

#### 输出

```text
va 0x0 pte 0x21FC885B pa 0x87F22000 perm 0x5B
va 0x1000 pte 0x21FC7C17 pa 0x87F1F000 perm 0x17
va 0x2000 pte 0x21FC7807 pa 0x87F1E000 perm 0x7
va 0x3000 pte 0x21FC74D7 pa 0x87F1D000 perm 0xD7
va 0x4000 pte 0x0 pa 0x0 perm 0x0
va 0x5000 pte 0x0 pa 0x0 perm 0x0
va 0x6000 pte 0x0 pa 0x0 perm 0x0
va 0x7000 pte 0x0 pa 0x0 perm 0x0
va 0x8000 pte 0x0 pa 0x0 perm 0x0
va 0x9000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF6000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF7000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF8000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF9000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFA000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFB000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFC000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFD000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFE000 pte 0x21FD08C7 pa 0x87F42000 perm 0xC7
va 0xFFFFF000 pte 0x2000184B pa 0x80006000 perm 0x4B
```

#### 解释

- va 0x0, perm 0x5B = V+R+X+U+A  
  用户可读/可执行，已访问；典型 .text/.rodata。
- va 0x1000, perm 0x17 = V+R+W+U  
  用户可读写（无执行）；典型**.data/.bss 或堆/栈页**（尚未出现 A/D，可能刚映射或未触达）。
- va 0x2000, perm 0x07 = V+R+W（无 U）  
  仅内核可读写，用户态不可访问。放在用户低地址空间而不带 U 不常见，值得关注（见下方“核查建议”）。
- va 0x3000, perm 0xD7 = V+R+W+U+A+D  
  用户可读写，且已访问且被写过；典型可写数据页。
- va 0x4000 ~ 0x9000, perm 0x0  
  未映射空洞。
- va 0xFFFF6000 ~ 0xFFFFD000, perm 0x0  
  高地址段多数为空洞，其中 0xFFFFD000 常作为栈保护页（guard page）。
- va 0xFFFFE000, perm 0xC7 = V+R+W+A+D（无 U/X）  
  trapframe：仅内核可读写，用户不可访问，且已访问/写过。
- va 0xFFFFF000, perm 0x4B = V+R+X+A（无 U/W）  
  trampoline：仅内核可读/可执行，用于态切换。

### Speed up system calls（难度：简单）

#### 题目

每个进程创建后，将一个只读页面映射到 USYSCALL（定义在memlayout.h中的虚拟地址）。在该页面的起始处，存储一个struct usyscall（同样定义在memlayout.h中），并将其初始化为当前进程的 PID。在本实验中，用户空间端已提供ugetpid() 函数，它将自动使用 USYSCALL 映射。如果在运行pgtbltest时ugetpid测试用例通过，您将获得本实验此部分的全部学分。  

#### 步骤

- `kernel/proc.h`中定义该页表指针
```c
...
struct proc {
...
struct usyscall *usyspgtbl;
};
```

- `kernel/proc.c`中改动如下四处：
```c
...
static struct proc*
allocproc(void)
{
  ...
  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  if ((p->usyspgtbl = (struct usyscall *)kalloc()) == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  memset(p->usyspgtbl, 0, PGSIZE);
  p->usyspgtbl->pid = p->pid;           // 1 申请只读共享页表 +7行
  ...
}
...
static void
freeproc(struct proc *p)
{
  ...
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  if (p->usyspgtbl)
    kfree((void *)p->usyspgtbl);
  p->usyspgtbl = 0;                     // 2 释放共享页表 +3行
}
...
pagetable_t
proc_pagetable(struct proc *p)
{
  ...
  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }
  
  if (mappages(pagetable, USYSCALL, PGSIZE,
               (uint64)(p->usyspgtbl), PTE_R | PTE_U) < 0) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, TRAPFRAME, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }                                     // 3 映射共享页 +7行
  ...
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmunmap(pagetable, USYSCALL, 1, 0); // 4 解映射共享页 +1行
  uvmfree(pagetable, sz);
}
...
```

### Print a page table（难度：简单）

#### 题目

我们添加了一个系统调用kpgtbl()，它会调用 vm.c中的vmprint()。它接受一个pagetable_t参数，你的任务是按照下面描述的格式打印该页表。当您运行print_kpgtbl()测试时，您的实现应该打印以下输出：
```text
page table 0x0000000087f22000
 ..0x0000000000000000: pte 0x0000000021fc7801 pa 0x0000000087f1e000
 .. ..0x0000000000000000: pte 0x0000000021fc7401 pa 0x0000000087f1d000
 .. .. ..0x0000000000000000: pte 0x0000000021fc7c5b pa 0x0000000087f1f000
 .. .. ..0x0000000000001000: pte 0x0000000021fc70d7 pa 0x0000000087f1c000
 .. .. ..0x0000000000002000: pte 0x0000000021fc6c07 pa 0x0000000087f1b000
 .. .. ..0x0000000000003000: pte 0x0000000021fc68d7 pa 0x0000000087f1a000
 ..0xffffffffc0000000: pte 0x0000000021fc8401 pa 0x0000000087f21000
 .. ..0xffffffffffe00000: pte 0x0000000021fc8001 pa 0x0000000087f20000
 .. .. ..0xffffffffffffd000: pte 0x0000000021fd4c13 pa 0x0000000087f53000
 .. .. ..0xffffffffffffe000: pte 0x0000000021fd00c7 pa 0x0000000087f40000
 .. .. ..0xfffffffffffff000: pte 0x000000002000184b pa 0x0000000080006000
```

#### 步骤

- `kernel/vm.c`中预留好的`vmprint`函数中实现
```c
...
static inline
uint64 sext39(uint64 x)
{
  if (x & ((uint64)1 << 38)) {
    x |= (~(uint64)0) << 39;
  }
  return x;
}

static void
vmprint_rec(const pagetable_t pt, const uint64 va_prefix, const int level)
{
  if (level < 0) {
    return;
  }

  for (int i = 0; i < 512; ++i) {
    const pte_t pte = pt[i];
    if ((pte & PTE_V) == 0) {
      continue; // 只打印有效项
    }

    const uint64 pa = PTE2PA(pte);
    const uint64 va = sext39((uint64)i << PXSHIFT(level) | va_prefix);

    for (int k = 0; k< (3 - level); ++k) {
      printf(" ..");  // 按深度缩进：顶层1个“ ..”
    }
    printf("%p: pte %p pa %p\n",
           (void *)va, (void *)pte, (void *)pa);

    // 若不是叶子（无 R/W/X），则继续向下打印子页表
    if ((pte & (PTE_R | PTE_W | PTE_X)) == 0) {
      vmprint_rec((pagetable_t)pa, va, level - 1);
    }
  }
}

void
vmprint(pagetable_t pagetable)
{
  // your code here
  printf("page table %p\n", (void *)pagetable);
  vmprint_rec(pagetable, 0, 2); // Sv39: 顶层 level=2
}
```

### Use superpages（难度：中等/困难）

#### 题目

你的任务是修改 xv6 内核以使用超级页面。具体来说，如果用户程序调用 sbrk() 函数，其大小为 2MB 或更大，并且新创建的地址范围包含一个或多个 2MB 对齐且大小至少为 2MB 的区域，则内核应该使用单个超级页面（而不是数百个普通页面）。如果在运行pgtbltest时superpg_test测试用例通过，你将获得本实验此部分的满分。  

#### 步骤

- `kernel/memlayout.h`中插入宏
```c
...
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)
#define SUPERBASE (KERNBASE + 2 * (1 << 20) * 12) // 超级页起始地址
...
```

- `kernel/defs.h`中添加`super`相关声明
```c
// kalloc.c
void*           kalloc(void);
void            kfree(void *);
void            kinit(void);
void*           superalloc(void);
void            superfree(void *);
void            superinit(void);
```

- `kernel/main.c`中添加`superinit`调用
```c
...
void
main()
{
  if (cpuid() == 0) {
    ...
    kinit();         // physical page allocator
    superinit();
    ...
}
```

- `kernel/kalloc.c`改动如下六处
```c
...
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem, supermem;                    // 1 增加超级页实例 修改1行

void
superfree(void *pa)
{
  if ((uint64)pa % SUPERPGSIZE ||
      (uint64)pa < SUPERBASE ||
      (uint64)pa >= PHYSTOP) {
    panic("superfree");
  }

  memset(pa, 1, SUPERPGSIZE);

  struct run *r = (struct run *)pa;
  acquire(&supermem.lock);
  r->next = supermem.freelist;
  supermem.freelist = r;
  release(&supermem.lock);
}                                    // 2 增加超级页释放 +17行

void *
superalloc(void)
{
  struct run *r;

  acquire(&supermem.lock);
  r = supermem.freelist;
  if (r) {
    supermem.freelist = r->next;
  }
  release(&supermem.lock);

  if (r) {
    memset(r, 5, SUPERPGSIZE);
  }
  return (void *)r;
}                                    // 3 增加超级页分配 +17行

void
superfreerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)SUPERPGROUNDUP((uint64)pa_start);
  while (p + SUPERPGSIZE <= (char *)pa_end) {
    superfree(p);
    p += SUPERPGSIZE;
  }
}                                    // 4 增加超级页批量释放 +10行 

void
superinit(void)
{
  initlock(&supermem.lock, "supermem");
  superfreerange((void *)SUPERBASE, (void *)PHYSTOP);
}                                    // 5 增加超级页初始化 + 11行

void
kinit(void)
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)SUPERBASE); // 6 减少普通页空间 修改1行
}
```

- `kernel/vm.c`中将几个函数修改至如下所示：
```c
...
pte_t *
superwalk(pagetable_t pagetable, uint64 va, int alloc) // 新增
{
  if (va >= MAXVA) {
    panic("superwalk");
  }

  pte_t *pte = pagetable + PX(2, va);
  if (*pte & PTE_V) {
    pagetable = (pagetable_t)PTE2PA(*pte);
    return pagetable + PX(1, va);
  } else {
    if (!alloc || (pagetable = (pte_t *)kalloc()) == 0) {
      return 0;
    }
    memset(pagetable, 0, PGSIZE);
    *pte = PA2PTE(pagetable) | PTE_V;
    return pagetable + PX(0, va);
  }
}
...
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm) // 修改
{
  uint64 a, last;
  uint64 sz = PGSIZE;
  pte_t *pte;

#ifdef LAB_PGTBL
  if (pa >= SUPERBASE) {
    sz = SUPERPGSIZE;
  }
#endif

  if((va % sz) != 0)
    panic("mappages: va not aligned");

  if((size % sz) != 0)
    panic("mappages: size not aligned");

  if(size == 0)
    panic("mappages: size");
  
  a = va;
  last = va + size - sz;
  for(;;){
#ifndef LAB_PGTBL
      if((pte = walk(pagetable, a, 1)) == 0) {
        return -1;
      }
#else
      if (sz == PGSIZE) {
        if ((pte = walk(pagetable, a, 1)) == 0) {
          return -1;
        }
      } else if ((pte = superwalk(pagetable, a, 1)) == 0) {
        return -1;
      }
#endif
    
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += sz;
    pa += sz;
  }
  return 0;
}
...
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free) // 修改
{
  uint64 a;
  pte_t *pte;
  int sz;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va, sz = PGSIZE; a < va + npages*PGSIZE; a += sz){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0) {
      printf("va=%ld pte=%ld\n", a, *pte);
      panic("uvmunmap: not mapped");
    }
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    uint64 pa = PTE2PA(*pte);
#ifdef LAB_PGTBL
    if (pa >= SUPERBASE) {
      a += SUPERPGSIZE - sz;
    }
#endif
    if(do_free){
#ifndef LAB_PGTBL
      kfree((void*)pa);
#else
      if (pa >= SUPERBASE) {
        superfree((void *)pa);
      } else {
        kfree((void *)pa);
      }
#endif
    }
    *pte = 0;
  }
}
...
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm) // 修改
{
  char *mem;
  uint64 a;
  int sz;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz, sz = PGSIZE;
#ifndef LAB_PGTBL
      a < newsz;
#else
      a < SUPERPGROUNDUP(oldsz) && a < newsz; // 利用超级页对齐浪费的空间
#endif
      a += sz){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
#ifndef LAB_SYSCALL
    memset(mem, 0, sz);
#endif
    if(mappages(pagetable, a, sz, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
#ifdef LAB_PGTBL
  // 尝试申请超级页
  for (sz = SUPERPGSIZE; a + SUPERPGSIZE < newsz; a += sz) {
    mem = superalloc();
    if (mem == 0) {
      break;
    }
    memset(mem, 0, sz);
    if (mappages(pagetable, a, sz, (uint64)mem, PTE_R | PTE_U | xperm) != 0) {
      superfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  // 普通页补齐剩余空间
  for (sz = PGSIZE; a < newsz; a += sz) {
    mem = kalloc();
    if (mem == 0) {
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, sz);
    if (mappages(pagetable, a, sz, (uint64)mem, PTE_R | PTE_U | xperm) != 0) {
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
#endif
  return newsz;
}
...
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz) // 修改
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;
  int szinc;

  for(i = 0; i < sz; i += szinc){
    szinc = PGSIZE;
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
#ifndef LAB_PGTBL
    if((mem = kalloc()) == 0)
      goto err;
#else
    if (pa >= SUPERBASE) {
      szinc = SUPERPGSIZE;
      if ((mem = superalloc()) == 0) {
        goto err;
      }
    } else if ((mem = kalloc()) == 0) {
      goto err;
    }
#endif
    memmove(mem, (char *)pa, szinc);
    if(mappages(new, i, szinc, (uint64)mem, flags) != 0){
#ifndef LAB_PGTBL
      kfree(mem);
#else
      if (szinc == PGSIZE) {
        kfree(mem);
      } else {
        superfree(mem);
      }
#endif
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
...
```

### 测试

![pgtbl测试结果](./img/pgtbl.png)

