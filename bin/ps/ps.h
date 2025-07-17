/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/queue.h>

#define	UNLIMITED	0	/* unlimited terminal width */
enum type { UNSPEC, /* For output routines that don't care and aliases. */
	    CHAR, UCHAR, SHORT, USHORT, INT, UINT, LONG, ULONG, KPTR, PGTOK };

typedef struct kinfo_str {
	STAILQ_ENTRY(kinfo_str) ks_next;
	char *ks_str;	/* formatted string */
} KINFO_STR;

typedef struct kinfo {
	struct kinfo_proc *ki_p;	/* kinfo_proc structure */
	const char *ki_args;	/* exec args */
	const char *ki_env;	/* environment */
	int ki_valid;		/* 1 => uarea stuff valid */
	double	 ki_pcpu;	/* calculated in main() */
	segsz_t	 ki_memsize;	/* calculated in main() */
	union {
		int level;	/* used in decendant_sort() */
		char *prefix;	/* calculated in decendant_sort() */
	} ki_d;
	STAILQ_HEAD(, kinfo_str) ki_ks;
} KINFO;

/* Keywords/variables to be printed. */
typedef struct varent {
	STAILQ_ENTRY(varent)	 next_ve;
	const char		*header;
	const struct var	*var;
	u_int			 width;
#define VE_KEEP		(1 << 0)
	uint16_t		flags;
} VARENT;
STAILQ_HEAD(velisthead, varent);

struct var;
typedef struct var VAR;
/* Structure representing one available keyword. */
struct var {
	const char *name;	/* name(s) of variable */
	union {
		/* Valid field depends on RESOLVED_ALIAS' presence. */
		const char	*aliased; /* keyword this one is an alias to */
		const VAR	*final_kw; /* final aliased keyword */
	};
	const char *header;	/* default header */
	const char *field;	/* xo field name */
#define COMM		0x01	/* needs exec arguments and environment (XXX) */
#define LJUST		0x02	/* left adjust on output (trailing blanks) */
#define USER		0x04	/* needs user structure */
#define INF127		0x10	/* values >127 displayed as 127 */
#define NOINHERIT	0x1000	/* Don't inherit flags from aliased keyword. */
#define RESOLVING_ALIAS	0x10000	/* Used transiently to resolve aliases. */
#define RESOLVED_ALIAS	0x20000	/* Mark that an alias has been resolved. */
	u_int	flag;
	/* output routine */
	char	*(*oproc)(struct kinfo *, struct varent *);
	/*
	 * The following (optional) elements are hooks for passing information
	 * to the generic output routine pvar (which prints simple elements
	 * from the well known kinfo_proc structure).
	 */
	size_t	off;		/* offset in structure */
	enum	type type;	/* type of element */
	const char *fmt;	/* printf format (depends on output routine) */
};

#include "extern.h"
