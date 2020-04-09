# Report for Lab 5

## 练习 1: 加载应用程序并执行

**设计实现过程**

修改`kern/process/proc.c`，需要填写的代码如下，

```c
static int
load_icode(unsigned char *binary, size_t size) {
    // ...
    tf->tf_cs = USER_CS;            // 代码段寄存器，设为用户的USER_CS
    tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS; // 数据段寄存器，设为用户的USER_DS
    tf->tf_esp = USTACKTOP;         // esp寄存器，设为用户栈顶
    tf->tf_eip = elf->e_entry;      // eip寄存器，设为用户代码入口
    tf->tf_eflags = FL_IF;          // eflags寄存器，打开中断
    // ...
}
```

**当创建一个用户态进程并加载了应用程序后，CPU 是如何让这个应用程序最终在用户态执行起来的？即这个用户态进程被 ucore 选择占用 CPU 执行（RUNNING 态）到具体执行应用程序第一条指令的整个经过。**

以下是CPU初始化到执行应用程序第一条指令的过程。

+ 在CPU在初始化过程中，会调用`proc_init`创建一个idle进程和一个init进程
+ 初始化完成后，会进入idle状态，不断调用`schedule`来寻找`RUNNABLE`状态的进程
+ 首先找到的是init进程，然后调用`switch_to`切换到init，在`init_main`里面会通过`kernel_thread`创建一个入口为`user_main`的用户进程，并重新调用`schedule`
+ 这时候就找到了这个`RUNNABLE`的用户进程，然后再次调用`switch_to`切换到该用户进程，进入`user_main`，调用`kernel_execve`，抛出一个类型为`SYS_exec`的系统调用
+ 进入系统调用的中断处理例程，根据`eax`中保存的系统调用号，会进入`sys_exec`函数
+ 进一步调用`do_execve`，切换到内核页表，释放当前进程的内存空间
+ 然后调用`load_icode`，为用户进程分配内存空间，配置好用户页表，加载ELF文件，分配用户栈，然后将中断帧的段寄存器改为用户态，设置esp指向用户栈顶，eip指向应用程序入口，eflags寄存器打开中断
+ `SYS_exec`系统调用返回，跳转到中断帧eip指向的应用程序入口，开始执行用户程序的第一条指令

## 练习 2: 父进程复制自己的内存空间给子进程

**设计实现过程**

修改`kern/mm/pmm.c`，需要填写的代码如下，

```c
int
copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share) {
    // ...
    // 原进程某页面的物理地址
    void * src_kvaddr = page2kva(page);
    // 新页面的物理地址
    void * dst_kvaddr = page2kva(npage);
    // 将原页面复制到新页面
    memcpy(dst_kvaddr, src_kvaddr, PGSIZE);
    // 将新页面插入目标进程的页表
    ret = page_insert(to, npage, start, perm);
    // ...
}
```

**如何设计实现”Copy on Write 机制“？给出概要设计**

可能的做法是，当`do_fork`拷贝内存的时候，不是立刻将内存复制一份，而是将新进程的页表项指向原进程的内存页面，并将新进程的页表项置为只读，这样在新进程写页面的时候就会触发Page Fault，然后在中断处理例程中对页面进行复制即可。

## 练习 3: 阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现

**对 fork/exec/wait/exit 函数的分析**

+ `do_fork`: 用于创建进程。主要功能是，创建进程控制块PCB，分配内核栈，拷贝原进程内存映射信息，拷贝原进程上下文，分配进程号PID，设置进程的父子关系和兄弟关系。
+ `do_execve`: 用于执行进程。主要功能是，先从旧进程页表切换到内核页表，释放旧进程内存空间，然后为新进程创建虚拟内存映射，加载应用程序的ELF文件，设置用户态段寄存器，设置应用程序入口点，并从内核页表切换到新进程页表，函数返回时就会开始执行应用程序。
+ `do_wait`: 用于等待进程。主要功能是，等待当前进程的任一子进程（或指定的某个子进程）进入僵尸状态，若找到僵尸子进程，则释放其内存空间并将其销毁，若找不到，则重新调度，直到找到为止。
+ `do_exit`: 用于销毁进程。主要功能是，将页表切换到内核页表，释放进程的虚拟内存映射，将进程设置为僵尸状态，记录进程返回值，唤醒等待中的父进程，维护进程的父子关系和兄弟关系，最后重新调度。

Q1: 请分析 fork/exec/wait/exit 在实现中是如何影响进程的执行状态的？

A1: 它们对进程的执行状态进行修改，而调度器对不同状态的进程有不同的调度策略。

Q2: 请给出 ucore 中一个用户态进程的执行状态生命周期图（包执行状态，执行状态之间的变换关系，以及产生变换的事件或函数调用）。（字符方式画即可）

A2: 如下图所示。

```                            
  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+ 
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  + 
                                           -----------------------wakeup_proc----------------------------------
```

## 我的实现与参考答案的区别

+ 练习1：基本一致
+ 练习2：基本一致

## 本实验对应的OS知识点

+ 练习1：用户进程的创建，应用程序的加载和执行
+ 练习2：进程空间分配
+ 练习3：进程状态模型

## 本实验未对应的OS知识点

+ 短进程优先，最高响应比优先等进程调度算法
