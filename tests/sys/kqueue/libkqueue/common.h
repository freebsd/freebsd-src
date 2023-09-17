/*
 * Copyright (c) 2009 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _COMMON_H
#define _COMMON_H

#include "config.h" /* Needed for HAVE_* defines */

#if HAVE_ERR_H
# include <err.h>
#else
# define err(rc,msg,...) do { perror(msg); exit(rc); } while (0)
# define errx(rc,msg,...) do { puts(msg); exit(rc); } while (0)
#endif
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sys/event.h>

extern int vnode_fd;
extern int kqfd;

char * kevent_to_str(struct kevent *);
struct kevent * kevent_get(int);
struct kevent * kevent_get_timeout(int, int);

#define kevent_cmp(a,b) _kevent_cmp(a,b, __FILE__, __LINE__)
void _kevent_cmp(struct kevent *expected, struct kevent *got, const char *file, int line);

void
kevent_add(int kqfd, struct kevent *kev, 
        uintptr_t ident,
        short     filter,
        u_short   flags,
        u_int     fflags,
        intptr_t  data,
        void      *udata);

/* DEPRECATED: */
#define KEV_CMP(kev,_ident,_filter,_flags) do {                 \
    if (kev.ident != (_ident) ||                                \
            kev.filter != (_filter) ||                          \
            kev.flags != (_flags)) \
        err(1, "kevent mismatch: got [%d,%d,%d] but expecting [%d,%d,%d]", \
                (int)_ident, (int)_filter, (int)_flags,\
                (int)kev.ident, kev.filter, kev.flags);\
} while (0);

/* Checks if any events are pending, which is an error. */
#define test_no_kevents() _test_no_kevents(__FILE__, __LINE__)
void _test_no_kevents(const char *, int);
void test_no_kevents_quietly(void);

void test_begin(const char *);
void success(void);

void test_evfilt_read(void);
void test_evfilt_signal(void);
void test_evfilt_vnode(void);
void test_evfilt_timer(void);
void test_evfilt_proc(void);
#if HAVE_EVFILT_USER
void test_evfilt_user(void);
#endif

#endif  /* _COMMON_H */
