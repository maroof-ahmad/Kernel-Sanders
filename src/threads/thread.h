/*! \file thread.h
 *
 * Declarations for the kernel threading functionality in PintOS.
 */

#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/fixed_point.h"

/*! States in a thread's life cycle. */
enum thread_status {
    THREAD_RUNNING,     /*!< Running thread. */
    THREAD_READY,       /*!< Not running but ready to run. */
    THREAD_BLOCKED,     /*!< Waiting for an event to trigger. */
    THREAD_DYING        /*!< About to be destroyed. */
};

/*! When a thread becomes ready of higher priority than that which is running,
    it must start running immediately.  If it is inside a process which has
    interrupts off, then we must wait until interrupts are on. */
bool thread_waiting;

/*! Thread identifier type.
    You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /*!< Error value for tid_t. */

/** Process identifier type. */
typedef int16_t pid_t;
#define PID_ERROR ((pid_t) -1)

/* Thread priorities. */
#define PRI_MIN 0                       /*!< Lowest priority. */
#define PRI_DEFAULT 31                  /*!< Default priority. */
#define PRI_MAX 63                      /*!< Highest priority. */

/*! A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

\verbatim
        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+
\endverbatim

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion.

   The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list.
*/
struct thread {
    /*! Owned by thread.c. */
    /**@{*/
    tid_t tid;                          /*!< Thread identifier. */
    pid_t pid;                          /*!< Process id, or -1 if not a proces. */
    enum thread_status status;          /*!< Thread state. */
    char name[16];                      /*!< Name (for debugging purposes). */
    uint8_t *stack;                     /*!< Saved stack pointer. */
    int priority;                       /*!< Priority. */
    struct list_elem allelem;           /*!< List element for all threads list. */
    int64_t wake_time;                  /*!< The time at which this thread will wake from a sleep() call */
    /**@}*/
    
    /* Used by BSD scheduler */
    /**@{*/
    int nice;                           /*!< Niceness. */
    fixed_point recent_cpu;             /*!< How much CPU time the process recently received. */
    /**@}*/

    /*! Shared between thread.c and synch.c. */
    /**@{*/
    struct list_elem elem;              /*!< List element. */
    /**@}*/
    
    /*! Priority donations received and given. */
    struct list priority_donations;		/*!< Threads from which priority was received. */
    struct list_elem priority_elem;		/*!< The resource passed around as priority donations. */
    int orig_priority;					/*!< Keep track of the priority before receiving donation. */
	struct lock *lock_needed;			/*!< The lock that the struct needs. */

#ifdef USERPROG
    /*! Owned by userprog/process.c. */
    /**@{*/
    uint32_t *pagedir;                  /*!< Page directory. */
    /**@{*/
#endif

    /** Needed for stack growth through a system call. */
    void* esp;                          /*!< Thread's stack pointer. */
    bool in_sc;                      /* Whether the thread is in a system call. */

    /*! Owned by thread.c. */
    /**@{*/
    unsigned magic;                     /* Detects stack overflow. */
    /**@}*/
};

/*! If false (default), use round-robin scheduler.
    If true, use multi-level feedback queue scheduler.
    Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);
void thread_sleep(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);
struct thread *thread_create_ptr(const char *name, int priority, thread_func *, void *, pid_t);

void thread_block(void);
void thread_unblock(struct thread *);

void thread_donate_priority(struct thread *donate_to);
void thread_takeback_priority(struct thread *donor, struct thread *donee);

struct thread *thread_current (void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(int code) NO_RETURN;
void thread_yield(void);

/*! Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread *t, void *aux);

void thread_foreach(thread_action_func *, void *);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

bool is_thread(struct thread*);

/*! Returns the list_item of the highest priority thread in the list. */
struct list_elem *list_highest_priority (struct list *in_list);

#endif /* threads/thread.h */

