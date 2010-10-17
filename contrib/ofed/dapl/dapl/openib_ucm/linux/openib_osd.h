#ifndef OPENIB_OSD_H
#define OPENIB_OSD_H

#include <endian.h>
#include <netinet/in.h>

#if __BYTE_ORDER == __BIG_ENDIAN
#define htonll(x) (x)
#define ntohll(x) (x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define htonll(x)  bswap_64(x)
#define ntohll(x)  bswap_64(x)
#endif
#ifndef STATIC
#define STATIC static
#endif /* STATIC */
#ifndef _INLINE_
#define _INLINE_ __inline__
#endif /* _INLINE_ */

#define DAPL_SOCKET int
#define DAPL_INVALID_SOCKET -1
#define DAPL_FD_SETSIZE 16

#define closesocket close

struct dapl_thread_signal
{
	DAPL_SOCKET scm[2];
};

STATIC _INLINE_ void dapls_thread_signal(struct dapl_thread_signal *signal)
{
	send(signal->scm[1], "w", sizeof "w", 0);
}

#endif // OPENIB_OSD_H
