/*
 * Copyright (c) 1995 Ugen J.S.Antsilevich
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * Snoop stuff.
 */

#ifndef _SNOOP_H_
#define	_SNOOP_H_

#define SNOOP_MINLEN		(4*1024)	/* This should be power of 2.
						 * 4K tested to be the minimum
						 * for which on normal tty
						 * usage there is no need to
						 * allocate more.
						 */
#define SNOOP_MAXLEN		(64*1024)	/* This one also,64K enough
						 * If we grow more,something
						 * really bad in this world..
						 */

/*
 * This is the main snoop per-device
 * structure...
 */

struct snoop {
	dev_t		snp_target;	/* major/minor number of device*/
	struct tty	*snp_tty;	/* tty device pointer	       */
	u_long 		snp_len;	/* buffer data length	       */
	u_long		snp_base;	/* buffer data base	       */
	u_long		snp_blen;	/* Overall buffer len	       */
	caddr_t		snp_buf;	/* Data buffer		       */
	int 		snp_flags;	/* Flags place		       */
#define SNOOP_NBIO		0x0001
#define SNOOP_ASYNC		0x0002
#define SNOOP_OPEN		0x0004
#define SNOOP_RWAIT		0x0008
#define SNOOP_OFLOW		0x0010
#define SNOOP_DOWN		0x0020
	struct selinfo	snp_sel;	/* Selection info	       */
};

/*
 * Theese are snoop io controls
 * SNPSTTY accepts 'struct snptty' as input.
 * If ever type or  unit set to -1,snoop device
 * detached from it's current tty.
 */

#define SNPSTTY       _IOW('T', 90, dev_t)
#define SNPGTTY       _IOR('T', 89, dev_t)

/*
 * Theese values would be returned by FIONREAD ioctl
 * instead of number of characters in buffer in case
 * of specific errors.
 */
#define SNP_OFLOW		-1
#define SNP_TTYCLOSE		-2
#define SNP_DETACH		-3

#ifdef KERNEL
/* XXX several wrong storage classes and types here. */
int	snpdown __P((struct snoop *snp));
int	snpin __P((struct snoop *snp, char *buf, int n));
int	snpinc __P((struct snoop *snp, char c));
#endif /* KERNEL */

#endif /* _SNOOP_H_ */
