/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011-2023, Juniper Networks, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __LIBVERIEXEC_H__
#define __LIBVERIEXEC_H__

struct mac_veriexec_syscall_params;

int	veriexec_check_fd_mode(int, unsigned int);
int	veriexec_check_path_mode(const char *, unsigned int);
int	veriexec_check_fd(int);
int	veriexec_check_path(const char *);
int	veriexec_get_pid_params(pid_t, struct mac_veriexec_syscall_params *);
int	veriexec_get_path_params(const char *,
	    struct mac_veriexec_syscall_params *);
int	veriexec_check_path_label(const char *, const char *);
int	veriexec_check_pid_label(pid_t, const char *);
char *	veriexec_get_path_label(const char *, char *, size_t);
char *	veriexec_get_pid_label(pid_t, char *, size_t);
unsigned int gbl_check_path(const char *);
unsigned int gbl_check_pid(pid_t);
int	execv_script(const char *, char * const *);

#define HAVE_GBL_CHECK_PID 1
#define HAVE_VERIEXEC_CHECK_PATH_LABEL 1
#define HAVE_VERIEXEC_CHECK_PID_LABEL 1
#define HAVE_VERIEXEC_GET_PATH_LABEL 1
#define HAVE_VERIEXEC_GET_PID_LABEL 1

#endif  /* __LIBVERIEXEC_H__ */
