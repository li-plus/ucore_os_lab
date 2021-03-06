# Report for Lab 8

## 练习 1: 完成读文件操作的实现

**设计实现过程**

需要实现`sfs_inode.c`中的`sfs_io_nolock`函数，该函数将用户对inode的读写操作转换为对设备的读写操作。

需要注意的是，用户读写的首尾地址不一定是按块对齐的。对于首尾的非对齐部分，可以使用`sfs_buf_op`函数读写指定的大小，对于中间的对齐部分，可以使用`sfs_block_op`函数进行整块的读写操作。

事实上，`sfs_buf_op`是对`sfs_block_op`的一层封装，具体实现为，首先从磁盘中读出一整块数据到`sfs->sfs_buffer`，如果是读操作，则将所需部分拷贝到最终的`buf`并返回，如果是写操作，则将传入的`buf`写入`sfs->sfs_buffer`的所需位置，然后将整块写回设备。

**实现“UNIX 的 PIPE 机制”的概要设计方案**

管道机制可以通过文件系统实现，具体实现流程如下，

+ 当用户进程需要通过管道通信时，向操作系统发出系统调用，请求创建一个管道
+ 操作系统创建一个管道文件，映射到一块内存区域中，并返回文件描述符给用户进程
+ 管道文件可采用环形队列结构，分为读端和写端，初始时均指向文件起始位置
+ 用户进程可通过标准的文件读写操作访问管道，构成一个生产者-消费者问题，可由操作系统来保证读写的互斥性
+ 用户关闭管道时，操作系统将管道文件删除，释放相应的内存空间

## 练习 2: 完成基于文件系统的执行程序机制的实现

**设计实现过程**

实现过程大致如下，

+ 为用户进程创建一个虚拟地址映射
+ 为该映射分配一页，创建页目录
+ 根据文件描述符，加载ELF程序到用户内存空间
+ 建立用户栈
+ 设置用户的cr3
+ 设置参数`argc`和`argv`
+ 设置中断帧

**实现基于“UNIX 的硬链接和软链接机制”的概要设计方案**

对于硬链接，例如`ln TARGET LINK_NAME`，首先找到`TARGET`文件指向的inode，然后创建`LINK_NAME`文件，使它指向的inode与`TARGET`相同，此时该inode的引用计数应当加一。

对于软链接，例如`ln -s TARGET LINK_NAME`，首先创建`LINK_NAME`文件，并分配一个空闲的inode和对应的数据块，标记inode的类型为软链接，数据块内存储`TARGET`的路径。访问软链接时，操作系统需要读出软链接文件存储的目标路径，然后访问该路径指向的文件。

可以这么理解，**硬链接是指向inode的引用**，inode的引用计数就是硬链接的个数，当硬链接减少到零时，inode和数据块将被释放，显然，常规文件就是一个硬链接。**软链接是指向硬链接的指针**，它是一个独立的文件，有自己的inode，数据块存储了目标文件的路径，因此软链接的创建和删除对目标文件没有任何影响。

## 我的实现与参考答案的区别

+ 练习1：基本一致
+ 练习2：基本一致

## 本实验对应的OS知识点

+ 练习1：文件系统的基本概念
+ 练习2：用户进程的加载

## 本实验未对应的OS知识点

+ 空闲空间管理
+ 冗余磁盘阵列RAID
