/*
 * lib_strbuf.c - init_lib() and library string storage
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <isc/mutex.h>
#include <isc/net.h>
#include <isc/result.h>

#include "ntp_fp.h"
#include "ntp_stdlib.h"
#include "lib_strbuf.h"

#define LIB_NUMBUF	10

/*
 * Storage declarations
 */
static char		lib_stringbuf_storage[LIB_NUMBUF][LIB_BUFLENGTH];
static char *		lib_stringbuf[LIB_NUMBUF];
int			lib_inited;
static isc_mutex_t	lib_mutex;
int			ipv4_works;
int			ipv6_works;
int			debug;

/*
 * initialization routine.  Might be needed if the code is ROMized.
 */
void
init_lib(void)
{
	u_int	u;

	if (lib_inited) {
		return;
	}
	ipv4_works = (ISC_R_SUCCESS == isc_net_probeipv4());
	ipv6_works = (ISC_R_SUCCESS == isc_net_probeipv6());
	init_systime();
	/*
	 * Avoid -Wrestrict warnings by keeping a pointer to each buffer
	 * so the compiler can see copying from one buffer to another is
	 * not violating restrict qualifiers on, e.g. memcpy() args.
	 */
	for (u = 0; u < COUNTOF(lib_stringbuf); u++) {
		lib_stringbuf[u] = lib_stringbuf_storage[u];
	}
	isc_mutex_init(&lib_mutex);
	lib_inited = TRUE;
}


char *
lib_getbuf(void)
{
	static int	lib_nextbuf;
	int		mybuf;

	if (!lib_inited) {
		init_lib();
	}
	isc_mutex_lock(&lib_mutex);
	mybuf = lib_nextbuf;
	lib_nextbuf = (1 + mybuf) % COUNTOF(lib_stringbuf);
	isc_mutex_unlock(&lib_mutex);
	zero_mem(lib_stringbuf[mybuf], LIB_BUFLENGTH);

	return lib_stringbuf[mybuf];
}