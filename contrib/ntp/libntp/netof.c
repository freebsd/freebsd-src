/* 
 * netof - return the net address part of an ip address
 *         (zero out host part)
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "ntp_stdlib.h"

u_int32
netof(
	u_int32 num
	)
{
	register u_int32 netnum;

	netnum = num;
	if(IN_CLASSC(netnum))
	    netnum &= IN_CLASSC_NET;
	else if (IN_CLASSB(netnum))
	    netnum &= IN_CLASSB_NET;
	else			/* treat all other like class A */
	    netnum &= IN_CLASSA_NET;
	return netnum;
}
