# Report for Lab 6

## 练习 1: 使用 Round Robin 调度算法

**请理解并分析 `sched_class` 中各个函数指针的用法，并结合 Round Robin 调度算法描述 ucore 的调度执行过程**

`sched_class`各个函数指针的功能如下，

+ `init`: 初始化就绪队列`run_queue`
+ `enqueue`: 将进程加入就绪队列，维护进程的时间片
+ `dequeue`: 将进程移出就绪队列
+ `pick_next`: 找到就绪队列的队首进程
+ `proc_tick`: 每次时钟中断时，将进程的时间片减一，若减到零则设置重新调度标志

ucore 的调度是通过`schedule`函数实现的，其执行过程如下，

+ 将当前进程换出，加入到就绪队列(`enqueue`)
+ 找到就绪队列的队首(`pick_next`)，将其出队(`dequeue`)，作为新进程
+ 切换到新进程执行(`proc_run`)

此外，在每次时钟中断时，会调用`proc_tick`更新进程的时间片，若时间片用完，则中断返回前会重新执行`schedule`进行调度。

**请在实验报告中简要说明如何设计实现”多级反馈队列调度算法“，给出概要设计，鼓励给出详细设计**

在现有框架下，可以新增一个多级反馈队列调度算法类，并实现以下方法，

+ `init`: 初始化多个就绪队列，分别代表不同的优先级
+ `enqueue`: 如果进程时间片已经用完，则降低其优先级，根据进程的优先级，将其加入对应的就绪队列，设置时间片大小，优先级越高的进程时间片越短
+ `dequeue`: 将进程移出指定的就绪队列
+ `pick_next`: 根据优先级，选择下一个进程，最低优先级的进程采用Round Robin调度算法，其它优先级的进程采用FCFS调度算法
+ `proc_tick`: 每次时钟中断时，将进程的时间片减一，若减到零则设置重新调度标志

此外，如果进程由于I/O等原因主动放弃CPU，则下次入队时，有两种处理方式，

+ 优先级不变，仍加入同一队列
+ 优先级提高一个等级，加入高一级队列

## 练习 2: 实现 Stride Scheduling 调度算法

**设计实现过程**

这里采用斜堆`skew_heap`实现就绪队列`run_pool`，需要实现以下函数，

+ `init`: 初始化就绪队列`run_pool`
+ `enqueue`: 将进程加入就绪队列，维护进程的时间片
+ `dequeue`: 将进程移出就绪队列
+ `pick_next`: 找到`stride`最小的进程，并根据优先级增加它的`stride`，增量与优先级成反比
+ `proc_tick`: 每次时钟中断时，将进程的时间片减一，若减到零则设置重新调度标志

对于`BIG_STRIDE`的选取，考虑到`stride`的比较函数是通过作差判断的，并且队列中所有进程的`stride`的最大值与最小值之差不超过`BIG_STRIDE`，因此，只要`BIG_STRIDE <= 0x7FFFFFFF`，作差时就不会溢出，为了支持更多的优先级，可以直接取为`0x7FFFFFFF`。

## 我的实现与参考答案的区别

+ 练习2：参考答案实现了链表和斜堆的版本，我仅实现了斜堆版本

## 本实验对应的OS知识点

+ 练习1：Round Robin调度算法
+ 练习2：Stride Scheduling调度算法

## 本实验未对应的OS知识点

+ 短进程优先，公平共享等单处理机调度算法
+ O1，BFS等多处理机调度算法
+ 优先级反置和优先级继承
