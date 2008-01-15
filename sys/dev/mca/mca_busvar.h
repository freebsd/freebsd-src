/*-
 * Copyright (c) 1999 Matthew N. Dodd <winter@jurai.net>
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
 * $FreeBSD: src/sys/dev/mca/mca_busvar.h,v 1.2 1999/09/26 07:02:05 mdodd Exp $
 */

typedef u_int16_t mca_id_t;

struct mca_ident {
	mca_id_t	id;
	char		*name;
};

const char *	mca_match_id	(u_int16_t, struct mca_ident *);

/*
 * Simplified accessors for isa devices
 */

enum mca_device_ivars {
	MCA_IVAR_SLOT,
	MCA_IVAR_ID,
	MCA_IVAR_ENABLED,
};

#define MCA_ACCESSOR(A, B, T)						\
									\
static __inline T mca_get_ ## A(device_t dev)				\
{									\
	uintptr_t v;							\
	BUS_READ_IVAR(device_get_parent(dev), dev, MCA_IVAR_ ## B, &v);	\
	return (T) v;							\
}

MCA_ACCESSOR(slot,	SLOT,		int)
MCA_ACCESSOR(id,	ID,		mca_id_t)
MCA_ACCESSOR(enabled,	ENABLED,	int)

/* don't use these! */
void		mca_pos_set	(device_t, u_int8_t, u_int8_t);
u_int8_t	mca_pos_get	(device_t, u_int8_t);

u_int8_t	mca_pos_read	(device_t, u_int8_t);

void		mca_add_irq	(device_t, int);
void		mca_add_drq	(device_t, int);
void		mca_add_iospace	(device_t, u_long, u_long);
void		mca_add_mspace	(device_t, u_long, u_long);
