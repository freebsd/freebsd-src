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

#ifndef SNOOP_H
#define SNOOP_H		

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
	int	snp_unit;		/* Pty unit number to snoop on */
	int	snp_type;		/* Type same as st_type later  */
	u_long 	snp_len,snp_base;	/* Buffer data len and base    */
	u_long	snp_blen;		/* Overall buffer len	       */
	char	*snp_buf;		/* Data buffer		       */
	int 	snp_flags;		/* Flags place		       */
#define SNOOP_NBIO		0x0001
#define SNOOP_ASYNC		0x0002
#define SNOOP_OPEN		0x0004
#define SNOOP_RWAIT		0x0008
#define SNOOP_OFLOW		0x0010
#define SNOOP_DOWN		0x0020
	struct selinfo	snp_sel;	/* Selection info	       */
};



/*
 * This is structure to be passed
 * to ioctl() so we can define different
 * types of tty's..
 */
struct snptty {
	int	st_unit;
	int	st_type;
#define ST_PTY		0	/* Regular Pty       */
#define	ST_VTY		1	/* Vty for SysCons.. */
#define ST_SIO		2	/* Serial lines	     */
#define ST_MAXTYPE	2
};

#define SNPSTTY       _IOW('T', 90, struct snptty)
#define SNPGTTY       _IOR('T', 89, struct snptty)



#endif

