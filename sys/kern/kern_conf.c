/*-
 * Parts Copyright (c) 1995 Terrence R. Lambert
 * Copyright (c) 1995 Julian R. Elischer
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Julian R. Elischer ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: kern_conf.c,v 1.4 1995/11/29 12:38:46 julian Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/conf.h>

/*
 * (re)place an entry in the bdevsw or cdevsw table
 * return the slot used in major(*descrip)
 */
#define ADDENTRY(TTYPE,NXXXDEV) \
int TTYPE##_add(dev_t *descrip,						\
		struct TTYPE *newentry,					\
		struct TTYPE *oldentry)					\
{									\
	int i ;								\
	if ( (int)*descrip == -1) {	/* auto (0 is valid) */		\
		/*							\
		 * Search the table looking for a slot...		\
		 */							\
		for (i = 0; i < NXXXDEV; i++)				\
			if (TTYPE[i].d_open == NULL)			\
				break;		/* found one! */	\
		/* out of allocable slots? */				\
		if (i == NXXXDEV) {					\
			return ENFILE;					\
		}							\
	} else {				/* assign */		\
		i = major(*descrip);					\
		if (i < 0 || i >= NXXXDEV) {				\
			return EINVAL;					\
		}							\
	}								\
									\
	/* maybe save old */						\
        if (oldentry) {							\
		bcopy(&TTYPE[i], oldentry, sizeof(struct TTYPE));	\
	}								\
	/* replace with new */						\
	bcopy(newentry, &TTYPE[i], sizeof(struct TTYPE));		\
									\
	/* done! */							\
	*descrip = makedev(i,0);					\
	return 0;							\
} \

ADDENTRY(bdevsw, nblkdev)
ADDENTRY(cdevsw, nchrdev)
