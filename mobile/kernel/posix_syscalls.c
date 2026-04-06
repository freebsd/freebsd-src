/*
 * POSIX Syscall Implementation
 * uOS(m) - User OS Mobile
 */

#include "posix_syscalls.h"
#include "task.h"
#include "memory.h"
#include "ipc.h"

/* Global file descriptor table */
#define MAX_FDS 256
fd_t fd_table[MAX_FDS];

/* Display functions from main kernel */
extern void uart_puts(const char *s);
extern void uart_putc(char c);

/* Simple implementations */

int64_t sys_exit(int code) {
    task_t *current = task_get_current();
    if (current) {
        char msg[] = "Process exiting with code: ";
        uart_puts(msg);
        uart_putc('0' + code);
        uart_puts("\n");
        task_exit(code);
    }
    return 0;
}

int64_t sys_write(int fd, const char *buf, size_t count) {
    if (fd == 1 || fd == 2) {  /* stdout or stderr */
        for (size_t i = 0; i < count; i++) {
            uart_putc(buf[i]);
        }
        return count;
    }
    return -1;
}

int64_t sys_read(int fd, char *buf, size_t count) {
    /* Stub: would read from actual file descriptor */
    return 0;
}

int64_t sys_open(const char *path, int flags) {
    /* Stub: would open file and allocate descriptor */
    return -1;
}

int64_t sys_close(int fd) {
    /* Stub: would close file descriptor */
    return 0;
}

int64_t sys_fork(void) {
    task_t *new_task = NULL;
    if (task_fork(&new_task) == 0) {
        task_t *current = task_get_current();
        return new_task->pid;  /* Parent returns child PID */
    }
    return -1;
}

int64_t sys_execve(const char *path, char *const argv[]) {
    task_t *current = task_get_current();
    if (current) {
        return task_execve(current->pid, path, argv);
    }
    return -1;
}

int64_t sys_getpid(void) {
    task_t *current = task_get_current();
    if (current) {
        return current->pid;
    }
    return -1;
}

int64_t sys_getuid(void) {
    /* Stub: would return real UID from task struct */
    return 0;  /* root */
}

int64_t sys_kill(uint32_t pid, int sig) {
    /* Stub: would send signal to process */
    return 0;
}

int64_t sys_mmap(void *addr, size_t len, int prot, int flags) {
    /* Stub: would map memory region */
    return (int64_t)mem_alloc(len);
}

int64_t sys_munmap(void *addr, size_t len) {
    /* Stub: would unmap memory region */
    mem_free(addr);
    return 0;
}

/* Syscall dispatch table */
static syscall_handler_t syscall_table[] = {
    NULL,               /* 0 */
    (syscall_handler_t)sys_exit,       /* 1 */
    (syscall_handler_t)sys_fork,       /* 2 */
    (syscall_handler_t)sys_read,       /* 3 */
    (syscall_handler_t)sys_write,      /* 4 */
    (syscall_handler_t)sys_open,       /* 5 */
    (syscall_handler_t)sys_close,      /* 6 */
};

#define NUM_SYSCALLS (sizeof(syscall_table) / sizeof(syscall_handler_t))

int64_t syscall_dispatch(uint32_t syscall_num, uint64_t arg1, uint64_t arg2, 
                        uint64_t arg3, uint64_t arg4) {
    if (syscall_num >= NUM_SYSCALLS || syscall_table[syscall_num] == NULL) {
        uart_puts("Invalid syscall: ");
        uart_putc('0' + syscall_num);
        uart_puts("\n");
        return -1;
    }
    
    return syscall_table[syscall_num](arg1, arg2, arg3, arg4);
}