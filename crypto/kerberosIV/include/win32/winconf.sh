#!/bin/sh

# $Id: winconf.sh,v 1.1 1997/11/09 14:35:15 joda Exp $

cat ../config.h.in | sed '
s%#undef gid_t$%#define gid_t int%
s%#undef STDC_HEADERS$%#define STDC_HEADERS 1%
s%#undef uid_t$%#define uid_t int%
s%#undef ssize_t$%#define ssize_t int%
s%#undef VOID_RETSIGTYPE$%#define VOID_RETSIGTYPE 1%
s%#undef HAVE_H_ERRNO$%#define HAVE_H_ERRNO 1%
s%#undef HAVE_H_ERRNO_DECLARATION$%#define HAVE_H_ERRNO_DECLARATION 1%
s%#undef HAVE__STRICMP$%#define HAVE__STRICMP 1%
s%#undef HAVE_GETHOSTBYNAME$%#define HAVE_GETHOSTBYNAME 1%
s%#undef HAVE_GETHOSTNAME$%#define HAVE_GETHOSTNAME 1%
s%#undef HAVE_GETSERVBYNAME$%#define HAVE_GETSERVBYNAME 1%
s%#undef HAVE_GETSOCKOPT$%#define HAVE_GETSOCKOPT 1%
s%#undef HAVE_MEMMOVE$%#define HAVE_MEMMOVE 1%
s%#undef HAVE_MKTIME$%#define HAVE_MKTIME 1%
s%#undef HAVE_RAND$%#define HAVE_RAND 1%
s%#undef HAVE_SETSOCKOPT$%#define HAVE_SETSOCKOPT 1%
s%#undef HAVE_SOCKET$%#define HAVE_SOCKET 1%
s%#undef HAVE_STRDUP$%#define HAVE_STRDUP 1%
s%#undef HAVE_STRFTIME$%#define HAVE_STRFTIME 1%
s%#undef HAVE_STRLWR$%#define HAVE_STRLWR 1%
s%#undef HAVE_STRUPR$%#define HAVE_STRUPR 1%
s%#undef HAVE_SWAB$%#define HAVE_SWAB 1%
s%#undef HAVE_FCNTL_H$%#define HAVE_FCNTL_H 1%
s%#undef HAVE_IO_H$%#define HAVE_IO_H 1%
s%#undef HAVE_SIGNAL_H$%#define HAVE_SIGNAL_H 1%
s%#undef HAVE_SYS_LOCKING_H$%#define HAVE_SYS_LOCKING_H 1%
s%#undef HAVE_SYS_STAT_H$%#define HAVE_SYS_STAT_H 1%
s%#undef HAVE_SYS_TIMEB_H$%#define HAVE_SYS_TIMEB_H 1%
s%#undef HAVE_SYS_TYPES_H$%#define HAVE_SYS_TYPES_H 1%
s%#undef HAVE_WINSOCK_H$%#define HAVE_WINSOCK_H 1%
s%#undef KRB4$%#define KRB4 1%
s%#undef DES_QUAD_DEFAULT$%#define DES_QUAD_DEFAULT DES_QUAD_GUESS%' > config.h
