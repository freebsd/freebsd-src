/*-
 * Copyright (c) 2016 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/user.h>

#include <compat/cheriabi/cheriabi.h>
#include <compat/cheriabi/cheriabi_proto.h>

int
cheriabi___mac_get_proc(struct thread *td, struct cheriabi___mac_get_proc_args *uap)
{

	return(ENOSYS);
}

int
cheriabi___mac_set_proc(struct thread *td, struct cheriabi___mac_set_proc_args *uap)
{

	return(ENOSYS);
}

int
cheriabi___mac_get_fd(struct thread *td, struct cheriabi___mac_get_fd_args *uap)
{

	return(ENOSYS);
}

int
cheriabi___mac_get_file(struct thread *td, struct cheriabi___mac_get_file_args *uap)
{

	return(ENOSYS);
}

int
cheriabi___mac_set_fd(struct thread *td, struct cheriabi___mac_set_fd_args *uap)
{

	return(ENOSYS);
}

int
cheriabi___mac_set_file(struct thread *td, struct cheriabi___mac_set_file_args *uap)
{

	return(ENOSYS);
}

int
cheriabi___mac_get_pid(struct thread *td, struct cheriabi___mac_get_pid_args *uap)
{

	return(ENOSYS);
}

int
cheriabi___mac_get_link(struct thread *td, struct cheriabi___mac_get_link_args *uap)
{

	return(ENOSYS);
}

int
cheriabi___mac_set_link(struct thread *td, struct cheriabi___mac_set_link_args *uap)
{

	return(ENOSYS);
}

int
cheriabi___mac_execve(struct thread *td, struct cheriabi___mac_execve_args *uap)
{

	return(ENOSYS);
}

