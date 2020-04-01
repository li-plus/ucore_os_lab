# Report for Lab 4

## 练习 1：分配并初始化一个进程控制块

**设计实现过程**

修改`kern/process/proc.c`，代码如下，实现思路如注释所述。

```c
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        proc->state = PROC_UNINIT;  // 进程状态，为未初始化
        proc->pid = -1;             // 进程ID，初始化为-1
        proc->runs = 0;             // 运行次数，初始化为0
        proc->kstack = 0;           // 内核栈虚拟地址，初始化为0
        proc->need_resched = 0;     // 不需要调度
        proc->parent = NULL;        // 父进程，初始化为NULL
        proc->mm = NULL;            // 虚拟内存管理器，对内核线程无用
        memset(&(proc->context), 0, sizeof(struct context));  // 进程上下文信息
        proc->tf = NULL;            // 中断帧，初始化为NULL
        proc->cr3 = boot_cr3;       // 一级页表的物理基址
        proc->flags = 0;            // 进程标志，lab4无用
        memset(proc->name, 0, PROC_NAME_LEN);   // 进程名字，初始化为空
    }
    return proc;
}
```

**请说明`proc_struct`中`struct context context`和`struct trapframe *tf`成员变量含义和在本实验中的作用**

`context`保存了当前进程的上下文信息，包括通用寄存器和指令寄存器，用于进程切换。

`tf`保存了中断帧的信息，包括通用寄存器，指令寄存器，段寄存器，异常号等等，用于中断处理。

## 练习 2：为新创建的内核线程分配资源

**设计实现过程**

修改`kern/process/proc.c`，代码如下，设计思路如注释所述。

```c
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;

    // 分配一个进程控制块
    proc = alloc_proc();
    if (proc == NULL) {
        goto fork_out;
    }

    // 设置父进程
    proc->parent = current;

    // 分配一个内核栈
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }

    // 复制原进程的内存管理信息到新进程，但这里的内核线程并不需要
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }

    // 复制原进程上下文到新进程
    copy_thread(proc, stack, tf);

    // 关中断
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        // 分配PID
        proc->pid = get_pid();
        // 将新进程添加到哈希表中，方便查找
        hash_proc(proc);
        // 将新进程添加到进程列表
        list_add(&proc_list, &(proc->list_link));
        // 更新进程数
        ++nr_process;
    }
    // 开中断
    local_intr_restore(intr_flag);

    // 唤醒新进程
    wakeup_proc(proc);

    // 返回新进程号
    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```

**请说明 ucore 是否做到给每个新 fork 的线程一个唯一的 id？请说明你的分析和理由。**

能做到。ucore在`do_fork`中先关中断，然后才调用`get_pid`，因此分配PID时不会被打断，在`get_pid`内，有避免PID重复分配的检查，使得PID不会超过其最大值，且不会与未终止的进程重复。

## 练习 3：阅读代码，理解 `proc_run` 函数和它调用的函数是如何完成进程切换的。

完成进程切换的过程如注释所述。

```c
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        // 关中断
        local_intr_save(intr_flag);
        {
            // 将当前进程指针指向新进程
            current = proc;
            // 设置TSS的esp0，使得在用户线程内中断时，CPU能够切换到对应的内核栈
            load_esp0(next->kstack + KSTACKSIZE);
            // 切换cr3，从当前页表切换到新进程的页表
            lcr3(next->cr3);
            // 切换进程上下文，switch_to会将自己的返回地址置为新进程的入口地址，
            // 即kernel_thread_entry，当ret返回时直接切换到新进程执行
            switch_to(&(prev->context), &(next->context));
        }
        // 开中断
        local_intr_restore(intr_flag);
    }
}
```

Q1: 在本实验的执行过程中，创建且运行了几个内核线程？

A1: 创建且运行了`idle`和`init`两个内核线程。

Q2: 语句`local_intr_save(intr_flag);....local_intr_restore(intr_flag);`在这里有何作用?请说明理由。

A2: 该语句的作用是，先关中断，执行完中间的代码后，再开中断。这样做是为了保证进程切换是一个原子操作，避免造成不可预料的错误。

## 我的实现与参考答案的区别

+ 练习1：基本一致
+ 练习2：基本一致，一开始将所有操作都放在关中断环境下进行，事实上只需在写共享变量时关中断即可，如分配PID，更新进程数等操作，后来根据参考答案进行了修改

## 本实验对应的OS知识点

+ 练习1：进程控制块的初始化
+ 练习2：内核线程的创建
+ 练习3：进程的切换

## 本实验未对应的OS知识点

+ 进程退出时的资源回收工作
+ 三状态进程模型
