/* @(#)yppasswd.x	2.1 88/08/01 4.0 RPCSRC */
/* @(#)yppasswd.x 1.1 87/04/13 Copyr 1987 Sun Micro */

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
 * YP password update protocol
 * Requires unix authentication
 */
program YPPASSWDPROG {
	version YPPASSWDVERS {
		/*
		 * Update my passwd entry 
		 */
		int
		YPPASSWDPROC_UPDATE(yppasswd) = 1;
	} = 1;
} = 100009;


struct passwd {
	string pw_name<>;	/* username */
	string pw_passwd<>;	/* encrypted password */
	int pw_uid;		/* user id */
	int pw_gid;		/* group id */
	string pw_gecos<>;	/* in real life name */
	string pw_dir<>;	/* home directory */
	string pw_shell<>;	/* default shell */
};

struct yppasswd {
	string oldpass<>;	/* unencrypted old password */
	passwd newpw;		/* new passwd entry */
};


