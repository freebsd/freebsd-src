/*
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * $FreeBSD$
 */
#ifndef _DEV_UTOPIA_UTOPIA_H
#define	_DEV_UTOPIA_UTOPIA_H

/* Structure for user-level register formatting */
struct utopia_print {
	uint8_t		type;	/* register type */
	uint8_t		reg;	/* register number */
	const char	*name;	/* register name */
	const char	*fmt;	/* format for printing */
};

/*
 * Types of registers
 */
#define	UTP_REGT_BITS		0x0	/* use printb to print */
#define	UTP_REGT_INT8		0x1	/* 8 bit hex number */
#define	UTP_REGT_INT10BITS	0x2	/* 10 bit hex number + 6 bit printb */
#define	UTP_REGT_INT12		0x3	/* 12 bit LE hex */
#define	UTP_REGT_INT16		0x4	/* 16 bit LE hex */
#define	UTP_REGT_INT19		0x5	/* 19 bit LE hex */
#define	UTP_REGT_INT20		0x6	/* 20 bit LE hex */
#define	UTP_REGT_INT21		0x7	/* 21 bit LE hex */

/* number of additional registers per type */
#define	UTP_REG_ADD	0, 0, 1, 1, 1, 2, 2, 2

/* flags field */
#define	UTP_FL_NORESET		0x0001	/* cannot write MRESET register */
#define	UTP_FL_POLL_CARRIER	0x0002	/* need to poll for carrier */

/* state field */
#define	UTP_ST_ACTIVE		0x0001	/* registers accessible */
#define	UTP_ST_SDH		0x0002	/* SDH or SONET */
#define	UTP_ST_UNASS		0x0004	/* produce unassigned cells */
#define	UTP_ST_NOSCRAMB		0x0008	/* no scrambling */
#define	UTP_ST_DETACH		0x0010	/* detaching */
#define	UTP_ST_ATTACHED		0x0020	/* successful attached */

/* carrier field */
#define	UTP_CARR_UNKNOWN	0
#define	UTP_CARR_OK		1
#define	UTP_CARR_LOST		2

/* loopback field */
#define	UTP_LOOP_NONE		0x0000
#define	UTP_LOOP_TIME		0x0001	/* timing source loopback */
#define	UTP_LOOP_DIAG		0x0002	/* diagnostic loopback */
#define	UTP_LOOP_LINE		0x0004	/* serial line loopback */
#define	UTP_LOOP_PARAL		0x0008	/* parallel diagnostic loopback */
#define	UTP_LOOP_TWIST		0x0010	/* twisted pair diagnostic loopback */
#define	UTP_LOOP_PATH		0x0020	/* diagnostic path loopback */

/* type */
#define	UTP_TYPE_UNKNOWN	0
#define	UTP_TYPE_SUNI_LITE	1
#define	UTP_TYPE_SUNI_ULTRA	2
#define	UTP_TYPE_SUNI_622	3
#define	UTP_TYPE_IDT77105	4

#ifdef _KERNEL

#include <sys/queue.h>

/*
 * These must be implemented by the card driver
 */
struct utopia_methods {
	/* read at most n PHY registers starting at reg into val */
	int	(*readregs)(struct ifatm *, u_int reg, uint8_t *val, u_int *n);

	/* change the bits given by mask to them in val in register reg */
	int	(*writereg)(struct ifatm *, u_int reg, u_int mask, u_int val);
};

/*
 * Public state
 */
struct utopia {
	struct ifatm	*ifatm;		/* driver data */
	struct ifmedia	*media;		/* driver supplied */
	struct mtx	*lock;		/* driver supplied */
	const struct utopia_methods *methods;
	LIST_ENTRY(utopia) link;	/* list of these structures */
	u_int		flags;		/* flags set by the driver */
	u_int		state;		/* current state */
	u_int		carrier;	/* carrier state */
	u_int		loopback;	/* loopback mode */
	const struct utopia_chip *chip;	/* chip operations */
};

struct utopia_chip {
	/* type and name of the chip */
	u_int	type;
	const char *const name;

	/* number of registers */
	u_int	nregs;

	/* reset chip to known state */
	int	(*reset)(struct utopia *);

	/* set SONET/SDH mode */
	int	(*set_sdh)(struct utopia *, int sdh);

	/* set idle/unassigned cells */
	int	(*set_unass)(struct utopia *, int unass);

	/* enable/disable scrambling */
	int	(*set_noscramb)(struct utopia *, int noscramb);

	/* update carrier status */
	int	(*update_carrier)(struct utopia *);

	/* set loopback mode */
	int	(*set_loopback)(struct utopia *, u_int mode);

	/* handle interrupt */
	void	(*intr)(struct utopia *);
};

/*
 * These are implemented in the common utopia code
 */
int utopia_attach(struct utopia *, struct ifatm *, struct ifmedia *,
    struct mtx *, struct sysctl_ctx_list *, struct sysctl_oid_list *,
    const struct utopia_methods *);
void utopia_detach(struct utopia *);

int utopia_start(struct utopia *);
void utopia_stop(struct utopia *);

void utopia_init_media(struct utopia *);
void utopia_reset_media(struct utopia *);

#define	utopia_reset(S)			((S)->chip->reset((S)))
#define	utopia_set_sdh(S, SDH)		((S)->chip->set_sdh((S), (SDH)))
#define	utopia_set_unass(S, U)		((S)->chip->set_unass((S), (U)))
#define	utopia_set_noscramb(S, N)	((S)->chip->set_noscramb((S), (N)))
#define	utopia_update_carrier(S)	((S)->chip->update_carrier((S)))
#define	utopia_set_loopback(S, L)	((S)->chip->set_loopback((S), (L)))
#define	utopia_intr(S)			((S)->chip->intr((S)))

#endif /* _KERNEL */

#endif	/* _DEV_UTOPIA_UTOPIA_H */
