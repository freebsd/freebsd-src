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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: mini_inetd.c,v 1.10 1997/05/02 14:30:07 assar Exp $");
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <roken.h>

void
mini_inetd (int port)
{
     struct sockaddr_in sa;
     int s = socket(AF_INET, SOCK_STREAM, 0);
     int s2;
     int one = 1;
     if(s < 0){
	  perror("socket");
	  exit(1);
     }
#if defined(SO_REUSEADDR) && defined(HAVE_SETSOCKOPT)
     if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void *)&one,
		   sizeof(one)) < 0){
	  perror("setsockopt");
	  exit(1);
     }
#endif
     memset(&sa, 0, sizeof(sa));
     sa.sin_family = AF_INET;
     sa.sin_port = port;
     sa.sin_addr.s_addr = INADDR_ANY;
     if(bind(s, (struct sockaddr*)&sa, sizeof(sa)) < 0){
	  perror("bind");
	  exit(1);
     }
     if(listen(s, SOMAXCONN) < 0){
	  perror("listen");
	  exit(1);
     }
     s2 = accept(s, NULL, 0);
     if(s2 < 0){
	  perror("accept");
	  exit(1);
     }
     close(s);
     dup2(s2, STDIN_FILENO);
     dup2(s2, STDOUT_FILENO);
     /* dup2(s2, STDERR_FILENO); */
     close(s2);
}
