/*-
 * Copyright (c) 2002, 2003 Networks Associates Technology, Inc.
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
__FBSDID("$FreeBSD$");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mac.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/pipe.h>
#include <sys/sysctl.h>

#include <sys/mac_policy.h>

#include <security/mac/mac_internal.h>

static int	mac_enforce_pipe = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_pipe, CTLFLAG_RW,
    &mac_enforce_pipe, 0, "Enforce MAC policy on pipe operations");
TUNABLE_INT("security.mac.enforce_pipe", &mac_enforce_pipe);

#ifdef MAC_DEBUG
static unsigned int nmacpipes;
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, pipes, CTLFLAG_RD,
    &nmacpipes, 0, "number of pipes in use");
#endif

struct label *
mac_pipe_label_alloc(void)
{
	struct label *label;

	label = mac_labelzone_alloc(M_WAITOK);
	MAC_PERFORM(init_pipe_label, label);
	MAC_DEBUG_COUNTER_INC(&nmacpipes);
	return (label);
}

void
mac_init_pipe(struct pipe *pipe)
{

	pipe->pipe_label = pipe->pipe_peer->pipe_label =
	    mac_pipe_label_alloc();
}

void
mac_pipe_label_free(struct label *label)
{

	MAC_PERFORM(destroy_pipe_label, label);
	mac_labelzone_free(label);
	MAC_DEBUG_COUNTER_DEC(&nmacpipes);
}

void
mac_destroy_pipe(struct pipe *pipe)
{

	mac_pipe_label_free(pipe->pipe_label);
	pipe->pipe_label = NULL;
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
mac_create_pipe(struct ucred *cred, struct pipe *pipe)
{

	MAC_PERFORM(create_pipe, cred, pipe, pipe->pipe_label);
}

static void
mac_relabel_pipe(struct ucred *cred, struct pipe *pipe, struct label *newlabel)
{

	MAC_PERFORM(relabel_pipe, cred, pipe, pipe->pipe_label, newlabel);
}

int
mac_check_pipe_ioctl(struct ucred *cred, struct pipe *pipe, unsigned long cmd,
    void *data)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_ioctl, cred, pipe, pipe->pipe_label, cmd, data);

	return (error);
}

int
mac_check_pipe_poll(struct ucred *cred, struct pipe *pipe)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_poll, cred, pipe, pipe->pipe_label);

	return (error);
}

int
mac_check_pipe_read(struct ucred *cred, struct pipe *pipe)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_read, cred, pipe, pipe->pipe_label);

	return (error);
}

static int
mac_check_pipe_relabel(struct ucred *cred, struct pipe *pipe,
    struct label *newlabel)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_relabel, cred, pipe, pipe->pipe_label, newlabel);

	return (error);
}

int
mac_check_pipe_stat(struct ucred *cred, struct pipe *pipe)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_stat, cred, pipe, pipe->pipe_label);

	return (error);
}

int
mac_check_pipe_write(struct ucred *cred, struct pipe *pipe)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_write, cred, pipe, pipe->pipe_label);

	return (error);
}

int
mac_pipe_label_set(struct ucred *cred, struct pipe *pipe, struct label *label)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	error = mac_check_pipe_relabel(cred, pipe, label);
	if (error)
		return (error);

	mac_relabel_pipe(cred, pipe, label);

	return (0);
}
