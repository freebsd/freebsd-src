/*
 * Task and Process Management Implementation
 * uOS(m) - User OS Mobile
 */

#include "task.h"
#include "memory.h"
#include "vm.h"
#include "config.h"

/* Scheduler helpers */
static void idle_task(void);
static void init_task(void);
static task_t *scheduler_pick_next(void);
static void scheduler_enqueue(task_t *task);
static void scheduler_dequeue(task_t *task);
static void scheduler_requeue(task_t *task);

/* Task management */
#define MAX_TASKS 256
static task_t task_table[MAX_TASKS];
static uint32_t next_pid = 1;
static scheduler_t scheduler;
static cpu_context_t scheduler_context;

extern void uart_puts(const char *s);
extern void uart_putc(char c);
extern int vm_init(void);
extern vm_context_t *vm_create_context(uint32_t pid);
extern void vm_destroy_context(vm_context_t *ctx);
extern int gpu_init(void);
extern void *gpu_framebuffer(void);
extern uint32_t gpu_width(void);
extern uint32_t gpu_height(void);
extern void gpu_flush_rect(int x, int y, int w, int h);
extern int mobile_ui_init(void);
extern int mobile_ui_start(void);
extern void mobile_ui_event_loop(void);

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
int task_create(const char *name, uint64_t entry_point, sched_priority_t priority, task_t **task) {
    if (!task || !name) return -1;

    /* Clamp priority */
    if (priority > SCHED_PRIORITY_REALTIME) priority = SCHED_PRIORITY_NORMAL;

    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].pid == 0) {
            task_t *t = &task_table[i];

            t->pid = next_pid++;
            t->tid = t->pid;
            t->ppid = 0;
            t->state = TASK_READY;
            t->priority = priority;
            t->time_slice = (priority == SCHED_PRIORITY_REALTIME) ? 200 : 50;
            t->remaining_slice = t->time_slice;

            for (int j = 0; j < 31 && name[j]; j++) {
                t->name[j] = name[j];
            }
            t->name[31] = '\0';

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

            t->context.pc = entry_point;
            t->context.sp = 0x80000;

            t->fds = NULL;
            t->fd_count = 0;

            t->parent = NULL;
            t->children = NULL;
            t->next = NULL;
            t->prev = NULL;

            *task = t;
            return t->pid;
        }
    }

    return -1;
}

/* Destroy a task */
int task_destroy(uint32_t pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].pid == pid) {
            scheduler_dequeue(&task_table[i]);
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

    child->ppid = parent->pid;
    child->context = parent->context;
    child->stack_pointer = parent->stack_pointer;
    child->state = TASK_READY;
    scheduler_enqueue(child);
    *new_task = child;
    return 0;
}

/* Execute a new program */
int task_execve(uint32_t pid, const char *path, char *const argv[]) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].pid == pid) {
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
        scheduler_dequeue(t);
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
    return scheduler.current_task;
}

/* Set current task */
void task_set_current(task_t *t) {
    scheduler.current_task = t;
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
        scheduler_dequeue(t);
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
        scheduler_enqueue(t);
        return 0;
    }
    return -1;
}

static void scheduler_enqueue(task_t *task) {
    if (!task || task->state != TASK_READY) return;

    sched_priority_t prio = task->priority;
    runqueue_t *rq = &scheduler.queues[prio];

    task->next = NULL;
    task->prev = NULL;
    if (rq->head == NULL) {
        rq->head = task;
        rq->tail = task;
    } else {
        rq->tail->next = task;
        task->prev = rq->tail;
        rq->tail = task;
    }
    rq->mask |= (1U << prio);
    scheduler.task_count++;
}

static void scheduler_dequeue(task_t *task) {
    if (!task) return;

    sched_priority_t prio = task->priority;
    runqueue_t *rq = &scheduler.queues[prio];

    if (task->prev) {
        task->prev->next = task->next;
    } else {
        rq->head = task->next;
    }
    if (task->next) {
        task->next->prev = task->prev;
    } else {
        rq->tail = task->prev;
    }

    task->next = NULL;
    task->prev = NULL;

    if (rq->head == NULL) {
        rq->mask &= ~(1U << prio);
    }
    if (scheduler.task_count > 0) {
        scheduler.task_count--;
    }
}

static task_t *scheduler_pick_next(void) {
    task_t *best = NULL;
    for (int p = SCHED_PRIORITY_REALTIME; p >= 0; p--) {
        runqueue_t *rq = &scheduler.queues[p];
        if (rq->head) {
            best = rq->head;
            break;
        }
    }
    if (!best) return NULL;

    scheduler_dequeue(best);
    if (best->remaining_slice == 0) {
        best->remaining_slice = best->time_slice;
    }
    best->state = TASK_RUNNING;
    scheduler.current_task = best;
    return best;
}

static void scheduler_requeue(task_t *task) {
    if (!task || task->state == TASK_BLOCKED || task->state == TASK_ZOMBIE) return;
    task->state = TASK_READY;
    scheduler_enqueue(task);
}

static void idle_task(void) {
    while (1) {
        asm volatile("wfi");
    }
}

static void init_task(void) {
    uart_puts("[init] launching desktop compositor\n");

    if (mobile_ui_init() == 0) {
        uart_puts("[init] UI init OK\n");
        mobile_ui_start();
        uart_puts("[init] desktop running\n");

        for (;;) {
            mobile_ui_event_loop();
        }
    } else {
        uart_puts("[init] UI init FAILED\n");
    }

    for (;;) {
        asm volatile("wfi");
    }
}

/* Initialize scheduler */
int scheduler_init(void) {
    uart_puts("Scheduler initializing...\n");

    for (int p = 0; p <= SCHED_PRIORITY_REALTIME; p++) {
        scheduler.queues[p].head = NULL;
        scheduler.queues[p].tail = NULL;
        scheduler.queues[p].mask = 0;
    }
    scheduler.current_task = NULL;
    scheduler.ticks = 0;
    scheduler.task_count = 0;
    scheduler.tick_rate_hz = 1000 / TIMER_TICK_MS;

    task_t *init;
    task_t *idle;
    task_create("init", (uint64_t)init_task, SCHED_PRIORITY_NORMAL, &init);
    task_create("idle", (uint64_t)idle_task, SCHED_PRIORITY_LOW, &idle);
    init->state = TASK_READY;
    idle->state = TASK_READY;

    uart_puts("Scheduler ready\n");
    return 0;
}

void scheduler_tick(void) {
    scheduler.ticks++;
    scheduler.need_reschedule = 1;
}

/* Scheduler main loop */
void scheduler_run(void) {
    uart_puts("Scheduler running...\n");

    while (1) {
        task_t *next = scheduler_pick_next();
        if (!next) {
            asm volatile("wfi");
            continue;
        }

        task_t *prev = scheduler.current_task;
        if (prev) {
            int preempt = scheduler.need_reschedule;
            scheduler.need_reschedule = 0;

            if (prev->state == TASK_RUNNING) {
                if (preempt && prev != next) {
                    prev->remaining_slice = prev->time_slice;
                    scheduler_requeue(prev);
                } else {
                    prev->remaining_slice -= TIMER_TICK_MS;
                    if (prev->remaining_slice <= 0) {
                        prev->remaining_slice = prev->time_slice;
                        scheduler_requeue(prev);
                    } else {
                        prev->state = TASK_READY;
                        scheduler_enqueue(prev);
                    }
                }
            }
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

    context_switch(old_ctx, &next->context);
}

/* Context switch (assembly would be called here) */

/* Synchronization primitives */
int kmutex_init(kmutex_t *m) {
    if (!m) return -1;
    m->locked = 0;
    m->owner_pid = 0;
    m->waiters = 0;
    return 0;
}

int kmutex_lock(kmutex_t *m) {
    if (!m) return -1;
    task_t *self = task_get_current();
    uint32_t my_pid = self ? self->pid : 0;

    for (;;) {
        uint32_t expected = 0;
        uint32_t desired = 1;
        if (__atomic_compare_exchange_n(&m->locked, &expected, desired, 0,
                                        __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
            m->owner_pid = my_pid;
            return 0;
        }
        /* Spin/park */
        asm volatile("wfi");
    }
}

int kmutex_unlock(kmutex_t *m) {
    if (!m) return -1;
    if (m->locked && m->owner_pid == (task_get_current() ? task_get_current()->pid : 0)) {
        m->locked = 0;
        m->owner_pid = 0;
        return 0;
    }
    return -1;
}

int ksem_init(ksemaphore_t *s, int value, int max) {
    if (!s || max <= 0 || value < 0 || value > max) return -1;
    s->count = value;
    s->max_count = (uint32_t)max;
    s->waiters = 0;
    return 0;
}

static void ksem_slow_down(ksemaphore_t *s) {
    while (__atomic_sub_fetch(&s->count, 1, __ATOMIC_ACQ_REL) < 0) {
        __atomic_add_fetch(&s->waiters, 1, __ATOMIC_RELAXED);
        task_t *self = task_get_current();
        if (self) {
            self->state = TASK_BLOCKED;
            scheduler_dequeue(self);
        }
        asm volatile("wfi");
        if (self) {
            self->state = TASK_READY;
            scheduler_enqueue(self);
        }
    }
}

static void ksem_slow_up(ksemaphore_t *s) {
    int old = __atomic_add_fetch(&s->count, 1, __ATOMIC_ACQ_REL);
    if (old < 0 && s->waiters > 0) {
        /* Wake one blocked waiter via scheduler */
        for (int i = 0; i < MAX_TASKS && s->waiters > 0; i++) {
            if (task_table[i].pid && task_table[i].state == TASK_BLOCKED) {
                task_wake(task_table[i].pid);
                break;
            }
        }
    }
}

int ksem_down(ksemaphore_t *s) {
    if (!s) return -1;
    ksem_slow_down(s);
    return 0;
}

int ksem_up(ksemaphore_t *s) {
    if (!s) return -1;
    ksem_slow_up(s);
    return 0;
}
