/*
 File: ContFramePool.C
 
 Author: Nithin Gowtham Saravanan
         Department of Electrical and Computer Engineering
         Texas A&M University
 Date  : 09/17/2025
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

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

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* STATE MANAGEMENT */
/*--------------------------------------------------------------------------*/

ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no)
{
    unsigned long bitmap_index = _frame_no / 4; // 4 frames per byte (2 bits each)
    unsigned int shift = (_frame_no % 4) * 2;    // select which 2-bit pair
    unsigned char state = (bitmap[bitmap_index] >> shift) & 0x3;
    return static_cast<FrameState>(state);
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state)
{
    unsigned long bitmap_index = _frame_no / 4; // 4 frames per byte (2 bits each)
    unsigned int shift = (_frame_no % 4) * 2;   // select which 2-bit pair
    unsigned char mask = 0x3 << shift;          // Select the 2 bits to clear
    // Clear the bits and set the new state
    bitmap[bitmap_index] = (bitmap[bitmap_index] & ~mask) | 
                           (static_cast<unsigned char>(_state) << shift);
}

/* To maintain the pool list to track in which pool the given frame is*/
ContFramePool *ContFramePool::pool_list = nullptr;

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

/* Constructor Implementation */
ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    // Bitmap must fit in a single frame!
    assert(_n_frames <= FRAME_SIZE * 4); // 4 frames per byte (2 bits each)

    base_frame_no = _base_frame_no;
    nframes = _n_frames;
    nFreeFrames = _n_frames;
    info_frame_no = _info_frame_no;

    // If _info_frame_no is zero then we keep management info in the first
    // frame, else we use the provided frame to keep management info
    if(info_frame_no == 0) {
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);
    }

    // Everything ok. Proceed to mark all frame as free.
    for(int fno = 0; fno < _n_frames; fno++) {
        set_state(fno, FrameState::Free);
    }

    // Mark metadata/info frame(s) as used
    if (_info_frame_no == 0) 
    {
        set_state(0, FrameState::HoS);
        nFreeFrames--;
    } 
    else if (_info_frame_no >= base_frame_no && _info_frame_no < base_frame_no + nframes) 
    {
        unsigned long local = _info_frame_no - base_frame_no;
        set_state(local, FrameState::HoS);
        nFreeFrames--;
   }

    // Insert this pool into the global pool list
    this->next = pool_list;
    pool_list  = this;

    Console::puts("Contiguous Frame Pool initialized\n");
}


unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    Console::puts("Executing get_frames\n");

    // Any frames left to allocate?
    if (_n_frames == 0) 
        return 0;
    if (_n_frames > nFreeFrames)
        return 0;
    
    // Console::puts("ContFramePool::get_frames: requested ");
    // Console::puti(_n_frames);
    // Console::puts(" frames\n");

    // Scan for a contiguous run of _n_frames free frames
    unsigned long run = 0;
    unsigned long start = 0;

    for (unsigned long frame_no = 0; frame_no < nframes; frame_no++) 
    {
        if (get_state(frame_no) == FrameState::Free) 
        {
            if (run == 0) 
                start = frame_no;   // start of new run
            run++;

            if (run == _n_frames) 
            {
                // Found enough contiguous free frames
                Console::puts("Success: found contiguous block from local frames ");
                Console::puti(start);
                Console::puts(" to ");
                Console::puti(start + _n_frames - 1);
                Console::puts("\n");
                set_state(start, FrameState::HoS);

                for (unsigned long i = 1; i < _n_frames; i++) {
                    set_state(start + i, FrameState::Used);
                }
                nFreeFrames -= _n_frames;
                unsigned long abs_frame = start + base_frame_no;
                // Console::puts("Returning absolute frame no ");
                // Console::puti(abs_frame);
                // Console::puts("\n");

                return abs_frame;
            }
        } 
        else
            run = 0;  // reset run if frame is not free
    }

    // If we reach here, no contiguous block found
    Console::puts("No contiguous block available!\n");
    return 0;
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    Console::puts("Marking inaccessible memory range: ");
    Console::puti(_base_frame_no);
    Console::puts(" - ");
    Console::puti(_base_frame_no + _n_frames - 1);
    Console::puts(" (frame numbers)\n");

    // Mark all frames in the range as being used (absolute frame numbers)
    for (unsigned long fno = _base_frame_no; fno < _base_frame_no + _n_frames; fno++)
    {
        if (fno >= base_frame_no && fno < base_frame_no + nframes) {
            unsigned long local = fno - base_frame_no;

            FrameState prev = get_state(local);
            if (fno == _base_frame_no)
                set_state(local, FrameState::HoS);
            else
                set_state(local, FrameState::Used);
            if (prev == FrameState::Free)
                nFreeFrames--;
        }
    }
}


void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    Console::puts("ContFramePool::release_frames called for frame ");
    Console::puti(_first_frame_no);
    Console::puts("\n");

    ContFramePool *pool = pool_list;
    while (pool) 
    {
        if (_first_frame_no >= pool->base_frame_no &&
            _first_frame_no < pool->base_frame_no + pool->nframes)
        {
            unsigned long local = _first_frame_no - pool->base_frame_no;
            assert(pool->get_state(local) == FrameState::HoS);

            Console::puts("Freeing HoS at local frame ");
            Console::puti(local);
            Console::puts("\n");

            pool->set_state(local, FrameState::Free);   //set HoS frame to Free
            pool->nFreeFrames++;

            for (unsigned long i = local + 1; i < pool->nframes; i++) 
            {
                FrameState st = pool->get_state(i);
                if (st == FrameState::Used) 
                {
                    Console::puts("  Freeing Used frame at local ");
                    Console::puti(i);
                    Console::puts("\n");
                    pool->set_state(i, FrameState::Free);
                    pool->nFreeFrames++;
                } 
                else
                    break;
            }
            return;
        }
        pool = pool->next;
    }

    // If we reach here, no pool found for the given frame
    Console::puts("Error: release_frames called on unknown frame\n");
    assert(false);
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    // 2 bits per frame → total bytes needed
    unsigned long bytes_needed = (_n_frames * 2 + 7) / 8;

    // Round up to nearest frame
    unsigned long frames_needed = (bytes_needed + FRAME_SIZE - 1) / FRAME_SIZE;

    Console::puts("ContFramePool::needed_info_frames: frames needed = ");
    Console::puti(frames_needed);
    Console::puts("\n");

    return frames_needed;
}
