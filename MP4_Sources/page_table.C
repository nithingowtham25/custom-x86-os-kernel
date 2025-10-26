/*
 File: page_table.C
 
 Author: Nithin Gowtham Saravanan
         Department of Electrical and Computer Engineering
         Texas A&M University
 Date  : 10/23/2025
 
 */
#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"
#include "vm_pool.H"

/* Initialization */
PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;

/* Constants for page flags */
#define P_PRESENT 0x1   // Present bit (must be 1 for valid entry) -> bit 0
#define P_RW      0x2   // Read/Write (0 - read only; 1 = r/w) -> bit 1
#define P_USER    0x4   // User/Supervisor (0 - Kernel; 1 - user accessible) -> bit 2

/* Invalidate a TLB entry */
static inline void invlpg(void *addr) 
{
    asm volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

/* Helper constants for recursive mapping:
 * PDE logical base: 0xFFFFF000  (maps page directory entries as 1023th PDE -> PD)
 * PTE logical base: 0xFFC00000  (maps page tables via recursive PDE trick)
 */
static const unsigned long RECURSIVE_PDE_BASE = 0xFFFFF000UL;
static const unsigned long RECURSIVE_PTE_BASE = 0xFFC00000UL;

/* Initialize Paging */
void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
   kernel_mem_pool  = _kernel_mem_pool;
   process_mem_pool = _process_mem_pool;
   shared_size      = _shared_size;
   paging_enabled   = 0;
   current_page_table = nullptr;

   Console::puts("Initialized Paging System\n");
   Console::puts("\tKernel pool and process pool registered\n");
   Console::puts("\tShared region size is set to 4 MB\n\n");
}

/* Return pointer (logical) to the PDE of 'addr' using recursive trick */
unsigned long * PageTable::PDE_address(unsigned long addr)
{
    unsigned int pd_idx = (addr >> 22) & 0x3FF;          // top 10 bits
    unsigned long addr_pde = RECURSIVE_PDE_BASE + (pd_idx * sizeof(unsigned long));
    return (unsigned long *)addr_pde;
}

/* Return pointer (logical) to the PTE of 'addr' using recursive trick */
unsigned long * PageTable::PTE_address(unsigned long addr)
{
    unsigned int pd_idx = (addr >> 22) & 0x3FF;
    unsigned int pt_idx = (addr >> 12) & 0x3FF;
    unsigned long addr_pte = RECURSIVE_PTE_BASE
                            + ((unsigned long)pd_idx << 12)    // select page table page
                            + (pt_idx * sizeof(unsigned long)); // select entry
    return (unsigned long *)addr_pte;
}

PageTable::PageTable()
{
   Console::puts("Page Table Constructor\n");

   /* Step 1: Allocate one frame for the page directory */
   unsigned long pd_frame = process_mem_pool->get_frames(1);
   page_directory = (unsigned long *)(pd_frame * PAGE_SIZE);
   
   // Inline zeroing
   unsigned char *p = (unsigned char *)page_directory;
   for (unsigned long i = 0; i < PAGE_SIZE; ++i) 
      p[i] = 0;

   Console::puts("\tPage Directory frame is allocated in process pool\n");

   /* Set the recursive PDE (last entry) to point to the directory itself.
      This allows us to access page directory and page tables through
      recursive logical mappings after paging is enabled.
      Note: Store PDE now as physical frame reference.
    */
   page_directory[1023] = (pd_frame << 12) | P_PRESENT | P_RW;

   /* Step 2: Compute number of shared pages and PDEs */
   unsigned long shared_pages = (shared_size + PAGE_SIZE - 1) / PAGE_SIZE;
   unsigned long num_pde = (shared_pages + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;

   Console::puts("\tShared region requires ");
   Console::putui(shared_pages);
   Console::puts(" pages across ");
   Console::putui(num_pde);
   Console::puts(" page directory entries\n");

   /* Step 3: Build page tables for directly-mapped shared region 
      Note: Page tables are allocated from process frame pool now (for MP4)*/
   for (unsigned long pd_idx = 0; pd_idx < num_pde; ++pd_idx) 
   {
      unsigned long pt_frame = process_mem_pool->get_frames(1);
      unsigned long *page_table = (unsigned long *)(pt_frame * PAGE_SIZE);
      
      // Inline zeroing
      unsigned char *pt_ptr = (unsigned char *)page_table;
      for (unsigned long i = 0; i < PAGE_SIZE; ++i) 
         pt_ptr[i] = 0;

      unsigned long base_page = pd_idx * ENTRIES_PER_PAGE;
      for (unsigned long i = 0; i < ENTRIES_PER_PAGE; ++i) 
      {
         unsigned long page_no = base_page + i;
         if (page_no >= shared_pages) 
            break;
         /* Identity-map shared pages: PTE points to physical frame with same page number */
         page_table[i] = (page_no << 12) | P_PRESENT | P_RW;
      }

      /* PDE points to the page-table frame (physical) */
      page_directory[pd_idx] = (pt_frame << 12) | P_PRESENT | P_RW;

      Console::puts("\tBuilt Page Table ");
      Console::putui(pd_idx);
      Console::puts(" for shared region\n");
   }

   /* Step 4: Set this page table as current (not loaded yet) */
   current_page_table = this;

   Console::puts("Constructed Page Table object\n\n");
}


void PageTable::load()
{
   Console::puts("Loading Page Table\n");

   unsigned long pd_phys = (unsigned long)page_directory;
   write_cr3(pd_phys);               // Load physical address of directory into CR3
   current_page_table = this;

   Console::puts("\tPage Directory loaded into CR3\n");
   Console::puts("\tThis Page Table is now active\n");
   Console::puts("Loaded page table\n\n");
}

void PageTable::enable_paging()
{
   Console::puts("Enabling Paging in CR0\n");

   unsigned long cr0 = read_cr0();
   cr0 |= 0x80000000UL;    // Set PG bit (bit 31)
   write_cr0(cr0);

   paging_enabled = 1;

   Console::puts("\tPG bit set, paging is now enabled!\n\n");
}

void PageTable::handle_fault(REGS * _r)
{
   Console::puts("Page Fault triggered\n");
   /* Step 1: Get the faulting virtual address */
   unsigned long fault_addr = read_cr2();

   /* Legitimacy check only if any pools are registered (VM-pool mode).
      In pure page-table test mode (no pools), allow demand paging. */
   if (current_page_table->vm_pools_head != nullptr) 
   {
      VMPool *vp = current_page_table->vm_pools_head;
      bool legitimate = false;
      while (vp) 
      {
         if (vp->is_legitimate(fault_addr)) 
         { 
            legitimate = true; break; 
         }
         vp = vp->next;
      }
      if (!legitimate) 
      {
         Console::puts("Page Fault: Invalid access (address not allocated by any VMPool). Aborting.\n");
         assert(false);
         return;
      }
   }

   /* Directory index for the fault */
   unsigned int pd_idx = (fault_addr >> 22) & 0x3FF;

   /* Step 2: Ensure page table exists — use recursive PDE pointer */
   unsigned long *pde_ptr = current_page_table->PDE_address(fault_addr);
   unsigned long  pde     = *pde_ptr;

   if ((pde & P_PRESENT) == 0) {
      Console::puts("\tMissing Page Table. Allocating new one...\n");

      unsigned long new_pt_frame = process_mem_pool->get_frames(1);
      *pde_ptr = (new_pt_frame << 12) | P_PRESENT | P_RW;

      /* Make the new PT page visible via recursive window */
      unsigned long cr3 = read_cr3(); write_cr3(cr3);   // full TLB flush (simple & safe)

      /* Zero the PT via its recursive mapping */
      unsigned long *pt_page = (unsigned long *)(RECURSIVE_PTE_BASE + ((unsigned long)pd_idx << 12));
      for (unsigned i = 0; i < ENTRIES_PER_PAGE; ++i) pt_page[i] = 0;

      Console::puts("\tNew Page Table allocated and linked\n");
   }

   /* Step 3: Map the faulting page via recursive PTE pointer */
   unsigned long *pte_ptr = current_page_table->PTE_address(fault_addr);
   if ((*pte_ptr & P_PRESENT) == 0) {
      Console::puts("\tPage not present. Allocating data frame...\n");

      /* Allocate the physical frame from the owning VMPool’s frame pool */
      unsigned long data_frame = process_mem_pool->get_frames(1);

      *pte_ptr = (data_frame << 12) | P_PRESENT | P_RW;
      invlpg((void *)fault_addr); // Flush only this entry

      Console::puts("\tMapped faulted VA using a frame from its VMPool\n");
   } 
   else {
      Console::puts("\tPage already present\n");
   }
}

void PageTable::register_pool(VMPool * _vm_pool)
{
   Console::puts("PageTable::register_pool called\n");
   assert(_vm_pool != nullptr);
   _vm_pool->next = vm_pools_head;
   vm_pools_head = _vm_pool;
   Console::puts("\tVMPool registered with PageTable\n");
}

void PageTable::free_page(unsigned long _page_no)
{
   Console::puts("PageTable::free_page called for page ");
   Console::putui(_page_no);
   Console::puts("\n");

   unsigned long virtual_addr = _page_no * PAGE_SIZE;

   /* Get pointer to PTE via recursive mapping */
   unsigned long * pte_ptr = PTE_address(virtual_addr);
   unsigned long pte = *pte_ptr;

   if ((pte & P_PRESENT) == 0) {
      Console::puts("\tPage not present (nothing to free)\n");
      return;
   }

   /* Extract physical frame number from PTE */
   unsigned long phys_frame = (pte >> 12) & 0xFFFFF;

   Console::puts("\tReleasing physical frame ");
   Console::putui(phys_frame);
   Console::puts("\n");

   /* Release the frame back to the frame pool (absolute frame number) */
   ContFramePool::release_frames(phys_frame);

   /* Invalidate the PTE and flush that page from the TLB */
   *pte_ptr = 0;
   invlpg((void *)virtual_addr);

   Console::puts("\tPage freed and TLB entry invalidated\n");
   return;
}
