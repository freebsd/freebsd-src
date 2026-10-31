/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#ifndef _LINUX_TEST_SYSCALLS_H_
#define	_LINUX_TEST_SYSCALLS_H_

#include <compat/linux/linux_errno.h>
#include <compat/linux/linux_event.h>

#define	LINUX_O_RDWR	2
#define	LINUX_AT_FDCWD	-100
#define	LINUX_SIGCHLD	17
#define	LINUX_F_GETFD	1
#define	LINUX_O_CLOEXEC	02000000

typedef int linux_pid_t;
typedef __SIZE_TYPE__ linux_size_t;
typedef __PTRDIFF_TYPE__ linux_ssize_t;

/* The test helpers are single-threaded, so errno does not require TLS. */
extern int errno;

struct linux_epoll_event {
	unsigned int events;
#if defined(__aarch64__)
	unsigned int padding;
#endif
	unsigned long data;
}
#if defined(__amd64__)
__attribute__((packed))
#endif
;

#if defined(__amd64__)
_Static_assert(sizeof(struct linux_epoll_event) == 12,
    "Linux x86-64 epoll ABI");
#elif defined(__aarch64__)
_Static_assert(sizeof(struct linux_epoll_event) == 16,
    "Linux AArch64 epoll ABI");
#endif

int bind(int fd, const void *address, unsigned int address_length);
int close(int fd);
int close_range(unsigned int first, unsigned int last,
    unsigned int flags);
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd,
    struct linux_epoll_event *event);
int epoll_pwait(int epfd, struct linux_epoll_event *events,
    int maxevents, int timeout, const void *sigmask, linux_size_t sigsetsize);
int fcntl(int fd, int command, long argument);
int getsockname(int fd, void *address, unsigned int *address_length);
#if defined(__amd64__)
linux_pid_t clone(unsigned long flags, void *stack, int *parent_tid,
    int *child_tid, unsigned long tls);
#elif defined(__aarch64__)
linux_pid_t clone(unsigned long flags, void *stack, int *parent_tid,
    unsigned long tls, int *child_tid);
#endif
int openat(int dirfd, const char *path, int flags, int mode);
int pipe2(int pipefd[2], int flags);
int ppoll(void *fds, unsigned int count, void *timeout, const void *sigmask,
    linux_size_t sigsetsize);
int recvmmsg(int fd, void *messages, unsigned int count, int flags,
    void *timeout);
linux_ssize_t recvmsg(int fd, void *message, int flags);
linux_ssize_t sendmsg(int fd, const void *message, int flags);
linux_ssize_t sendto(int fd, const void *buffer, linux_size_t length,
    int flags, const void *address, unsigned int address_length);
int setsockopt(int fd, int level, int name, const void *value,
    unsigned int value_length);
int socket(int domain, int type, int protocol);
linux_pid_t wait4(linux_pid_t pid, int *status, int options,
    void *rusage);
linux_ssize_t write(int fd, const void *buffer, linux_size_t length);
_Noreturn void exit_group(int status);

#endif /* _LINUX_TEST_SYSCALLS_H_ */
