#ifndef OPENIB_OSD_H
#define OPENIB_OSD_H

#include <infiniband/endian.h>
#include <netinet/in.h>

#if __BYTE_ORDER == __BIG_ENDIAN
#define htonll(x) (x)
#define ntohll(x) (x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define htonll(x)  bswap_64(x)
#define ntohll(x)  bswap_64(x)
#endif

#define DAPL_SOCKET int
#define DAPL_INVALID_SOCKET -1
#define DAPL_FD_SETSIZE 16384

#define closesocket close

#endif // OPENIB_OSD_H
