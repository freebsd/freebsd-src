/*
 * Copyright (C) 1994, David Greenman. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/clist.h>
#include <sys/malloc.h>

char cwaiting;
struct cblock *cfreelist = 0;
int cfreecount, nclist = 256;

void
clist_init()
{
	int i;
	struct cblock *tmp;

	for (i = 0; i < nclist; ++i) {
		tmp = malloc(sizeof(struct cblock), M_TTYS, M_NOWAIT);
		if (!tmp)
			panic("clist_init: could not allocate cblock");
		bzero((char *)tmp, sizeof(struct cblock));
		tmp->c_next = cfreelist;
		cfreelist = tmp;
		cfreecount += CBSIZE;
	}
	return;
}

/*
 * Get a character from head of clist.
 */
int
getc(clistp)
	struct clist *clistp;
{
	int chr = -1;
	int s;
	struct cblock *cblockp;

	s = spltty();

	/* If there are characters in the list, get one */
	if (clistp->c_cc) {
		cblockp = (struct cblock *)((long)clistp->c_cf & ~CROUND);
		chr = (u_char)*clistp->c_cf;
#if 0
		/*
		 * If this char is quoted, set the flag.
		 */
		if (isset(cblockp->c_quote, clistp->c_cf - (char *)cblockp->c_info))
			chr |= TTY_QUOTE;
#endif
		clistp->c_cf++;
		clistp->c_cc--;
		if ((clistp->c_cf >= (char *)(cblockp+1)) || (clistp->c_cc == 0)) {
			if (clistp->c_cc > 0) {
				clistp->c_cf = cblockp->c_next->c_info;
			} else {
				clistp->c_cf = clistp->c_cl = NULL;
			}
			cblockp->c_next = cfreelist;
			cfreelist = cblockp;
			cfreecount += CBSIZE;
		}
	}

	splx(s);
	return (chr);
}

/*
 * Copy 'amount' of chars, beginning at head of clist 'clistp' to
 * destination linear buffer 'dest'.
 */
int
q_to_b(clistp, dest, amount)
	struct clist *clistp;
	char *dest;
	int amount;
{
	struct cblock *cblockp;
	struct cblock *cblockn;
	char *dest_orig = dest;
	int numc;
	int s;

	s = spltty();

	while (clistp && amount && (clistp->c_cc > 0)) {
		cblockp = (struct cblock *)((long)clistp->c_cf & ~CROUND);
		cblockn = cblockp + 1; /* pointer arithmetic! */
		numc = min(amount, (char *)cblockn - clistp->c_cf);
		numc = min(numc, clistp->c_cc);
		bcopy(clistp->c_cf, dest, numc);
		amount -= numc;
		clistp->c_cf += numc;
		clistp->c_cc -= numc;
		dest += numc;
		if ((clistp->c_cf >= (char *)cblockn) || (clistp->c_cc == 0)) {
			if (clistp->c_cc > 0) {
				clistp->c_cf = cblockp->c_next->c_info;
			} else {
				clistp->c_cf = clistp->c_cl = NULL;
			}
			cblockp->c_next = cfreelist;
			cfreelist = cblockp;
			cfreecount += CBSIZE;
		}
	}

	splx(s);
	return (dest - dest_orig);
}

/*
 * Flush 'amount' of chars, beginning at head of clist 'clistp'.
 */
void
ndflush(clistp, amount)
	struct clist *clistp;
	int amount;
{
	struct cblock *cblockp;
	struct cblock *cblockn;
	int numc;
	int s;

	s = spltty();

	while (amount && (clistp->c_cc > 0)) {
		cblockp = (struct cblock *)((long)clistp->c_cf & ~CROUND);
		cblockn = cblockp + 1; /* pointer arithmetic! */
		numc = min(amount, (char *)cblockn - clistp->c_cf);
		numc = min(numc, clistp->c_cc);
		amount -= numc;
		clistp->c_cf += numc;
		clistp->c_cc -= numc;
		if ((clistp->c_cf >= (char *)cblockn) || (clistp->c_cc == 0)) {
			if (clistp->c_cc > 0) {
				clistp->c_cf = cblockp->c_next->c_info;
			} else {
				clistp->c_cf = clistp->c_cl = NULL;
			}
			cblockp->c_next = cfreelist;
			cfreelist = cblockp;
			cfreecount += CBSIZE;
		}
	}

	splx(s);
	return;
}

int
putc(chr, clistp)
	int chr;
	struct clist *clistp;
{
	struct cblock *cblockp;
	struct cblock *bclockn;
	int s;

	s = spltty();

	cblockp = (struct cblock *)((long)clistp->c_cl & ~CROUND);

	if (clistp->c_cl == NULL) {
		if (cfreelist) {
			cblockp = cfreelist;
			cfreelist = cfreelist->c_next;
			cfreecount -= CBSIZE;
			cblockp->c_next = NULL;
			clistp->c_cf = clistp->c_cl = cblockp->c_info;
			clistp->c_cc = 0;
		} else {
			splx(s);
			return (-1);
		}
	} else {
		if (((long)clistp->c_cl & CROUND) == 0) {
			if (cfreelist) {
				cblockp = (cblockp-1)->c_next = cfreelist;
				cfreelist = cfreelist->c_next;
				cfreecount -= CBSIZE;
				cblockp->c_next = NULL;
				clistp->c_cl = cblockp->c_info;
			} else {
				splx(s);
				return (-1);
			}
		}
	}

#if 0
	if (chr & TTY_QUOTE)
		setbit(cblockp->c_quote, clistp->c_cl - (char *)cblockp->c_info);
#endif
	*clistp->c_cl++ = chr;
	clistp->c_cc++;

	splx(s);
	return (0);
}

/*
 * Copy data from linear buffer to clist chain.
 */
int
b_to_q(src, amount, clistp)
	char *src;
	int amount;
	struct clist *clistp;
{
	struct cblock *cblockp;
	struct cblock *bclockn;
	int s;
	int numc;

	s = spltty();

	/*
	 * If there are no cblocks assigned to this clist yet,
	 * then get one.
	 */
	if (clistp->c_cl == NULL) {
		if (cfreelist) {
			cblockp = cfreelist;
			cfreelist = cfreelist->c_next;
			cfreecount -= CBSIZE;
			cblockp->c_next = NULL;
			clistp->c_cf = clistp->c_cl = cblockp->c_info;
			clistp->c_cc = 0;
		} else {
			splx(s);
			return (amount);
		}
	} else {
		cblockp = (struct cblock *)((long)clistp->c_cl & ~CROUND);
	}

	while (amount) {
		/*
		 * Get another cblock if needed.
		 */
		if (((long)clistp->c_cl & CROUND) == 0) {
			if (cfreelist) {
				cblockp = (cblockp-1)->c_next = cfreelist;
				cfreelist = cfreelist->c_next;
				cfreecount -= CBSIZE;
				cblockp->c_next = NULL;
				clistp->c_cl = cblockp->c_info;
			} else {
				splx(s);
				return (amount);
			}
		}

		/*
		 * Copy a chunk of the linear buffer up to the end
		 * of this cblock.
		 */
		cblockp += 1;
		numc = min(amount, (char *)(cblockp) - clistp->c_cl);
		bcopy(src, clistp->c_cl, numc);
		
		/*
		 * Clear quote bits.
		 */

		/*
		 * ...and update pointer for the next chunk.
		 */
		src += numc;
		clistp->c_cl += numc;
		clistp->c_cc += numc;
		amount -= numc;
	}

	splx(s);
	return (amount);
}

char *
nextc(clistp, cp, dst)
	struct clist *clistp;
	char *cp;
	int *dst;
{
	struct cblock *cblockp;

	++cp;
	if (clistp->c_cc && (cp != clistp->c_cl)) {
		if (((long)cp & CROUND) == 0)
			cp = ((struct cblock *)cp - 1)->c_next->c_info;
		cblockp = (struct cblock *)((long)cp & ~CROUND);
#if 0
		*dst = *cp | (isset(cblockp->c_quote, cp - (char *)cblockp->c_info) ? TTY_QUOTE : 0);
#endif
		*dst = (u_char)*cp;
		return (cp);
	}

	return (NULL);
}

int
unputc(clistp)
	struct clist *clistp;
{
	struct cblock *cblockp = 0, *cbp = 0;
	int s;
	int chr = -1;


	s = spltty();

	if (clistp->c_cc) {
		--clistp->c_cc;
		chr = (u_char)*--clistp->c_cl;
		/*
		 * Get the quote flag and 'unput' it, too.
		 */

		/* XXX write me! */

		cblockp = (struct cblock *)((long)clistp->c_cl & ~CROUND);

		/*
		 * If all of the characters have been unput in this
		 * cblock, the find the previous one and free this
		 * one.
		 */
		if (clistp->c_cc && (clistp->c_cl <= (char *)cblockp->c_info)) {
			cbp = (struct cblock *)((long)clistp->c_cf & ~CROUND);

			while (cbp->c_next != cblockp)
				cbp = cbp->c_next;

			clistp->c_cl = (char *)(cbp+1);
			cblockp->c_next = cfreelist;
			cfreelist = cblockp;
			cfreecount += CBSIZE;
			cbp->c_next = NULL;
		}
	}

	/*
	 * If there are no more characters on the list, then
	 * free the last cblock.
	 */
	if ((clistp->c_cc == 0) && clistp->c_cl) {
		cblockp = (struct cblock *)((long)clistp->c_cl & ~CROUND);
		cblockp->c_next = cfreelist;
		cfreelist = cblockp;
		cfreecount += CBSIZE;
		clistp->c_cf = clistp->c_cl = NULL;
	}

	splx(s);
	return (chr);
}

void
catq(src_clistp, dest_clistp)
	struct clist *src_clistp, *dest_clistp;
{
	char buffer[CBSIZE*2];
	int amount;

	while (src_clistp->c_cc) {
		amount = q_to_b(src_clistp, buffer, sizeof(buffer));
		b_to_q(buffer, amount, dest_clistp);
	}

	return;
}
