/*
 * modified by Jun-ichiro itojun Itoh <itojun@itojun.org>, 1997
 */
/*
 * in6_debug.h  --  Insipired by Craig Metz's Net/2 in6_debug.h, but
 *                  not quite as heavyweight (initially, anyway).
 *
 *                  In particular, if function exit-entries are to be
 *                  documented, do them in a lightweight fashion.
 *
 * Copyright 1995 by Dan McDonald, Bao Phan, and Randall Atkinson,
 *	All Rights Reserved.  
 *      All Rights under this copyright have been assigned to NRL.
 */

/*----------------------------------------------------------------------
#       @(#)COPYRIGHT   1.1a (NRL) 17 August 1995

COPYRIGHT NOTICE

All of the documentation and software included in this software
distribution from the US Naval Research Laboratory (NRL) are
copyrighted by their respective developers.

This software and documentation were developed at NRL by various
people.  Those developers have each copyrighted the portions that they
developed at NRL and have assigned All Rights for those portions to
NRL.  Outside the USA, NRL also has copyright on the software
developed at NRL. The affected files all contain specific copyright
notices and those notices must be retained in any derived work.

NRL LICENSE

NRL grants permission for redistribution and use in source and binary
forms, with or without modification, of the software and documentation
created at NRL provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. All advertising materials mentioning features or use of this software
   must display the following acknowledgement:

        This product includes software developed at the Information
        Technology Division, US Naval Research Laboratory.

4. Neither the name of the NRL nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation
are those of the authors and should not be interpreted as representing
official policies, either expressed or implied, of the US Naval
Research Laboratory (NRL).

----------------------------------------------------------------------*/

#ifdef KERNEL

/* IDL_* is IPv6 Debug Level */

#define IDL_ALL 0xFFFFFFFE  /* Report all messages. */
#define IDL_NONE 0          /* Report no messages.  */

#define IDL_CRITICAL 3
#define IDL_ERROR 7
#define IDL_MAJOR_EVENT 10
#define IDL_EVENT 15
#define IDL_GROSS_EVENT 20
#define IDL_FINISHED 0xFFFFFFF0

/*
 * Make sure argument for DPRINTF is in parentheses.
 *
 * For both DPRINTF and DDO, and attempt was made to make both macros
 * be usable as normal C statments.  There is a small amount of compiler
 * trickery (if-else clauses with effectively null statements), which may
 * cause a few compilers to complain.
 */

#ifdef KEY_DEBUG

/*
 * DPRINTF() is a general printf statement.  The "arg" is literally what
 * would follow the function name printf, which means it has to be in
 * parenthesis.  Unlimited arguments can be used this way.
 *
 * EXAMPLE:
 *        DPRINTF(IDL_MAJOR_EVENT,("Hello, world.  IP version %d.\n",vers));
 */
#define DPRINTF(lev,arg) if ((lev) < in6_debug_level) { \
						      printf arg; \
						      } \
                         else in6_debug_level = in6_debug_level

/*
 * DDO() executes a series of statements at a certain debug level.  The
 * "stmt" argument is a statement in the sense of a "statement list" in a
 * C grammar.  "stmt" does not have to end with a semicolon.
 *
 * EXAMPLE:
 *        DDO(IDL_CRITICAL,dump_ipv6(header), dump_inpcb(inp));
 */
#define DDO(lev,stmt) if ((lev) < in6_debug_level) { stmt ; } \
                       else in6_debug_level = in6_debug_level

/*
 * DP() is a shortcut for DPRINTF().  Basically:
 *
 *        DP(lev, var, fmt)   ==   DPRINTF(IDL_lev, ("var = %fmt\n", var))
 *
 * It is handy for printing single variables without a lot of typing.
 *
 * EXAMPLE:
 *
 *        DP(CRITICAL,length,d);
 * same as DPRINTF(IDL_CRITICAL, ("length = %d\n", length))
 */
#define DP(lev, var, fmt) DPRINTF(IDL_ ## lev, (#var " = %" #fmt "\n", var))

struct inpcb;

extern void in6_debug_init __P((void));
#ifdef INET6
extern void dump_in6_addr __P((struct in6_addr *));
#endif
extern void dump_in_addr __P((struct in_addr *));
#ifdef INET6
extern void dump_sockaddr_in6 __P((struct sockaddr_in6 *));
#endif
extern void dump_sockaddr_in __P((struct sockaddr_in *));
extern void dump_sockaddr __P((struct sockaddr *));
extern void dump_sockaddr_dl __P((struct sockaddr_dl *));
extern void dump_smart_sockaddr __P((struct sockaddr *));
#ifdef INET6
extern void dump_ipv6 __P((struct ip6 *));
extern void dump_ipv6_icmp __P((struct icmp6 *));
#endif /*INET6*/
extern void dump_mbuf_hdr __P((struct mbuf *));
extern void dump_mbuf __P((struct mbuf *));
extern void dump_mchain __P((struct mbuf *));
extern void dump_tcpdump __P((struct mbuf *));
extern void dump_ifa __P((struct ifaddr *));
extern void dump_ifp __P((struct ifnet *));
extern void dump_route __P((struct route *));
extern void dump_rtentry __P((struct rtentry *));
extern void dump_inpcb __P((struct inpcb *));
#ifdef INET6
extern void dump_in6pcb __P((struct in6pcb *));
#endif
extern void dump_buf __P((char *, int));
extern void dump_keytblnode __P((struct key_tblnode *));
extern void dump_secassoc __P((struct key_secassoc *));
extern void dump_keymsghdr __P((struct key_msghdr *));
extern void dump_keymsginfo __P((struct key_msgdata *));

#else   /* ! KEY_DEBUG */

#define DPRINTF(lev,arg) 
#define DDO(lev, stmt) 
#define DP(x, y, z) 

#endif  /* KEY_DEBUG */

#ifndef INET6_DEBUG_C
extern unsigned int in6_debug_level;
#endif

#endif /*KERNEL*/
