/*! \file frame.c
 * 
 *  Contains methods for the frame table and frame pages.
 */

#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/process.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "frame.h"
#include "page.h"
#include "swap.h"

static struct list frame_table;	/* Frames in the table */
 // The frame table is accessed by multiple processes simultaneously. 
// Stay safe with a lock.
static struct lock frame_lock;
struct frame *frame_choose_victim(void); /* Chooses the next frame to free. */


/*! init_frame_table
 * 
 *  @description Initializes the frame table.
 * 
 */
void init_frame_table(void) {
	/* Initialize the frame table list. */
	list_init(&frame_table);
    lock_init(&frame_lock);
}

/*! frame_create
 *  
 *  @description This gets a new page from the user pool and adds it to the
 *  frame table.  If there are no available pages, this evicts one and 
 *  tries again.
 *  
 *  @return a pointer to the new page
 */
struct frame *frame_create(int flags, bool pinned) {

	lock_acquire(&frame_lock);

    /* Get a page from the user pool. */	
    void *kpage = palloc_get_page(flags);
	
	/* If couldn't get page, evict and try again. */
	if (!kpage) {
		frame_evict(frame_choose_victim());
		kpage = palloc_get_page(flags);
	}

    if (!kpage) {
        PANIC("Page eviction failed.");
    }

	/* Create and update the frame struct so we can add it to our table. */
	struct frame *new_frame = (struct frame *)calloc(1, sizeof(struct frame));
	new_frame->phys_addr = kpage;
    new_frame->page = NULL;
    new_frame->evicting = false;
    new_frame->pinned = (int)pinned;
    pagedir_set_dirty(thread_current()->pagedir, kpage, false);
	
	/* Add the ne page to the frame table. */
	list_push_back(&frame_table, &new_frame->frame_elem);

    lock_release(&frame_lock);
	
	return new_frame;
}

/*! frame_free
 * 
 *  @description This frees a frame and removes it from the frame list.
 *  Then it frees the page inside of it.
 * 
 *  @param fr - the frame to be freed
 *
 *  @return 0 if no errors, 1 otherwise.
 */
int frame_free(struct frame *fr) {
    if (!fr) return 1;

    bool unlock = false;
    if (!lock_held_by_current_thread(&frame_lock)) {
        unlock = true;
        lock_acquire(&frame_lock);
    }

	/* Remove the frame from the frame table. */
	list_remove(&fr->frame_elem);
    
    pagedir_clear_page(fr->page->pd, fr->page->vaddr);
	
	/* Remove the page so there is space. */
	palloc_free_page(fr->phys_addr);

	free((void*)fr);

    if (unlock) lock_release(&frame_lock);

	/* Return 0 if successful, 1 otherwise. */
	return 0;
}

/*! frame_choose_victim
 * 
 *  @description Chooses a frame whose page will be evicted to free it for the
 *  next page.
 * 
 *  @return a pointer to the frame whose page should be evicted.
 */
struct frame *frame_choose_victim(void) {
    // Implements second chance FIFO
    while(1) {
	    struct list_elem* cur = list_pop_front(&frame_table);
        struct frame *cur_frame = list_entry(cur, struct frame, frame_elem);
        struct supp_page *cur_page = cur_frame->page;
        // If the frame has been accessed, give it a second chance by putting 
        // it back in the queue with access bit reset
        if (pagedir_is_accessed(cur_page->pd, cur_frame->phys_addr) ||
                pagedir_is_accessed(cur_page->pd, cur_page->vaddr) ||
                cur_frame->evicting || cur_frame->pinned) {
            pagedir_set_accessed(cur_page->pd, cur_frame->phys_addr, false);
            pagedir_set_accessed(cur_page->pd, cur_page->vaddr, false);
            list_push_back(&frame_table, cur);
        } else {
            // Otherwise the frame has already been removed so return it
            return cur_frame;
        }
    }
}

/*! frame_evict
 *  
 *  @description Evicts a page from a frame to free it for another page's use.
 */
void frame_evict(struct frame *fr) {
    fr->evicting = true;

    bool unlock = false;
    if (!lock_held_by_current_thread(&frame_lock)) {
        unlock = true;
        lock_acquire(&frame_lock);
    }
    
	struct supp_page *spg = fr->page;
    ASSERT(spg->fr == fr);
    ASSERT(fr->evicting);
	switch (spg->type) {
		case filesys :
	        if (pagedir_is_dirty(spg->pd, spg->vaddr)) {
	            /* If it is dirty, we need to store the data. */
				/* Write it back to the file. */
				file_write_at(spg->fil, spg->fr->phys_addr, 
                           spg->bytes, spg->offset);
                pagedir_set_dirty(spg->pd, spg->vaddr, false);
		    }
			break;
		case swapslot :
		    /* Write to a swap. */
			spg->swap = swap_put_page(spg->fr->phys_addr);
			break;
		default :
			PANIC ("Error evicting frame.\n");
			break;
	}

    /* Clear the page and frame. */
    ASSERT(!frame_free(spg->fr));
    spg->fr = NULL;
    if (unlock) lock_release(&frame_lock);
}
