/*----------------------------------------------------------------------
 * key.h :     Declarations and Definitions for Key Engine for BSD.
 *
 * Copyright 1995 by Bao Phan, Randall Atkinson, & Dan McDonald,
 * All Rights Reserved.  All rights have been assigned to the US
 * Naval Research Laboratory (NRL).  The NRL Copyright Notice and
 * License Agreement governs distribution and use of this software.
 *
 * Patents are pending on this technology.  NRL grants a license
 * to use this technology at no cost under the terms below with
 * the additional requirement that software, hardware, and
 * documentation relating to use of this technology must include
 * the note that:
 *    	This product includes technology developed at and
 *      licensed from the Information Technology Division,
 *	US Naval Research Laboratory.
 *
 ----------------------------------------------------------------------*/
/*----------------------------------------------------------------------
#	@(#)COPYRIGHT	1.1a (NRL) 17 August 1995

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

#ifndef _netkey_key_h
#define	_netkey_key_h	1

/*
 * PF_KEY messages
 */

#define KEY_ADD          1
#define KEY_DELETE       2
#define KEY_UPDATE       3
#define KEY_GET          4
#define KEY_ACQUIRE      5
#define KEY_GETSPI       6
#define KEY_REGISTER     7
#define KEY_EXPIRE       8
#define KEY_DUMP         9
#define KEY_FLUSH        10

#define KEY_VERSION      1
#define POLICY_VERSION   1

#define SECURITY_TYPE_NONE		0

#define KEY_TYPE_AH           1
#define KEY_TYPE_ESP          2
#define KEY_TYPE_RSVP         3
#define KEY_TYPE_OSPF         4
#define KEY_TYPE_RIPV2        5
#define KEY_TYPE_MIPV4        6
#define KEY_TYPE_MIPV6        7
#define KEY_TYPE_MAX          7

/*
 * Security association state
 */

#define K_USED           0x1	/* Key used/not used */
#define K_UNIQUE         0x2	/* Key unique/reusable */
#define K_LARVAL         0x4	/* SPI assigned, but sa incomplete */
#define K_ZOMBIE         0x8	/* sa expired but still useable */
#define K_DEAD           0x10	/* sa marked for deletion, ready for reaping */
#define K_INBOUND        0x20	/* sa for inbound packets, ie. dst=myhost */
#define K_OUTBOUND       0x40	/* sa for outbound packets, ie. src=myhost */


#ifndef MAX_SOCKADDR_SZ
#ifdef INET6
#define MAX_SOCKADDR_SZ (sizeof(struct sockaddr_in6))
#else /* INET6 */
#define MAX_SOCKADDR_SZ (sizeof(struct sockaddr_in))
#endif /* INET6 */
#endif /* MAX_SOCKADDR_SZ */

#ifndef MAX_KEY_SZ
#define MAX_KEY_SZ 16
#endif /* MAX_KEY_SZ */

#ifndef MAX_IV_SZ
#define MAX_IV_SZ 16
#endif /* MAX_IV_SZ */

/* Security association data for IP Security */
struct key_secassoc {
	u_int8_t        len;	/* Length of the data (for radix) */
	u_int8_t        type;	/* Type of association */
	u_int8_t        state;	/* State of the association */
	u_int8_t        label;	/* Sensitivity label (unused) */
	u_int32_t       spi;	/* SPI */
	u_int8_t        keylen;	/* Key length */
	u_int8_t        ivlen;	/* Initialization vector length */
	u_int8_t        algorithm;	/* Algorithm switch index */
	u_int8_t        lifetype;	/* Type of lifetime */
	caddr_t         iv;	/* Initialization vector */
	caddr_t         key;	/* Key */
	u_int32_t       lifetime1;	/* Lifetime value 1 */
	u_int32_t       lifetime2;	/* Lifetime value 2 */
	struct sockaddr *src;	/* Source host address */
	struct sockaddr *dst;	/* Destination host address */
	struct sockaddr *from;	/* Originator of association */
};

/*
 * Structure for key message header. PF_KEY message consists of key_msghdr
 * followed by src struct sockaddr, dest struct sockaddr, from struct
 * sockaddr, key, and iv. Assumes size of key message header less than MHLEN.
 */

struct key_msghdr {
	u_short         key_msglen;	/* length of message including
					 * src/dst/from/key/iv */
	u_char          key_msgvers;	/* key version number */
	u_char          key_msgtype;	/* key message type, eg. KEY_ADD */
	pid_t           key_pid;/* process id of message sender */
	int             key_seq;/* message sequence number */
	int             key_errno;	/* error code */
	u_int8_t        type;	/* type of security association */
	u_int8_t        state;	/* state of security association */
	u_int8_t        label;	/* sensitivity level */
	u_int8_t        pad;	/* padding for allignment */
	u_int32_t       spi;	/* spi value */
	u_int8_t        keylen;	/* key length */
	u_int8_t        ivlen;	/* iv length */
	u_int8_t        algorithm;	/* algorithm identifier */
	u_int8_t        lifetype;	/* type of lifetime */
	u_int32_t       lifetime1;	/* lifetime value 1 */
	u_int32_t       lifetime2;	/* lifetime value 2 */
};

struct key_msgdata {
	struct sockaddr *src;	/* source host address */
	struct sockaddr *dst;	/* destination host address */
	struct sockaddr *from;	/* originator of security association */
	caddr_t         iv;	/* initialization vector */
	caddr_t         key;	/* key */
	int             ivlen;	/* key length */
	int             keylen;	/* iv length */
};

struct policy_msghdr {
	u_short         policy_msglen;	/* message length */
	u_char          policy_msgvers;	/* message version */
	u_char          policy_msgtype;	/* message type */
	int             policy_seq;	/* message sequence number */
	int             policy_errno;	/* error code */
};

/*
 * Key engine table structures
 */

struct socketlist {
	struct socket  *socket;	/* pointer to socket */
	struct socketlist *next;/* next */
};

struct key_tblnode {
	int             alloc_count;	/* number of sockets allocated to
					 * secassoc */
	int             ref_count;	/* number of sockets referencing
					 * secassoc */
	struct socketlist *solist;	/* list of sockets allocated to
					 * secassoc */
	struct key_secassoc *secassoc;	/* security association */
	struct key_tblnode *next;	/* next node */
};

struct key_allocnode {
	struct key_tblnode *keynode;
	struct key_allocnode *next;
};

struct key_so2spinode {
	struct socket  *socket;	/* socket pointer */
	struct key_tblnode *keynode;	/* pointer to tblnode containing
					 * secassoc */
	/* info for socket  */
	struct key_so2spinode *next;
};

struct key_registry {
	u_int8_t        type;	/* secassoc type that key mgnt. daemon can
				 * acquire */
	struct socket  *socket;	/* key management daemon socket pointer */
	struct key_registry *next;
};

struct key_acquirelist {
	u_int8_t        type;	/* secassoc type to acquire */
	struct sockaddr *target;/* destination address of secassoc */
	u_int32_t       count;	/* number of acquire messages sent */
	u_long          expiretime;	/* expiration time for acquire
					 * message */
	struct key_acquirelist *next;
};

struct keyso_cb {
	int             ip4_count;	/* IPv4 */
#ifdef INET6
	int             ip6_count;	/* IPv6 */
#endif				/* INET6 */
	int             any_count;	/* Sum of above counters */
};

#ifdef KERNEL
int key_inittables __P((void));
int key_secassoc2msghdr __P((struct key_secassoc *, struct key_msghdr *,
			     struct key_msgdata *));
int key_msghdr2secassoc __P((struct key_secassoc *, struct key_msghdr *,
			     struct key_msgdata *));
int key_add     __P((struct key_secassoc *));
int key_delete  __P((struct key_secassoc *));
int key_get     __P((u_int, struct sockaddr *, struct sockaddr *, u_int32_t,
		     struct key_secassoc **));
void key_flush  __P((void));
int key_dump    __P((struct socket *));
int key_getspi  __P((u_int, struct sockaddr *, struct sockaddr *, u_int32_t, 
		     u_int32_t, u_int32_t *));
int key_update  __P((struct key_secassoc *));
int key_register __P((struct socket *, u_int));
void key_unregister __P((struct socket *, u_int, int));
int key_acquire __P((u_int, struct sockaddr *, struct sockaddr *));
int getassocbyspi __P((u_int, struct sockaddr *, struct sockaddr *, u_int32_t,
		       struct key_tblnode **));
int getassocbysocket __P((u_int, struct sockaddr *, struct sockaddr *, 
			  struct socket *, u_int, struct key_tblnode **));
void key_free   __P((struct key_tblnode *));
int key_parse   __P((struct key_msghdr ** km, struct socket * so, int *));
#endif /* KERNEL */

#endif /* _netkey_key_h */
