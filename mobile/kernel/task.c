/*
 * Task and Process Management Implementation
 * uOS(m) - User OS Mobile
 */

#include "task.h"
#include "memory.h"
#include "vm.h"

/* Scheduler helpers */
static task_t *task_find_next_ready(task_t *after);
static void idle_task(void);
static void init_task(void);

/* Task management */
#define MAX_TASKS 256
static task_t task_table[MAX_TASKS];
static uint32_t next_pid = 1;
static task_t *current_task = NULL;
static cpu_context_t scheduler_context;

extern void uart_puts(const char *s);
extern void uart_putc(char c);
extern int vm_init(void);
extern vm_context_t *vm_create_context(uint32_t pid);
extern void vm_destroy_context(vm_context_t *ctx);

/* Initialize task management */
int task_init(void) {
    uart_puts("Task management initializing...\n");
    
    for (int i = 0; i < MAX_TASKS; i++) {
        task_table[i].pid = 0;
        task_table[i].state = TASK_CREATED;
    }
    
    uart_puts("Task management ready\n");
    return 0;
}

/* Create a new task/process */
int task_create(const char *name, uint64_t entry_point, uint32_t priority, task_t **task) {
    if (!task || !name) return -1;
    
    /* Find free task slot */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].pid == 0) {
            task_t *t = &task_table[i];
            
            /* Initialize task */
            t->pid = next_pid++;
            t->tid = t->pid;
            t->ppid = 0;
            t->state = TASK_READY;
            t->priority = priority;
            t->time_slice = 50;  /* 50ms time slice */
            
            /* Copy name */
            for (int j = 0; j < 31 && name[j]; j++) {
                t->name[j] = name[j];
            }
            t->name[31] = '\0';
            
            /* Set up memory */
            t->code_start = 0x1000;
            t->code_end = entry_point;
            t->heap_start = 0x10000;
            t->heap_current = 0x10000;
            t->heap_limit = 0x11000;
            t->stack_start = 0x80000;
            t->stack_pointer = 0x80000;
            t->vm_ctx = vm_create_context(t->pid);
            if (!t->vm_ctx) {
                task_table[i].pid = 0;
                task_table[i].state = TASK_CREATED;
                return -1;
            }

            vm_alloc_and_map_page(t->vm_ctx, t->heap_start, PTE_R | PTE_W | PTE_U);
            vm_alloc_and_map_page(t->vm_ctx, t->stack_start - PAGE_SIZE, PTE_R | PTE_W | PTE_U);
            
            /* Set up context */
            t->context.pc = entry_point;
            t->context.sp = 0x80000;
            
            /* FDs */
            t->fds = NULL;
            t->fd_count = 0;
            
            /* Relationships */
            t->parent = NULL;
            t->children = NULL;
            t->next = NULL;
            t->prev = NULL;
            
            *task = t;
            return t->pid;
        }
    }
    
    return -1;  /* No free tasks */
}

/* Destroy a task */
int task_destroy(uint32_t pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].pid == pid) {
            if (task_table[i].vm_ctx) {
                vm_destroy_context((vm_context_t *)task_table[i].vm_ctx);
                task_table[i].vm_ctx = NULL;
            }
            task_table[i].pid = 0;
            task_table[i].state = TASK_CREATED;
            return 0;
        }
    }
    return -1;
}

/* Fork a task */
int task_fork(task_t **new_task) {
    if (!new_task) return -1;
    
    task_t *parent = task_get_current();
    if (!parent) return -1;
    
    task_t *child;
    if (task_create("forked_task", parent->context.pc, parent->priority, &child) < 0) {
        return -1;
    }
    
    /* Copy parent state */
    child->ppid = parent->pid;
    child->context = parent->context;
    child->stack_pointer = parent->stack_pointer;
    child->state = TASK_READY;
    *new_task = child;
    return 0;
}

/* Execute a new program */
int task_execve(uint32_t pid, const char *path, char *const argv[]) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].pid == pid) {
            /* In a real implementation, would load executable */
            uart_puts("Executing: ");
            uart_puts(path);
            uart_puts("\n");
            return 0;
        }
    }
    return -1;
}

/* Exit the current task */
int task_exit(int code) {
    task_t *t = task_get_current();
    if (t) {
        t->state = TASK_ZOMBIE;
        char msg[] = "Task exited: ";
        uart_puts(msg);
        uart_putc('0' + (code % 10));
        uart_puts("\n");
        return 0;
    }
    return -1;
}

/* Get current task */
task_t *task_get_current(void) {
    return current_task;
}

/* Set current task */
void task_set_current(task_t *t) {
    current_task = t;
}

/* Get task by PID */
task_t *task_get_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].pid == pid) {
            return &task_table[i];
        }
    }
    return NULL;
}

/* Set task state */
int task_set_state(uint32_t pid, task_state_t state) {
    task_t *t = task_get_by_pid(pid);
    if (t) {
        t->state = state;
        return 0;
    }
    return -1;
}

/* Sleep a task */
int task_sleep(uint64_t ms) {
    task_t *t = task_get_current();
    if (t) {
        t->state = TASK_BLOCKED;
        return 0;
    }
    return -1;
}

/* Wake a task */
int task_wake(uint32_t pid) {
    task_t *t = task_get_by_pid(pid);
    if (t && t->state == TASK_BLOCKED) {
        t->state = TASK_READY;
        return 0;
    }
    return -1;
}

/* Helper: find the next READY task after the given task */
static task_t *task_find_next_ready(task_t *after) {
    int start = 0;
    if (after) {
        start = after - task_table + 1;
        if (start < 0 || start >= MAX_TASKS) start = 0;
    }

    for (int i = start; i < MAX_TASKS; i++) {
        if (task_table[i].pid != 0 && task_table[i].state == TASK_READY) {
            return &task_table[i];
        }
    }
    for (int i = 0; i < start; i++) {
        if (task_table[i].pid != 0 && task_table[i].state == TASK_READY) {
            return &task_table[i];
        }
    }
    return NULL;
}

static void idle_task(void) {
    while (1) {
        asm volatile("wfi");
    }
}

static void init_task(void) {
    uart_puts("[init] starting process manager\n");
    for (int i = 0; i < 3; i++) {
        uart_puts("[init] heartbeat \n");
    }
    task_exit(0);
}

/* Initialize scheduler */
int scheduler_init(void) {
    uart_puts("Scheduler initializing...\n");
    
    task_t *init;
    task_t *idle;
    task_create("init", (uint64_t)init_task, 1, &init);
    task_create("idle", (uint64_t)idle_task, 0, &idle);
    current_task = NULL;
    init->state = TASK_READY;
    idle->state = TASK_READY;
    
    uart_puts("Scheduler ready\n");
    return 0;
}

/* Scheduler main loop */
void scheduler_run(void) {
    uart_puts("Scheduler running...\n");

    while (1) {
        task_t *next = task_find_next_ready(current_task);
        if (!next) {
            asm volatile("wfi");
            continue;
        }

        task_t *prev = current_task;
        if (next == prev && next->state != TASK_READY) {
            asm volatile("wfi");
            continue;
        }

        if (prev && prev->state == TASK_RUNNING) {
            prev->state = TASK_READY;
        }
        next->state = TASK_RUNNING;
        task_set_current(next);
        task_switch(prev, next);
    }
}

/* Task switching */
void task_switch(task_t *prev, task_t *next) {
    if (!next) return;

    cpu_context_t *old_ctx = (prev) ? &prev->context : &scheduler_context;

    if (next->vm_ctx) {
        vm_activate(next->vm_ctx);
    }

    if (prev == next) {
        return;
    }

    /* Save previous context and load next context */
    context_switch(old_ctx, &next->context);
}

/* Context switch (assembly would be called here) */
