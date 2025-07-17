/*-
 * Copyright (c) 2014 John Baldwin <jhb@FreeBSD.org>
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

#ifndef __DEVCTL_H__
#define	__DEVCTL_H__

#include <stdbool.h>

__BEGIN_DECLS
int	devctl_attach(const char *device);
int	devctl_detach(const char *device, bool force);
int	devctl_enable(const char *device);
int	devctl_disable(const char *device, bool force_detach);
int	devctl_suspend(const char *device);
int	devctl_resume(const char *device);
int	devctl_set_driver(const char *device, const char *driver, bool force);
int	devctl_clear_driver(const char *device, bool force);
int	devctl_rescan(const char *device);
int	devctl_delete(const char *device, bool force);
int	devctl_freeze(void);
int	devctl_thaw(void);
int	devctl_reset(const char *device, bool detach);
int	devctl_getpath(const char *device, const char *locator, char **buffer);
__END_DECLS

#endif /* !__DEVCTL_H__ */
