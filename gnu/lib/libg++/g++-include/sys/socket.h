#ifndef __libgxx_sys_socket_h

#include <_G_config.h>

extern "C"
{
#ifdef __sys_socket_h_recursive
#include_next <sys/socket.h>
#else
#define __sys_socket_h_recursive
#include <time.h>

#ifdef VMS
#include "GNU_CC_INCLUDE:[sys]socket.h"
#else
#include_next <sys/socket.h>
#endif

#define __libgxx_sys_socket_h 1

// void* in select, since different systems use int* or fd_set*
int       accept _G_ARGS((int, struct sockaddr*, int*));
#ifndef __386BSD__
int       select _G_ARGS((int, void*, void*, void*, struct timeval*));

int       bind _G_ARGS((int, const void*, int));
int       connect _G_ARGS((int, struct sockaddr*, int));
#else
int       select _G_ARGS((int, struct fd_set*, struct fd_set*, struct fd_set*, struct timeval*));

int       bind _G_ARGS((int, const struct sockaddr *, int));
int       connect _G_ARGS((int, const struct sockaddr*, int));
#endif
int       getsockname _G_ARGS((int, struct sockaddr*, int*));
int       getpeername _G_ARGS((int, struct sockaddr*, int*));
int       getsockopt(int, int, int, void*, int*);
int       listen(int, int);
#ifndef hpux
int       rcmd _G_ARGS((char**, int, const char*, const char*, const char*, int*));
#endif
int       recv(int, void*, int, int);
int       recvmsg(int, struct msghdr*, int);
int       rexec(char**, int, const char*, const char*, const char*, int*);
int       rresvport(int*);
int       send _G_ARGS((int, const void*, int, int));
int       sendmsg _G_ARGS((int, const struct msghdr*, int));
int       shutdown(int, int);
int       socket(int, int, int);
int       socketpair(int, int, int, int sv[2]);

#ifndef __386BSD__
int       recvfrom _G_ARGS((int, void*, int, int, void*, int *));
int       sendto _G_ARGS((int, const void*, int, int, void*, int));
int       setsockopt _G_ARGS((int, int, int, const char*, int));
#else
int       recvfrom _G_ARGS((int, void*, int, int, struct sockaddr*, int *));
int       sendto _G_ARGS((int, const void*, int, int, const struct sockaddr*, int));
int       setsockopt _G_ARGS((int, int, int, const void*, int));
#endif
#endif
}

#endif
