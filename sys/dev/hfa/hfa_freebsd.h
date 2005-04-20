/*-
 * Copyright (c) 2002 Matthew N. Dodd <winter@jurai.net>
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
 *
 * $FreeBSD$
 */

struct hfa_softc {
	device_t		dev;

        struct resource *       mem;
        int                     mem_rid;
        int                     mem_type;

        struct resource *       irq;
        int                     irq_rid;
        void *                  irq_ih;

        struct mtx              mtx;

	Fore_unit		fup;

	/* sysctl support */
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid *	sysctl_tree;
};

#define	HFA_LOCK(_sc)	mtx_lock(&(_sc)->mtx)
#define	HFA_UNLOCK(_sc)	mtx_unlock(&(_sc)->mtx)

extern devclass_t	hfa_devclass;

int	hfa_alloc	(device_t);
int	hfa_free	(device_t);

int	hfa_attach	(device_t);
int	hfa_detach	(device_t);

void	hfa_intr	(void *);
void	hfa_reset	(device_t);
