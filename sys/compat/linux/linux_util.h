/*-
 * Copyright (c) 1994 Christos Zoulas
 * Copyright (c) 1995 Frank van der Linden
 * Copyright (c) 1995 Scott Bartram
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: svr4_util.h,v 1.5 1994/11/18 02:54:31 christos Exp
 * from: linux_util.h,v 1.2 1995/03/05 23:23:50 fvdl Exp
 * $FreeBSD: src/sys/compat/linux/linux_util.h,v 1.29 2007/03/29 02:11:45 julian Exp $
 */

/*
 * This file is pretty much the same as Christos' svr4_util.h
 * (for now).
 */

#ifndef	_LINUX_UTIL_H_
#define	_LINUX_UTIL_H_

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <machine/vmparam.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/cdefs.h>
#include <sys/uio.h>

extern const char linux_emul_path[];

int linux_emul_convpath(struct thread *, char *, enum uio_seg, char **, int);

#define LCONVPATH_SEG(td, upath, pathp, i, seg)				\
	do {								\
		int _error;						\
									\
		_error = linux_emul_convpath(td, upath, seg,		\
		    pathp, i);						\
		if (*(pathp) == NULL)					\
			return (_error);				\
	} while (0)

#define LCONVPATH(td, upath, pathp, i) 	\
   LCONVPATH_SEG(td, upath, pathp, i, UIO_USERSPACE)

#define LCONVPATHEXIST(td, upath, pathp) LCONVPATH(td, upath, pathp, 0)
#define LCONVPATHCREAT(td, upath, pathp) LCONVPATH(td, upath, pathp, 1)
#define LFREEPATH(path)	free(path, M_TEMP)

#define DUMMY(s)							\
int									\
linux_ ## s(struct thread *td, struct linux_ ## s ## _args *args)	\
{									\
	static pid_t pid;						\
									\
	if (pid != td->td_proc->p_pid) {				\
		linux_msg(td, "syscall %s not implemented", #s);	\
		pid = td->td_proc->p_pid;				\
	};								\
	return (ENOSYS);						\
}									\
struct __hack

void linux_msg(const struct thread *td, const char *fmt, ...)
	__printflike(2, 3);

struct linux_device_handler {
	char	*bsd_driver_name;
	char	*linux_driver_name;
	char	*bsd_device_name;
	char	*linux_device_name;
	int	linux_major;
	int	linux_minor;
	int	linux_char_device;
};

int	linux_device_register_handler(struct linux_device_handler *h);
int	linux_device_unregister_handler(struct linux_device_handler *h);
char	*linux_driver_get_name_dev(device_t dev);
int	linux_driver_get_major_minor(char *node, int *major, int *minor);
char	*linux_get_char_devices(void);
void	linux_free_get_char_devices(char *string);

#endif /* !_LINUX_UTIL_H_ */
