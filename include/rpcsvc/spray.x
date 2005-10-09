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
 * Spray a server with packets
 * Useful for testing flakiness of network interfaces
 */

#ifndef RPC_HDR
%#ifndef lint
%/*static char sccsid[] = "from: @(#)spray.x 1.2 87/09/18 Copyr 1987 Sun Micro";*/
%/*static char sccsid[] = "from: @(#)spray.x	2.1 88/08/01 4.0 RPCSRC";*/
%#endif /* not lint */
%#include <sys/cdefs.h>
%__FBSDID("$FreeBSD$");
#endif

const SPRAYMAX = 8845;	/* max amount can spray */

/*
 * GMT since 0:00, 1 January 1970
 */
struct spraytimeval {
	unsigned int sec;
	unsigned int usec;
};

/*
 * spray statistics
 */
struct spraycumul {
	unsigned int counter;
	spraytimeval clock;
};

/*
 * spray data
 */
typedef opaque sprayarr<SPRAYMAX>;

program SPRAYPROG {
	version SPRAYVERS {
		/*
		 * Just throw away the data and increment the counter
		 * This call never returns, so the client should always 
		 * time it out.
		 */
		void
		SPRAYPROC_SPRAY(sprayarr) = 1;

		/*
		 * Get the value of the counter and elapsed time  since
		 * last CLEAR.
		 */
		spraycumul	
		SPRAYPROC_GET(void) = 2;

		/*
		 * Clear the counter and reset the elapsed time
		 */
		void
		SPRAYPROC_CLEAR(void) = 3;
	} = 1;
} = 100012;
