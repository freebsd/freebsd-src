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
 * Find out about remote users
 */

#ifndef RPC_HDR
%#ifndef lint
%/*static char sccsid[] = "from: @(#)rnusers.x 1.2 87/09/20 Copyr 1987 Sun Micro";*/
%/*static char sccsid[] = "from: @(#)rnusers.x	2.1 88/08/01 4.0 RPCSRC";*/
%static char rcsid[] = "$Id: rnusers.x,v 1.1 1993/09/14 17:42:46 jtc Exp $";
%#endif /* not lint */
#endif

const MAXUSERS = 100;
const MAXUTLEN = 256;

struct utmp {
	string ut_line<MAXUTLEN>;
	string ut_name<MAXUTLEN>;
	string ut_host<MAXUTLEN>;
	int ut_time;
};


struct utmpidle {
	utmp ui_utmp;
	unsigned int ui_idle;
};

typedef utmp utmparr<MAXUSERS>;

typedef utmpidle utmpidlearr<MAXUSERS>;

program RUSERSPROG {
	/*
	 * Old version does not include idle information
	 */
	version RUSERSVERS_ORIG {
		int
		RUSERSPROC_NUM(void) = 1;

		utmparr
		RUSERSPROC_NAMES(void) = 2;

		utmparr
		RUSERSPROC_ALLNAMES(void) = 3;
	} = 1;

	/*
	 * Includes idle information
	 */
	version RUSERSVERS_IDLE {
		int
		RUSERSPROC_NUM(void) = 1;

		utmpidlearr
		RUSERSPROC_NAMES(void) = 2;

		utmpidlearr
		RUSERSPROC_ALLNAMES(void) = 3;
	} = 2;
} = 100002;
	
