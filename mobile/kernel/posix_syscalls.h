/*
 * POSIX Syscall Interface
 * uOS(m) - User OS Mobile
 * Hybrid kernel POSIX layer
 */

#ifndef _POSIX_SYSCALLS_H_
#define _POSIX_SYSCALLS_H_

#include <stdint.h>
#include <stddef.h>

/* Syscall numbers */
#define SYS_exit        1
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_read        3
#define SYS_fork        2
#define SYS_execve      11
#define SYS_mmap        9
#define SYS_munmap      11
#define SYS_brk         12
#define SYS_getpid      20
#define SYS_getuid      24
#define SYS_kill        37
#define SYS_pause       29
#define SYS_access      33

/* File descriptor flags */
#define O_RDONLY   0x0000
#define O_WRONLY   0x0001
#define O_RDWR     0x0002
#define O_CREAT    0x0040
#define O_APPEND   0x0400

/* Process structure */
typedef struct {
    uint32_t pid;           /* Process ID */
    uint32_t ppid;          /* Parent PID */
    uint32_t uid;           /* User ID */
    uint32_t gid;           /* Group ID */
    char *name;             /* Process name */
    void *stack;            /* Stack pointer */
    void *heap;             /* Heap pointer */
    uint64_t entry;         /* Entry point */
} process_t;

/* File descriptor */
typedef struct {
    int flags;
    uint64_t offset;
    void *inode;
} fd_t;

/* Syscall handler prototype */
typedef int64_t (*syscall_handler_t)(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);

/* Syscall implementations */
int64_t sys_exit(int code);
int64_t sys_write(int fd, const char *buf, size_t count);
int64_t sys_read(int fd, char *buf, size_t count);
int64_t sys_open(const char *path, int flags);
int64_t sys_close(int fd);
int64_t sys_fork(void);
int64_t sys_execve(const char *path, char *const argv[]);
int64_t sys_getpid(void);
int64_t sys_getuid(void);
int64_t sys_kill(uint32_t pid, int sig);
int64_t sys_mmap(void *addr, size_t len, int prot, int flags);
int64_t sys_munmap(void *addr, size_t len);

/* Syscall dispatch */
int64_t syscall_dispatch(uint32_t syscall_num, uint64_t arg1, uint64_t arg2, 
                        uint64_t arg3, uint64_t arg4);

#endif /* _POSIX_SYSCALLS_H_ */