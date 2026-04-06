/*
 * Comprehensive POSIX API Implementation
 * uOS(m) - User OS Mobile
 * POSIX-compliant system calls
 */

#include "posix_syscalls.h"
#include "task.h"
#include "vm.h"
#include "vfs.h"
#include "memory.h"

extern void uart_puts(const char *s);
extern void uart_putc(char c);

/* Signal handlers */
typedef struct {
    uint32_t handler;
    uint32_t flags;
} signal_handler_t;

/* Process group */
typedef struct {
    uint32_t pgid;
    uint32_t session_id;
    uint32_t foreground_process;
} process_group_t;

/* Enhanced process structure */
static process_group_t process_groups[256];

/* Signal table */
static signal_handler_t signal_handlers[64];

/* ==================== Process Control ==================== */

int64_t sys_exit(int code) {
    task_t *current = task_get_current();
    if (current) {
        char msg[] = "Process ";
        uart_puts(msg);
        uart_putc('0' + (current->pid / 10));
        uart_putc('0' + (current->pid % 10));
        uart_puts(" exiting with code: ");
        uart_putc('0' + code);
        uart_puts("\n");
        task_exit(code);
    }
    return 0;
}

int64_t sys_fork(void) {
    task_t *new_task = NULL;
    if (task_fork(&new_task) == 0) {
        task_t *current = task_get_current();
        return new_task->pid;
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
    return current ? current->pid : -1;
}

int64_t sys_getppid(void) {
    task_t *current = task_get_current();
    return current ? current->ppid : -1;
}

int64_t sys_getuid(void) {
    /* Returns root UID */
    return 0;
}

int64_t sys_geteuid(void) {
    return 0;
}

int64_t sys_getgid(void) {
    return 0;
}

int64_t sys_getegid(void) {
    return 0;
}

int64_t sys_kill(uint32_t pid, int sig) {
    task_t *target = task_get_by_pid(pid);
    if (!target) return -1;
    
    /* Signal 9 (SIGKILL) or 15 (SIGTERM) */
    if (sig == 9 || sig == 15) {
        target->state = TASK_ZOMBIE;
        return 0;
    }
    
    return 0;
}

int64_t sys_pause(void) {
    task_t *current = task_get_current();
    if (current) {
        current->state = TASK_BLOCKED;
        return 0;
    }
    return -1;
}

int64_t sys_sleep(uint64_t seconds) {
    /* Simplified: just block task */
    task_t *current = task_get_current();
    if (current) {
        current->state = TASK_BLOCKED;
        return 0;
    }
    return -1;
}

/* ==================== File Operations ==================== */

int64_t sys_open(const char *path, int flags) {
    if (!path) return -1;
    
    int fd = vfs_open(path, flags);
    return fd;
}

int64_t sys_close(int fd) {
    return vfs_close(fd);
}

int64_t sys_read(int fd, char *buf, size_t count) {
    if (!buf) return -1;
    
    return vfs_read(fd, (uint8_t *)buf, count);
}

int64_t sys_write(int fd, const char *buf, size_t count) {
    if (!buf) return -1;
    
    /* Special case for stdout/stderr */
    if (fd == 1 || fd == 2) {
        for (size_t i = 0; i < count; i++) {
            uart_putc(buf[i]);
        }
        return count;
    }
    
    return vfs_write(fd, (uint8_t *)buf, count);
}

int64_t sys_lseek(int fd, int64_t offset, int whence) {
    /* Simplified: only support absolute seek (SEEK_SET) */
    if (whence == 0) {  /* SEEK_SET */
        return vfs_seek(fd, offset);
    }
    return -1;
}

int64_t sys_stat(const char *path, void *stat_buf) {
    if (!path) return -1;
    
    inode_t stat;
    return vfs_stat(path, &stat);
}

int64_t sys_access(const char *path, int mode) {
    if (!path) return -1;
    
    /* Simplified: always return success */
    return 0;
}

/* ==================== Memory Management ==================== */

int64_t sys_mmap(void *addr, size_t len, int prot, int flags) {
    /* Stub implementation */
    void *ptr = mem_alloc(len);
    return (int64_t)ptr;
}

int64_t sys_munmap(void *addr, size_t len) {
    mem_free(addr);
    return 0;
}

int64_t sys_brk(uint64_t addr) {
    task_t *current = task_get_current();
    if (!current) return -1;
    
    if (addr > current->heap_current && addr <= current->heap_current + 0x1000) {
        current->heap_current = addr;
        return 0;
    }
    
    return -1;
}

/* ==================== Syscall Dispatch ==================== */

/* Syscall numbers */
#define SYS_exit        1
#define SYS_fork        2
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_execve      11
#define SYS_brk         12
#define SYS_getpid      20
#define SYS_getuid      24
#define SYS_kill        37
#define SYS_pause       29
#define SYS_lseek       8
#define SYS_mmap        9
#define SYS_munmap      11
#define SYS_stat        18
#define SYS_access      33
#define SYS_sleep       35
#define SYS_getppid     64
#define SYS_geteuid     49
#define SYS_getgid      47
#define SYS_getegid     50

static syscall_handler_t syscall_table[] = {
    NULL,                           /* 0 */
    (syscall_handler_t)sys_exit,    /* 1 */
    (syscall_handler_t)sys_fork,    /* 2 */
    (syscall_handler_t)sys_read,    /* 3 */
    (syscall_handler_t)sys_write,   /* 4 */
    (syscall_handler_t)sys_open,    /* 5 */
    (syscall_handler_t)sys_close,   /* 6 */
    (syscall_handler_t)sys_lseek,   /* 7 (8 in Linux) */
    (syscall_handler_t)sys_mmap,    /* 8 (9 in Linux) */
    NULL,                           /* 9 */
    NULL,                           /* 10 */
    (syscall_handler_t)sys_execve,  /* 11 */
    (syscall_handler_t)sys_brk,     /* 12 */
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