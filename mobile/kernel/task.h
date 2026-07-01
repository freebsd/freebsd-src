/*
 * Task and Process Management
 * uOS(m) - User OS Mobile
 * Process scheduling, context switching
 */

#ifndef _TASK_H_
#define _TASK_H_

#include <stdint.h>
#include <stddef.h>
#include "vm.h"

/* Task states */
typedef enum {
    TASK_CREATED = 0,
    TASK_READY = 1,
    TASK_RUNNING = 2,
    TASK_BLOCKED = 3,
    TASK_ZOMBIE = 4,
    TASK_STOPPED = 5
} task_state_t;

typedef enum {
    SCHED_PRIORITY_LOW = 0,
    SCHED_PRIORITY_NORMAL = 1,
    SCHED_PRIORITY_HIGH = 2,
    SCHED_PRIORITY_REALTIME = 3
} sched_priority_t;

/* Forward declaration for runqueue_t */
typedef struct task_struct task_t;

typedef struct {
    uint32_t mask;           /* Which priority levels have tasks */
    task_t *head;            /* Head of runqueue per priority */
    task_t *tail;            /* Tail for O(1) enqueue */
} runqueue_t;

/* Context structure - RISC-V registers */
typedef struct {
    uint64_t pc;            /* Program counter */
    uint64_t sp;            /* Stack pointer */
    uint64_t ra;            /* Return address */
    uint64_t gp;            /* Global pointer */
    uint64_t tp;            /* Thread pointer */
    uint64_t t0, t1, t2;    /* Temporary registers */
    uint64_t s0, s1;        /* Saved registers */
    uint64_t a0, a1, a2, a3, a4, a5, a6, a7; /* Argument/return registers */
    uint64_t s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    uint64_t t3, t4, t5, t6;
} cpu_context_t;

/* Task control block */
typedef struct task_struct {
    uint32_t pid;                   /* Process ID */
    uint32_t tid;                   /* Task ID */
    uint32_t ppid;                  /* Parent PID */
    task_state_t state;             /* Task state */
    cpu_context_t context;          /* CPU context */
    
    /* Memory */
    uint64_t code_start;
    uint64_t code_end;
    uint64_t data_start;
    uint64_t data_end;
    uint64_t heap_start;
    uint64_t heap_current;
    uint64_t heap_limit;
    uint64_t stack_start;
    uint64_t stack_pointer;
    vm_context_t *vm_ctx;
    
    /* File descriptors */
    int *fds;
    int fd_count;
    
    /* Scheduling */
    uint32_t priority;              /* Priority level */
    uint64_t time_slice;            /* Time slice for round-robin */
    uint64_t remaining_slice;       /* Remaining time in current slice */
    uint64_t total_time;            /* Total execution time */
    
    /* Linked list for scheduler queue */
    struct task_struct *next;
    struct task_struct *prev;
    
    /* Parent/child relationships */
    struct task_struct *parent;
    struct task_struct *children;
    
    char name[32];                  /* Task name */
} task_t;

/* Scheduler */
typedef struct {
    runqueue_t queues[SCHED_PRIORITY_REALTIME + 1];
    task_t *current_task;
    uint64_t ticks;
    uint32_t task_count;
    uint64_t tick_rate_hz;
    volatile int need_reschedule;
} scheduler_t;

/* Task operations */
int task_create(const char *name, uint64_t entry_point, sched_priority_t priority, task_t **task);
int task_destroy(uint32_t pid);
int task_fork(task_t **new_task);
int task_execve(uint32_t pid, const char *path, char *const argv[]);
int task_exit(int code);

/* Scheduling */
int scheduler_init(void);
void scheduler_run(void);
void scheduler_tick(void);
void task_switch(task_t *prev, task_t *next);
void context_switch(cpu_context_t *old, cpu_context_t *new);

/* Task management */
task_t *task_get_current(void);
task_t *task_get_by_pid(uint32_t pid);
int task_set_state(uint32_t pid, task_state_t state);
int task_sleep(uint64_t ms);
int task_wake(uint32_t pid);

/* Synchronization primitives */
typedef struct {
    volatile uint32_t locked;
    uint32_t owner_pid;
    uint32_t waiters;
} kmutex_t;

typedef struct {
    volatile int32_t count;
    uint32_t max_count;
    uint32_t waiters;
} ksemaphore_t;

int kmutex_init(kmutex_t *m);
int kmutex_lock(kmutex_t *m);
int kmutex_unlock(kmutex_t *m);
int ksem_init(ksemaphore_t *s, int value, int max);
int ksem_down(ksemaphore_t *s);
int ksem_up(ksemaphore_t *s);

#endif /* _TASK_H_ */