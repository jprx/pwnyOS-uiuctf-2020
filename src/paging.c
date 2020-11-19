#include "paging.h"
#include "defines.h"
#include "x86_stuff.h"
#include "vga.h"

// Page directory (aligned to a page):
static pde_t page_dir[PD_NUM_ENTRIES] __attribute__((aligned(PAGE_SIZE)));

// Lower 4MB table:
//static pte_t lower_page_table[PT_NUM_ENTRIES] __attribute__((aligned(PAGE_SIZE)));

// Free page tables for use by map_page*:
#define NUM_TABLES ((16))
static pte_t free_page_tables[NUM_TABLES][PT_NUM_ENTRIES] __attribute__((aligned(PAGE_SIZE)));
static bool free_page_tables_in_use[NUM_TABLES]; // Which tables are in use?

// For use by alloc_huge_page:
// First free huge page is the one after the filesystem:
// Let's start allocating huge pages at 1MB (start of high memory in most x86 systems)
#define FIRST_FREE_HUGE_PAGE ((((FS_PAGENUM + 3)) << 22))
static bool free_huge_pages_in_use[NUM_HUGE_PAGES]; // Which huge pages are free for use by alloc_huge_page?

// Update page mappings by loading new_dir into CR3
// This will flush TLB
void _load_page_dir(pde_t *new_dir) {
    // Update page tables:
    // (Manual says that a jump instruction should follow changing CR3)
    asm volatile (
        "movl %%eax, %%cr3" : : "a"(new_dir)
    );
}

// "Allocates" a page table by marking it as in-use
// This will also initialize all fields to be empty
pte_t *alloc_page_table() {
    uint32_t i;
    pte_t *page_table = NULL;
    for (i = 0; i < NUM_TABLES; i++) {
        if (!free_page_tables_in_use[i]) {
            free_page_tables_in_use[i] = true;
            page_table = (pte_t *)(&free_page_tables[i]);
            break;
        }
    }

    // Setup and clear this page table:
    if (page_table) {
        for (i = 0; i < PT_NUM_ENTRIES; i++) {
            page_table[i].present = 0;
            page_table[i].val = 0;
        }
    }

    return page_table;
}

// Mark a page table allocated by alloc_page_table as being free
// Please only call this with valid page table pointers allocated by alloc_page_table
// otherwise terrible things will happen
// Also do NOT call this on a page table that is mapped into a page directory
void free_page_table (pte_t *table) {
    if (!table) return;
    // uint32_t offset_from_base = (table - &(free_page_tables[0]));

    // // If table is less than free_page_tables, this difference will be huge
    // // If table is larger than the end of free_page_tables, this difference will still
    // // be bigger than NUM_TABLES
    // if (offset_from_base < NUM_TABLES) {
    //     free_page_tables_in_use[offset_from_base] = false;
    // }

    // The best portable C way to be 100% sure that a pointer points to an
    // array is to traverse the array in O(n) time and check, apparently.
    // Since freeing pages is rare, it's a small price to pay for salvation
    uint32_t i;
    for (i = 0; i < NUM_TABLES; i++) {
        if (table == (pte_t*)&(free_page_tables[i])) {
            free_page_tables_in_use[i] = false;
            table->present = 0;
        }
    }

    return;
}

/*
 * alloc_huge_page_virt
 *
 * Allocates a huge page (if possible) and returns virtual address to the caller.
 * On failure this returns a NULL address
 */
uint32_t alloc_huge_page_virt () {
    uint32_t i;
    for (i = FIRST_FREE_HUGE_PAGE; i < PD_NUM_ENTRIES; i++) {
        // Make use of the user-available bits to mark a page as having been allocated
        // This allows some degree of thread safety
        if (!page_dir[i].present && !page_dir[i].avail) {
            page_dir[i].avail = 1;
            return UNTAG_PDE(i);
        }
    }
    return NULL;
}

/*
 * free_huge_page_virt
 *
 * Frees a huge page allocated by alloc_huge_page
 * If the requested page is not a huge page, doesn't do anything
 */
void free_huge_page_virt (uint32_t idx) {
    if (idx >= PD_NUM_ENTRIES) return;
    if (!page_dir[idx].present) return;
    if (page_dir[idx].size == 0) return;
    page_dir[idx].val = 0;
    page_dir[idx].present = 0;

    // Mark this page as free for other alloc_huge_page calls:
    // (Recal avail is the user-defined bits, I am setting it to 0
    // if not allocated by alloc_huge_page, and 1 if it was).
    // This allows pages to be marked as allocated but not present.
    page_dir[idx].avail = 0;
}

/*
 * alloc_huge_page
 *
 * Allocates a huge page (if possible) and returns physical address to the caller.
 * On failure this returns a NULL address.
 */
void *alloc_huge_page () {
    uint32_t i;
    for (i = 0; i < NUM_HUGE_PAGES; i++) {
        if (!free_huge_pages_in_use[i]) {
            free_huge_pages_in_use[i] = true;
            return (void *)(UNTAG_PDE(i)) + FIRST_FREE_HUGE_PAGE;
        }
    }
    return NULL;
}

/*
 * free_huge_page
 *
 * Frees a page allocated by alloc_huge_page
 */
void free_huge_page(void *pg_ptr) {
    uint32_t pg = (uint32_t)pg_ptr;
    uint32_t pg_idx = TAG_PDE(pg - FIRST_FREE_HUGE_PAGE);
    if (pg_idx < NUM_HUGE_PAGES) {
        free_huge_pages_in_use[pg_idx] = false;
    }
}

/*
 * _enable_huge_allocator
 *
 * Sets up data structures needed by huge page allocator
 */
void _enable_huge_allocator() {
    uint32_t i;
    for (i = 0; i < NUM_HUGE_PAGES; i++) {
        free_huge_pages_in_use[i] = false;
    }
}

bool map_huge_page (uint32_t virt, uint32_t phys, bool user_page, bool writeable) {
    // Need an entire directory entry:
    uint32_t virt_dir_idx = DIR_IDX(virt);

    page_dir[virt_dir_idx].phys_addr = TAG(phys);
    page_dir[virt_dir_idx].size = 1;
    page_dir[virt_dir_idx].user_supervisor = user_page;
    page_dir[virt_dir_idx].read_write = writeable;
    page_dir[virt_dir_idx].present = 1;

    // Flush TLB and update page mappings:
    _load_page_dir(&(page_dir[0]));
    return true;
}

void unmap_huge_page(uint32_t virt) {
    uint32_t virt_dir_idx = DIR_IDX(virt);

    // Leave current page in a safe state:
    // Physical address is NULL, set it to non-writeable kernel page
    page_dir[virt_dir_idx].phys_addr = 0;
    page_dir[virt_dir_idx].size = 1;
    page_dir[virt_dir_idx].user_supervisor = 0;
    page_dir[virt_dir_idx].read_write = 0;

    // Unmap it
    page_dir[virt_dir_idx].present = 0;
    _load_page_dir(&(page_dir[0]));
}

// Map a virtual address to physical address, write into CR3
// Returns true on success, false on failure (couldn't find a free page table)
bool map_page(uint32_t virt, uint32_t phys, bool user_page, bool writeable) {
    // Indices can only be up to 1023 (lower 10 bits), so AND with 0x3FF
    uint32_t virt_dir_idx = DIR_IDX(virt);
    uint32_t virt_page_idx = PAGE_IDX(virt);

    // Virt and physical addresses (4kb aligned)
    virt = PAGE_ALIGN(virt);
    phys = PAGE_ALIGN(phys);

    // Find a free page table
    pte_t *page_table = NULL;
    if (!page_dir[virt_dir_idx].present) {
        // No page table present at this directory entry
        // Allocate a page table from the free tables
        page_table = alloc_page_table();

        // No free page tables, can't allocate this address
        if (!page_table) {
            return false;
        }

        // Write the found page table into the directory:
        page_dir[virt_dir_idx].phys_addr = TAG((uint32_t)(page_table));
        page_dir[virt_dir_idx].size = 0;
        page_dir[virt_dir_idx].user_supervisor = user_page;
        page_dir[virt_dir_idx].read_write = writeable;
        page_dir[virt_dir_idx].present = 1;
    }
    else if (page_dir[virt_dir_idx].present && page_dir[virt_dir_idx].size == 1) {
        // This is a huge page! Can't map a small page into it
        return false;
    }
    else {
        // Get a pointer to the page table already in use here
        page_table = (pte_t*)(UNTAG(page_dir[virt_dir_idx].phys_addr));
    }

    // We have a valid page directory pointing to a valid page table
    page_table[virt_page_idx].phys_addr = TAG(phys);
    page_table[virt_page_idx].user_supervisor = user_page;
    page_table[virt_page_idx].read_write = writeable;
    page_table[virt_page_idx].present = 1;

    // Reload the page mapping:
    // (This will flush TLB)
    _load_page_dir(&page_dir[0]);
    return true;
}

// Setup default page tables and directories, write into CR3
void enable_paging () {
    int i;
    // (Assumption: kernel fits into 1 huge page)

    // Enable all free pages:
    for (i = 0; i < NUM_TABLES; i++) {
        free_page_tables_in_use[i] = false;
    }

    // Setup huge page allocator
    _enable_huge_allocator();

    // Disable all pages in directory:
    for (i = 0; i < PD_NUM_ENTRIES; i++) {
        page_dir[i].val = 0;
    }

    // Map a huge page for the kernel and
    // a page table containing video memory
    // Kernel huge page:
    map_huge_page_kern ((uint32_t)&kernel_slide, (uint32_t)&kernel_slide);

    // Video memory & lower page table:
    map_page_kern (VGA_VIDMEM, VGA_VIDMEM);

    // Update page tables:
    // (Manual says that a jump instruction should follow changing CR3)
    asm volatile (
        "movl %%eax, %%cr3" : : "a"(&page_dir)
    );

    // Enable paging:
    // IA 32 Manual Page 4-4 Vol 3.A says CR4.PSE must be enabled for huge pages
    // This is bit 4 of CR4
    // Enable huge pages:
    asm volatile (
        "movl %%cr4, %%eax\n" \
        "orl $0x10, %%eax\n" \
        "movl %%eax, %%cr4" : : : "eax"
    );

    // Enable paging (bit 31 of CR0)
    asm volatile (
        "movl %%cr0, %%eax\n"
        "orl $0x80000000, %%eax\n" \
        "movl %%eax, %%cr0" : : : "eax"
    );
}

// Maps a single huge page for the filesystem
// @TODO: Walk the filesystem and map as many huge pages as needed
void *map_filesys_pages(void *fs_root) {
    uint32_t virt_root = ((uint32_t)fs_root + (FS_PAGENUM << 22));
    uint32_t end_addr = ((FS_PAGENUM + 1) << 22);

    uint32_t i;
    for (i = virt_root; i < end_addr; i += PAGE_SIZE) {
        if (!map_page(virt_root + (i-virt_root), (uint32_t)fs_root + (i-virt_root), false, true)) {
            return NULL;
        }
    }

    // Return the final address:
    return (void *)virt_root;
}

/*******************
 * Nicer paging methods
 *******************/
bool map_page_kern(uint32_t virt, uint32_t phys) {
    return map_page(virt, phys, false, true);
}

bool map_page_user(uint32_t virt, uint32_t phys) {
    return map_page(virt, phys, true, true);
}

bool map_page_user_readonly(uint32_t virt, uint32_t phys) {
    return map_page(virt, phys, true, false);
}

bool map_huge_page_kern(uint32_t virt, uint32_t phys) {
    return map_huge_page(virt, phys, false, true);
}

bool map_huge_page_user(uint32_t virt, uint32_t phys) {
    return map_huge_page(virt, phys, true, true);
}

bool map_huge_page_user_readonly(uint32_t virt, uint32_t phys) {
    return map_huge_page(virt, phys, true, false);
}
