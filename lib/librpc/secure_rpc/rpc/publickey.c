#ifndef lint
static char sccsid[] = 	"@(#)publickey.c	2.3 88/08/15 4.0 RPCSRC";
#endif
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 1.3
 */

/*
 * Public key lookup routines
 */
#include <stdio.h>
#include <pwd.h>
#include <rpc/rpc.h>
#include <rpc/key_prot.h>


extern char *index();
extern char *strcpy();

static char PKMAP[] = "publickey.byname";

/*
 * Get somebody's encrypted secret key from the database, using
 * the given passwd to decrypt it.
 */
getsecretkey(netname, secretkey, passwd)
	char *netname;
	char *secretkey;
	char *passwd;
{
	char *domain;
	int len;
	char *lookup;
	int err;
	char *p;


	err = yp_get_default_domain(&domain);
	if (err) {
		return(0);
	}
	err = yp_match(domain, PKMAP, netname, strlen(netname), &lookup, &len);
	if (err) {
		return(0);
	}
	lookup[len] = 0;
	p = index(lookup,':');
	if (p == NULL) {
		free(lookup);
		return(0);
	}
	p++;
	if (!xdecrypt(p, passwd)) {
		free(lookup);
		return(0);
	}
	if (bcmp(p, p + HEXKEYBYTES, KEYCHECKSUMSIZE) != 0) {
		secretkey[0] = 0;
		free(lookup);
		return(1);
	}
	p[HEXKEYBYTES] = 0;
	(void) strcpy(secretkey, p);
	free(lookup);
	return(1);
}



/*
 * Get somebody's public key
 */
getpublickey(netname, publickey)
	char *netname;
	char *publickey;
{
	char *domain;
	int len;
	char *lookup;
	int err;
	char *p;

	err = yp_get_default_domain(&domain);	
	if (err) {
		return(0);
	}
	err = yp_match(domain, PKMAP, netname, strlen(netname), &lookup, &len);
	if (err) {
		return(0);
	}
	p = index(lookup, ':');
	if (p == NULL) {
		free(lookup);
		return(0);
	}
	*p = 0;	
	(void) strcpy(publickey, lookup);
	return(1);
}
