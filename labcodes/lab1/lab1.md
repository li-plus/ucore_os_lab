# Report for Lab 1

## Ex 1: Makefile Analysis

**Q1: 操作系统镜像文件 ucore.img 是如何一步一步生成的？**

ucore.img 的生成代码如下。

```makefile
# create ucore.img
UCOREIMG	:= $(call totarget,ucore.img)

$(UCOREIMG): $(kernel) $(bootblock)
	$(V)dd if=/dev/zero of=$@ count=10000
	$(V)dd if=$(bootblock) of=$@ conv=notrunc
	$(V)dd if=$(kernel) of=$@ seek=1 conv=notrunc

$(call create_target,ucore.img)
```

实际执行代码如下。

```bash
dd if=/dev/zero of=bin/ucore.img count=10000
dd if=bin/bootblock of=bin/ucore.img conv=notrunc
dd if=bin/kernel of=bin/ucore.img seek=1 conv=notrunc
```

其中`dd`命令用于转换数据，`if`为输入文件名，`of`为输出文件名，`count=blocks`表示只拷贝`blocks`个块，`conv=notrunc`表示出错时不停止，`seek=blocks`表示跳过开头的`blocks`个块再开始复制，块大小由`bs`指定，默认为 512 字节。

可以看到代码将 ucore.img 初始化为 10000 个块的全零，然后将 bootblock 拷贝到 ucore.img 的第一个页面，将 kernel 拷贝到后续的页面。

bootblock 的生成代码如下。

```makefile
# create bootblock
bootfiles = $(call listf_cc,boot)
$(foreach f,$(bootfiles),$(call cc_compile,$(f),$(CC),$(CFLAGS) -Os -nostdinc))

bootblock = $(call totarget,bootblock)

$(bootblock): $(call toobj,$(bootfiles)) | $(call totarget,sign)
	@echo + ld $@
	$(V)$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 $^ -o $(call toobj,bootblock)
	@$(OBJDUMP) -S $(call objfile,bootblock) > $(call asmfile,bootblock)
	@$(OBJCOPY) -S -O binary $(call objfile,bootblock) $(call outfile,bootblock)
	@$(call totarget,sign) $(call outfile,bootblock) $(bootblock)

$(call create_target,bootblock)
```

实际执行的代码如下。

```bash
gcc -Iboot/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootasm.S -o obj/boot/bootasm.o
gcc -Iboot/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootmain.c -o obj/boot/bootmain.o
ld -m    elf_i386 -nostdlib -N -e start -Ttext 0x7C00 obj/boot/bootasm.o obj/boot/bootmain.o -o obj/bootblock.o
```

`gcc` 用于编译，`-I`指定头文件路径，`-march`指定CPU架构，`-fno-builtin`不识别内置函数，`-fno-PIC`不产生独立于位置的代码，`-Wall`提示所有警告信息，`-ggdb`生成调试的符号信息，`-m32`生成32位i386代码，`-gstabs`生成STABS格式的调试信息，`-nostdinc`不使用标准库，`-fno-stack-protector`不产生保护栈的代码，`-Os`优化代码大小。

`ld`用于链接，`-m EMULATION`设置模拟链接器，`-nostdlib`不链接标准库，`-N`不分页对齐数据，`-e ADDRESS`设置开始地址，`-Ttext ADDRESS`设置 .text 段的地址。

可以看到，在 `bootfiles` 中列出了 boot/ 目录下的 bootasm.S 和 bootmain.c，然后通过 `gcc` 编译成 bootasm.o 和 bootmain.o ，最后将 bootasm.o 和 bootmain.o 链接成 bootblock。

注意到 bootblock 还有一个依赖为 sign，其生成过程如下。

```makefile
# create 'sign' tools
$(call add_files_host,tools/sign.c,sign,sign)
$(call create_target_host,sign,sign)
```

实际执行代码如下，这里不再赘述。

```bash
gcc -Itools/ -g -Wall -O2 -c tools/sign.c -o obj/sign/tools/sign.o
gcc -g -Wall -O2 obj/sign/tools/sign.o -o bin/sign
```

kernel 的生成过程如下。

```makefile
# create kernel target
kernel = $(call totarget,kernel)

$(kernel): tools/kernel.ld

$(kernel): $(KOBJS)
	@echo + ld $@
	$(V)$(LD) $(LDFLAGS) -T tools/kernel.ld -o $@ $(KOBJS)
	@$(OBJDUMP) -S $@ > $(call asmfile,kernel)
	@$(OBJDUMP) -t $@ | $(SED) '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(call symfile,kernel)

$(call create_target,kernel)
```

实际执行代码如下。

```bash
ld -m    elf_i386 -nostdlib -T tools/kernel.ld -o bin/kernel  obj/kern/init/init.o obj/kern/libs/stdio.o obj/kern/libs/readline.o obj/kern/debug/panic.o obj/kern/debug/kdebug.o obj/kern/debug/kmonitor.o obj/kern/driver/clock.o obj/kern/driver/console.o obj/kern/driver/picirq.o obj/kern/driver/intr.o obj/kern/trap/trap.o obj/kern/trap/vectors.o obj/kern/trap/trapentry.o obj/kern/mm/pmm.o  obj/libs/string.o obj/libs/printfmt.o
```

其中 `-T` 指定链接脚本。

可以看出，代码将 `KOBJS` 中的所有目标链接起来，生成 `KOBJS` 的代码如下。

```makefile
$(call add_files_cc,$(call listf_cc,$(LIBDIR)),libs,)
$(call add_files_cc,$(call listf_cc,$(KSRCDIR)),kernel,$(KCFLAGS))
KOBJS	= $(call read_packet,kernel libs)
```

实际执行代码如下，这里以 init.c 为例，其余同理，不再赘述。

```bash
gcc -Ikern/init/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/init/init.c -o obj/kern/init/init.o
```

**Q2: 一个被系统认为是符合规范的硬盘主引导扇区的特征是什么？**

从 `tools/sign.c` 可以看出，一个规范的主引导扇区必须为512字节，且最后两个字节必须为 `0x55` 和 `0xAA`。

## Ex 2: Debug with qemu and gdb

**Q1: 从 CPU 加电后执行的第一条指令开始，单步跟踪 BIOS 的执行。**

在一个终端中打开 qemu，使其停在第一条指令处，命令如下

```bash
qemu -S -s -monitor stdio -hda bin/ucore.img
```

另一个终端打开 gdb，并执行

```
(gdb) set architecture i8086
(gdb) target remote :1234
```

在 gdb 中查看此时的 cs 段寄存器和 pc 寄存器

```
(gdb) i r $cs $pc
cs             0xf000	61440
pc             0xfff0	0xfff0
```

此时 CPU 运行在实模式， cs 为 `0xf000`，但需要注意的是，初始化时 cs 段基地址为 `0xffff`，因此第一条指令的实地址为 `0xfffffff0`。

在 qemu 中执行下列命令查看加电后的第一条指令，如下所示，这是一条长跳转指令，它会将 cs 段基地址更新为 `$cs * 16`，这样就能通过 `$cs * 16 + $pc` 计算出实地址，即跳转到 `0xfe05b`。

```
(qemu) x/i $pc
0xfffffff0:  ljmp   $0xf000,$0xe05b
```

在 gdb 中执行 `ni` 使 qemu 执行下一条命令，然后在 qemu 中执行

```
(qemu) x/8i $pc
0x000fe05b:  cmpl   $0x0,%cs:0x70c8
0x000fe062:  jne    0xfd414
0x000fe066:  xor    %dx,%dx
0x000fe068:  mov    %dx,%ss
0x000fe06a:  mov    $0x7000,%esp
0x000fe070:  mov    $0xf2d4e,%edx
0x000fe076:  jmp    0xfff00
0x000fe079:  push   %ebp
```

可以看出第一条指令确实跳转到了 `0xfe05b` ，然后继续执行。

**Q2: 在初始化位置 0x7c00 设置实地址断点,测试断点正常。**

在 Q1 的基础上，在 gdb 中执行如下代码，结果如下。

```
0x0000fff0 in ?? ()
(gdb) b *0x7c00
Breakpoint 1 at 0x7c00
(gdb) c
Continuing.

Breakpoint 1, 0x00007c00 in ?? ()
```

可以看出程序正常停在了断点处。

**Q3: 从 0x7c00 开始跟踪代码运行,将单步跟踪反汇编得到的代码与 bootasm.S 和 bootblock.asm 进行比较。**

在 Q2 的 `0x7c00` 断点处，在 qemu 中执行 `x/33i $pc` 查看 `bootmain` 函数之前的汇编代码，反汇编结果为

```asm
(qemu) x/33i $pc
0x00007c00:  cli    
0x00007c01:  cld    
0x00007c02:  xor    %ax,%ax
0x00007c04:  mov    %ax,%ds
0x00007c06:  mov    %ax,%es
0x00007c08:  mov    %ax,%ss
0x00007c0a:  in     $0x64,%al
0x00007c0c:  test   $0x2,%al
0x00007c0e:  jne    0x7c0a
0x00007c10:  mov    $0xd1,%al
0x00007c12:  out    %al,$0x64
0x00007c14:  in     $0x64,%al
0x00007c16:  test   $0x2,%al
0x00007c18:  jne    0x7c14
0x00007c1a:  mov    $0xdf,%al
0x00007c1c:  out    %al,$0x60
0x00007c1e:  lgdtw  0x7c6c
0x00007c23:  mov    %cr0,%eax
0x00007c26:  or     $0x1,%eax
0x00007c2a:  mov    %eax,%cr0
0x00007c2d:  ljmp   $0x8,$0x7c32
0x00007c32:  mov    $0xd88e0010,%eax
0x00007c38:  mov    %ax,%es
0x00007c3a:  mov    %ax,%fs
0x00007c3c:  mov    %ax,%gs
0x00007c3e:  mov    %ax,%ss
0x00007c40:  mov    $0x0,%bp
0x00007c43:  add    %al,(%bx,%si)
0x00007c45:  mov    $0x7c00,%sp
0x00007c48:  add    %al,(%bx,%si)
0x00007c4a:  call   0x7d0f
0x00007c4d:  add    %al,(%bx,%si)
0x00007c4f:  jmp    0x7c4f
```

与 bootblock.asm 和 bootasm.S 中的汇编代码一致。此外，可通过 `x/143i $pc` 查看整个 bootblock.asm 的反汇编内容，结果均一致。

**Q4: 自己找一个 bootloader 或内核中的代码位置，设置断点并进行测试。**

这里在 `kern/init/init.c:kern_init` 函数处打断点并测试。执行

```bash
make debug
```

在 gdb 中调试。

```
0x0000fff0 in ?? ()
Breakpoint 1 at 0x100000: file kern/init/init.c, line 17.

Breakpoint 1, kern_init () at kern/init/init.c:17
17	kern_init(void) {
(gdb) l
12	int kern_init(void) __attribute__((noreturn));
13	void grade_backtrace(void);
14	static void lab1_switch_test(void);
15	
16	int
17	kern_init(void) {
18	    extern char edata[], end[];
19	    memset(edata, 0, end - edata);
20	
21	    cons_init();                // init the console
(gdb) n
19	    memset(edata, 0, end - edata);
(gdb) n
21	    cons_init();                // init the console
```

可以看出程序能正常调试和运行。

## Ex 3: Entering Protected Mode

**分析 bootloader 进入保护模式的过程。**

从实模式进入保护模式需要以下几个步骤

- 开启 A20
- 初始化段描述符表 GDT (Global Descriptor Table)
- 使能 CR0 寄存器的 PE 位，跳转到 32 位地址

详见下面 `boot/bootasm.S` 的中文注释

```asm
#include <asm.h>

# Start the CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.set PROT_MODE_CSEG,        0x8                     # kernel code segment selector
.set PROT_MODE_DSEG,        0x10                    # kernel data segment selector
.set CR0_PE_ON,             0x1                     # protected mode enable flag

# start address should be 0:7c00, in real mode, the beginning address of the running bootloader
.globl start
start:
.code16                                             # Assemble for 16-bit mode
    # 初始化环境，关闭中断，将DF位清零，段寄存器DS, ES, SS清零
    cli                                             # Disable interrupts
    cld                                             # String operations increment

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    movw %ax, %ds                                   # -> Data Segment
    movw %ax, %es                                   # -> Extra Segment
    movw %ax, %ss                                   # -> Stack Segment

    # Enable A20:
    #  For backwards compatibility with the earliest PCs, physical
    #  address line 20 is tied low, so that addresses higher than
    #  1MB wrap around to zero by default. This code undoes this.

    # 开启A20的原因：
    # A20是地址线的第20位，初始时总为0，直到系统软件通过一定的IO操作去打开它。
    # 显然，在实模式下要访问高端内存区，这个开关必须打开，在保护模式下，
    # 由于使用32位地址线，如果A20恒等于0，那么系统只能访问奇数兆的内存，
    # 即只能访问0-1M、2-3M、4-5M等等，无法有效访问所有可用内存。
    # 因此在保护模式下，这个开关也必须打开。

    # 开启A20的步骤：
    # 首先等待8042键盘控制器空闲，判断是否空闲的方法是从0x64端口读取一个字节，
    # 记为x，判断其第2位是否为0，即若(x & 0x02)为0，则空闲，否则，表示正在忙。
    # 当控制器空闲时，向0x64端口写入0xd1，即写入8042控制器的P2端口，
    # 然后再次等待8042控制器空闲，向它发送0xdf，其第2位为1，表示开启A20。
seta20.1:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.1

    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64                                 # 0xd1 means: write data to 8042's P2 port

seta20.2:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.2

    movb $0xdf, %al                                 # 0xdf -> port 0x60
    outb %al, $0x60                                 # 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1

    # Switch from real to protected mode, using a bootstrap GDT
    # and segment translation that makes virtual addresses
    # identical to physical addresses, so that the
    # effective memory map does not change during the switch.
    # GDT表已经在下面定义为gdtdesc了，通过lgdt载入，即可初始化GDT
    lgdt gdtdesc
    # 通过使能CR0的PE位，进入保护模式
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0

    # Jump to next instruction, but in 32-bit code segment.
    # Switches processor into 32-bit mode.
    # 长跳转到下一条指令，更新CS的段基地址，进入32位地址模式
    ljmp $PROT_MODE_CSEG, $protcseg

.code32                                             # Assemble for 32-bit mode
protcseg:
    # Set up the protected-mode data segment registers
    # 初始化保护模式的数据段寄存器
    movw $PROT_MODE_DSEG, %ax                       # Our data segment selector
    movw %ax, %ds                                   # -> DS: Data Segment
    movw %ax, %es                                   # -> ES: Extra Segment
    movw %ax, %fs                                   # -> FS
    movw %ax, %gs                                   # -> GS
    movw %ax, %ss                                   # -> SS: Stack Segment

    # Set up the stack pointer and call into C. The stack region is from 0--start(0x7c00)
    # 初始化栈空间，置ebp为0，esp为0x7c00，然后调用主函数bootmain
    movl $0x0, %ebp
    movl $start, %esp
    call bootmain

    # If bootmain returns (it shouldn't), loop.
spin:
    jmp spin

# 下面是GDT表中段描述符的定义，包含空描述符，代码段描述符，以及数据段描述符
# Bootstrap GDT
.p2align 2                                          # force 4 byte alignment
gdt:
    SEG_NULLASM                                     # null seg
    SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)           # code seg for bootloader and kernel
    SEG_ASM(STA_W, 0x0, 0xffffffff)                 # data seg for bootloader and kernel

# GDT表的定义，包含了GDT的大小，以及GDT的入口地址
gdtdesc:
    .word 0x17                                      # sizeof(gdt) - 1
    .long gdt                                       # address gdt
```

## Ex 4: Reading ELF

**Q1: bootloader 是如何读取硬盘扇区的？**

通过 `boot/bootmain.c:readsect` 函数实现，详见下面代码注释。

```c
/* readsect - read a single sector at @secno into @dst */
static void
readsect(void *dst, uint32_t secno) {
    // wait for disk to be ready
    waitdisk();

    // 表明读写1个扇区
    outb(0x1F2, 1);                         // count = 1

    // 0x1F3 - 0x1F5端口，分别对应LBA参数的0-7, 8-15, 16-23位，
    // 0x1F4端口，0-3位对应LBA参数的24-27位，第4位这里置为0，表示主盘，第5-7位强制为1
    outb(0x1F3, secno & 0xFF);
    outb(0x1F4, (secno >> 8) & 0xFF);
    outb(0x1F5, (secno >> 16) & 0xFF);
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
    // 从0x1F7端口发送读取命令0x20
    outb(0x1F7, 0x20);                      // cmd 0x20 - read sectors

    // wait for disk to be ready
    waitdisk();

    // read a sector
    // 硬盘不忙，从0x1F0端口读取扇区数据，存储到dst中
    // 由于insl一次读32位数据，因此读(SECTSIZE / 4)次即可
    insl(0x1F0, dst, SECTSIZE / 4);
}
```

**Q2: bootloader 是如何加载 ELF 格式的 OS？**

通过 `boot/bootmain.c:bootmain` 函数实现，详见下面代码注释。

```c
/* bootmain - the entry of bootloader */
void
bootmain(void) {
    // read the 1st page off disk
    // readseg其实就是readsect外面套了一层，能够读取任意长度的磁盘空间
    // 这里读取8个扇区到地址0x10000处，并且强制转为ELF头
    readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);

    // is this a valid ELF?
    // 判断ELF是否合法，即ELF头的前四个字符是否"\x7FELF"
    if (ELFHDR->e_magic != ELF_MAGIC) {
        goto bad;
    }

    struct proghdr *ph, *eph;

    // load each program segment (ignores ph flags)
    // 将ELF记录的每个程序段加载到指定的虚拟地址
    ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph ++) {
        readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    }

    // call the entry point from the ELF header
    // note: does not return
    // 进入内核，不再返回，实际上调用的是kern_init
    ((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();

bad:
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8E00);

    /* do nothing */
    while (1);
}
```

## Ex 5: Tracing Stack

编程实现了 `kern/debug/kdebug.c:print_stackframe` 函数，实现思路如下：

```c
void
print_stackframe(void) {
    /*
     * Stack at runtime:
     * +---------+ high addr
     * | arg 3   |
     * | arg 2   |
     * | arg 1   |
     * | arg 0   |
     * +---------+
     * | old eip |
     * | old ebp |  <- ebp
     * +---------+ low addr
     */

    // 获取 ebp 和 eip
    uint32_t ebp = read_ebp();
    uint32_t eip = read_eip();

    // 从内到外逐层遍历，直到达到最外层（ebp为0），或者超出最大深度限制
    for (int depth = 0; depth < STACKFRAME_DEPTH && ebp; ++depth) {
        // 输出 ebp 和 eip
        cprintf("ebp:0x%08x eip:0x%08x args:", ebp, eip);
        // 输出每一个参数
        for (int arg_idx = 0; arg_idx < 4; ++arg_idx) {
            cprintf("0x%08x ", *((uint32_t *)ebp + 2 + arg_idx));
        }
        cprintf("\n");
        // 输出调试信息
        print_debuginfo(eip - 1);
        // 退栈，更新 eip 和 ebp
        eip = *((uint32_t *)ebp + 1);
        ebp = *((uint32_t *)ebp);
    }
}
```

执行 `make debug` 后的输出如下

```
ebp:0x00007b28 eip:0x00100a63 args:0x00010094 0x00010094 0x00007b58 0x00100092 
    kern/debug/kdebug.c:307: print_stackframe+21
ebp:0x00007b38 eip:0x00100d44 args:0x00000000 0x00000000 0x00000000 0x00007ba8 
    kern/debug/kmonitor.c:125: mon_backtrace+10
ebp:0x00007b58 eip:0x00100092 args:0x00000000 0x00007b80 0xffff0000 0x00007b84 
    kern/init/init.c:48: grade_backtrace2+33
ebp:0x00007b78 eip:0x001000bc args:0x00000000 0xffff0000 0x00007ba4 0x00000029 
    kern/init/init.c:53: grade_backtrace1+38
ebp:0x00007b98 eip:0x001000db args:0x00000000 0x00100000 0xffff0000 0x0000001d 
    kern/init/init.c:58: grade_backtrace0+23
ebp:0x00007bb8 eip:0x00100101 args:0x001032bc 0x001032a0 0x0000130a 0x00000000 
    kern/init/init.c:63: grade_backtrace+34
ebp:0x00007be8 eip:0x00100055 args:0x00000000 0x00000000 0x00000000 0x00007c4f 
    kern/init/init.c:28: kern_init+84
ebp:0x00007bf8 eip:0x00007d72 args:0xc031fcfa 0xc08ed88e 0x64e4d08e 0xfa7502a8 
    <unknow>: -- 0x00007d71 --
```

其中最后一行为

```
ebp:0x00007bf8 eip:0x00007d72 args:0xc031fcfa 0xc08ed88e 0x64e4d08e 0xfa7502a8 
    <unknow>: -- 0x00007d71 --
```

对应于 `boot/bootmain.c:bootmain` 中的这一处调用

```c
((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();
```

在 `bootblock.asm` 找到对应的汇编为

```asm
    7d66:	a1 18 00 01 00       	mov    0x10018,%eax
    7d6b:	25 ff ff ff 00       	and    $0xffffff,%eax
    7d70:	ff d0                	call   *%eax
```

从头开始运行，当 bootasm.S 调用 `bootmain` 时，将 `%esp` 置为 `$start`，即为 `0x7c00`，执行 `call bootmain` 会将返回地址压栈，此时 `%esp` 变为 `0x7bfc`，然后执行

```asm
    7d11:	55                   	push   %ebp
    7d12:	31 c9                	xor    %ecx,%ecx
    7d14:	89 e5                	mov    %esp,%ebp
```

由于再有一次 `push` 操作，因此当前的 `%ebp` 就是 `0x7bf8`。

当程序运行到 `0x7d70` 时，进入内核，然后把返回地址 `0x7d72` 压栈，这就是 `%eip`。

至于参数，这里其实一个参数都没有，对参数的访问已经越过了栈区，到了代码段 `start`，截取部分代码如下。

```asm
.globl start
start:
.code16                                             # Assemble for 16-bit mode
    cli                                             # Disable interrupts
    7c00:	fa                   	cli    
    cld                                             # String operations increment
    7c01:	fc                   	cld    

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    7c02:	31 c0                	xor    %eax,%eax
    movw %ax, %ds                                   # -> Data Segment
    7c04:	8e d8                	mov    %eax,%ds
    movw %ax, %es                                   # -> Extra Segment
    7c06:	8e c0                	mov    %eax,%es
```

在小端意义下将这些指令的机器码连起来，就是输入参数，比如第一个参数为 `0xc031fcfa`，就是由 `cli, cld, xorw` 组成的，后面以此类推。

由于 bootmain 是直接编译成汇编的，没有符号信息，所以显示 `<unknow>`，后面的地址 `0x7d71` 就是调用内核的 `call` 指令的地址。

## Ex 6: Trap

**Q1: 中断描述符表（也可简称为保护模式下的中断向量表）中一个表项占多少字节？其中哪几位代表中断处理代码的入口？**

在 `kern/mm/mmu.h` 中有 `gatedesc` 的定义

```c
/* Gate descriptors for interrupts and traps */
struct gatedesc {
    unsigned gd_off_15_0 : 16;        // low 16 bits of offset in segment
    unsigned gd_ss : 16;            // segment selector
    unsigned gd_args : 5;            // # args, 0 for interrupt/trap gates
    unsigned gd_rsv1 : 3;            // reserved(should be zero I guess)
    unsigned gd_type : 4;            // type(STS_{TG,IG32,TG32})
    unsigned gd_s : 1;                // must be 0 (system)
    unsigned gd_dpl : 2;            // descriptor(meaning new) privilege level
    unsigned gd_p : 1;                // Present
    unsigned gd_off_31_16 : 16;        // high bits of offset in segment
};
```

一个表项共 64 位，即 8 字节，其中 0-15 和 48-63 位组成 32 位段偏移，16-31 位是段选择子，根据段选择子可以在 GDT 中查到段基地址，段基地址加上段偏移，就是中断处理程序的入口地址。

**Q2: 编程完善 kern/trap/trap.c 中对中断向量表进行初始化的函数 idt_init。**

实现如下

```c
/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S */
void
idt_init(void) {
    // 找到中断向量的定义
    extern uintptr_t __vectors[];
    // 设置IDT中每个中断门描述符(gate descriptor)的段选择子，段偏移，特权级别
    for (int i = 0; i < sizeof(idt) / sizeof(struct gatedesc); ++i) {
        SETGATE(idt[i], 0, KERNEL_CS, __vectors[i], DPL_KERNEL);
    }
    // 加载IDT
    lidt(&idt_pd);
}
```

**Q3: 编程完善 trap.c 中的中断处理函数 trap，使操作系统每遇到 100 次时钟中断后，调用 print_ticks 子程序，向屏幕上打印一行文字”100 ticks”**

实现如下

```c
    case IRQ_OFFSET + IRQ_TIMER:
        ++ticks;
        if (ticks % TICK_NUM == 0) {
            print_ticks();
        }
        break;
```

## Challenge: Switching between Kernel and User Mode

**Challenge 1: 增加一用户态函数（可执行一特定系统调用：获得时钟计数值）**

首先在 IDT 初始化 `T_SWITCH_TOK` 中断，给予用户权限

```c
    SETGATE(idt[T_SWITCH_TOK], 0, KERNEL_CS, __vectors[T_SWITCH_TOK], DPL_USER);
```

通过中断请求内核态和用户态之间的切换

```c
static void
lab1_switch_to_user(void) {
    //LAB1 CHALLENGE 1 : TODO
    // 由于iret返回时会pop出ss和esp，因此需要先压栈两次，并手动维护esp
	asm volatile (
	    "sub $0x8, %%esp \n"
	    "int %0 \n"
	    "movl %%ebp, %%esp \n"
	    : 
	    : "i"(T_SWITCH_TOU)
	);
}

static void
lab1_switch_to_kernel(void) {
    //LAB1 CHALLENGE 1 :  TODO
    // 同样的逻辑
    asm volatile (
        // 经debug发现，user到kernel时trapframe内并没有末尾的ss和esp，所以这一行可省略
	    // "sub $0x8, %%esp \n"
	    "int %0 \n"
	    "movl %%ebp, %%esp \n"
	    : 
	    : "i"(T_SWITCH_TOK)
	);
}
```

在中断响应函数里面完成内核态和用户态之间的转换

```c
    //LAB1 CHALLENGE 1 : 2017011620 you should modify below codes.
    case T_SWITCH_TOU:
        if (tf->tf_cs != USER_CS) {
            // 原址修改段寄存器
            tf->tf_cs = USER_CS;
            tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;
            // 降低IO权限，使用户能够使用IO，否则用户态将没有输出信息
            tf->tf_eflags |= FL_IOPL_MASK;
        }
        break;
    case T_SWITCH_TOK:
        if (tf->tf_cs != KERNEL_CS) {
            // 原址修改段寄存器
            tf->tf_cs = KERNEL_CS;
            tf->tf_ds = tf->tf_es = KERNEL_DS;
            // 恢复IO权限
            tf->tf_eflags &= ~FL_IOPL_MASK;
        }
        break;
```

**Challenge 2: 用键盘实现用户模式内核模式切换。**

硬盘的中断服务程序入口在 `IRQ_OFFSET + IRQ_KBD`，仿照 Challenge 1 的切换逻辑，根据读入字符进行相应处理即可

```c
    case IRQ_OFFSET + IRQ_KBD:
        c = cons_getc();

        if (c == '0') {
            if (tf->tf_cs != KERNEL_CS) {
                tf->tf_cs = KERNEL_CS;
                tf->tf_ds = tf->tf_es = KERNEL_DS;
                tf->tf_eflags &= ~FL_IOPL_MASK;
                print_trapframe(tf);
                cprintf("switched to kernel\n");
            }
        } else if (c == '3') {
            if (tf->tf_cs != USER_CS) {
                tf->tf_cs = USER_CS;
                tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;
                tf->tf_eflags |= FL_IOPL_MASK;
                print_trapframe(tf);
                cprintf("switched to user\n");
            }
        }

        cprintf("kbd [%03d] %c\n", c, c);
        break;
```

## 我的实现与参考答案的区别

相比于参考答案：

+ Ex 1: 我的命令和参数分析更加完整全面，但逻辑可能不够清晰。
+ Ex 2: 我没有改 Makefile 和 gdbinit，此外我是用 qemu 看 BIOS 代码的，因为我的 gdb 在反汇编遇到 ljmp 时会有奇怪的 bug，并且不能自动计算实模式的地址。
+ Ex 3: 我的分析要更完整一些，标注了GDT以及段描述符定义的位置。
+ Ex 4: 分析基本一致。
+ Ex 5: 我还画出了运行时调用栈的示意图，在最后一层的分析中，除了参考答案的ebp，我还分析了eip，四个参数，以及调用语句的地址的含义。
+ Ex 6: 参考答案的段选择子写成 `GD_KTEXT`，我认为写成 `KERNEL_CS` 更好。
+ Challenge: 内核态和用户态之间切换时，我只修改了段寄存器和 IO 权限，而参考答案还有一些其余的操作。

## 本实验对应的OS知识点

OS 原理的知识点在实验中基本都对应上了。

+ Ex 1: 系统镜像格式，MBR 格式
+ Ex 2: BIOS 和 bootloader 的功能
+ Ex 3: 实模式的寻址方式，保护模式的建立过程，全局描述符表 GDT
+ Ex 4: ELF文件格式，磁盘扇区的读取
+ Ex 5: 调用栈结构，课后作业的 backtrace
+ Ex 6: 中断描述符表 IDT，中断服务例程
+ Challenge: 内核态和用户态的切换

