/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)extern.h	8.1 (Berkeley) 6/6/93
 */

struct hosttbl;
struct netinfo;
struct sockaddr_in;
struct timeval;
struct tsp;

struct hosttbl *addmach __P((char *, struct sockaddr_in *, struct netinfo *));
struct hosttbl *findhost __P((char *));
struct hosttbl *remmach __P((struct hosttbl *));

struct tsp *readmsg __P((int,
	    char *, struct timeval *, struct netinfo *));
struct tsp *acksend __P((struct tsp *,
	    struct sockaddr_in *, char *, int, struct netinfo *, int));

void	 addnetname __P((char *));
void	 adj_msg_time __P((struct tsp *, struct timeval *));
void	 bytehostorder __P((struct tsp *));
void	 bytenetorder __P((struct tsp *));
void	 byteorder __P((struct tsp *));
long	 casual __P((long, long));
int	 cksum __P((u_short *, int));
void	 correct __P((long));
char	*date __P((void));
void	 doquit __P((struct tsp *));
int	 election __P((struct netinfo *));
void	 get_goodgroup __P((int));
int	 good_host_name __P((char *));
void	 ignoreack __P((void));
int	 in_cksum __P((u_short *, int));
void	 lookformaster __P((struct netinfo *));
void	 makeslave __P((struct netinfo *));
int	 master __P((void));
void	 masterack __P((void));
void	 masterup __P((struct netinfo *));
int	 measure __P((u_long, u_long, char *, struct sockaddr_in *, int));
void	 msterup __P((struct netinfo *));
void	 mstotvround __P((struct timeval *, long));
long	 networkdelta __P((void));
void	 newslave __P((struct tsp *));
void	 print __P((struct tsp *, struct sockaddr_in *));
void	 prthp __P((clock_t));
void	 rmnetmachs __P((struct netinfo *));
void	 setstatus __P((void));
int	 slave __P((void));
void	 slaveack __P((void));
void	 spreadtime __P((void));
void	 suppress __P((struct sockaddr_in *, char *, struct netinfo *));
void	 synch __P((long));
void	 timevaladd __P((struct timeval *, struct timeval *));
void	 timevalsub __P((struct timeval *, struct timeval *, struct timeval *));
void	 traceoff __P((char *));
void	 traceon __P((void));
void	 xmit __P((int, u_int, struct sockaddr_in *));
