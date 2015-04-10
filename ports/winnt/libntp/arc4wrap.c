/*
 * arc4wrap.c - wrapper for libevent's ARCFOUR random number generator
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 * --------------------------------------------------------------------
 * This is an inclusion wrapper for the ARCFOUR implementation in
 * libevent. It's main usage is to enable a openSSL-free build on Win32
 * without a full integration of libevent. This provides Win32 specific
 * glue to make the PRNG working. Porting to POSIX should be easy, but
 * on most POSIX systems using openSSL is no problem and falling back to
 * using ARCFOUR instead of the openSSL PRNG is not necessary. And even
 * if it is, there's a good chance that ARCFOUR is a system library.
 */
#include <config.h>
#ifdef _WIN32
# include <wincrypt.h>
# include <process.h>
#else
# error this is currently a pure windows port
#endif

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "ntp_types.h"
#include "ntp_stdlib.h"

/* ARCFOUR implementation glue */
/* export type is empty, since this goes into a static library*/
#define ARC4RANDOM_EXPORT
/* we use default uint32_t as UINT32 */
#define ARC4RANDOM_UINT32 uint32_t
/* do not use ARCFOUR's default includes - we gobble it all up here. */
#define ARC4RANDOM_NO_INCLUDES
/* And the locking. Could probably be left empty. */
#define ARC4_LOCK_() private_lock_()
#define ARC4_UNLOCK_() private_unlock_()

/* support code */

static void
evutil_memclear_(
	void  *buf,
	size_t len)
{
	memset(buf, 0, len);
}

/* locking uses a manual thread-safe ONCE pattern. There's no static
 * initialiser pattern that can be used for critical sections, and
 * we must make sure we do the creation exactly once on the first call.
 */

static long             once_ = 0;
static CRITICAL_SECTION csec_;

static void
private_lock_(void)
{
again:
	switch (InterlockedCompareExchange(&once_, 1, 0)) {
	case 0:
		InitializeCriticalSection(&csec_);
		InterlockedExchange(&once_, 2);
	case 2:
		EnterCriticalSection(&csec_);
		break;

	default:
		YieldProcessor();
		goto again;
	}
}

static void
private_unlock_(void)
{
	if (InterlockedExchangeAdd(&once_, 0) == 2)
		LeaveCriticalSection(&csec_);
}

#pragma warning(disable : 4244)
#include "../../../sntp/libevent/arc4random.c"
