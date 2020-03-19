# Report for Lab 2

## 练习 0：填写已有实验

将 Lab 1 的`kdebug.c`, `init.c`, `trap.c`中的代码移植到 Lab 2 即可。

## 练习 1：实现 first-fit 连续物理内存分配算法

**设计实现过程**

修改`default_pmm.c`如下，设计和实现思路见注释。

```c
static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    // 由于page_init中是按地址顺序init_memmap的，
    // 并且first-fit算法也需要按地址排序，所以这里每次都插到链表最后。
    list_add_before(&free_list, &(base->page_link));
}

static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) {
        if (page->property > n) {
            // 空闲块大小大于所需大小，需要进行分割，
            // 首先找到多余部分的起始页，作为空闲块。
            struct Page *p = page + n;
            // 设置空闲块的大小
            p->property = page->property - n;
            // 将PG_property置为1，表示此空闲块有效。
            SetPageProperty(p);
            // 将新的空闲块插到原来的空闲块后面
            list_add_after(&(page->page_link), &(p->page_link));
        }
        // 删除原来的空闲块。
        list_del(&(page->page_link));
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}

static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    list_entry_t *le = list_next(&free_list);
    // 找到base后面的那块空闲块le及其对应的页p
    while (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property <= p) {
            break;
        }
        le = list_next(le);
    }
    // 找到base前面那块空闲块le_prev及其对应的页p_prev
    list_entry_t *le_prev = list_prev(le);
    struct Page *p_prev = le2page(le_prev, page_link);
    // 判断是否跟前面的空闲块合并，需要判断连续性，以及le_prev是否为guard(free_list)
    if (le_prev != &free_list && p_prev + p_prev->property == base) {
        p_prev->property += base->property;
        ClearPageProperty(base);
        base = p_prev;
        list_del(&(p_prev->page_link));
    }
    // 判断是否跟后面的空闲块合并，同样需要判断连续性，以及le是否为guard。
    if (le != &free_list && base + base->property == p) {
        base->property += p->property;
        ClearPageProperty(p);
        // 删掉后面的空闲块之前要对le进行维护，保证最后的list_add_before能正常进行
        le = list_next(le);
        list_del(&(p->page_link));
    }
    // 更新guard里面存的总空闲大小。
    nr_free += n;
    // 将base对应的空闲块插入到空闲列表中的le前面
    list_add_before(le, &(base->page_link));
}
```

**你的 first fit 算法是否有进一步的改进空间**

在执行效果方面，first-fit算法会在低地址空间造成严重的碎片化。可能的改进方法是，根据实际情况采用best-fit或worst-fit算法。

在执行效率方面，first-fit每次分配或释放空间时，都需要遍历链表，时间复杂度为$O(n)$。可能的改进方法是，可以将空闲块列表按地址组织成二叉树，这样每次分配需要$O(n)$时间，但释放只需要$O(\log n)$时间。

进一步的改进方法是直接采用buddy system，能有效缓解碎片化问题，且每次分配和释放空间都仅需$O(\log n)$时间。

## 练习 2：实现寻找虚拟地址对应的页表项

**设计实现过程**

修改`pmm.c:get_pte`如下，设计和实现思路见注释。

```c
pte_t *
get_pte(pde_t *pgdir, uintptr_t la, bool create) {
    // 这里的pgdir是cr3，la是虚拟地址，
    // 取la的最高10位与pgdir相加，就得到一级页表项PDE
    pde_t *pdep = pgdir + PDX(la);
    // 如果PDE不存在
    if (!(*pdep & PTE_P)) {
        // 如果不指定创建，那只好返回空
        if (!create) {
            return NULL;
        }
        // 分配一页空间，创建二级页表
        struct Page *page = alloc_page();
        if (page == NULL) {
            return NULL;
        }
        // 设置其引用计数为1，因为只有PDE指向它。
        set_page_ref(page, 1);
        // 计算该页的内核虚拟地址，初始化整页为全零。
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        // 设置PDE的物理地址，存在位，可写位，以及访问权限
        *pdep = pa | PTE_U | PTE_W | PTE_P;
    }
    // 取出PDE存的PPN，转换为内核虚拟地址，然后加上la的高11-20位，就得到PTE的虚拟地址。
    pte_t *ptep = (pte_t *)KADDR(PDE_ADDR(*pdep)) + PTX(la);
    return ptep;
}
```

**请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中每个组成部分的含义以及对 ucore 而言的潜在用处。**

页目录项PDE结构示意图如下

```
 31                             12 11   9 8 7 6 5 4 3 2 1 0
+---------------------------------+------+-+-+-+-+-+-+-+-+-+
| Page Table 4-kb aligned Address |Avail.|G|S|0|A|D|W|U|R|P|
+---------------------------------+------+-+-+-+-+-+-+-+-+-+
```

其中

```
Page Table 4-kb aligned Address: 二级页表的物理页号，已被ucore使用
Avail.: CPU不用的空间，可供ucore的页面替换算法使用
G: Ignored，没用
S: Page Size，页面大小，可在ucore支持可变页面大小时使用
A: Accessed，是否被访问过，可在ucore与cache交互时使用
D: Cache Disabled，是否允许高速缓存，可在ucore与cache交互时使用
W: Write Through，是否采用写穿透策略，可在ucore与cache交互时使用
U: User/Supervisor，表示访问权限，已被ucore使用
R: Read/Write，表示是否可写，已被ucore使用
P: Present，表示是否存在，已被ucore使用
```

页表项PTE结构示意图如下

```
 31                   12 11   9 8 7 6 5 4 3 2 1 0
+-----------------------+------+-+-+-+-+-+-+-+-+-+
| Physical Page Address |Avail.|G|0|D|A|C|W|U|R|P|
+-----------------------+------+-+-+-+-+-+-+-+-+-+
```

其中

```
Physical Page Address: 物理页号，已被ucore使用
Avail.: 同PDE
G: Global，表示全局页，防止TLB更新它的地址，在与TLB交互时用到
D: Dirty，表示该页是否被写过，在页替换时需要用到
A: Accessed，同PDE
C: Cache Disabled，同PDE
W: Write Through，同PDE
U: User/Supervisor，同PDE
R: Read/Write，同PDE
P: Present，同PDE
```

**如果 ucore 执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？**

如果出现页访问异常（Page Fault），则CPU会做如下处理，

+ 保存现场，设置异常相关的寄存器，如异常原因，产生异常的指令地址等
+ 切换到内核态
+ 读取中断向量表，根据异常号跳转到对应的中断服务例程
+ 由操作系统接管，执行中断服务例程

## 练习 3：释放某虚地址所在的页并取消对应二级页表项的映射

**设计实现过程**

修改`pmm.c:page_remove_pte`如下，设计和实现思路见注释。

```c
static inline void
page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep) {
    // 如果PTE存在
    if (*ptep & PTE_P) {
        // 找到PTE所在页
        struct Page *page = pte2page(*ptep);
        // 将该页引用减一，如果减到零则释放该页
        if (page_ref_dec(page) == 0) {
            free_page(page);
        }
        // 将PTE重置为零，意味着存在位也置为零
        *ptep = 0;
        // 手动清除该页表项在TLB的缓存
        tlb_invalidate(pgdir, la);
    }
}
```

**数据结构 Page 的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？**

有对应关系。`Page`数组`pages`的第`i`项描述了从物理地址`0x00000000`开始的第`i`个物理页面，其物理页号为`i`，物理地址为`i << 12`。页目录项PDE/页表项PTE中的高20位存储了指向页面的物理页号`ppn`，其物理地址为`ppn << 12`。

可以看出，PDE/PTE可以转换为`Page`，在`pmm.h`中有对应的函数`pde2page`和`pte2page`，逻辑是将PDE/PTE指向页面的物理地址算出来，得到物理页号`ppn`，然后`pages[ppn]`就是对应的`Page`。因此一个PDE/PTE对应了一个`Page`。

**如果希望虚拟地址与物理地址相等，则需要如何修改 lab2，完成此事？ 鼓励通过编程来具体完成这个问题**

将内核虚拟地址的基地址`KERNBASE`设置为`0x00000000`，将链接脚本`kernel.ld`中的内核加载地址从`0xC0100000`改成`0x00100000`。为了能进入`kern_init`，需要在`kern/init/entry.S:kern_entry`中取消分页机制，注释掉`enable paging`那一段。还需要禁用`check_pgdir`和`check_boot_pgdir`的页机制检查，避免抛出致命错误。如此而来，虚拟地址就等于物理地址了。

## 扩展练习 Challenge：buddy system（伙伴系统）分配算法

这里参考了实验指导书上的[伙伴分配器的一个极简实现](https://coolshell.cn/articles/10427.html)中的[wuwenbin](https://github.com/wuwenbin/buddy2)版本，实现了Buddy System。

基本的设计思想在这份开源代码中已有体现，即利用一个`longest`数组组织一棵满二叉树，其中每个叶子节点管辖一个页面，每个内部节点管辖其下所有叶子节点对应的页面，每个节点都有一个`longest`值，表示其管辖区域的最大可分配空间。

初始时，首先需要在可分配页面起始处分配`longest`数组的空间，并将可分配页面的数量缩小到2的某个幂，作为根节点的`longest`值，每往下一层节点的值减半。

分配空间时，首先将所需页面数放大到2的某个幂，然后自顶向下找到可分配节点，计算出其对应的页面，分配完成后，需要回溯节点的祖先，更新它们的`longest`值为左右孩子的最大值。

释放空间时，首先自底向上找到被分配的节点，然后释放它，并回溯它的祖先，更新他们的`longest`值，若左右孩子的`longest`值之和等于该节点对应的页面大小，需要合并左右子树，即重置该节点的`longest`，否则，更新该节点的`longest`值为左右孩子的最大值。

测试用例采用了[维基百科](https://en.wikipedia.org/wiki/Buddy_memory_allocation)上的例子，可以验证实现的正确性。

如需测试，在`kern/mm/pmm.c:init_pmm_manager`中将`pmm_manager`改为`buddy_pmm_manager`，并在文件头部添加`#include <buddy_pmm.h>`，即可运行Buddy System。

## 我的实现与参考答案的区别

+ 练习1：在释放空闲块时，参考答案在空闲链表中遍历了两遍，第一遍合并相邻块，第二遍寻找插入的位置，效率较低。我的实现只需要遍历一遍，找到插入位置，然后判断待插入的空闲块能否和前面或后面的空闲块合并即可，效率较高。
+ 练习2：参考答案写的更紧凑，但逻辑一致。
+ 练习3：基本一致。

## 本实验对应的OS知识点

+ 练习1：连续内存管理，first-fit算法
+ 练习2：页式虚存管理，多级页表结构，二级页表的创建
+ 练习3：页式虚存管理，多级页表结构，二级页表的删除

## 本实验未对应的OS知识点

+ best-fit，worst-fit算法
+ 内存碎片整理
