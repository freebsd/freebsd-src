/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "bsd_locl.h"

RCSID("$Id: rcmd_util.c,v 1.15 1997/05/02 14:27:44 assar Exp $");

int
get_login_port(int kerberos, int encryption)
{
  char *service="login";
  int port=htons(513);

  if(kerberos && encryption){
    service="eklogin";
    port=htons(2105);
  }
  
  if(kerberos && !encryption){
    service="klogin";
    port=htons(543);
  }
  return k_getportbyname (service, "tcp", port);
}

int
get_shell_port(int kerberos, int encryption)
{
  char *service="shell";
  int port=htons(514);

  if(kerberos && encryption){
    service="ekshell";
    port=htons(545);
  }
  
  if(kerberos && !encryption){
    service="kshell";
    port=htons(544);
  }

  return k_getportbyname (service, "tcp", port);
}

/* 
 * On reasonable systems, `cf[gs]et[io]speed' use values of bit/s
 * directly, and the following functions are just identity functions.
 * This is however a slower way of doing those
 * should-be-but-are-not-always idenity functions.  
 */

static struct { int speed; int bps; } conv[] = {
#ifdef B0
    {B0, 0},
#endif
#ifdef B50
    {B50, 50},
#endif
#ifdef B75
    {B75, 75},
#endif
#ifdef B110
    {B110, 110},
#endif
#ifdef B134
    {B134, 134},
#endif
#ifdef B150
    {B150, 150},
#endif
#ifdef B200
    {B200, 200},
#endif
#ifdef B300
    {B300, 300},
#endif
#ifdef B600
    {B600, 600},
#endif
#ifdef B1200
    {B1200, 1200},
#endif
#ifdef B1800
    {B1800, 1800},
#endif
#ifdef B2400
    {B2400, 2400},
#endif
#ifdef B4800
    {B4800, 4800},
#endif
#ifdef B9600
    {B9600, 9600},
#endif
#ifdef B19200
    {B19200, 19200},
#endif
#ifdef B38400
    {B38400, 38400},
#endif
#ifdef B57600
    {B57600, 57600},
#endif
#ifdef B115200
    {B115200, 115200},
#endif
#ifdef B153600
    {B153600, 153600},
#endif
#ifdef B230400
    {B230400, 230400},
#endif
#ifdef B307200
    {B307200, 307200},
#endif
#ifdef B460800
    {B460800, 460800},
#endif
};

#define N (sizeof(conv)/sizeof(*conv))

int
speed_t2int (speed_t s)
{
  int l, r, m;

  l = 0;
  r = N - 1;
  while(l <= r) {
    m = (l + r) / 2;
    if (conv[m].speed == s)
      return conv[m].bps;
    else if(conv[m].speed < s)
      l = m + 1;
    else
      r = m - 1; 
  }
  return -1;
}

/*
 *
 */

speed_t
int2speed_t (int i)
{
  int l, r, m;

  l = 0;
  r = N - 1;
  while(l <= r) {
    m = (l + r) / 2;
    if (conv[m].bps == i)
      return conv[m].speed;
    else if(conv[m].bps < i)
      l = m + 1;
    else
      r = m - 1;
  }
  return -1;
}

/*
 * If there are any IP options on `sock', die.
 */

void
ip_options_and_die (int sock, struct sockaddr_in *fromp)
{
#if defined(IP_OPTIONS) && defined(HAVE_GETSOCKOPT)
  u_char optbuf[BUFSIZ/3], *cp;
  char lbuf[BUFSIZ], *lp;
  int optsize = sizeof(optbuf), ipproto;
  struct protoent *ip;

  if ((ip = getprotobyname("ip")) != NULL)
    ipproto = ip->p_proto;
  else
    ipproto = IPPROTO_IP;
  if (getsockopt(sock, ipproto, IP_OPTIONS,
		 (void *)optbuf, &optsize) == 0 &&
      optsize != 0) {
    lp = lbuf;
    for (cp = optbuf; optsize > 0; cp++, optsize--, lp += 3)
      snprintf(lp, sizeof(lbuf) - (lp - lbuf), " %2.2x", *cp);
    syslog(LOG_NOTICE,
	   "Connection received from %s using IP options (dead):%s",
	   inet_ntoa(fromp->sin_addr), lbuf);
    exit(1);
  }
#endif
}

void
warning(const char *fmt, ...)
{
    char *rstar_no_warn = getenv("RSTAR_NO_WARN");
    va_list args;

    va_start(args, fmt);
    if (rstar_no_warn == NULL)
	rstar_no_warn = "";
    if (strncmp(rstar_no_warn, "yes", 3) != 0) {
	/* XXX */
	fprintf(stderr, "%s: warning, using standard ", __progname);
	warnx(fmt, args);
    }
    va_end(args);
}
