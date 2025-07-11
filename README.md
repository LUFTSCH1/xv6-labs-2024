# xv6-riscv实验报告  

> 2253713 戴金瓯  

实验版本：[2024](https://pdos.csail.mit.edu/6.828/2024/)  

## 配置实验环境  

物理机环境：`Windows 11 (x64)`  
[官方文档-安装说明部分](https://pdos.csail.mit.edu/6.828/2024/tools.html) 给出了许多环境的配置方式，这里选择使用`WSL2`进行实验环境配置，代码编辑器选用`Visual Studio Code`。  

### 安装`WSL发行版`  

打开`Microsoft Store`应用，搜索`Debian`并安装即可，本次实验所使用的版本为`Debian 12.9`，安装完成可以直接点击“打开”、在“开始”菜单旁的“搜索”功能输入`Debian`或是直接在`CMD`/`Power Shell`中输入`bash`启动（若安装了多个`WSL发行版`，则应注意该方式启动的是默认发行版）  

### 配置`WSL发行版`  

#### 准备编辑器智能提示支持  

```bash
sudo apt install -y clangd bear gcc-multilib
```
简单解释一下安装内容：  
  * `clangd`：语言服务器核心  
  * `bear`：编译指令捕获工具  
  * `gcc-multilib`：跨架构编译支持库  

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

## Lab Utilities（git checkout util）  

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

int main(int argc, char *argv[]) {
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
  * `文件描述符`：`文件描述符`指向一个`打开的文件对象`，但其并非一对一的关系，而是一对多的关系。就像房卡与房间，可能有多名客人入住酒店的一间多床房间，每名客人都有一张可以开启房门的房卡，只有当所有客人都归还房卡（客人视角的“退房”操作）时，才可以让工作人员前往“释放”这个房间。对于`文件描述符`和`打开的文件对象`而言，这就是在底层增加了一个`引用计数`，其数值就是使用这个文件的进程数。`fork()`复制进程时会增加这个引用计数。可以手动`close(文件描述符)`来减少`引用计数`，也可以让操作系统自动在进程退出后减少对应的`引用计数`。`打开的文件对象`将在`引用计数`归零时释放。  
  * `pipe`：管道是一种虚拟文件，打开管道`int pipe(int p[])`将生成一对读、写文件描述符分别存入`&p[0]`、`&p[1]`处。在`打开的读、写文件对象`的`引用计数`均归零后管道会被释放。  
  * `wait`：子进程执行完后不会完全消失，而是进入`僵尸进程`状态，需要父进程调用`int wait(int *status)`进行回收，这是`wait`的根本作用。另外，由于父子进程执行顺序完全取决于操作系统的调度，尽管调度算法是确定的，但实际情况相当复杂，故可以认为执行顺序不确定。调整父进程中`wait`调用的位置可以保证其之后的语句一定在子进程退出后执行，在必要时可以通过`status`确定子进程执行是否成功。  

#### 代码  

```c
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

static void errquit(const int status, const char prompt[]) {
  fprintf(2, "%s\n", prompt);
  exit(status);
}

static void prime(const int rfd) {
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

int main(int argc, char *argv[]) {
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

需要阅读`user/ls.h`来了解如何读取目录。