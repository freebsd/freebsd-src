/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors
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

/* $Id: kip.h,v 1.18.2.1 2000/06/23 02:55:01 assar Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <sys/types.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#include <netdb.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <net/if.h>
#ifdef HAVE_NET_IF_VAR_H
#include <net/if_var.h>
#endif
#ifdef HAVE_NET_IF_TUN_H
#include <net/if_tun.h>
#endif
#include <err.h>

#include <getarg.h>

#ifdef SOCKS
#include <socks.h>
#endif

#include <krb.h>

#include <roken.h>

#define TUNDEV "tun"

#ifndef TUNMTU
#define TUNMTU 1500		/* everything is ethernet :) */
#endif

#define KIPPORT 2112

#define KIP_VERSION "KIPSRV.0"

int
copy_packets (int tundev, int netdev, int mtu, des_cblock *iv,
	      des_key_schedule schedule);

RETSIGTYPE childhandler (int);

extern sig_atomic_t disconnect;
extern int isserver;

int
tunnel_open (char *, size_t);

void
fatal (int fd, const char *s, des_key_schedule schedule, des_cblock *iv);

int
kip_exec (const char *cmd, char *msg, size_t len, ...);
