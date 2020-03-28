# Report for Lab 3

## 练习 1：给未被映射的地址映射上物理页

**设计实现过程**

修改`kern/mm/vmm.c`，需要实现的代码如下。

```c
int
do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr) {
    // ...
    ptep = get_pte(mm->pgdir, addr, 1);
    if (ptep == NULL) {
        cprintf("get_pte failed\n");
        goto failed;
    }

    if (*ptep == 0) {
        struct Page *page = pgdir_alloc_page(mm->pgdir, addr, perm);
        if (page == NULL) {
            cprintf("pgdir_alloc_page failed\n");
            goto failed;
        }
    }
    // ...
}
```

当页表项无效时，CPU会抛出缺页异常，在`kern/trap/trap.c:trap_dispatch`的中断处理例程将会调用`do_pgfault`函数来处理。

该函数已经实现了对一些非法内存访问的处理，如访存地址不在任何一个虚拟地址空间`vma`中，对应的`vma`的无读写权限等等。在此练习中，需要实现给未被映射的地址映射上物理页，首先获取虚拟地址的PTE，如果PTE为0，即不存在对应的物理页面，则只需调用`pgdir_alloc_page`，即可为该虚拟地址分配一个物理页面，并设置PTE的物理页号和标志位。

**请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对 ucore 实现页替换算法的潜在用处。**

PDE和PTE的结构请参见[lab2.md](../lab2/lab2.md)，对于实现页面置换算法，PDE的标志位用处不大，在PTE中，使用位（Accessed）和修改位（Dirty）可以用在扩展时钟置换算法中，Avail段可以在FRU算法中记录访问次数。

**如果 ucore 的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？**

CPU将保存现场，抛出Double Fault异常，并进入相应的中断处理例程，由操作系统接管，如果此时再次出现异常，就会抛出Triple Fault异常。不过这些情况极少发生，可能是由内核的编码错误所导致的。

## 练习 2：补充完成基于 FIFO 的页面替换算法

**设计实现过程**

在`kern/mm/vmm.c`中需要实现的代码如下。

```c
int
do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr) {
    // ...
    ptep = get_pte(mm->pgdir, addr, 1);
    if (ptep == NULL) {
        cprintf("get_pte failed\n");
        goto failed;
    }

    if (*ptep == 0) {
        // ...
    } else {
        if (swap_init_ok) {
            struct Page *page = NULL;
            ret = swap_in(mm, addr, &page);
            if (ret != 0) {
                cprintf("swap_in failed\n");
                goto failed;
            }
            page_insert(mm->pgdir, page, addr, perm);
            swap_map_swappable(mm, addr, page, 1);
            page->pra_vaddr = addr;
        } else {
            cprintf("no swap_init_ok but ptep is %x, failed\n", *ptep);
            goto failed;
        }
    }
    // ...
}
```

在`kern/mm/swap_fifo.c`中需要实现的代码如下。

```c
static int
_fifo_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    list_entry_t *entry=&(page->pra_page_link);
 
    assert(entry != NULL && head != NULL);
    list_add_before(head, entry);
    return 0;
}

static int
_fifo_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
        assert(head != NULL);
    assert(in_tick==0);

    list_entry_t *le = head->next;
    assert(le != head);
    *ptr_page = le2page(le, pra_page_link);
    list_del(le);
    return 0;
}
```

在此练习中，需要考虑页表项存在的情况，既然页表项存在，又触发了Page Fault，那么物理页面一定位于交换分区中，这时候首先需要分配一个新的页面（`alloc_page`），若物理内存已满无法分配，则首先利用页面置换算法将某个物理页面换出到交换分区（`swap_out`），然后再分配页面，接下来将交换分区中的页面加载到该内存页面中（`swap_in`），在页表中插入该映射的页表项并更新`Page`数组的内容（`page_insert`），然后通知页面置换算法有新的页面换入内存，算法做相应处理（`swap_map_swappable`）。

在FIFO页面置换算法的实现中，如果有页面换入，则将其添加到队列尾部，如果需要换出页面，则令队列头部的页面出队即可。

**如果要在 ucore 上实现"extended clock 页替换算法"请给你的设计方案，现有的 swap_manager 框架是否足以支持在 ucore 中实现此算法？如果是，请给你的设计方案。如果不是，请给出你的新的扩展和基此扩展的设计方案。并需要回答如下问题**

可以实现，设计方案详见Challenge 1。

Q1：需要被换出的页的特征是什么？

A1：该页的PTE中使用位（Accessed）和修改位（Dirty）均为0。

Q2：在 ucore 中如何判断具有这样特征的页？

A2：利用`get_pte`获取PTE，并判断其对应标志位即可。

Q3：何时进行换入和换出操作？

A3：当产生缺页异常，且物理页面不在内存中时，进行换入操作。当物理内存空间不够，无法分配页面时，进行换出操作。

## 扩展练习 Challenge 1：实现识别 dirty bit 的 extended clock 页替换算法

**设计实现过程**

对于改进的Clock算法，需要维护一个双向环形链表以及当前链表节点，每个节点对应一个页面。每次换入页面时，需要将新页面插入到当前节点的后面。每次换出页面时，从当前节点开始顺序检查环形链表的每个节点，根据节点对应PTE的使用位和修改位进行相应处理即可，详见下表。

| Accessed | Dirty | ->   | Accessed | Dirty |
| -------- | ----- | ---- | -------- | ----- |
| 0        | 0     | ->   | swap     | out   |
| 0        | 1     | ->   | 0        | 0     |
| 1        | 0     | ->   | 0        | 0     |
| 1        | 1     | ->   | 0        | 1     |

**测试**

测试样例选取课件9-4中的例子，设置内存大小为四个页面大小，初始时内存中依次有`a, b, c, d`四个页面，它们的使用位和修改位均为0，指针指向页面`a`，然后依次访问页面`c, a_w, d, b_w, e, b, a_w, b, c, d`，其中`_w`表示对页面的写操作，否则为读操作。按照课件，应当在第5次访存时换出页面c，第9次访存时换出页面d，第10次访存时换出页面b。

需要注意的是，初始化测试时驻留页面的页表项的使用位和修改位并非全0，因此需要清除这些标志位，并调用`tlb_invalidate`刷新TLB，才能保证测试环境与课件相符。

**运行**

如需运行改进的Clock算法，需要在`swap.c`中`#include <swap_ext_clock.h>`，然后令`sm = &swap_manager_ext_clock;`。程序输出如下，通过了测试样例，符合要求。

```
SWAP: manager = extended clock swap manager
BEGIN check_swap: count 1, total 31962
setup Page Table for vaddr 0X1000, so alloc a page
setup Page Table vaddr 0~4MB OVER!
set up init env for check_swap begin!
page fault at 0x00001000: K/W [no page found].
page fault at 0x00002000: K/W [no page found].
page fault at 0x00003000: K/W [no page found].
page fault at 0x00004000: K/W [no page found].
set up init env for check_swap over!
read Virt Page c in ext_clock_check_swap
write Virt Page a in ext_clock_check_swap
read Virt Page d in ext_clock_check_swap
write Virt Page b in ext_clock_check_swap
read Virt Page e in ext_clock_check_swap
page fault at 0x00005000: K/R [no page found].
swap_out: i 0, store page in vaddr 0x3000 to disk swap entry 4
read Virt Page b in ext_clock_check_swap
write Virt Page a in ext_clock_check_swap
read Virt Page b in ext_clock_check_swap
read Virt Page c in ext_clock_check_swap
page fault at 0x00003000: K/R [no page found].
swap_out: i 0, store page in vaddr 0x4000 to disk swap entry 5
swap_in: load disk swap entry 4 with swap_page in vadr 0x3000
read Virt Page d in ext_clock_check_swap
page fault at 0x00004000: K/R [no page found].
swap_out: i 0, store page in vaddr 0x2000 to disk swap entry 3
swap_in: load disk swap entry 5 with swap_page in vadr 0x4000
count is 0, total is 7
check_swap() succeeded!
```

## 我的实现与参考答案的区别

+ 练习1：基本一致
+ 练习2：我实现的FIFO的头部和尾部与答案恰好相反

## 本实验对应的OS知识点

+ 练习1：缺页异常的中断处理例程
+ 练习2：FIFO页面置换算法
+ 扩展练习 Challenge 1：改进的Clock页面置换算法

## 本实验未对应的OS知识点

+ 全局页面置换算法，如工作集置换算法，缺页率置换算法等。
