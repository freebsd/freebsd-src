/*-
 * Copyright (c) 2002-2003 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
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
__FBSDID("$FreeBSD: src/sys/security/mac/mac_pipe.c,v 1.112 2007/04/22 19:55:56 rwatson Exp $");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/pipe.h>
#include <sys/sysctl.h>

#include <security/mac/mac_framework.h>
#include <security/mac/mac_internal.h>
#include <security/mac/mac_policy.h>

struct label *
mac_pipe_label_alloc(void)
{
	struct label *label;

	label = mac_labelzone_alloc(M_WAITOK);
	MAC_PERFORM(init_pipe_label, label);
	return (label);
}

void
mac_init_pipe(struct pipepair *pp)
{

	pp->pp_label = mac_pipe_label_alloc();
}

void
mac_pipe_label_free(struct label *label)
{

	MAC_PERFORM(destroy_pipe_label, label);
	mac_labelzone_free(label);
}

void
mac_destroy_pipe(struct pipepair *pp)
{

	mac_pipe_label_free(pp->pp_label);
	pp->pp_label = NULL;
}

void
mac_copy_pipe_label(struct label *src, struct label *dest)
{

	MAC_PERFORM(copy_pipe_label, src, dest);
}

int
mac_externalize_pipe_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen)
{
	int error;

	MAC_EXTERNALIZE(pipe, label, elements, outbuf, outbuflen);

	return (error);
}

int
mac_internalize_pipe_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(pipe, label, string);

	return (error);
}

void
mac_create_pipe(struct ucred *cred, struct pipepair *pp)
{

	MAC_PERFORM(create_pipe, cred, pp, pp->pp_label);
}

static void
mac_relabel_pipe(struct ucred *cred, struct pipepair *pp,
    struct label *newlabel)
{

	MAC_PERFORM(relabel_pipe, cred, pp, pp->pp_label, newlabel);
}

int
mac_check_pipe_ioctl(struct ucred *cred, struct pipepair *pp,
    unsigned long cmd, void *data)
{
	int error;

	mtx_assert(&pp->pp_mtx, MA_OWNED);

	MAC_CHECK(check_pipe_ioctl, cred, pp, pp->pp_label, cmd, data);

	return (error);
}

int
mac_check_pipe_poll(struct ucred *cred, struct pipepair *pp)
{
	int error;

	mtx_assert(&pp->pp_mtx, MA_OWNED);

	MAC_CHECK(check_pipe_poll, cred, pp, pp->pp_label);

	return (error);
}

int
mac_check_pipe_read(struct ucred *cred, struct pipepair *pp)
{
	int error;

	mtx_assert(&pp->pp_mtx, MA_OWNED);

	MAC_CHECK(check_pipe_read, cred, pp, pp->pp_label);

	return (error);
}

static int
mac_check_pipe_relabel(struct ucred *cred, struct pipepair *pp,
    struct label *newlabel)
{
	int error;

	mtx_assert(&pp->pp_mtx, MA_OWNED);

	MAC_CHECK(check_pipe_relabel, cred, pp, pp->pp_label, newlabel);

	return (error);
}

int
mac_check_pipe_stat(struct ucred *cred, struct pipepair *pp)
{
	int error;

	mtx_assert(&pp->pp_mtx, MA_OWNED);

	MAC_CHECK(check_pipe_stat, cred, pp, pp->pp_label);

	return (error);
}

int
mac_check_pipe_write(struct ucred *cred, struct pipepair *pp)
{
	int error;

	mtx_assert(&pp->pp_mtx, MA_OWNED);

	MAC_CHECK(check_pipe_write, cred, pp, pp->pp_label);

	return (error);
}

int
mac_pipe_label_set(struct ucred *cred, struct pipepair *pp,
    struct label *label)
{
	int error;

	mtx_assert(&pp->pp_mtx, MA_OWNED);

	error = mac_check_pipe_relabel(cred, pp, label);
	if (error)
		return (error);

	mac_relabel_pipe(cred, pp, label);

	return (0);
}
