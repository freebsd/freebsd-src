/*
  This software is available to you under a choice of one of two
  licenses.  You may choose to be licensed under the terms of the GNU
  General Public License (GPL) Version 2, available at
  <http://www.fsf.org/copyleft/gpl.html>, or the OpenIB.org BSD
  license, available in the LICENSE.TXT file accompanying this
  software.  These details are also available at
  <http://openib.org/license.html>.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  Copyright (c) 2004 Topspin Communications.  All rights reserved.
  Copyright (c) 2005 Mellanox Technologies Ltd.  All rights reserved.

  $Id$
*/

/*
 * system includes
 */
#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

/*
  For non-i386 systems, we pass socket() through to library code using
  dlsym() instead of trying to make the system call directly.  This
  may cause problems if this library is LD_PRELOADed before the real C
  library is available.  Eventually we may want to add the same type
  of system call assembly code as i386 has for IA64 and AMD64, but for
  now....
*/
#ifndef i386
#define _GNU_SOURCE /* Get RTLD_NEXT */
#include <dlfcn.h>
#else
#include <sys/syscall.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
/*
 * SDP specific includes
 */
#include "linux/sdp_inet.h"

#if 0
#define _SDP_VERBOSE_PRELOAD
#endif

#define SOCKOP_socket           1
#define SOCKOP_bind             2
#define SOCKOP_connect          3
#define SOCKOP_listen           4
#define SOCKOP_accept           5
#define SOCKOP_getsockname      6
#define SOCKOP_getpeername      7
#define SOCKOP_socketpair       8
#define SOCKOP_send             9
#define SOCKOP_recv             10
#define SOCKOP_sendto           11
#define SOCKOP_recvfrom         12
#define SOCKOP_shutdown         13
#define SOCKOP_setsockopt       14
#define SOCKOP_getsockopt       15
#define SOCKOP_sendmsg          16
#define SOCKOP_recvmsg          17

extern char * program_invocation_name;
extern char * program_invocation_short_name;
extern char ** const environ;

/* ========================================================================= */
/*..socket -- replacment socket call. */
int socket
(
 int domain,
 int type,
 int protocol
)
{
#ifdef i386
  long  __ret;
  void *__scratch;
  int   call[3];
#endif
  char *test;
  char *inet;
  char **tenviron;

#ifdef _SDP_VERBOSE_PRELOAD
  FILE *fd;
#endif
  /*
   * check for magic enviroment variable
   */
  if ((AF_INET == domain || AF_INET6 == domain) &&
      SOCK_STREAM == type) {

    if (environ) {
      tenviron = environ;
      for (test = *tenviron; NULL != test; test = *++tenviron) {

        inet = AF_INET_STR;

        while (*inet == *test && '\0' != *inet) {

          inet++;
	  test++;
        } /* while */

        if ('\0' == *inet && '=' == *test) {

          domain = AF_INET_SDP;
          break;
        } /* if */
      } /* for */
    } /* if */
  } /* if */

#ifdef _SDP_VERBOSE_PRELOAD
  fd = fopen("/tmp/libsdp.log.txt", "a+");

  fprintf(fd, "SOCKET: <%s> domain <%d> type <%d> protocol <%d>\n",
	  program_invocation_short_name, domain, type, protocol);

  fclose(fd);
#endif

#ifdef i386
  /* Make the socket() system call directly, as described above */
  call[0] = domain;
  call[1] = type;
  call[2] = protocol;

  __asm__ __volatile__("movl %%ebx, %1\n" /* save %ebx */
                       "movl %3, %%ebx\n" /* put sockopt in %ebx as arg */
                       "int $0x80\n"      /* do syscall */
                       "movl %1, %%ebx\n" /* restore %ebx */
                       : "=a" (__ret), "=r" (__scratch)
                       : "0" (__NR_socketcall),
		       "g" (SOCKOP_socket),
		       "c" (call));
  return __ret;
#else /* i386 */
  /* Use the standard library socket() to pass through the call */
  {
    static int (*orig_socket)(int, int, int);

    if (!orig_socket) {
      orig_socket = dlsym(RTLD_NEXT, "socket");
    }

    return orig_socket(domain, type, protocol);
  }
#endif /* i386 */
} /* socket */
