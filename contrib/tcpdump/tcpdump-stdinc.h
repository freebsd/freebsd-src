/*
 * Copyright (c) 2002 - 2003
 * NetGroup, Politecnico di Torino (Italy)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Politecnico di Torino nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * @(#) $Header: /tcpdump/master/tcpdump/tcpdump-stdinc.h,v 1.7.2.1 2003/11/16 09:57:50 guy Exp $ (LBL)
 */

/*
 * Include the appropriate OS header files on Windows and various flavors
 * of UNIX, and also define some additional items and include various
 * non-OS header files on Windows, and; this isolates most of the platform
 * differences to this one file.
 */

#ifndef tcpdump_stdinc_h
#define tcpdump_stdinc_h

#ifdef WIN32

#include <stdio.h>
#include <winsock2.h>
#include "bittypes.h"
#include <ctype.h>
#include <time.h>
#include <io.h>
#include "IP6_misc.h"
#include <fcntl.h>

#ifdef __MINGW32__
#include <stdint.h>
int* _errno();
#define errno (*_errno())

#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

#endif /* __MINGW32__ */

#ifndef toascii
#define toascii(c) ((c) & 0x7f)
#endif

#ifndef caddr_t
typedef char* caddr_t;
#endif /* caddr_t */

#define MAXHOSTNAMELEN	64
#define	NI_MAXHOST	1025
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define RETSIGTYPE void

#if !defined(__MINGW32__) && !defined(__WATCOMC__)
#undef toascii
#define isascii __isascii
#define toascii __toascii
#define stat _stat
#define open _open
#define fstat _fstat
#define read _read
#define O_RDONLY _O_RDONLY

typedef short ino_t;
#endif /* __MINGW32__ */

#else /* WIN32 */

#include <ctype.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/types.h>			/* concession to AIX */
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif

#include <arpa/inet.h>

#endif /* WIN32 */

#ifdef INET6
#include "ip6.h"
#endif

#if defined(WIN32) || defined(MSDOS)
  #define FOPEN_READ_TXT   "rt"
  #define FOPEN_READ_BIN   "rb"
  #define FOPEN_WRITE_TXT  "wt"
  #define FOPEN_WRITE_BIN  "wb"
#else
  #define FOPEN_READ_TXT   "r"
  #define FOPEN_READ_BIN   FOPEN_READ_TXT
  #define FOPEN_WRITE_TXT  "w"
  #define FOPEN_WRITE_BIN  FOPEN_WRITE_TXT
#endif

#endif /* tcpdump_stdinc_h */
