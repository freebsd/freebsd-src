/*
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/**********************************************************************
 * HISTORY
 * $Log: netcryptvoid.c,v $
 * Revision 1.1.1.1  1993/08/21  00:46:33  jkh
 * Current sup with compression support.
 *
 * Revision 1.1.1.1  1993/05/21  14:52:17  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 2.2  92/09/09  22:04:34  mrt
 * 	Created.
 * 	[92/09/09            mrt]
 * 
 */
/*
 * DATA ENCRYPTION
 *	netcrypt (key)		turn on/off encryption of strings and files
 *	  char *key;			encryption key
 *
 */

/*
 * Replacement for subroutine version of "crypt" program
 *  for foreign and non-BSD-licensed sites. With this code
 *  you can only run unencrypted sups
 */

#include <libc.h>
#include "sup.h"
#include "supmsg.h"

/*********************************************
 ***    G L O B A L   V A R I A B L E S    ***
 *********************************************/

int cryptflag = 0;		/* whether to encrypt/decrypt data */
char *cryptbuf;			/* buffer for data encryption/decryption */

int netcrypt (pword)
char *pword;
{
	if (pword == NULL || (strcmp(pword,PSWDCRYPT) == 0)) {
		cryptflag = 0;
		(void) getcryptbuf (0);
		return (SCMOK);
	}
	return (SCMERR);
}
int getcryptbuf (x)
int x;
{
	static int cryptsize = 0;	/* size of current cryptbuf */

	if (cryptflag == 0) {
		return(SCMOK);
	} else 
		return (SCMERR);
}

void decode (in,out,count)
char *in,*out;
int count;
{
}


void encode (in,out,count)
char *in,*out;
int count;
{
}
