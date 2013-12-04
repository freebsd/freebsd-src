/*
 * lib_strbuf - library string storage
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <isc/net.h>
#include <isc/result.h>
#include "ntp_stdlib.h"
#include "lib_strbuf.h"

/*
 * Storage declarations
 */
char lib_stringbuf[LIB_NUMBUFS][LIB_BUFLENGTH];
int lib_nextbuf;
int ipv4_works;
int ipv6_works;
int lib_inited = 0;

/*
 * initialization routine.  Might be needed if the code is ROMized.
 */
void
init_lib(void)
{
	lib_nextbuf = 0;
	ipv4_works = (ISC_R_SUCCESS == isc_net_probeipv4());
	ipv6_works = (ISC_R_SUCCESS == isc_net_probeipv6());
	lib_inited = 1;
}
