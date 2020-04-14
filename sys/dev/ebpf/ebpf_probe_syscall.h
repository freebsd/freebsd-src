/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ryan Stone
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _EBPF_PROBE_SYSCALL_H
#define _EBPF_PROBE_SYSCALL_H

struct ebpf_symlink_res_bufs;

void *ebpf_map_path_lookup(struct ebpf_map *map, void **key);
int ebpf_map_enqueue(struct ebpf_map *, void *);
int ebpf_map_dequeue(struct ebpf_map *, void *);

int ebpf_probe_copyinstr(const void *uaddr, void *kaddr, size_t len,
    size_t *done);
int ebpf_probe_copyout(const void *kaddr, void *uaddr, size_t len);
int ebpf_probe_dup(int fd);
int ebpf_probe_openat(int fd, const char * path, int flags, int mode);
int ebpf_probe_fstat(int fd, struct stat *sb);
int ebpf_probe_fstatat(int fd, const char *path, struct stat *sb, int flag);
int ebpf_probe_faccessat(int fd, const char *path, int mode, int flag);
int ebpf_probe_set_errno(int error);
int ebpf_probe_set_syscall_retval(int ret0, int ret1);
pid_t ebpf_probe_pdfork(int *fd, int flags);
int ebpf_probe_pdwait4_nohang(int fd, int* status, int options,
   struct rusage *ru);
int ebpf_probe_pdwait4_defer(int fd, int options, void *arg, void *next);
int ebpf_probe_fexecve(int fd, char ** argv, char ** envp, char ** argv_pre);
void *ebpf_probe_memset(void *, int, size_t);
int ebpf_probe_readlinkat(int fd, const char *path, char *buf, size_t bufsize);
int ebpf_probe_exec_get_interp(int fd, char *buf, size_t bufsize, int *type);
int ebpf_probe_strncmp(const char *a, const char *b, size_t len);
int ebpf_probe_canonical_path(char *base, const char * rela, size_t bufsize);
int ebpf_probe_renameat(int fromfd, const char *from, int tofd, const char *to);
int ebpf_probe_mkdirat(int fd, const char *path, mode_t mode);
int ebpf_probe_fchdir(int fd);
pid_t ebpf_probe_getpid(void);
int ebpf_probe_get_errno(void);
int ebpf_probe_copyin(const void *, void *, size_t);
int ebpf_probe_ktrnamei(char *);
int ebpf_probe_symlink_path(char *base, const char * rela, size_t bufsize);
size_t ebpf_probe_strlcpy(char *dest, const char * src, size_t bufsize);
int ebpf_probe_kqueue(int);
int ebpf_probe_kevent_install(int, struct kevent *, int);
int ebpf_probe_kevent_poll(int, struct kevent *, int);
int ebpf_probe_kevent_block(int, const struct timespec *, void *);
int ebpf_probe_close(int);
int ebpf_probe_get_syscall_retval(void);
int ebpf_probe_symlinkat(const char *, int, const char *);
int ebpf_probe_resolve_one_symlink(struct ebpf_symlink_res_bufs *, int,
    char *, int);
int ebpf_probe_utimensat(int, const char *, struct timespec *, int);
int ebpf_probe_fcntl(int, int, int);
int ebpf_probe_unlinkat(int, const char *, int);
int ebpf_probe_fchown(int, uid_t, gid_t);
int ebpf_probe_fchownat(int, const char *, uid_t, gid_t, int);
int ebpf_probe_fchmod(int, mode_t);
int ebpf_probe_fchmodat(int, const char *, mode_t, int);
int ebpf_probe_futimens(int, struct timespec *);
int ebpf_probe_linkat(int, const char *, int, const char *, int);

#endif
