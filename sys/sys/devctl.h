/*-
 * Copyright 2020 M. Warner Losh <imp@FreeBSD.org>
 *
 * SPDX-License-Idnetifier: BSD-2-Clause
 *
 * $FreeBSD$
 *
 */

#ifndef _SYS_DEVCTL_H_
#define _SYS_DEVCTL_H_

#ifdef _KERNEL
/**
 * devctl hooks.  Typically one should use the devctl_notify
 * hook to send the message.
 */
bool devctl_process_running(void);
void devctl_notify(const char *__system, const char *__subsystem,
    const char *__type, const char *__data);
struct sbuf;
void devctl_safe_quote_sb(struct sbuf *__sb, const char *__src);
#endif

#endif /* _SYS_DEVCTL_H_ */
