#ifndef PAGING_H
#define PAGING_H
#include "types.h"
#include "defines.h"
#include "filesystem.h"

// Huge page index of the filesystem
#define FS_PAGENUM ((4))

void enable_paging(void);

// See i386 Manual Chapter 5: https://css.csail.mit.edu/6.858/2014/readings/i386.pdf
// Also see this page on OS Dev Wiki: https://wiki.osdev.org/Paging

// Convert to/ from address <-> page address "tag"
#define TAG(x) ((((x)) >> 12))
#define UNTAG(x) ((((x)) << 12))

#define TAG_PDE(x) ((((x)) >> 22))
#define UNTAG_PDE(x) ((((x)) << 22))

// Address manipulation defines
#define OFFSET(x) ((x & 0x0FFF))
#define PAGE_IDX(x) (((x >> 12) & 0x03FF))
#define DIR_IDX(x) (((x >> 22) & 0x03FF))
#define PAGE_ALIGN(x) ((x & ~0x0FFF))
#define DIR_ALIGN(x) ((x & ~0x03FFFFF))

// Page directory entry:
typedef struct __attribute__((packed)) pde_struct_t {
    union {
        uint32_t val;
        struct {
            uint8_t present : 1; // 1 = present, 0 = not present (unmapped)
            uint8_t read_write : 1; // If set, RW. Else Read-only. Can override this with CR0
            uint8_t user_supervisor : 1; // If 1, all can access, if 0, only kernel can access
            uint8_t write_through : 1; // If set, caches are write-through. If not, caches are write-back.
            uint8_t cache_disabled : 1; // If set, page will not be cached
            uint8_t accessed : 1; // Set when page read from or written into
            uint8_t dirty : 1; // Unused in PDE (hardware never sets this)
            uint8_t size : 1; // 0 for small, 1 for huge
            uint8_t global : 1;
            uint8_t avail : 3; // These bits are free for us to do whatever
            uint32_t phys_addr : 20;
        };
    };
} pde_t;

// Page table entry:
typedef struct __attribute__((packed)) pte_struct_t {
    union {
        uint32_t val;
        struct {
            uint8_t present : 1; // 1 = present, 0 = not present (unmapped)
            uint8_t read_write : 1; // If set, RW. Else Read-only. Can override this with CR0
            uint8_t user_supervisor : 1; // If 1, all can access, if 0, only kernel can access
            uint8_t write_through : 1; // If set, caches are write-through. If not, caches are write-back.
            uint8_t cache_disabled : 1; // If set, page will not be cached
            uint8_t accessed : 1; // Read from or written to
            uint8_t dirty : 1; // Page has been written to
            uint8_t res0 : 1; // Unused in PTE
            uint8_t global : 1;
            uint8_t avail : 3; // These bits are free for us to do whatever
            uint32_t phys_addr : 20;
        };
    };
} pte_t;

// Number of entries is the size of a page / size of an entry
#define PD_NUM_ENTRIES ((PAGE_SIZE/sizeof(pde_t)))
#define PT_NUM_ENTRIES ((PAGE_SIZE/sizeof(pte_t)))

// @TODO: Set this by reading multiboot high memory size
// The number of huge pages after 1 MB we that are free for allocating
#define NUM_HUGE_PAGES ((1024))
void *alloc_huge_page ();
void free_huge_page(void *pg_ptr);

// Map a requested virtual address to physical address
// Returns true on success, false on failure (couldn't find a free page table)
// Generic methods that actually do the writing:
bool map_page(uint32_t virt, uint32_t phys, bool user_page, bool writeable);
bool map_huge_page (uint32_t virt, uint32_t phys, bool user_page, bool writeable);

// Misc. methods that are more user-friendly:
// (These all call either map_page or map_huge_page)
bool map_page_kern(uint32_t virt, uint32_t phys);
bool map_page_user(uint32_t virt, uint32_t phys);
bool map_page_user_readonly(uint32_t virt, uint32_t phys);
bool map_huge_page_kern(uint32_t virt, uint32_t phys);
bool map_huge_page_user(uint32_t virt, uint32_t phys);
bool map_huge_page_user_readonly(uint32_t virt, uint32_t phys);

// Unmap a huge page:
void unmap_huge_page(uint32_t virt);

// Maps a single huge page for the filesystem
// @TODO: Walk the filesystem and map as many huge pages as needed
void *map_filesys_pages(void *fs_root);

#endif
