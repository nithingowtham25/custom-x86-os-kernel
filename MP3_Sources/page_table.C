#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

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

PageTable::PageTable()
{
   Console::puts("Page Table Constructor\n");

   /* Step 1: Allocate one frame for the page directory */
   unsigned long pd_frame = kernel_mem_pool->get_frames(1);
   page_directory = (unsigned long *)(pd_frame * PAGE_SIZE);
   
   // Inline zeroing
   unsigned char *p = (unsigned char *)page_directory;
   for (unsigned long i = 0; i < PAGE_SIZE; ++i) 
      p[i] = 0;

   Console::puts("\tPage Directory frame is allocated in kernel pool\n");

   /* Step 2: Compute number of shared pages and PDEs */
   unsigned long shared_pages = (shared_size + PAGE_SIZE - 1) / PAGE_SIZE;
   unsigned long num_pde = (shared_pages + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;

   Console::puts("\tShared region requires ");
   Console::putui(shared_pages);
   Console::puts(" pages across ");
   Console::putui(num_pde);
   Console::puts(" page directory entries\n");

   /* Step 3: Build page tables for directly-mapped shared region */
   for (unsigned long pd_idx = 0; pd_idx < num_pde; ++pd_idx) 
   {
      unsigned long pt_frame = kernel_mem_pool->get_frames(1);
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
         page_table[i] = (page_no << 12) | P_PRESENT | P_RW;
      }

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
   // Console::puts("  -> Faulting address: 0x");
   // print_hex(fault_addr);
   // Console::puts("\n");

   unsigned long fault_page = fault_addr / PAGE_SIZE; // Compute faulting page number
   // Console::puts("  -> Faulting Page #: ");
   // Console::putui(fault_page);
   // Console::puts("\n");

   /* Decode directory and table indexes */
   unsigned int pd_idx = (fault_addr >> 22) & 0x3FF;
   unsigned int pt_idx = (fault_addr >> 12) & 0x3FF;

   unsigned long *page_directory = current_page_table->page_directory;
   unsigned long pde = page_directory[pd_idx];

   /* Step 2: Ensure page table exists */
   if ((pde & P_PRESENT) == 0) {
      Console::puts("\tMissing Page Table. Allocating new one...\n");

      unsigned long new_pt_frame = kernel_mem_pool->get_frames(1);
      unsigned long *new_pt = (unsigned long *)(new_pt_frame * PAGE_SIZE);
      
      // Inline zeroing
      unsigned char *pt_ptr = (unsigned char *)new_pt;
      for (unsigned long i = 0; i < PAGE_SIZE; ++i) 
         pt_ptr[i] = 0;

      page_directory[pd_idx] = (new_pt_frame << 12) | P_PRESENT | P_RW;
      pde = page_directory[pd_idx];

      Console::puts("\tNew Page Table allocated and linked\n");
   }

   /* Step 3: Access the page table */
   unsigned long pt_frame = (pde >> 12) & 0xFFFFF;
   unsigned long *page_table = (unsigned long *)(pt_frame * PAGE_SIZE);
   unsigned long pte = page_table[pt_idx];

   /* Step 4: Check if the page itself is present */
   if ((pte & P_PRESENT) == 0) {
      Console::puts("\tPage not present. Allocating data frame...\n");

      unsigned long data_frame = process_mem_pool->get_frames(1);

      // Do NOT dereference physical memory here — safe mapping only
      page_table[pt_idx] = (data_frame << 12) | P_PRESENT | P_RW;
      invlpg((void *)fault_addr); // Flush TLB entry

      Console::puts("\tMapped Faulted Page #: ");
      Console::putui(fault_page);
      Console::puts(" from physical memory (process pool) to entry #: ");
      Console::putui(fault_page % 1024);
      Console::puts(" of page table #: ");
      Console::putui(fault_page / 1024);
      Console::puts("\n");

      Console::puts("Page Fault successfully handled\n\n");
   } 
   else {
      Console::puts("\tPage already present\n\n");
   }

   return;
}

