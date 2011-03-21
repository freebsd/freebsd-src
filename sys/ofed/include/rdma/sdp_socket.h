/* Stuff that should go into include/linux/socket.h */

#ifndef SDP_SOCKET_H
#define SDP_SOCKET_H

#ifndef AF_INET_SDP
#define AF_INET_SDP 27
#define PF_INET_SDP AF_INET_SDP
#endif

#ifndef SDP_ZCOPY_THRESH
#define SDP_ZCOPY_THRESH 80
#endif

#ifndef SDP_LAST_BIND_ERR
#define SDP_LAST_BIND_ERR 81
#endif

/* TODO: AF_INET6_SDP ? */

#endif
