#include <defs.h>
#include <x86.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_ext_clock.h>
#include <list.h>

list_entry_t ext_clock_head;
list_entry_t *curr_le;

static list_entry_t *
_ext_clock_next_le(list_entry_t *le) {
    assert(le != &ext_clock_head);
    list_entry_t *next_le = list_next(le);

    if (next_le == &ext_clock_head) {
        next_le = list_next(next_le);
    }

    return next_le;
}

static int
_ext_clock_init_mm(struct mm_struct *mm) {
    list_init(&ext_clock_head);
    mm->sm_priv = &ext_clock_head;
    curr_le = &ext_clock_head;
    return 0;
}

static int
_ext_clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in) {
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    list_entry_t *entry = &(page->pra_page_link);
    assert(entry != NULL && head != NULL);
    list_add_before(curr_le, entry);
    return 0;
}

static int
_ext_clock_swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick) {
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);
    assert(list_next(head) != head);

    if (curr_le == head) {
        curr_le = list_next(curr_le);
    }

    while (1) {
        struct Page *curr_page = le2page(curr_le, pra_page_link);
        pte_t *curr_pte = get_pte(mm->pgdir, curr_page->pra_vaddr, 0);
        assert(curr_pte != NULL);
        bool accessed_bit = (*curr_pte) & PTE_A;
        bool dirty_bit = (*curr_pte) & PTE_D;

        if (!accessed_bit && !dirty_bit) {
            break;
        }

        *curr_pte &= ~PTE_A;

        if (!accessed_bit && dirty_bit) {
            *curr_pte &= ~PTE_D;
        }

        curr_le = _ext_clock_next_le(curr_le);
    }

    *ptr_page = le2page(curr_le, pra_page_link);
    list_entry_t *victim_le = curr_le;
    curr_le = _ext_clock_next_le(curr_le);
    list_del(victim_le);
    return 0;
}

static inline void
_ext_clock_assert_swapped_out(uintptr_t la) {
    pte_t *ptep = get_pte(boot_pgdir, la, 0);
    assert(ptep != NULL);
    assert(!(*ptep & PTE_P));
}

static int
_ext_clock_check_swap(void) {
    for (uintptr_t la = 0x1000; la != 0x6000; la += 0x1000) {
        pte_t *ptep = get_pte(boot_pgdir, la, 0);
        assert(ptep != NULL);
        *ptep &= ~(PTE_A | PTE_D);
        tlb_invalidate(boot_pgdir, la);
    }

    unsigned char val;

    cprintf("read Virt Page c in ext_clock_check_swap\n");
    val = *(unsigned char *)0x3000;
    assert(pgfault_num == 4);

    cprintf("write Virt Page a in ext_clock_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4);

    cprintf("read Virt Page d in ext_clock_check_swap\n");
    val = *(unsigned char *)0x4000;
    assert(pgfault_num == 4);

    cprintf("write Virt Page b in ext_clock_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 4);

    cprintf("read Virt Page e in ext_clock_check_swap\n");
    val = *(unsigned char *)0x5000;
    assert(pgfault_num == 5);
    _ext_clock_assert_swapped_out(0x3000);

    cprintf("read Virt Page b in ext_clock_check_swap\n");
    val = *(unsigned char *)0x2000;
    assert(pgfault_num == 5);

    cprintf("write Virt Page a in ext_clock_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 5);

    cprintf("read Virt Page b in ext_clock_check_swap\n");
    val = *(unsigned char *)0x2000;
    assert(pgfault_num == 5);

    cprintf("read Virt Page c in ext_clock_check_swap\n");
    val = *(unsigned char *)0x3000;
    assert(pgfault_num == 6);
    _ext_clock_assert_swapped_out(0x4000);

    cprintf("read Virt Page d in ext_clock_check_swap\n");
    val = *(unsigned char *)0x4000;
    assert(pgfault_num == 7);
    _ext_clock_assert_swapped_out(0x2000);

    return 0;
}

static int
_ext_clock_init(void) {
    return 0;
}

static int
_ext_clock_set_unswappable(struct mm_struct *mm, uintptr_t addr) {
    return 0;
}

static int
_ext_clock_tick_event(struct mm_struct *mm) {
    return 0;
}

struct swap_manager swap_manager_ext_clock = {
    .name = "extended clock swap manager",
    .init = &_ext_clock_init,
    .init_mm = &_ext_clock_init_mm,
    .tick_event = &_ext_clock_tick_event,
    .map_swappable = &_ext_clock_map_swappable,
    .set_unswappable = &_ext_clock_set_unswappable,
    .swap_out_victim = &_ext_clock_swap_out_victim,
    .check_swap = &_ext_clock_check_swap,
};
