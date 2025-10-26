/*
 File: vm_pool.C
 
 Author: Nithin Gowtham Saravanan
         Department of Electrical and Computer Engineering
         Texas A&M University
 Date  : 10/25/2025
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "page_table.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

// Helper function to round up bytes to pages
static inline unsigned long bytes_to_pages(unsigned long bytes)
{
    return (bytes + PageTable::PAGE_SIZE - 1) / PageTable::PAGE_SIZE;
}

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) 
{
    assert(_frame_pool != nullptr);
    assert(_page_table != nullptr);

    base_address = _base_address;
    size_bytes = _size;
    frame_pool = _frame_pool;
    page_table = _page_table;

    n_allocated = 0;
    n_free = 0;

    /* Initialize free region to entire pool (in pages, using absolute virtual pages) */
    unsigned long base_page = base_address / PageTable::PAGE_SIZE;
    unsigned long total_pages = bytes_to_pages(size_bytes);

    free_regions[0].base_page = base_page;
    free_regions[0].length = total_pages;
    n_free = 1;

    /* Register ourselves with the PageTable so page-fault handler can query legitimacy */
    page_table->register_pool(this);

    Console::puts("Constructed VMPool object.\n");
    Console::puts("\tBase virt addr: ");
    Console::putui(base_address);
    Console::puts(", size bytes: ");
    Console::putui(size_bytes);
    Console::puts(", pages: ");
    Console::putui(total_pages);
    Console::puts("\n");
}

unsigned long VMPool::allocate(unsigned long _size) 
{
    Console::puts("VMPool::allocate called (size bytes = ");
    Console::putui(_size);
    Console::puts(")\n");

    if (_size == 0) 
        return 0;

    unsigned long need_pages = bytes_to_pages(_size);

    /* First-fit: Find a free region with enough pages */
    for (unsigned int i = 0; i < n_free; ++i) 
    {
        if (free_regions[i].length >= need_pages)
        {
            /* Use the first portion of this free region */
            unsigned long alloc_base_page = free_regions[i].base_page;
            /* Update free region */
            free_regions[i].base_page += need_pages;
            free_regions[i].length -= need_pages;
            if (free_regions[i].length == 0) 
            {
                /* Remove this free region by shifting */
                for (unsigned int j = i; j + 1 < n_free; ++j)
                    free_regions[j] = free_regions[j+1];
                n_free--;
            }

            /* Add to allocated regions */
            assert(n_allocated < MAX_REGIONS); // otherwise out of bookkeeping space
            allocated_regions[n_allocated].base_page = alloc_base_page;
            allocated_regions[n_allocated].length = need_pages;
            n_allocated++;

            unsigned long vaddr = alloc_base_page * PageTable::PAGE_SIZE;
            Console::puts("\tVMPool::allocate returning virtual addr ");
            Console::putui(vaddr);
            Console::puts("\n");
            return vaddr;
        }
    }

    /* No suitable free region */
    Console::puts("\tVMPool::allocate failed - no contiguous free region\n");
    return 0;
}

void VMPool::release(unsigned long _start_address) 
{
    Console::puts("VMPool::release called for address ");
    Console::putui(_start_address);
    Console::puts("\n");

    /* Compute start page and find allocated region */
    unsigned long start_page = _start_address / PageTable::PAGE_SIZE;
    int idx = -1;
    for (unsigned int i = 0; i < n_allocated; ++i) 
    {
        if (allocated_regions[i].base_page == start_page) {
            idx = (int)i;
            break;
        }
    }

    if (idx == -1) {
        Console::puts("\tVMPool::release: start address not found in allocated regions\n");
        assert(false);
        return;
    }

    unsigned long length = allocated_regions[idx].length;

    /* For each page in the region, ask page table to free if present */
    for (unsigned long p = 0; p < length; ++p) 
    {
        unsigned long page_no = allocated_regions[idx].base_page + p;
        page_table->free_page(page_no);
    }

    /* Move this region from allocated array to free array (no coalescing) */
    assert(n_free < MAX_REGIONS);
    free_regions[n_free].base_page = allocated_regions[idx].base_page;
    free_regions[n_free].length = allocated_regions[idx].length;
    n_free++;

    /* Remove allocated region by shifting */
    for (unsigned int j = idx; j + 1 < n_allocated; ++j)
        allocated_regions[j] = allocated_regions[j+1];
    n_allocated--;

    Console::puts("\tVMPool::release completed - Released region of memory.\n");
}

bool VMPool::is_legitimate(unsigned long _address) 
{
    unsigned long page = _address / PageTable::PAGE_SIZE;

    for (unsigned int i = 0; i < n_allocated; ++i) 
    {
        unsigned long start = allocated_regions[i].base_page;
        unsigned long end = start + allocated_regions[i].length; // exclusive
        if (page >= start && page < end) 
            return true;
    }
    return false;

    Console::puts("Checked whether address is part of an allocated region.\n");
}

