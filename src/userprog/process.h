#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_PROCESSES 1024
#define MAX_NAME_LEN 260
#define MAX_ARGS 100
#define INIT_PID 0

/** A single element in the system process table. */
struct process {

    /** A pointer to the thread structure. Only valid if process is running. */
    struct thread *thread_ptr; 

    /** A buffer storing the process name. **/
    char name[MAX_NAME_LEN + 1];

    /** Ids of this process and its parent. */
    pid_t pid;
    pid_t parent_pid; /** -1 if parent has died. */

    /** A list of child processes. */
    struct list children;

    /** The return code the process supplies to exit() or -1 if killed by kernel.
     * Only valid if process is not running. */
    int return_code;

    /** Linked list structure. Reused for children list and free_list. */
    struct list_elem elem;

    /** Whether the process is running, or has completed. */
    bool running;

    /** Whether this struct represents a valid process, or is unallocated. */
    bool valid;

    /** Whether the parent process is blocked waiting on this process.  */
    bool blocked;

    bool loaded;

    bool load_success;

};

void process_init(void);
int process_count(void);
pid_t process_execute(const char *file_name);
int process_wait(pid_t);
void process_exit(int code);
void process_activate(void);

#endif /* userprog/process.h */

