/*
 * Copyright (C) 1994, David Greenman. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 *
 * $Id: tty_subr.c,v 1.6 1994/09/13 16:02:20 davidg Exp $
 */

/*
 * clist support routines
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/clist.h>
#include <sys/malloc.h>

struct cblock *cfreelist = 0;
int cfreecount = 0;

#ifndef INITIAL_CBLOCKS
#define INITIAL_CBLOCKS 50
#endif

#define MBUF_DIAG
#ifdef MBUF_DIAG
void
print_nblocks()
{
	printf("There are currently %d bytes in cblocks\n", cfreecount);
}
#endif

/*
 * Called from init_main.c
 */
void
clist_init()
{
	/*
	 * Allocate an initial base set of cblocks as a 'slush'.
	 * We allocate more with each ttyopen().
	 */
	cblock_alloc_cblocks(INITIAL_CBLOCKS);
	return;
}

/*
 * Remove a cblock from the cfreelist queue and return a pointer
 * to it.
 */
static inline struct cblock *
cblock_alloc()
{
	struct cblock *cblockp;

	cblockp = cfreelist;
	if (!cblockp) {
		/* XXX should syslog a message that we're out! */
		return (0);
	}
	cfreelist = cblockp->c_next;
	cblockp->c_next = NULL;
	cfreecount -= CBSIZE;
	return (cblockp);
}

/*
 * Add a cblock to the cfreelist queue.
 */
static inline void
cblock_free(cblockp)
	struct cblock *cblockp;
{
	cblockp->c_next = cfreelist;
	cfreelist = cblockp;
	cfreecount += CBSIZE;
	return;
}

/*
 * Allocate some cblocks for the cfreelist queue.
 */
void
cblock_alloc_cblocks(number)
	int number;
{
	int i;
	struct cblock *tmp;

	for (i = 0; i < number; ++i) {
		tmp = malloc(sizeof(struct cblock), M_TTYS, M_NOWAIT);
		if (!tmp) {
			printf("cblock_alloc_cblocks: could not malloc cblock");
			break;
		}
		bzero((char *)tmp, sizeof(struct cblock));
		cblock_free(tmp);
	}
	return;
}

/*
 * Free some cblocks from the cfreelist queue back to the
 * system malloc pool.
 */
void
cblock_free_cblocks(number)
	int number;
{
	int i;
	struct cblock *tmp;

	for (i = 0; i < number; ++i) {
		tmp = cblock_alloc();
		free(tmp, M_TTYS);
	}
}


/*
 * Get a character from the head of a clist.
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

		/*
		 * If this char is quoted, set the flag.
		 */
		if (isset(cblockp->c_quote, clistp->c_cf - (char *)cblockp->c_info))
			chr |= TTY_QUOTE;

		/*
		 * Advance to next character.
		 */
		clistp->c_cf++;
		clistp->c_cc--;
		/*
		 * If we have advanced the 'first' character pointer
		 * past the end of this cblock, advance to the next one.
		 * If there are no more characters, set the first and
		 * last pointers to NULL. In either case, free the
		 * current cblock.
		 */
		if ((clistp->c_cf >= (char *)(cblockp+1)) || (clistp->c_cc == 0)) {
			if (clistp->c_cc > 0) {
				clistp->c_cf = cblockp->c_next->c_info;
			} else {
				clistp->c_cf = clistp->c_cl = NULL;
			}
			cblock_free(cblockp);
		}
	}

	splx(s);
	return (chr);
}

/*
 * Copy 'amount' of chars, beginning at head of clist 'clistp' to
 * destination linear buffer 'dest'. Return number of characters
 * actually copied.
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
		/*
		 * If this cblock has been emptied, advance to the next
		 * one. If there are no more characters, set the first
		 * and last pointer to NULL. In either case, free the
		 * current cblock.
		 */
		if ((clistp->c_cf >= (char *)cblockn) || (clistp->c_cc == 0)) {
			if (clistp->c_cc > 0) {
				clistp->c_cf = cblockp->c_next->c_info;
			} else {
				clistp->c_cf = clistp->c_cl = NULL;
			}
			cblock_free(cblockp);
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
		/*
		 * If this cblock has been emptied, advance to the next
		 * one. If there are no more characters, set the first
		 * and last pointer to NULL. In either case, free the
		 * current cblock.
		 */
		if ((clistp->c_cf >= (char *)cblockn) || (clistp->c_cc == 0)) {
			if (clistp->c_cc > 0) {
				clistp->c_cf = cblockp->c_next->c_info;
			} else {
				clistp->c_cf = clistp->c_cl = NULL;
			}
			cblock_free(cblockp);
		}
	}

	splx(s);
	return;
}

/*
 * Add a character to the end of a clist. Return -1 is no
 * more clists, or 0 for success.
 */
int
putc(chr, clistp)
	int chr;
	struct clist *clistp;
{
	struct cblock *cblockp;
	int s;

	s = spltty();

	cblockp = (struct cblock *)((long)clistp->c_cl & ~CROUND);

	if (clistp->c_cl == NULL) {
		cblockp = cblock_alloc();
		if (cblockp) {
			clistp->c_cf = clistp->c_cl = cblockp->c_info;
			clistp->c_cc = 0;
		} else {
			splx(s);
			return (-1);
		}
	} else {
		if (((long)clistp->c_cl & CROUND) == 0) {
			struct cblock *prev = (cblockp - 1);
			cblockp = cblock_alloc();
			if (cblockp) {
				prev->c_next = cblockp;
				clistp->c_cl = cblockp->c_info;
			} else {
				splx(s);
				return (-1);
			}
		}
	}

	/*
	 * If this character is quoted, set the quote bit, if not, clear it.
	 */
	if (chr & TTY_QUOTE)
		setbit(cblockp->c_quote, clistp->c_cl - (char *)cblockp->c_info);
	else
		clrbit(cblockp->c_quote, clistp->c_cl - (char *)cblockp->c_info);

	*clistp->c_cl++ = chr;
	clistp->c_cc++;

	splx(s);
	return (0);
}

/*
 * Copy data from linear buffer to clist chain. Return the
 * number of characters not copied.
 */
int
b_to_q(src, amount, clistp)
	char *src;
	int amount;
	struct clist *clistp;
{
	struct cblock *cblockp;
	char *firstbyte, *lastbyte;
	u_char startmask, endmask;
	int startbit, endbit, num_between, numc;
	int s;

	s = spltty();

	/*
	 * If there are no cblocks assigned to this clist yet,
	 * then get one.
	 */
	if (clistp->c_cl == NULL) {
		cblockp = cblock_alloc();
		if (cblockp) {
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
			struct cblock *prev = cblockp - 1;
			cblockp = cblock_alloc();
			if (cblockp) {
				prev->c_next = cblockp;
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
		numc = min(amount, (char *)(cblockp + 1) - clistp->c_cl);
		bcopy(src, clistp->c_cl, numc);
		
		/*
		 * Clear quote bits. The following could probably be made into
		 * a seperate "bitzero()" routine, but why bother?
		 */
		startbit = clistp->c_cl - (char *)cblockp->c_info;
		endbit = startbit + numc - 1;

		firstbyte = (u_char *)cblockp->c_quote + (startbit / NBBY);
		lastbyte = (u_char *)cblockp->c_quote + (endbit / NBBY);

		/*
		 * Calculate mask of bits to preserve in first and
		 * last bytes.
		 */
		startmask = NBBY - (startbit % NBBY);
		startmask = 0xff >> startmask;
		endmask = (endbit % NBBY);
		endmask = 0xff << (endmask + 1);

		if (firstbyte != lastbyte) {
			*firstbyte &= startmask;
			*lastbyte &= endmask;

			num_between = lastbyte - firstbyte - 1;
			if (num_between)
				bzero(firstbyte + 1, num_between);
		} else {
			*firstbyte &= (startmask | endmask);
		}

		/*
		 * ...and update pointer for the next chunk.
		 */
		src += numc;
		clistp->c_cl += numc;
		clistp->c_cc += numc;
		amount -= numc;
		/*
		 * If we go through the loop again, it's always
		 * for data in the next cblock, so by adding one (cblock),
		 * (which makes the pointer 1 beyond the end of this
		 * cblock) we prepare for the assignment of 'prev'
		 * above.
		 */
		cblockp += 1;

	}

	splx(s);
	return (amount);
}

/*
 * Get the next character in the clist. Store it at dst. Don't
 * advance any clist pointers, but return a pointer to the next
 * character position.
 */
char *
nextc(clistp, cp, dst)
	struct clist *clistp;
	char *cp;
	int *dst;
{
	struct cblock *cblockp;

	++cp;
	/*
	 * See if the next character is beyond the end of
	 * the clist.
	 */
	if (clistp->c_cc && (cp != clistp->c_cl)) {
		/*
		 * If the next character is beyond the end of this
		 * cblock, advance to the next cblock.
		 */
		if (((long)cp & CROUND) == 0)
			cp = ((struct cblock *)cp - 1)->c_next->c_info;
		cblockp = (struct cblock *)((long)cp & ~CROUND);

		/*
		 * Get the character. Set the quote flag if this character
		 * is quoted.
		 */
		*dst = (u_char)*cp | (isset(cblockp->c_quote, cp - (char *)cblockp->c_info) ? TTY_QUOTE : 0);

		return (cp);
	}

	return (NULL);
}

/*
 * "Unput" a character from a clist.
 */
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
		--clistp->c_cl;

		chr = (u_char)*clistp->c_cl;

		cblockp = (struct cblock *)((long)clistp->c_cl & ~CROUND);

		/*
		 * Set quote flag if this character was quoted.	
		 */
		if (isset(cblockp->c_quote, (u_char *)clistp->c_cl - cblockp->c_info))
			chr |= TTY_QUOTE;

		/*
		 * If all of the characters have been unput in this
		 * cblock, then find the previous one and free this
		 * one.
		 */
		if (clistp->c_cc && (clistp->c_cl <= (char *)cblockp->c_info)) {
			cbp = (struct cblock *)((long)clistp->c_cf & ~CROUND);

			while (cbp->c_next != cblockp)
				cbp = cbp->c_next;

			/*
			 * When the previous cblock is at the end, the 'last'
			 * pointer always points (invalidly) one past.
			 */
			clistp->c_cl = (char *)(cbp+1);
			cblock_free(cblockp);
			cbp->c_next = NULL;
		}
	}

	/*
	 * If there are no more characters on the list, then
	 * free the last cblock.
	 */
	if ((clistp->c_cc == 0) && clistp->c_cl) {
		cblockp = (struct cblock *)((long)clistp->c_cl & ~CROUND);
		cblock_free(cblockp);
		clistp->c_cf = clistp->c_cl = NULL;
	}

	splx(s);
	return (chr);
}

/*
 * Move characters in source clist to destination clist,
 * preserving quote bits.
 */
void
catq(src_clistp, dest_clistp)
	struct clist *src_clistp, *dest_clistp;
{
	int chr, s;

	s = spltty();
	/*
	 * If the destination clist is empty (has no cblocks atttached),
	 * then we simply assign the current clist to the destination.
	 */
	if (!dest_clistp->c_cf) {
		dest_clistp->c_cf = src_clistp->c_cf;
		dest_clistp->c_cl = src_clistp->c_cl;
		src_clistp->c_cf = src_clistp->c_cl = NULL;

		dest_clistp->c_cc = src_clistp->c_cc;
		src_clistp->c_cc = 0;

		splx(s);
		return;
	}
	splx(s);

	/*
	 * XXX  This should probably be optimized to more than one
	 * character at a time.
	 */
	while ((chr = getc(src_clistp)) != -1)
		putc(chr, dest_clistp);

	return;
}
