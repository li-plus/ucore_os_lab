#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>

#define IS_POWER_OF_2(x) (((x) & ((x)-1)) == 0)
#define LEFT_LEAF(index) ((index)*2 + 1)
#define RIGHT_LEAF(index) ((index)*2 + 2)

#define PARENT(index) (((index) + 1) / 2 - 1)
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define BUDDY_MAX_DEPTH 30

static size_t *buddy_longest;
static size_t buddy_max_pages;
static struct Page *buddy_base;

static size_t
buddy_fix_size(size_t size) {
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    return size + 1;
}

static void
buddy_init(void) {}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    buddy_max_pages = 1;

    for (size_t i = 1; i < BUDDY_MAX_DEPTH; ++i) {
        // The total page must be large enough to hold both
        // the allocable pages of buddy system (buddy_max_pages)
        // and the longest array (2 * buddy_max_pages * 4B / 4KB)
        if (buddy_max_pages + buddy_max_pages / 512 >= n) {
            buddy_max_pages /= 2;
            break;
        }

        buddy_max_pages *= 2;
    }

    // The first (buddy_max_pages / 512 + 1) pages are for the longest array, 
    // and the following pages are allocable
    buddy_base = base + buddy_max_pages / 512 + 1;
    buddy_longest = (size_t *)KADDR(page2pa(base));

    size_t node_size = buddy_max_pages * 2;

    for (size_t i = 0; i < 2 * buddy_max_pages - 1; ++i) {
        if (IS_POWER_OF_2(i + 1)) {
            node_size /= 2;
        }

        buddy_longest[i] = node_size;
    }

    // reserve pages for longest array
    for (struct Page *p = base; p != buddy_base; ++p) {
        SetPageReserved(p);
    }

    // free allocable pages
    for (struct Page *p = buddy_base; p != base + n; ++p) {
        assert(PageReserved(p));
        ClearPageReserved(p);
        SetPageProperty(p);
        set_page_ref(p, 0);
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);

    if (n > buddy_longest[0]) {
        return NULL;
    }

    // Round up to the power of 2
    if (!IS_POWER_OF_2(n)) {
        n = buddy_fix_size(n);
    }

    // Start from the root node
    size_t index = 0;
    // Size of the current node
    size_t node_size;

    // Find the desired node top-down
    for (node_size = buddy_max_pages; node_size != n; node_size /= 2) {
        if (buddy_longest[LEFT_LEAF(index)] >= n) {
            index = LEFT_LEAF(index);
        } else {
            index = RIGHT_LEAF(index);
        }
    }

    // Allocate this node
    buddy_longest[index] = 0;
    // Find its corresponding page
    size_t offset = (index + 1) * node_size - buddy_max_pages;
    struct Page *page = buddy_base + offset;

    // Set new page
    for (struct Page *p = page; p != page + node_size; ++p) {
        set_page_ref(p, 0);
        ClearPageProperty(p);
    }

    // Update longest values of its parents
    while (index) {
        index = PARENT(index);
        buddy_longest[index] = MAX(buddy_longest[LEFT_LEAF(index)], buddy_longest[RIGHT_LEAF(index)]);
    }

    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);

    if (!IS_POWER_OF_2(n)) {
        n = buddy_fix_size(n);
    }
    // Get the node index of the page
    size_t offset = base - buddy_base;
    size_t index = offset + buddy_max_pages - 1;
    size_t node_size = 1;

    // Find the nearest allocated ancestor
    while (buddy_longest[index]) {
        node_size *= 2;
        assert(index != 0);
        index = PARENT(index);
    }

    assert(node_size == n);

    // Free the allocated pages
    for (struct Page *p = base; p != base + n; ++p) {
        assert(!PageReserved(p) && !PageProperty(p));
        SetPageProperty(p);
        set_page_ref(p, 0);
    }

    // Update this node
    buddy_longest[index] = node_size;

    // Update ancestor bottom-up starting from this node
    while (index != 0) {
        index = PARENT(index);
        node_size *= 2;
        size_t left_longest = buddy_longest[LEFT_LEAF(index)];
        size_t right_longest = buddy_longest[RIGHT_LEAF(index)];

        if (left_longest + right_longest == node_size) {
            // Merge
            buddy_longest[index] = node_size;
        } else {
            // Not merge but update
            buddy_longest[index] = MAX(left_longest, right_longest);
        }
    }
}

static size_t
buddy_nr_free_pages(void) {
    return buddy_longest[0];
}

static void
buddy_check(void) {
    size_t total_pages = nr_free_pages();
    // https://en.wikipedia.org/wiki/Buddy_memory_allocation
    // alloc
    struct Page *pa, *pb, *pc, *pd;
    pa = alloc_pages(34);
    assert(pa == buddy_base);
    assert(!PageReserved(pa) && !PageProperty(pa));
    pb = alloc_pages(66);
    assert(pb == buddy_base + 128);
    assert(!PageReserved(pb) && !PageProperty(pb));
    pc = alloc_pages(35);
    assert(pc == buddy_base + 64);
    assert(!PageReserved(pc) && !PageProperty(pc));
    pd = alloc_pages(67);
    assert(pd == buddy_base + 256);
    assert(!PageReserved(pd) && !PageProperty(pd));
    // free
    free_pages(pb, 128);
    assert(PageProperty(pb));
    assert(page_ref(pb) == 0);
    free_pages(pd, 128);
    assert(PageProperty(pd));
    assert(page_ref(pd) == 0);
    free_pages(pa, 64);
    assert(PageProperty(pa));
    assert(page_ref(pa) == 0);
    free_pages(pc, 64);
    assert(PageProperty(pc));
    assert(page_ref(pc) == 0);
    // done
    assert(nr_free_pages() == total_pages);
    assert(alloc_pages(total_pages + 1) == NULL);
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
