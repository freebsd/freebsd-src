/*
 * modified by Jun-ichiro itojun Itoh <itojun@itojun.org>, 1997
 */
/*----------------------------------------------------------------------
  key.c :         Key Management Engine for BSD

  Copyright 1995 by Bao Phan,  Randall Atkinson, & Dan McDonald,
  All Rights Reserved.  All Rights have been assigned to the US
  Naval Research Laboratory (NRL).  The NRL Copyright Notice and
  License governs distribution and use of this software.

  Patents are pending on this technology.  NRL grants a license
  to use this technology at no cost under the terms below with
  the additional requirement that software, hardware, and 
  documentation relating to use of this technology must include
  the note that:
     	This product includes technology developed at and
	licensed from the Information Technology Division, 
	US Naval Research Laboratory.

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

#include "opt_key.h"

#ifdef KEY

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/domain.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/raw_cb.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>

#ifdef INET6
#include <netinet6/in6.h>
#include <netinet6/in6_var.h>
#endif /* INET6 */

#include <netkey/key.h>
#include <netkey/key_debug.h>

static MALLOC_DEFINE(M_SECA, "key mgmt", "security associations, key management");

#define KMALLOC(p, t, n) (p = (t) malloc((unsigned long)(n), M_SECA, M_DONTWAIT))
#define KFREE(p) free((caddr_t)p, M_SECA);

#define CRITICAL_DCL int critical_s;
#define CRITICAL_START critical_s = splnet()
#define CRITICAL_END splx(critical_s)

#ifdef INET6
#define MAXHASHKEYLEN (2 * sizeof(int) + 2 * sizeof(struct sockaddr_in6))
#else
#define MAXHASHKEYLEN (2 * sizeof(int) + 2 * sizeof(struct sockaddr_in))
#endif

/*
 *  Not clear whether these values should be 
 *  tweakable at kernel config time.
 */
#define KEYTBLSIZE 61
#define KEYALLOCTBLSIZE 61
#define SO2SPITBLSIZE 61

/*
 *  These values should be tweakable...
 *  perhaps by using sysctl
 */

#define MAXLARVALTIME 240;   /* Lifetime of a larval key table entry */ 
#define MAXKEYACQUIRE 1;     /* Max number of key acquire messages sent */
                             /*   per destination address               */
#define MAXACQUIRETIME 15;   /* Lifetime of acquire message */

/*
 *  Key engine tables and global variables
 */

struct key_tblnode keytable[KEYTBLSIZE];
struct key_allocnode keyalloctbl[KEYALLOCTBLSIZE];
struct key_so2spinode so2spitbl[SO2SPITBLSIZE];

struct keyso_cb keyso_cb;
struct key_tblnode nullkeynode;
struct key_registry *keyregtable;
struct key_acquirelist *key_acquirelist;
u_long maxlarvallifetime = MAXLARVALTIME;
int maxkeyacquire = MAXKEYACQUIRE;
u_long maxacquiretime = MAXACQUIRETIME;

extern struct sockaddr key_addr;

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) \
	{ x += ROUNDUP(n); }

static int addrpart_equal __P((struct sockaddr *, struct sockaddr *));
static int key_freetables __P((void));
static int key_gethashval __P((char *, int, int));
static int key_createkey __P((char *, u_int, struct sockaddr *,
	struct sockaddr *, u_int32_t, u_int));
static struct key_so2spinode *key_sosearch __P((u_int, struct sockaddr *,
	struct sockaddr *, struct socket *));
static void key_deleteacquire __P((u_int, struct sockaddr *));
static struct key_tblnode *key_search __P((u_int, struct sockaddr *,
	struct sockaddr *, u_int32_t, int, struct key_tblnode **));
static struct key_tblnode *key_addnode __P((int, struct key_secassoc *));
static int key_alloc __P((u_int, struct sockaddr *, struct sockaddr *,
	struct socket *, u_int, struct key_tblnode **));
static int key_xdata __P((struct key_msghdr *, struct key_msgdata *, int));
static int key_sendup __P((struct socket *, struct key_msghdr *));
static void key_init __P((void));
static int my_addr __P((struct sockaddr *));
static int key_output __P((struct mbuf *, struct socket *));
static int key_attach __P((struct socket *, int, struct proc *));
static int key_detach __P((struct socket *));
static void key_cbinit __P((void));

/*----------------------------------------------------------------------
 * key_secassoc2msghdr(): 
 *      Copy info from a security association into a key message buffer.
 *      Assume message buffer is sufficiently large to hold all security
 *      association information including src, dst, from, key and iv.
 ----------------------------------------------------------------------*/
int
key_secassoc2msghdr(secassoc, km, keyinfo)
  struct key_secassoc *secassoc;
  struct key_msghdr *km;
  struct key_msgdata *keyinfo;
{
  char *cp;
  DPRINTF(IDL_FINISHED, ("Entering key_secassoc2msghdr\n"));

  if ((km == 0) || (keyinfo == 0) || (secassoc == 0))
    return(-1);

  km->type = secassoc->type;
  km->vers = secassoc->vers;
  km->state = secassoc->state;
  km->label = secassoc->label;
  km->spi = secassoc->spi;
  km->keylen = secassoc->keylen;
  km->ekeylen = secassoc->ekeylen;
  km->ivlen = secassoc->ivlen;
  km->algorithm = secassoc->algorithm;
  km->lifetype = secassoc->lifetype;
  km->lifetime1 = secassoc->lifetime1;
  km->lifetime2 = secassoc->lifetime2;
  km->antireplay = secassoc->antireplay;

  /*
   *  Stuff src/dst/from/key/iv/ekey in buffer after
   *  the message header.
   */
  cp = (char *)(km + 1);

  DPRINTF(IDL_FINISHED, ("sa2msghdr: 1\n"));
  keyinfo->src = (struct sockaddr *)cp;
  if (secassoc->src->sa_len) {
    bcopy(secassoc->src, cp, secassoc->src->sa_len);
    ADVANCE(cp, secassoc->src->sa_len);
  } else {
    bzero(cp, MAX_SOCKADDR_SZ);
    ADVANCE(cp, MAX_SOCKADDR_SZ);
  }

  DPRINTF(IDL_FINISHED, ("sa2msghdr: 2\n"));
  keyinfo->dst = (struct sockaddr *)cp;
  if (secassoc->dst->sa_len) {
    bcopy(secassoc->dst, cp, secassoc->dst->sa_len);
    ADVANCE(cp, secassoc->dst->sa_len);
  } else {
    bzero(cp, MAX_SOCKADDR_SZ);
    ADVANCE(cp, MAX_SOCKADDR_SZ);
  }

  DPRINTF(IDL_FINISHED, ("sa2msghdr: 3\n"));
  keyinfo->from = (struct sockaddr *)cp;
  if (secassoc->from->sa_len) {
    bcopy(secassoc->from, cp, secassoc->from->sa_len);
    ADVANCE(cp, secassoc->from->sa_len);
  } else {
    bzero(cp, MAX_SOCKADDR_SZ);
    ADVANCE(cp, MAX_SOCKADDR_SZ);
  }

  DPRINTF(IDL_FINISHED, ("sa2msghdr: 4\n"));

  keyinfo->key = cp;
  keyinfo->keylen = secassoc->keylen;
  if (secassoc->keylen) {
    bcopy((char *)(secassoc->key), cp, secassoc->keylen);
    ADVANCE(cp, secassoc->keylen);
  }

  DPRINTF(IDL_FINISHED, ("sa2msghdr: 5\n"));
  keyinfo->iv = cp;
  keyinfo->ivlen = secassoc->ivlen;
  if (secassoc->ivlen) {
    bcopy((char *)(secassoc->iv), cp, secassoc->ivlen);
    ADVANCE(cp, secassoc->ivlen);
  }

  DPRINTF(IDL_FINISHED, ("sa2msghdr: 6\n"));
  keyinfo->ekey = cp;
  keyinfo->ekeylen = secassoc->ekeylen;
  if (secassoc->ekeylen) {
    bcopy((char *)(secassoc->ekey), cp, secassoc->ekeylen);
    ADVANCE(cp, secassoc->ekeylen);
  }

  DDO(IDL_FINISHED,printf("msgbuf(len=%d):\n",(char *)cp - (char *)km));
  DDO(IDL_FINISHED,dump_buf((char *)km, (char *)cp - (char *)km));
  DPRINTF(IDL_FINISHED, ("sa2msghdr: 6\n"));
  return(0);
}


/*----------------------------------------------------------------------
 * key_msghdr2secassoc():
 *      Copy info from a key message buffer into a key_secassoc 
 *      structure
 ----------------------------------------------------------------------*/
int
key_msghdr2secassoc(secassoc, km, keyinfo)
  struct key_secassoc *secassoc;
  struct key_msghdr *km;
  struct key_msgdata *keyinfo;
{
  DPRINTF(IDL_FINISHED, ("Entering key_msghdr2secassoc\n"));

  if ((km == 0) || (keyinfo == 0) || (secassoc == 0))
    return(-1);

  secassoc->len = sizeof(*secassoc);
  secassoc->type = km->type;
  secassoc->vers = km->vers;
  secassoc->state = km->state;
  secassoc->label = km->label;
  secassoc->spi = km->spi;
  secassoc->keylen = km->keylen;
  secassoc->ekeylen = km->ekeylen;
  secassoc->ivlen = km->ivlen;
  secassoc->algorithm = km->algorithm;
  secassoc->lifetype = km->lifetype;
  secassoc->lifetime1 = km->lifetime1;
  secassoc->lifetime2 = km->lifetime2;
  secassoc->antireplay = km->antireplay;

  if (keyinfo->src) {
    KMALLOC(secassoc->src, struct sockaddr *, keyinfo->src->sa_len);
    if (!secassoc->src) {
      DPRINTF(IDL_ERROR,("msghdr2secassoc: can't allocate mem for src\n"));
      return(-1);
    }
    bcopy((char *)keyinfo->src, (char *)secassoc->src,
	  keyinfo->src->sa_len);
  } else
    secassoc->src = NULL;

  if (keyinfo->dst) {
    KMALLOC(secassoc->dst, struct sockaddr *, keyinfo->dst->sa_len);
    if (!secassoc->dst) {
      DPRINTF(IDL_ERROR,("msghdr2secassoc: can't allocate mem for dst\n"));
      return(-1);
    }
    bcopy((char *)keyinfo->dst, (char *)secassoc->dst,
	  keyinfo->dst->sa_len);
  } else
    secassoc->dst = NULL;

  if (keyinfo->from) {
    KMALLOC(secassoc->from, struct sockaddr *, keyinfo->from->sa_len);
    if (!secassoc->from) {
      DPRINTF(IDL_ERROR,("msghdr2secassoc: can't allocate mem for from\n"));
      return(-1);
    }
    bcopy((char *)keyinfo->from, (char *)secassoc->from,
	  keyinfo->from->sa_len);
  } else
    secassoc->from = NULL;

  /*
   *  Make copies of key and iv
   */
  if (secassoc->ivlen) {
    KMALLOC(secassoc->iv, caddr_t, secassoc->ivlen);
    if (secassoc->iv == 0) {
      DPRINTF(IDL_ERROR,("msghdr2secassoc: can't allocate mem for iv\n"));
      return(-1);
    }
    bcopy((char *)keyinfo->iv, (char *)secassoc->iv, secassoc->ivlen);
  } else
    secassoc->iv = NULL;
	     
  if (secassoc->keylen) {
    KMALLOC(secassoc->key, caddr_t, secassoc->keylen);
    if (secassoc->key == 0) {
      DPRINTF(IDL_ERROR,("msghdr2secassoc: can't allocate mem for key\n"));
      if (secassoc->iv)
	KFREE(secassoc->iv);
      return(-1);
    }
    bcopy((char *)keyinfo->key, (char *)secassoc->key, secassoc->keylen);
  } else
    secassoc->key = NULL;

  if (secassoc->ekeylen) {
    KMALLOC(secassoc->ekey, caddr_t, secassoc->ekeylen);
    if (secassoc->ekey == 0) {
      DPRINTF(IDL_ERROR,("msghdr2secassoc: can't allocate mem for ekey\n"));
      if (secassoc->iv)
	KFREE(secassoc->iv);
      if (secassoc->key)
	KFREE(secassoc->key);
      return(-1);
    }
    bcopy((char *)keyinfo->ekey, (char *)secassoc->ekey, secassoc->ekeylen);
  } else
    secassoc->ekey = NULL;

  return(0);
}


/*----------------------------------------------------------------------
 * addrpart_equal():
 *      Determine if the address portion of two sockaddrs are equal.
 *      Currently handles only AF_INET and AF_INET6 address families.
 ----------------------------------------------------------------------*/
static int
addrpart_equal(sa1, sa2)
  struct sockaddr *sa1;
  struct sockaddr *sa2;
{
  if ((sa1->sa_family != sa2->sa_family) ||
      (sa1->sa_len != sa2->sa_len))
    return 0;

  switch(sa1->sa_family) {
  case AF_INET:
    return (((struct sockaddr_in *)sa1)->sin_addr.s_addr == 
	    ((struct sockaddr_in *)sa2)->sin_addr.s_addr);
#ifdef INET6
  case AF_INET6:
    return (IN6_ARE_ADDR_EQUAL(&((struct sockaddr_in6 *)sa1)->sin6_addr, 
			       &((struct sockaddr_in6 *)sa2)->sin6_addr));
#endif /* INET6 */
  }
  return(0);
}

/*----------------------------------------------------------------------
 * key_inittables():
 *      Allocate space and initialize key engine tables
 ----------------------------------------------------------------------*/
int
key_inittables()
{
  int i;

  KMALLOC(keyregtable, struct key_registry *, sizeof(struct key_registry));
  if (!keyregtable)
    return -1;
  bzero((char *)keyregtable, sizeof(struct key_registry));
  KMALLOC(key_acquirelist, struct key_acquirelist *, 
	   sizeof(struct key_acquirelist));
  if (!key_acquirelist)
    return -1;
  bzero((char *)key_acquirelist, sizeof(struct key_acquirelist));
  for (i = 0; i < KEYTBLSIZE; i++) 
    bzero((char *)&keytable[i], sizeof(struct key_tblnode));
  for (i = 0; i < KEYALLOCTBLSIZE; i++)
    bzero((char *)&keyalloctbl[i], sizeof(struct key_allocnode));
  for (i = 0; i < SO2SPITBLSIZE; i++)
    bzero((char *)&so2spitbl[i], sizeof(struct key_so2spinode));

  return 0;
}

static int
key_freetables()
{
  KFREE(keyregtable);
  keyregtable = NULL;
  KFREE(key_acquirelist);
  key_acquirelist = NULL;
  return 0;
}

/*----------------------------------------------------------------------
 * key_gethashval():
 *      Determine keytable hash value.
 ----------------------------------------------------------------------*/
static int
key_gethashval(buf, len, tblsize)
  char *buf;
  int len;
  int tblsize;
{
  int i, j = 0;

  /* 
   * Todo: Use word size xor and check for alignment
   *       and zero pad if necessary.  Need to also pick 
   *       a good hash function and table size.
   */
  if (len <= 0) {
    DPRINTF(IDL_ERROR,("key_gethashval got bogus len!\n"));
    return(-1);
  }
  for(i = 0; i < len; i++) {
    j ^=  (u_int8_t)(*(buf + i));
  }
  return (j % tblsize);
}


/*----------------------------------------------------------------------
 * key_createkey():
 *      Create hash key for hash function
 *      key is: type+src+dst if keytype = 1
 *              type+src+dst+spi if keytype = 0
 *      Uses only the address portion of the src and dst sockaddrs to 
 *      form key.  Currently handles only AF_INET and AF_INET6 sockaddrs
 ----------------------------------------------------------------------*/
static int
key_createkey(buf, type, src, dst, spi, keytype)
  char *buf;
  u_int type;
  struct sockaddr *src;
  struct sockaddr *dst;
  u_int32_t spi;
  u_int keytype;
{
  char *cp, *p;

  DPRINTF(IDL_FINISHED,("Entering key_createkey\n"));

  if (!buf || !src || !dst)
    return(-1);

  cp = buf;
  bcopy((char *)&type, cp, sizeof(type));
  cp += sizeof(type);

#ifdef INET6
  /*
   * Assume only IPv4 and IPv6 addresses.
   */
#define ADDRPART(a) \
    ((a)->sa_family == AF_INET6) ? \
    (char *)&(((struct sockaddr_in6 *)(a))->sin6_addr) : \
    (char *)&(((struct sockaddr_in *)(a))->sin_addr)

#define ADDRSIZE(a) \
    ((a)->sa_family == AF_INET6) ? sizeof(struct in6_addr) : \
    sizeof(struct in_addr)  
#else /* INET6 */
#define ADDRPART(a) (char *)&(((struct sockaddr_in *)(a))->sin_addr)
#define ADDRSIZE(a) sizeof(struct in_addr)  
#endif /* INET6 */

  DPRINTF(IDL_FINISHED,("src addr:\n"));
  DDO(IDL_FINISHED,dump_smart_sockaddr(src));
  DPRINTF(IDL_FINISHED,("dst addr:\n"));
  DDO(IDL_FINISHED,dump_smart_sockaddr(dst)); 

  p = ADDRPART(src);
  bcopy(p, cp, ADDRSIZE(src));
  cp += ADDRSIZE(src);

  p = ADDRPART(dst);
  bcopy(p, cp, ADDRSIZE(dst));
  cp += ADDRSIZE(dst);

#undef ADDRPART
#undef ADDRSIZE

  if (keytype == 0) {
    bcopy((char *)&spi, cp, sizeof(spi));
    cp += sizeof(spi);
  }

  DPRINTF(IDL_FINISHED,("hash key:\n"));
  DDO(IDL_FINISHED, dump_buf(buf, cp - buf));
  return(cp - buf);
}


/*----------------------------------------------------------------------
 * key_sosearch():
 *      Search the so2spi table for the security association allocated to 
 *      the socket.  Returns pointer to a struct key_so2spinode which can
 *      be used to locate the security association entry in the keytable.
 ----------------------------------------------------------------------*/
static struct key_so2spinode *
key_sosearch(type, src, dst, so)
  u_int type;
  struct sockaddr *src;
  struct sockaddr *dst;
  struct socket *so;
{
  struct key_so2spinode *np = 0;

  if (!(src && dst)) {
    DPRINTF(IDL_ERROR,("key_sosearch: got null src or dst pointer!\n"));
    return(NULL);
  }

  for (np = so2spitbl[((u_int32_t)so) % SO2SPITBLSIZE].next; np; np = np->next) {
    if ((so == np->socket) && (type == np->keynode->secassoc->type)
	&& addrpart_equal(src, np->keynode->secassoc->src)
	&& addrpart_equal(dst, np->keynode->secassoc->dst))
      return(np);
  }  
  return(NULL);
}


/*----------------------------------------------------------------------
 * key_sodelete():
 *      Delete entries from the so2spi table.
 *        flag = 1  purge all entries
 *        flag = 0  delete entries with socket pointer matching socket  
 ----------------------------------------------------------------------*/
void
key_sodelete(socket, flag)
  struct socket *socket;
  int flag;
{
  struct key_so2spinode *prevnp, *np;
  CRITICAL_DCL

  CRITICAL_START;

  DPRINTF(IDL_EVENT,("Entering keysodelete w/so=0x%x flag=%d\n",
		     (unsigned int)socket,flag));

  if (flag) {
    int i;

    for (i = 0; i < SO2SPITBLSIZE; i++)
      for(np = so2spitbl[i].next; np; np = np->next) {
	KFREE(np);
      }
    CRITICAL_END;
    return;
  }

  prevnp = &so2spitbl[((u_int32_t)socket) % SO2SPITBLSIZE];
  for(np = prevnp->next; np; np = np->next) {
    if (np->socket == socket) {
      struct socketlist *socklp, *prevsocklp;

      (np->keynode->alloc_count)--;

      /* 
       * If this socket maps to a unique secassoc,
       * we go ahead and delete the secassoc, since it
       * can no longer be allocated or used by any other 
       * socket.
       */
      if (np->keynode->secassoc->state & K_UNIQUE) {
	if (key_delete(np->keynode->secassoc) != 0)
	  panic("key_sodelete");
	np = prevnp;
	continue;
      }

      /*
       * We traverse the socketlist and remove the entry
       * for this socket
       */
      DPRINTF(IDL_FINISHED,("keysodelete: deleting from socklist..."));
      prevsocklp = np->keynode->solist;
      for (socklp = prevsocklp->next; socklp; socklp = socklp->next) {
	if (socklp->socket == socket) {
	  prevsocklp->next = socklp->next;
	  KFREE(socklp);
	  break;
	}
	prevsocklp = socklp;
      }
      DPRINTF(IDL_FINISHED,("done\n"));
      prevnp->next = np->next;
      KFREE(np);
      np = prevnp;
    }
    prevnp = np;  
  }
  CRITICAL_END;
}


/*----------------------------------------------------------------------
 * key_deleteacquire():
 *      Delete an entry from the key_acquirelist
 ----------------------------------------------------------------------*/
static void
key_deleteacquire(type, target)
  u_int type;
  struct sockaddr *target;
{
  struct key_acquirelist *ap, *prev;

  prev = key_acquirelist;
  for(ap = key_acquirelist->next; ap; ap = ap->next) {
    if (addrpart_equal(target, (struct sockaddr *)&(ap->target)) &&
	(type == ap->type)) {
      DPRINTF(IDL_EVENT,("Deleting entry from acquire list!\n"));
      prev->next = ap->next;
      KFREE(ap);
      ap = prev;
    }
    prev = ap;
  }
}


/*----------------------------------------------------------------------
 * key_search():
 *      Search the key table for an entry with same type, src addr, dest
 *      addr, and spi.  Returns a pointer to struct key_tblnode if found
 *      else returns null.
 ----------------------------------------------------------------------*/
static struct key_tblnode *
key_search(type, src, dst, spi, indx, prevkeynode)
  u_int type;
  struct sockaddr *src;
  struct sockaddr *dst;
  u_int32_t spi;
  int indx;
  struct key_tblnode **prevkeynode;
{
  struct key_tblnode *keynode, *prevnode;

  if (indx > KEYTBLSIZE || indx < 0)
    return (NULL);
  if (!(&keytable[indx]))
    return (NULL);

#define sec_type keynode->secassoc->type
#define sec_spi keynode->secassoc->spi
#define sec_src keynode->secassoc->src
#define sec_dst keynode->secassoc->dst

  prevnode = &keytable[indx];
  for (keynode = keytable[indx].next; keynode; keynode = keynode->next) {
    if ((type == sec_type) && (spi == sec_spi) && 
	addrpart_equal(src, sec_src)
	&& addrpart_equal(dst, sec_dst))
      break;
    prevnode = keynode;
  }
  *prevkeynode = prevnode;
  return(keynode);
}


/*----------------------------------------------------------------------
 * key_addnode():
 *      Insert a key_tblnode entry into the key table.  Returns a pointer 
 *      to the newly created key_tblnode.
 ----------------------------------------------------------------------*/
static struct key_tblnode *
key_addnode(indx, secassoc)
  int indx;
  struct key_secassoc *secassoc;
{
  struct key_tblnode *keynode;

  DPRINTF(IDL_FINISHED,("Entering key_addnode w/indx=%d secassoc=0x%x\n",
			indx, (unsigned int)secassoc));

  if (!(&keytable[indx]))
    return(NULL);
  if (!secassoc) {
    panic("key_addnode: Someone passed in a null secassoc!\n");
  }

  KMALLOC(keynode, struct key_tblnode *, sizeof(struct key_tblnode));
  if (keynode == 0)
    return(NULL);
  bzero((char *)keynode, sizeof(struct key_tblnode));

  KMALLOC(keynode->solist, struct socketlist *, sizeof(struct socketlist));
  if (keynode->solist == 0) {
    KFREE(keynode);
    return(NULL);
  }
  bzero((char *)(keynode->solist), sizeof(struct socketlist));

  keynode->secassoc = secassoc;
  keynode->solist->next = NULL;
  keynode->next = keytable[indx].next;
  keytable[indx].next = keynode;
  return(keynode);
}


/*----------------------------------------------------------------------
 * key_add():
 *      Add a new security association to the key table.  Caller is
 *      responsible for allocating memory for the key_secassoc as  
 *      well as the buffer space for the key,  iv.  Assumes the security 
 *      association passed in is well-formed.
 ----------------------------------------------------------------------*/
int
key_add(secassoc)
  struct key_secassoc *secassoc;
{
  char buf[MAXHASHKEYLEN];
  int len, indx;
  int inbound = 0;
  int outbound = 0;
  struct key_tblnode *keynode, *prevkeynode;
  struct key_allocnode *np = NULL;
  CRITICAL_DCL

  DPRINTF(IDL_FINISHED, ("Entering key_add w/secassoc=0x%x\n",
			 (unsigned int)secassoc));

  if (!secassoc) {
    panic("key_add: who the hell is passing me a null pointer");
  }

  /*
   * Should we allow a null key to be inserted into the table ? 
   * or can we use null key to indicate some policy action...
   */

#if 0
  /*
   *  For esp using des-cbc or tripple-des we call 
   * des_set_odd_parity.
   */
  if (secassoc->key && (secassoc->type == KEY_TYPE_ESP) && 
      ((secassoc->algorithm == IPSEC_ALGTYPE_ESP_DES_CBC) ||
       (secassoc->algorithm == IPSEC_ALGTYPE_ESP_3DES)))
    des_set_odd_parity(secassoc->key);
#endif /* 0 */

  /*
   * initialization for anti-replay services.
   */
  secassoc->sequence = 0;
  secassoc->replayright = 0;
  secassoc->replaywindow = 0;

  /*
   *  Check if secassoc with same spi exists before adding
   */
  bzero((char *)&buf, sizeof(buf));
  len = key_createkey((char *)&buf, secassoc->type, secassoc->src,
		      secassoc->dst, secassoc->spi, 0);
  indx = key_gethashval((char *)&buf, len, KEYTBLSIZE);
  DPRINTF(IDL_FINISHED,("keyadd: keytbl hash position=%d\n", indx));
  keynode = key_search(secassoc->type, secassoc->src, secassoc->dst,
		       secassoc->spi, indx, &prevkeynode);
  if (keynode) {
    DPRINTF(IDL_EVENT,("keyadd: secassoc already exists!\n"));
    return(-2);
  }

  inbound = my_addr(secassoc->dst);
  outbound = my_addr(secassoc->src);
  DPRINTF(IDL_FINISHED,("inbound=%d outbound=%d\n", inbound, outbound));

  /*
   * We allocate mem for an allocation entry if needed.
   * This is done here instead of in the allocaton code 
   * segment so that we can easily recover/cleanup from a 
   * memory allocation error.
   */
  if (outbound || (!inbound && !outbound)) {
    KMALLOC(np, struct key_allocnode *, sizeof(struct key_allocnode));
    if (np == 0) {
      DPRINTF(IDL_ERROR,("keyadd: can't allocate allocnode!\n"));
      return(-1);
    }
  }

  CRITICAL_START;

  if ((keynode = key_addnode(indx, secassoc)) == NULL) {
    DPRINTF(IDL_ERROR,("keyadd: key_addnode failed!\n"));
    if (np)
      KFREE(np);
    CRITICAL_END;
    return(-1);
  }
  DPRINTF(IDL_GROSS_EVENT,("Added new keynode:\n"));
  DDO(IDL_FINISHED, dump_keytblnode(keynode));
  DDO(IDL_FINISHED, dump_secassoc(keynode->secassoc));
 
  /*
   *  We add an entry to the allocation table for
   *  this secassoc if the interfaces are up, 
   *  the secassoc is outbound.  In the case 
   *  where the interfaces are not up, we go ahead
   * ,  do it anyways.  This wastes an allocation
   *  entry if the secassoc later turned out to be
   *  inbound when the interfaces are ifconfig up.
   */
  if (outbound || (!inbound && !outbound)) {
    len = key_createkey((char *)&buf, secassoc->type, secassoc->src,
			secassoc->dst, 0, 1);
    indx = key_gethashval((char *)&buf, len, KEYALLOCTBLSIZE);
    DPRINTF(IDL_FINISHED,("keyadd: keyalloc hash position=%d\n", indx));
    np->keynode = keynode;
    np->next = keyalloctbl[indx].next;
    keyalloctbl[indx].next = np;
  }
  if (inbound)
    secassoc->state |= K_INBOUND;
  if (outbound)
    secassoc->state |= K_OUTBOUND;

  key_deleteacquire(secassoc->type, secassoc->dst);

  CRITICAL_END;
  return 0;
}


/*----------------------------------------------------------------------
 * key_get():
 *      Get a security association from the key table.
 ----------------------------------------------------------------------*/
int
key_get(type, src, dst, spi, secassoc)
  u_int type;
  struct sockaddr *src;
  struct sockaddr *dst;
  u_int32_t spi;
  struct key_secassoc **secassoc;
{
  char buf[MAXHASHKEYLEN];
  struct key_tblnode *keynode, *prevkeynode;
  int len, indx;

  bzero(&buf, sizeof(buf));
  *secassoc = NULL;
  len = key_createkey((char *)&buf, type, src, dst, spi, 0);
  indx = key_gethashval((char *)&buf, len, KEYTBLSIZE);
  DPRINTF(IDL_FINISHED,("keyget: indx=%d\n",indx));
  keynode = key_search(type, src, dst, spi, indx, &prevkeynode);
  if (keynode) {
    DPRINTF(IDL_GROSS_EVENT,("keyget: found it! keynode=0x%x",
			     (unsigned int)keynode));
    *secassoc = keynode->secassoc;
    return(0);
  } else
    return(-1);  /* Not found */
}


/*----------------------------------------------------------------------
 * key_dump():
 *      Dump all valid entries in the keytable to a pf_key socket.  Each
 *      security associaiton is sent one at a time in a pf_key message.  A
 *      message with seqno = 0 signifies the end of the dump transaction.
 ----------------------------------------------------------------------*/
int
key_dump(so)
  struct socket *so;
{
  int len, i;
  int seq = 1;
  struct key_msgdata keyinfo;
  struct key_msghdr *km;
  struct key_tblnode *keynode;
  int kmlen;

  /*
   * Routine to dump the key table to a routing socket
   * Use for debugging only!
   */

  kmlen = sizeof(struct key_msghdr) + 3 * MAX_SOCKADDR_SZ + MAX_KEY_SZ
		+ MAX_IV_SZ;
  KMALLOC(km, struct key_msghdr *, kmlen);
  if (!km)
    return(ENOBUFS);

  DPRINTF(IDL_FINISHED,("Entering key_dump()"));
  /* 
   * We need to speed this up later.  Fortunately, key_dump 
   * messages are not sent often.
   */
  for (i = 0; i < KEYTBLSIZE; i++) {
    for (keynode = keytable[i].next; keynode; keynode = keynode->next) {
      /*
       * We exclude dead/larval/zombie security associations for now
       * but it may be useful to also send these up for debugging purposes
       */
      if (keynode->secassoc->state & (K_DEAD | K_LARVAL | K_ZOMBIE))
	continue;

      len = (sizeof(struct key_msghdr) +
	     ROUNDUP(keynode->secassoc->src->sa_len) + 
	     ROUNDUP(keynode->secassoc->dst->sa_len) +
	     ROUNDUP(keynode->secassoc->from->sa_len) +
	     ROUNDUP(keynode->secassoc->keylen) + 
	     ROUNDUP(keynode->secassoc->ivlen) + 
	     ROUNDUP(keynode->secassoc->ekeylen));

      if (kmlen < len) {
	KFREE(km);
	kmlen = len;
	KMALLOC(km, struct key_msghdr *, kmlen);
	if (!km)
	  return(ENOBUFS);
      }

      if (key_secassoc2msghdr(keynode->secassoc, km, &keyinfo) != 0)
	panic("key_dump");

      km->key_msglen = len;
      km->key_msgvers = KEY_VERSION;
      km->key_msgtype = KEY_DUMP;
      km->key_pid = curproc->p_pid;
      km->key_seq = seq++;
      km->key_errno = 0;

      key_sendup(so, km);
    }
  }
  bzero((char *)km, sizeof(struct key_msghdr));
  km->key_msglen = sizeof(struct key_msghdr);
  km->key_msgvers = KEY_VERSION;
  km->key_msgtype = KEY_DUMP;
  km->key_pid = curproc->p_pid;
  km->key_seq = 0;
  km->key_errno = 0;

  key_sendup(so, km);
  KFREE(km);
  DPRINTF(IDL_FINISHED,("Leaving key_dump()\n"));  
  return(0);
}

/*----------------------------------------------------------------------
 * key_delete():
 *      Delete a security association from the key table.
 ----------------------------------------------------------------------*/
int
key_delete(secassoc)
  struct key_secassoc *secassoc;
{
  char buf[MAXHASHKEYLEN];
  int len, indx;
  struct key_tblnode *keynode = 0;
  struct key_tblnode *prevkeynode = 0;
  struct socketlist *socklp, *deadsocklp;
  struct key_so2spinode *np, *prevnp;
  struct key_allocnode *ap, *prevap;
  CRITICAL_DCL

  DPRINTF(IDL_FINISHED,("Entering key_delete w/secassoc=0x%x\n",
			(unsigned int)secassoc));

  bzero((char *)&buf, sizeof(buf));
  len = key_createkey((char *)&buf, secassoc->type, secassoc->src,
		      secassoc->dst, secassoc->spi, 0);
  indx = key_gethashval((char *)&buf, len, KEYTBLSIZE);
  DPRINTF(IDL_FINISHED,("keydelete: keytbl hash position=%d\n", indx));
  keynode = key_search(secassoc->type, secassoc->src, secassoc->dst, 
		       secassoc->spi, indx, &prevkeynode); 
 
  if (keynode) {
    CRITICAL_START;
    DPRINTF(IDL_GROSS_EVENT,("keydelete: found keynode to delete\n"));
    keynode->secassoc->state |= K_DEAD;

    if (keynode->ref_count > 0) {
      DPRINTF(IDL_EVENT,("keydelete: secassoc still held, marking for deletion only!\n"));
      CRITICAL_END;
      return(0); 
    }

    prevkeynode->next = keynode->next;
    
    /*
     *  Walk the socketlist,  delete the
     *  entries mapping sockets to this secassoc
     *  from the so2spi table.
     */
    DPRINTF(IDL_FINISHED,("keydelete: deleting socklist..."));
    for(socklp = keynode->solist->next; socklp; ) {
      prevnp = &so2spitbl[((u_int32_t)(socklp->socket)) % SO2SPITBLSIZE];
      for(np = prevnp->next; np; np = np->next) {
	if ((np->socket == socklp->socket) && (np->keynode == keynode)) {
	  prevnp->next = np->next;
	  KFREE(np);
	  break; 
	}
	prevnp = np;  
      }
      deadsocklp = socklp;
      socklp = socklp->next;
      KFREE(deadsocklp);
    }
    DPRINTF(IDL_FINISHED,("done\n"));
    /*
     * If an allocation entry exist for this
     * secassoc, delete it.
     */
    bzero((char *)&buf, sizeof(buf));
    len = key_createkey((char *)&buf, secassoc->type, secassoc->src,
			secassoc->dst, 0, 1);
    indx = key_gethashval((char *)&buf, len, KEYALLOCTBLSIZE);
    DPRINTF(IDL_FINISHED,("keydelete: alloctbl hash position=%d\n", indx));
    prevap = &keyalloctbl[indx];
    for (ap = prevap->next; ap; ap = ap->next) {
      if (ap->keynode == keynode) {
	prevap->next = ap->next;
	KFREE(ap);
	break; 
      }
      prevap = ap;
    }    

    if (keynode->secassoc->iv)
      KFREE(keynode->secassoc->iv);
    if (keynode->secassoc->key)
      KFREE(keynode->secassoc->key);
    if (keynode->secassoc->ekey)
      KFREE(keynode->secassoc->ekey);
    KFREE(keynode->secassoc);
    if (keynode->solist)
      KFREE(keynode->solist);
    KFREE(keynode);
    CRITICAL_END;
    return(0);
  }
  return(-1);
}


/*----------------------------------------------------------------------
 * key_flush():
 *      Delete all entries from the key table.
 ----------------------------------------------------------------------*/
void
key_flush()
{
  struct key_tblnode *keynode;
  int i;
#if 1
  int timo;
#endif

  /* 
   * This is slow, but simple.
   */
  DPRINTF(IDL_FINISHED,("Flushing key table..."));
  for (i = 0; i < KEYTBLSIZE; i++) {
    timo = 0;
    while ((keynode = keytable[i].next)) {
      if (key_delete(keynode->secassoc) != 0)
	panic("key_flush");
      timo++;
      if (10000 < timo) {
printf("key_flush: timo exceeds limit; terminate the loop to prevent hangup\n");
	break;
      }
    }
  }
  DPRINTF(IDL_FINISHED,("done\n"));
}


/*----------------------------------------------------------------------
 * key_getspi():
 *      Get a unique spi value for a key management daemon/program.  The 
 *      spi value, once assigned, cannot be assigned again (as long as the 
 *      entry with that same spi value remains in the table).
 ----------------------------------------------------------------------*/
int
key_getspi(type, vers, src, dst, lowval, highval, spi)
  u_int type;
  u_int vers;
  struct sockaddr *src;
  struct sockaddr *dst;
  u_int32_t lowval;
  u_int32_t highval;
  u_int32_t *spi;
{
  struct key_secassoc *secassoc;
  struct key_tblnode *keynode, *prevkeynode;
  int count, done, len, indx;
  int maxcount = 1000;
  u_int32_t val;
  char buf[MAXHASHKEYLEN];
  CRITICAL_DCL
  
  DPRINTF(IDL_EVENT,("Entering getspi w/type=%d,low=%u,high=%u\n",
			   type, lowval, highval));
  if (!(src && dst))
    return(EINVAL);

  if ((lowval == 0) || (highval == 0))
    return(EINVAL);

  if (lowval > highval) {
    u_int32_t temp;
    temp = lowval;
    lowval = highval;
    highval = lowval;
  }

  done = count = 0;
  do {
    count++;
    /* 
     *  This may not be "random enough".
     */
    val = lowval + (random() % (highval - lowval + 1));

    if (lowval == highval)
      count = maxcount;
    DPRINTF(IDL_FINISHED,("%u ",val));
    if (val) {
      DPRINTF(IDL_FINISHED,("\n"));
      bzero(&buf, sizeof(buf));
      len = key_createkey((char *)&buf, type, src, dst, val, 0);
      indx = key_gethashval((char *)&buf, len, KEYTBLSIZE);
      if (!key_search(type, src, dst, val, indx, &prevkeynode)) {
	CRITICAL_START;
	KMALLOC(secassoc, struct key_secassoc *, sizeof(struct key_secassoc));
	if (secassoc == 0) {
	  DPRINTF(IDL_ERROR,("key_getspi: can't allocate memory\n"));
	  CRITICAL_END;
	  return(ENOBUFS);
	}
	bzero((char *)secassoc, sizeof(*secassoc));

	DPRINTF(IDL_FINISHED,("getspi: indx=%d\n",indx));
	secassoc->len = sizeof(struct key_secassoc);
	secassoc->type = type;
	secassoc->vers = vers;
	secassoc->spi = val;
	secassoc->state |= K_LARVAL;
	if (my_addr(dst))
	  secassoc->state |= K_INBOUND;
	if (my_addr(src))
	  secassoc->state |= K_OUTBOUND;

	KMALLOC(secassoc->src, struct sockaddr *, src->sa_len);
	if (!secassoc->src) {
	  DPRINTF(IDL_ERROR,("key_getspi: can't allocate memory\n"));
	  KFREE(secassoc);
	  CRITICAL_END;
	  return(ENOBUFS);
	}
	bcopy((char *)src, (char *)secassoc->src, src->sa_len);
	KMALLOC(secassoc->dst, struct sockaddr *, dst->sa_len);
	if (!secassoc->dst) {
	  DPRINTF(IDL_ERROR,("key_getspi: can't allocate memory\n"));
	  KFREE(secassoc->src);
	  KFREE(secassoc);
	  CRITICAL_END;
	  return(ENOBUFS);
	}
	bcopy((char *)dst, (char *)secassoc->dst, dst->sa_len);

	/* We fill this in with a plausable value now to insure
	   that other routines don't break. These will get
	   overwritten later with the correct values. */
#if 0
#ifdef INET6
	secassoc->from->sa_family = AF_INET6;
	secassoc->from->sa_len = sizeof(struct sockaddr_in6);
#else /* INET6 */
	secassoc->from->sa_family = AF_INET;
	secassoc->from->sa_len = sizeof(struct sockaddr_in);
#endif /* INET6 */
#endif

	/* 
	 * We need to add code to age these larval key table
	 * entries so they don't linger forever waiting for
	 * a KEY_UPDATE message that may not come for various
	 * reasons.  This is another task that key_reaper can
	 * do once we have it coded.
	 */
	secassoc->lifetime1 += time_second + maxlarvallifetime;

	if (!(keynode = key_addnode(indx, secassoc))) {
	  DPRINTF(IDL_ERROR,("key_getspi: can't add node\n"));
	  CRITICAL_END;
	  return(ENOBUFS);
	} 
	DPRINTF(IDL_FINISHED,("key_getspi: added node 0x%x\n",
			      (unsigned int)keynode));
	done++;
	CRITICAL_END;
      }
    }
  } while ((count < maxcount) && !done);
  DPRINTF(IDL_EVENT,("getspi returns w/spi=%u,count=%d\n",val,count));
  if (done) {
    *spi = val;
    return(0);
  } else {
    *spi = 0;
    return(EADDRNOTAVAIL);
  }
}


/*----------------------------------------------------------------------
 * key_update():
 *      Update a keytable entry that has an spi value assigned but is 
 *      incomplete (e.g. no key/iv).
 ----------------------------------------------------------------------*/
int
key_update(secassoc)
  struct key_secassoc *secassoc;
{
  struct key_tblnode *keynode, *prevkeynode;
  struct key_allocnode *np = 0;
  u_int8_t newstate;
  int len, indx, inbound, outbound;
  char buf[MAXHASHKEYLEN];
  CRITICAL_DCL

  bzero(&buf, sizeof(buf));
  len = key_createkey((char *)&buf, secassoc->type, secassoc->src,
		      secassoc->dst, secassoc->spi, 0);
  indx = key_gethashval((char *)&buf, len, KEYTBLSIZE);
  if(!(keynode = key_search(secassoc->type, secassoc->src, secassoc->dst, 
			    secassoc->spi, indx, &prevkeynode))) {  
    return(ESRCH);
  }
  if (keynode->secassoc->state & K_DEAD)
    return(ESRCH);

  /* Should we also restrict updating of only LARVAL entries ? */

  CRITICAL_START;

  inbound = my_addr(secassoc->dst);
  outbound = my_addr(secassoc->src);

  newstate = keynode->secassoc->state;
  newstate &= ~K_LARVAL;

  if (inbound)
    newstate |= K_INBOUND;
  if (outbound)
    newstate |= K_OUTBOUND;

  if (outbound || (!inbound && !outbound)) {
    KMALLOC(np, struct key_allocnode *, sizeof(struct key_allocnode));
    if (np == 0) {
      DPRINTF(IDL_ERROR,("keyupdate: can't allocate allocnode!\n"));
      CRITICAL_END;
      return(ENOBUFS);
    }
  }

  /*
   *  Free the old key,  iv if they're there.
   */
  if (keynode->secassoc->key)
    KFREE(keynode->secassoc->key);
  if (keynode->secassoc->iv)
    KFREE(keynode->secassoc->iv);
  if (keynode->secassoc->ekey)
    KFREE(keynode->secassoc->ekey);

  /*
   *  We now copy the secassoc over. We don't need to copy
   *  the key,  iv into new buffers since the calling routine
   *  does that already.  
   */

  *(keynode->secassoc) = *secassoc;
  keynode->secassoc->state = newstate;

  /*
   * Should we allow a null key to be inserted into the table ? 
   * or can we use null key to indicate some policy action...
   */

#if 0  
  if (keynode->secassoc->key &&
       (keynode->secassoc->type == KEY_TYPE_ESP) &&
       ((keynode->secassoc->algorithm == IPSEC_ALGTYPE_ESP_DES_CBC) ||
	(keynode->secassoc->algorithm == IPSEC_ALGTYPE_ESP_3DES)))
      des_set_odd_parity(keynode->secassoc->key);
#endif /* 0 */

  /*
   *  We now add an entry to the allocation table for this 
   *  updated key table entry.
   */
  if (outbound || (!inbound && !outbound)) {
    len = key_createkey((char *)&buf, secassoc->type, secassoc->src,
			secassoc->dst, 0, 1);
    indx = key_gethashval((char *)&buf, len, KEYALLOCTBLSIZE);
    DPRINTF(IDL_FINISHED,("keyupdate: keyalloc hash position=%d\n", indx));
    np->keynode = keynode;
    np->next = keyalloctbl[indx].next;
    keyalloctbl[indx].next = np;
  }

  key_deleteacquire(secassoc->type, (struct sockaddr *)&(secassoc->dst));

  CRITICAL_END;
  return(0);
}

/*----------------------------------------------------------------------
 * key_register():
 *      Register a socket as one capable of acquiring security associations
 *      for the kernel.
 ----------------------------------------------------------------------*/
int
key_register(socket, type)
  struct socket *socket;
  u_int type;
{
  struct key_registry *p, *new;
  CRITICAL_DCL

  CRITICAL_START;

  DPRINTF(IDL_EVENT,("Entering key_register w/so=0x%x,type=%d\n",
		     (unsigned int)socket,type));

  if (!(keyregtable && socket))
    panic("key_register");
  
  /*
   * Make sure entry is not already in table
   */
  for(p = keyregtable->next; p; p = p->next) {
    if ((p->type == type) && (p->socket == socket)) {
      CRITICAL_END;
      return(EEXIST);
    }
  }

  KMALLOC(new, struct key_registry *, sizeof(struct key_registry));  
  if (new == 0) {
    CRITICAL_END;
    return(ENOBUFS);
  }
  new->type = type;
  new->socket = socket;
  new->next = keyregtable->next;
  keyregtable->next = new;
  CRITICAL_END;
  return(0);
}

/*----------------------------------------------------------------------
 * key_unregister():
 *      Delete entries from the registry list.
 *         allflag = 1 : delete all entries with matching socket
 *         allflag = 0 : delete only the entry matching socket,  type
 ----------------------------------------------------------------------*/
void
key_unregister(socket, type, allflag)
  struct socket *socket;
  u_int type;
  int allflag;
{
  struct key_registry *p, *prev;
  CRITICAL_DCL

  CRITICAL_START;

  DPRINTF(IDL_EVENT,("Entering key_unregister w/so=0x%x,type=%d,flag=%d\n",
		     (unsigned int)socket, type, allflag));

  if (!(keyregtable && socket))
    panic("key_register");
  prev = keyregtable;
  for(p = keyregtable->next; p; p = p->next) {
    if ((allflag && (p->socket == socket)) ||
	((p->type == type) && (p->socket == socket))) {
      prev->next = p->next;
      KFREE(p);
      p = prev;
    }
    prev = p;
  }
  CRITICAL_END;
}


/*----------------------------------------------------------------------
 * key_acquire():
 *      Send a key_acquire message to all registered key mgnt daemons 
 *      capable of acquire security association of type type.
 *
 *      Return: 0 if succesfully called key mgnt. daemon(s)
 *              -1 if not successfull.
 ----------------------------------------------------------------------*/
int
key_acquire(type, src, dst)
  u_int type;
  struct sockaddr *src;
  struct sockaddr *dst;
{
  struct key_registry *p;
  struct key_acquirelist *ap, *prevap;
  int success = 0, created = 0;
  u_int etype;
  struct key_msghdr *km = NULL;
  int len;

  DPRINTF(IDL_EVENT,("Entering key_acquire()\n"));

  if (!keyregtable || !src || !dst)
    return (-1);

  /*
   * We first check the acquirelist to see if a key_acquire
   * message has been sent for this destination.
   */
  etype = type;
  prevap = key_acquirelist;
  for(ap = key_acquirelist->next; ap; ap = ap->next) {
    if (addrpart_equal(dst, ap->target) &&
	(etype == ap->type)) {
      DPRINTF(IDL_EVENT,("acquire message previously sent!\n"));
      if (ap->expiretime < time_second) {
	DPRINTF(IDL_EVENT,("acquire message has expired!\n"));
	ap->count = 0;
	break;
      }
      if (ap->count < maxkeyacquire) {
	DPRINTF(IDL_EVENT,("max acquire messages not yet exceeded!\n"));
	break;
      }
      return(0);
    } else if (ap->expiretime < time_second) {
      /*
       *  Since we're already looking at the list, we may as
       *  well delete expired entries as we scan through the list.
       *  This should really be done by a function like key_reaper()
       *  but until we code key_reaper(), this is a quick,  dirty
       *  hack.
       */
      DPRINTF(IDL_EVENT,("found an expired entry...deleting it!\n"));
      prevap->next = ap->next;
      KFREE(ap);
      ap = prevap;
    }
    prevap = ap;
  }

  /*
   * Scan registry,  send KEY_ACQUIRE message to 
   * appropriate key management daemons.
   */  
  for(p = keyregtable->next; p; p = p->next) {
    if (p->type != type) 
      continue;

    if (!created) {      
      len = sizeof(struct key_msghdr) + ROUNDUP(src->sa_len) + 
	ROUNDUP(dst->sa_len);
      KMALLOC(km, struct key_msghdr *, len);
      if (!km) {
	DPRINTF(IDL_ERROR,("key_acquire: no memory\n"));
	return(-1);
      }
      DPRINTF(IDL_FINISHED,("key_acquire/created: 1\n"));
      bzero((char *)km, len);
      km->key_msglen = len;
      km->key_msgvers = KEY_VERSION;
      km->key_msgtype = KEY_ACQUIRE;
      km->type = type;
      DPRINTF(IDL_FINISHED,("key_acquire/created: 2\n"));
      /*
       * This is inefficient,  slow.
       */

      /*
       * We zero out sin_zero here for AF_INET addresses because
       * ip_output() currently does not do it for performance reasons.
       */
      if (src->sa_family == AF_INET)
	bzero((char *)(((struct sockaddr_in *)src)->sin_zero),
	      sizeof(((struct sockaddr_in *)src)->sin_zero));
      if (dst->sa_family == AF_INET)
	bzero((char *)(((struct sockaddr_in *)dst)->sin_zero), 
	      sizeof(((struct sockaddr_in *)dst)->sin_zero));

      bcopy((char *)src, (char *)(km + 1), src->sa_len);
      bcopy((char *)dst, (char *)((int)(km + 1) + ROUNDUP(src->sa_len)),
	    dst->sa_len);
      DPRINTF(IDL_FINISHED,("key_acquire/created: 3\n"));
      created++; 
    }
    if (key_sendup(p->socket, km))
      success++;
  }

  if (km)
    KFREE(km);
      
  /*
   *  Update the acquirelist 
   */
  if (success) {
    if (!ap) {
      DPRINTF(IDL_EVENT,("Adding new entry in acquirelist\n"));
      KMALLOC(ap, struct key_acquirelist *,
	sizeof(struct key_acquirelist) + dst->sa_len);
      if (ap == 0)
	return(success ? 0 : -1);
      bzero((char *)ap, sizeof(struct key_acquirelist));
      ap->target = (struct sockaddr *)(ap + 1);
      bcopy((char *)dst, (char *)ap->target, dst->sa_len);
      ap->type = etype;
      ap->next = key_acquirelist->next;
      key_acquirelist->next = ap;
    }
    DPRINTF(IDL_GROSS_EVENT,("Updating acquire counter,  expiration time\n"));
    ap->count++;
    ap->expiretime = time_second + maxacquiretime;
  }
  DPRINTF(IDL_EVENT,("key_acquire: done! success=%d\n",success));
  return(success ? 0 : -1);
}

/*----------------------------------------------------------------------
 * key_alloc():
 *      Allocate a security association to a socket.  A socket requesting 
 *      unique keying (per-socket keying) is assigned a security assocation
 *      exclusively for its use.  Sockets not requiring unique keying are
 *      assigned the first security association which may or may not be
 *      used by another socket.
 ----------------------------------------------------------------------*/
static int
key_alloc(type, src, dst, socket, unique_key, keynodep)
  u_int type;
  struct sockaddr *src;
  struct sockaddr *dst;
  struct socket *socket;
  u_int  unique_key;
  struct key_tblnode **keynodep;
{
  struct key_tblnode *keynode;
  char buf[MAXHASHKEYLEN];
  struct key_allocnode *np, *prevnp;
  struct key_so2spinode *newnp;
  int len;
  int indx;

  DPRINTF(IDL_FINISHED,("Entering key_alloc w/type=%u!\n",type));
  if (!(src && dst)) {
    DPRINTF(IDL_ERROR,("key_alloc: received null src or dst!\n"));
    return(-1);
  }

  /*
   * Search key allocation table
   */
  bzero((char *)&buf, sizeof(buf));
  len = key_createkey((char *)&buf, type, src, dst, 0, 1);
  indx = key_gethashval((char *)&buf, len, KEYALLOCTBLSIZE);  

#define np_type np->keynode->secassoc->type
#define np_state np->keynode->secassoc->state
#define np_src np->keynode->secassoc->src
#define np_dst np->keynode->secassoc->dst
  
  prevnp = &keyalloctbl[indx];
  for (np = keyalloctbl[indx].next; np; np = np->next) {
    if ((type == np_type) && addrpart_equal(src, np_src) &&
	addrpart_equal(dst, np_dst) &&
	!(np_state & (K_LARVAL | K_DEAD | K_UNIQUE))) {
      if (!(unique_key))
	break;
      if (!(np_state & K_USED)) 
	break;
    }
    prevnp = np;
  }

  if (np) {
    struct socketlist *newsp;
    CRITICAL_DCL

    CRITICAL_START;

    DPRINTF(IDL_EVENT,("key_alloc: found node to allocate\n"));
    keynode = np->keynode;

    KMALLOC(newnp, struct key_so2spinode *, sizeof(struct key_so2spinode));
    if (newnp == 0) {
      DPRINTF(IDL_ERROR,("key_alloc: Can't alloc mem for so2spi node!\n"));
      CRITICAL_END;
      return(ENOBUFS);
    }
    KMALLOC(newsp, struct socketlist *, sizeof(struct socketlist));
    if (newsp == 0) {
      DPRINTF(IDL_ERROR,("key_alloc: Can't alloc mem for socketlist!\n"));
      if (newnp)
	KFREE(newnp);
      CRITICAL_END;
      return(ENOBUFS);
    }

    /*
     * Add a hash entry into the so2spi table to
     * map socket to allocated secassoc.
     */
    DPRINTF(IDL_FINISHED,("key_alloc: adding entry to so2spi table..."));
    newnp->keynode = keynode;
    newnp->socket = socket;
    newnp->next = so2spitbl[((u_int32_t)socket) % SO2SPITBLSIZE].next; 
    so2spitbl[((u_int32_t)socket) % SO2SPITBLSIZE].next = newnp;
    DPRINTF(IDL_FINISHED,("done\n"));

    if (unique_key) {
      /*
       * Need to remove the allocation entry
       * since the secassoc is now unique,  
       * can't be allocated to any other socket
       */
      DPRINTF(IDL_EVENT,("key_alloc: making keynode unique..."));
      keynode->secassoc->state |= K_UNIQUE;
      prevnp->next = np->next;
      KFREE(np);
      DPRINTF(IDL_EVENT,("done\n"));
    }
    keynode->secassoc->state |= K_USED;
    keynode->secassoc->state |= K_OUTBOUND;
    keynode->alloc_count++;

    /*
     * Add socket to list of socket using secassoc.
     */
    DPRINTF(IDL_FINISHED,("key_alloc: adding so to solist..."));
    newsp->socket = socket;
    newsp->next = keynode->solist->next;
    keynode->solist->next = newsp;
    DPRINTF(IDL_FINISHED,("done\n"));
    *keynodep = keynode;
    CRITICAL_END;
    return(0);
  } 
  *keynodep = NULL;
  return(0);
}


/*----------------------------------------------------------------------
 * key_free():
 *      Decrement the refcount for a key table entry.  If the entry is 
 *      marked dead,,  the refcount is zero, we go ahead,  delete it.
 ----------------------------------------------------------------------*/
void
key_free(keynode)
  struct key_tblnode *keynode;
{
  DPRINTF(IDL_GROSS_EVENT,("Entering key_free w/keynode=0x%x\n",
			   (unsigned int)keynode));
  if (!keynode) {
    DPRINTF(IDL_ERROR,("Warning: key_free got null pointer\n"));
    return;
  }
  (keynode->ref_count)--;
  if (keynode->ref_count < 0) {
    DPRINTF(IDL_ERROR,("Warning: key_free decremented refcount to %d\n",keynode->ref_count));
  }
  if ((keynode->secassoc->state & K_DEAD) && (keynode->ref_count <= 0)) {
    DPRINTF(IDL_GROSS_EVENT,("key_free: calling key_delete\n"));
    key_delete(keynode->secassoc);
  }
}

/*----------------------------------------------------------------------
 * getassocbyspi():
 *      Get a security association for a given type, src, dst,,  spi.
 *
 *      Returns: 0 if sucessfull
 *               -1 if error/not found
 *
 *      Caller must convert spi to host order.  Function assumes spi is  
 *      in host order!
 ----------------------------------------------------------------------*/
int
getassocbyspi(type, src, dst, spi, keyentry)
  u_int type;
  struct sockaddr *src;
  struct sockaddr *dst;
  u_int32_t spi;
  struct key_tblnode **keyentry;
{
  char buf[MAXHASHKEYLEN];
  int len, indx;
  struct key_tblnode *keynode, *prevkeynode = 0;

  DPRINTF(IDL_FINISHED,("Entering getassocbyspi w/type=%u spi=%u\n",type,spi));

  *keyentry = NULL;
  bzero(&buf, sizeof(buf));
  len = key_createkey((char *)&buf, type, src, dst, spi, 0);
  indx = key_gethashval((char *)&buf, len, KEYTBLSIZE);
  DPRINTF(IDL_FINISHED,("getassocbyspi: indx=%d\n",indx));
  DDO(IDL_FINISHED,dump_sockaddr(src);dump_sockaddr(dst));
  keynode = key_search(type, src, dst, spi, indx, &prevkeynode);
  DPRINTF(IDL_FINISHED,("getassocbyspi: keysearch ret=0x%x\n",
			(unsigned int)keynode));
  if (keynode && !(keynode->secassoc->state & (K_DEAD | K_LARVAL))) {
    DPRINTF(IDL_GROSS_EVENT,("getassocbyspi: found secassoc!\n"));
    (keynode->ref_count)++;
    keynode->secassoc->state |= K_USED;
    *keyentry = keynode;
  } else {
    DPRINTF(IDL_EVENT,("getassocbyspi: secassoc not found!\n"));
    return (-1);
  }
  return(0);
}


/*----------------------------------------------------------------------
 * getassocbysocket():
 *      Get a security association for a given type, src, dst,,  socket.
 *      If not found, try to allocate one.
 *      Returns: 0 if successfull
 *              -1 if error condition/secassoc not found (*keyentry = NULL)
 *               1 if secassoc temporarily unavailable (*keynetry = NULL)
 *                 (e.g., key mgnt. daemon(s) called)
 ----------------------------------------------------------------------*/
int
getassocbysocket(type, src, dst, socket, unique_key, keyentry)
  u_int type;
  struct sockaddr *src;
  struct sockaddr *dst;
  struct socket *socket;
  u_int unique_key;
  struct key_tblnode **keyentry;
{
  struct key_tblnode *keynode = 0;
  struct key_so2spinode *np;
  u_int realtype;
 
  DPRINTF(IDL_FINISHED,("Entering getassocbysocket w/type=%u so=0x%x\n",
			type,(unsigned int)socket));

  /*
   *  We treat esp-transport mode,  esp-tunnel mode 
   *  as a single type in the keytable.  This has a side
   *  effect that socket using both esp-transport, 
   *  esp-tunnel will use the same security association
   *  for both modes.  Is this a problem?
   */
  realtype = type;
  if ((np = key_sosearch(type, src, dst, socket))) {
    if (np->keynode && np->keynode->secassoc && 
	!(np->keynode->secassoc->state & (K_DEAD | K_LARVAL))) {
      DPRINTF(IDL_FINISHED,("getassocbysocket: found secassoc!\n"));
      (np->keynode->ref_count)++;
      *keyentry = np->keynode;
      return(0);
    }
  }

  /*
   * No secassoc has been allocated to socket, 
   * so allocate one, if available
   */
  DPRINTF(IDL_GROSS_EVENT,("getassocbyso: can't find it, trying to allocate!\n"));
  if (key_alloc(realtype, src, dst, socket, unique_key, &keynode) == 0) {
    if (keynode) {
      DPRINTF(IDL_GROSS_EVENT,("getassocbyso: key_alloc found secassoc!\n"));
      keynode->ref_count++;
      *keyentry = keynode;
      return(0);
    } else {
      /* 
       * Kick key mgnt. daemon(s) 
       * (this should be done in ipsec_output_policy() instead or
       * selectively called based on a flag value)
       */
      DPRINTF(IDL_FINISHED,("getassocbyso: calling key mgnt daemons!\n"));
      *keyentry = NULL;
      if (key_acquire(realtype, src, dst) == 0)
	return (1);
      else
	return(-1);
    }
  }
  *keyentry = NULL;
  return(-1);
}

/*----------------------------------------------------------------------
 * key_xdata():
 *      Parse message buffer for src/dst/from/iv/key if parseflag = 0
 *      else parse for src/dst only.
 ----------------------------------------------------------------------*/
static int
key_xdata(km, kip, parseflag)
  struct key_msghdr *km;
  struct key_msgdata *kip;
  int parseflag;
{
  char *cp, *cpmax;

  if (!km || (km->key_msglen <= 0))
    return (-1);

  cp = (caddr_t)(km + 1);
  cpmax = (caddr_t)km + km->key_msglen;

  /*
   * Assumes user process passes message with 
   * correct word alignment.
   */

  /* 
   * Need to clean up this code later.  
   */

  /* Grab src addr */
  kip->src = (struct sockaddr *)cp;
  if (!kip->src->sa_len) {
    DPRINTF(IDL_MAJOR_EVENT,("key_xdata couldn't parse src addr\n"));
    return(-1);
  }

  ADVANCE(cp, kip->src->sa_len);

  /* Grab dest addr */
  kip->dst = (struct sockaddr *)cp;
  if (!kip->dst->sa_len) {
    DPRINTF(IDL_MAJOR_EVENT,("key_xdata couldn't parse dest addr\n"));
    return(-1);
  }

  ADVANCE(cp, kip->dst->sa_len);
  if (parseflag == 1) {
    kip->from = 0;
    kip->key = kip->iv = kip->ekey = 0;
    kip->keylen = kip->ivlen = kip->ekeylen = 0;
    return(0);
  }
 
  /* Grab from addr */
  kip->from = (struct sockaddr *)cp;
  if (!kip->from->sa_len) {
    DPRINTF(IDL_MAJOR_EVENT,("key_xdata couldn't parse from addr\n"));
    return(-1);
  }

  ADVANCE(cp, kip->from->sa_len);
 
  /* Grab key */
  if ((kip->keylen = km->keylen)) {
    kip->key = cp;
    ADVANCE(cp, km->keylen);
  } else 
    kip->key = 0;

  /* Grab iv */
  if ((kip->ivlen = km->ivlen)) {
    kip->iv = cp;
    ADVANCE(cp, km->ivlen);
  } else
    kip->iv = 0;

  /* Grab ekey */
  if ((kip->ekeylen = km->ekeylen)) {
    kip->ekey = cp;
    ADVANCE(cp, km->ekeylen);
  } else 
    kip->ekey = 0;

  return (0);
}


int
key_parse(kmp, so, dstfamily)
  struct key_msghdr **kmp;
  struct socket *so;
  int *dstfamily;
{
  int error = 0, keyerror = 0;
  struct key_msgdata keyinfo;
  struct key_secassoc *secassoc = NULL;
  struct key_msghdr *km = *kmp;

  DPRINTF(IDL_MAJOR_EVENT, ("Entering key_parse\n"));

#define senderr(e) \
  { error = (e); goto flush; }

  if (km->key_msgvers != KEY_VERSION) {
    DPRINTF(IDL_CRITICAL,("keyoutput: Unsupported key message version!\n"));
    senderr(EPROTONOSUPPORT);
  }

  km->key_pid = curproc->p_pid;

  DDO(IDL_MAJOR_EVENT, printf("keymsghdr:\n"); dump_keymsghdr(km));

  /*
   * Parse buffer for src addr, dest addr, from addr, key, iv
   */
  bzero((char *)&keyinfo, sizeof(keyinfo));

  switch (km->key_msgtype) {
  case KEY_ADD:
    DPRINTF(IDL_MAJOR_EVENT,("key_output got KEY_ADD msg\n"));

    if (key_xdata(km, &keyinfo, 0) < 0)
      goto parsefail;

    /*
     * Allocate the secassoc structure to insert 
     * into key table here.
     */
    KMALLOC(secassoc, struct key_secassoc *, sizeof(struct key_secassoc)); 
    if (secassoc == 0) {
      DPRINTF(IDL_CRITICAL,("keyoutput: No more memory!\n"));
      senderr(ENOBUFS);
    }

    if (key_msghdr2secassoc(secassoc, km, &keyinfo) < 0) {
      DPRINTF(IDL_CRITICAL,("keyoutput: key_msghdr2secassoc failed!\n"));
      KFREE(secassoc);
      senderr(EINVAL);
    }
    DPRINTF(IDL_MAJOR_EVENT,("secassoc to add:\n"));
    DDO(IDL_MAJOR_EVENT,dump_secassoc(secassoc));

    if ((keyerror = key_add(secassoc)) != 0) {
      DPRINTF(IDL_CRITICAL,("keyoutput: key_add failed\n"));
      if (secassoc->key)
	KFREE(secassoc->key);
      if (secassoc->iv)
	KFREE(secassoc->iv);
      if (secassoc->ekey)
	KFREE(secassoc->ekey);
      KFREE(secassoc);
      if (keyerror == -2) {
	senderr(EEXIST);
      } else {
	senderr(ENOBUFS);
      }
    }
    break;
  case KEY_DELETE:
    DPRINTF(IDL_MAJOR_EVENT,("key_output got KEY_DELETE msg\n"));

    if (key_xdata(km, &keyinfo, 1) < 0)
      goto parsefail;

    KMALLOC(secassoc, struct key_secassoc *, sizeof(struct key_secassoc)); 
    if (secassoc == 0) {
      senderr(ENOBUFS);
    }
    if (key_msghdr2secassoc(secassoc, km, &keyinfo) < 0) {
      KFREE(secassoc);
      senderr(EINVAL);
    }
    if (key_delete(secassoc) != 0) {
      if (secassoc->iv)
	KFREE(secassoc->iv);
      if (secassoc->key)
	KFREE(secassoc->key);
      if (secassoc->ekey)
	KFREE(secassoc->ekey);
      KFREE(secassoc);
      senderr(ESRCH);
    }
    if (secassoc->iv)
      KFREE(secassoc->iv);
    if (secassoc->key)
      KFREE(secassoc->key);
    if (secassoc->ekey)
      KFREE(secassoc->ekey);
    KFREE(secassoc);
    break;
  case KEY_UPDATE:
    DPRINTF(IDL_EVENT,("key_output got KEY_UPDATE msg\n"));

    if (key_xdata(km, &keyinfo, 0) < 0)
      goto parsefail;

    KMALLOC(secassoc, struct key_secassoc *, sizeof(struct key_secassoc)); 
    if (secassoc == 0) {
      senderr(ENOBUFS);
    }
    if (key_msghdr2secassoc(secassoc, km, &keyinfo) < 0) {
      KFREE(secassoc);
      senderr(EINVAL);
    }
    if ((keyerror = key_update(secassoc)) != 0) {
      DPRINTF(IDL_CRITICAL,("Error updating key entry\n"));
      if (secassoc->iv)
	KFREE(secassoc->iv);
      if (secassoc->key)
	KFREE(secassoc->key);
      if (secassoc->ekey)
	KFREE(secassoc->ekey);
      KFREE(secassoc);
      senderr(keyerror);
    }
    KFREE(secassoc);
    break;
  case KEY_GET:
    DPRINTF(IDL_EVENT,("key_output got KEY_GET msg\n"));

    if (key_xdata(km, &keyinfo, 1) < 0)
      goto parsefail;

    if (key_get(km->type, (struct sockaddr *)keyinfo.src, 
		(struct sockaddr *)keyinfo.dst, 
		km->spi, &secassoc) != 0) {
      DPRINTF(IDL_EVENT,("keyoutput: can't get key\n"));
      senderr(ESRCH);
    }

    if (secassoc) {
      int newlen;

      DPRINTF(IDL_EVENT,("keyoutput: Found secassoc!\n"));
      newlen = sizeof(struct key_msghdr) + ROUNDUP(secassoc->src->sa_len) +
	ROUNDUP(secassoc->dst->sa_len) + ROUNDUP(secassoc->from->sa_len) +
	  ROUNDUP(secassoc->keylen) + ROUNDUP(secassoc->ivlen) +
	  ROUNDUP(secassoc->ekeylen);
      DPRINTF(IDL_EVENT,("keyoutput: newlen=%d\n", newlen));
      if (newlen > km->key_msglen) {
	struct key_msghdr *newkm;

	DPRINTF(IDL_EVENT,("keyoutput: Allocating new buffer!\n"));
	KMALLOC(newkm, struct key_msghdr *, newlen); 
	if (newkm == 0) {
	  senderr(ENOBUFS);
	}
	bcopy((char *)km, (char *)newkm, km->key_msglen);
	DPRINTF(IDL_FINISHED,("keyoutput: 1\n"));
	KFREE(km);
	*kmp = km = newkm;
	DPRINTF(IDL_CRITICAL, ("km->key_msglen = %d, newlen = %d\n",
			       km->key_msglen, newlen));
	km->key_msglen = newlen;
      }
      DPRINTF(IDL_FINISHED,("keyoutput: 2\n"));
      if (key_secassoc2msghdr(secassoc, km, &keyinfo)) {
	DPRINTF(IDL_CRITICAL,("keyoutput: Can't create msghdr!\n"));
	senderr(EINVAL);
      }
      DPRINTF(IDL_FINISHED,("keyoutput: 3\n"));
    }
    break;
  case KEY_GETSPI:
    DPRINTF(IDL_EVENT,("key_output got KEY_GETSPI msg\n"));

    if (key_xdata(km, &keyinfo, 1) < 0)
      goto parsefail;

    if ((keyerror = key_getspi(km->type, km->vers, keyinfo.src, keyinfo.dst, 
			       km->lifetime1, km->lifetime2, 
			       &(km->spi))) != 0) {
      DPRINTF(IDL_CRITICAL,("keyoutput: getspi failed error=%d\n", keyerror));
      senderr(keyerror);
    }
    break;
  case KEY_REGISTER:
    DPRINTF(IDL_EVENT,("key_output got KEY_REGISTER msg\n"));
    key_register(so, km->type);
    break;
  case KEY_DUMP:
    DPRINTF(IDL_EVENT,("key_output got KEY_DUMP msg\n"));
    error = key_dump(so);
    return(error);
    break;
  case KEY_FLUSH:
    DPRINTF(IDL_EVENT,("key_output got KEY_FLUSH msg\n"));
    key_flush();
    break;
  default:
    DPRINTF(IDL_CRITICAL,("key_output got unsupported msg type=%d\n", 
			     km->key_msgtype));
    senderr(EOPNOTSUPP);
  }

  goto flush;

parsefail:
  keyinfo.dst = NULL;
  error = EINVAL;

flush:
  if (km)
    km->key_errno = error;

  if (dstfamily)
    *dstfamily = keyinfo.dst ? keyinfo.dst->sa_family : 0;

  DPRINTF(IDL_MAJOR_EVENT, ("key_parse exiting with error=%d\n", error));
  return error;
}

/*
 * Definitions of protocols supported in the KEY domain.
 */

struct	sockaddr key_addr = { 2, PF_KEY, };
struct	sockproto key_proto = { PF_KEY, };

#define KEYREAPERINT 120

static int
key_sendup(s, km)
  struct socket *s;
  struct key_msghdr *km;
{
  struct mbuf *m;

  if (!km)
    panic("km == NULL in key_sendup\n");
  MGETHDR(m, M_WAIT, MT_DATA);
  m->m_len = m->m_pkthdr.len = 0;
  m->m_next = 0;
  m->m_nextpkt = 0;
  m->m_pkthdr.rcvif = 0;
  if (km)
    m_copyback(m, 0, km->key_msglen, (caddr_t)km);
  
  if (sbappendaddr(&(s->so_rcv), &key_addr, m, NULL)) {
    sorwakeup(s);
    return 1;
  } else {
    if (m) m_freem(m);
  }

  return(0);
}

#ifdef notyet
/*----------------------------------------------------------------------
 * key_reaper():
 *      Scan key table,  nuke unwanted entries
 ----------------------------------------------------------------------*/
static void
key_reaper(whocares)
     void *whocares;
{
  DPRINTF(IDL_GROSS_EVENT,("Entering key_reaper()\n"));

  timeout(key_reaper, NULL, KEYREAPERINT * HZ);
}
#endif /* notyet */

/*----------------------------------------------------------------------
 * key_init():
 *      Init routine for key socket,  key engine
 ----------------------------------------------------------------------*/
static void
key_init()
{
  DPRINTF(IDL_EVENT,("Called key_init().\n"));
  if (key_inittables())
    panic("key_inittables failed!\n");
#ifdef notyet
  timeout(key_reaper, NULL, HZ);
#endif /* notyet */
  bzero((char *)&keyso_cb, sizeof(keyso_cb));
}

/*----------------------------------------------------------------------
 * my_addr():
 *      Determine if an address belongs to one of my configured interfaces.
 *      Currently handles only AF_INET,  AF_INET6 addresses.
 ----------------------------------------------------------------------*/
static int
my_addr(sa)
     struct sockaddr *sa;
{
  struct in6_ifaddr *i6a = 0;
  struct in_ifaddr *ia = 0;

  switch(sa->sa_family) {
#ifdef INET6
  case AF_INET6:
    for (i6a = in6_ifaddr; i6a; i6a = i6a->ia_next) {	/*XXX*/
      if (IN6_ARE_ADDR_EQUAL(&((struct sockaddr_in6 *)sa)->sin6_addr,
			     &i6a->ia_addr.sin6_addr))
	return(1);
    }
    break;
#endif /* INET6 */
  case AF_INET:
    for (ia = in_ifaddrhead.tqh_first; ia; ia = ia->ia_link.tqe_next) {
      if (((struct sockaddr_in *)sa)->sin_addr.s_addr == 
	   ia->ia_addr.sin_addr.s_addr) 
	return(1);
    }
    break;
  }
  return(0);
}

/*----------------------------------------------------------------------
 * key_output():
 *      Process outbound pf_key message.
 ----------------------------------------------------------------------*/
static int
key_output(m, so)
  struct mbuf *m;
  struct socket *so;
{
  struct key_msghdr *km = 0;
  caddr_t cp, cplimit;
  int len;
  int error = 0;
  int dstfamily = 0;

  DPRINTF(IDL_EVENT,("key_output() got a message len=%d.\n", m->m_pkthdr.len));

#undef senderr
#define senderr(e) \
  { error = (e); if (km) km->key_errno = error; goto flush; }

  if (m == 0 || ((m->m_len < sizeof(long)) && 
		 (m = m_pullup(m, sizeof(long))) == 0)) {
    DPRINTF(IDL_CRITICAL,("key_output can't pullup mbuf\n"));
    return (ENOBUFS);
  }
  if ((m->m_flags & M_PKTHDR) == 0)
    panic("key_output");

  DDO(IDL_FINISHED,dump_mbuf(m));  
  
  len = m->m_pkthdr.len;
  if (len < sizeof(*km) || len != mtod(m, struct key_msghdr *)->key_msglen) {
    DPRINTF(IDL_CRITICAL,("keyout: Invalid length field/length mismatch!\n"));
    senderr(EINVAL); 
  }
  KMALLOC(km, struct key_msghdr *, len); 
  if (km == 0) {
    DPRINTF(IDL_CRITICAL,("keyoutput: Can't malloc memory!\n"));
    senderr(ENOBUFS);
  }

  m_copydata(m, 0, len, (caddr_t)km);

  km->key_errno = error = key_parse(&km, so, &dstfamily);
  DPRINTF(IDL_MAJOR_EVENT, ("Back from key_parse\n"));
flush:
  if (km)
    key_sendup(so, km);
#if 0
  {
    struct rawcb *rp = 0;
    struct mbuf *m;

    if ((so->so_options & SO_USELOOPBACK) == 0) {
      if (keyso_cb.any_count <= 1) {
	if (km)
	  KFREE(km);
	return (error);
      }
      rp = sotorawcb(so);
    }

  DPRINTF(IDL_MAJOR_EVENT, ("key_output: foo\n"));
    key_proto.sp_protocol = dstfamily;

    if (km) {
      m = m_devget(km, len, 0, NULL, NULL);
      KFREE(km);
    }

  DPRINTF(IDL_MAJOR_EVENT, ("key_output: bar\n"));
    if (rp)
      rp->rcb_proto.sp_family = 0;   /* Prevent us from receiving message */

    raw_input(m, &key_proto, &key_addr, &key_addr);

    if (rp)
      rp->rcb_proto.sp_family = PF_KEY;
  }
  DPRINTF(IDL_MAJOR_EVENT, ("key_output: baz\n"));
#endif /* 0 */
  return (error);
}


/*----------------------------------------------------------------------
 * key_*():
 *      Handles protocol requests for pf_key sockets.
 ----------------------------------------------------------------------*/

static int
key_attach(struct socket *so, int proto, struct proc *p)
{
  register int error = 0;
  register struct rawcb *rp;
  int s;

  DPRINTF(IDL_EVENT,("Entering key_attach\n"));

  MALLOC(rp, struct rawcb *, sizeof(*rp), M_PCB, M_WAITOK);
  if (rp) {
    bzero(rp, sizeof(*rp));
    so->so_pcb = (caddr_t)rp;
  }
  s = splnet();
  error = (raw_usrreqs.pru_attach)(so, proto, p);
  if (!error) {			/* XXX was: if (!so) which didn't make sense */
    splx(s);
    return error;
  }

  rp = sotorawcb(so);		/* isn't this redundant? */
  if (rp) {
    int af = rp->rcb_proto.sp_protocol;
    if (error) {
      free((caddr_t)rp, M_PCB);
      splx(s);
      return error;
    }
    if (af == AF_INET)
      keyso_cb.ip4_count++;
#ifdef INET6
    else if (af == AF_INET6)
      keyso_cb.ip6_count++;
#endif /* INET6 */
    keyso_cb.any_count++;
#if 0 /*itojun*/
    rp->rcb_faddr = &key_addr;
#else
    {
      struct mbuf *m;
      MGET(m, M_DONTWAIT, MT_DATA);
      if (m) {			/* XXX but what about sin_len here? -PW */
	rp->rcb_faddr = mtod(m, struct sockaddr *);
	bcopy(&key_addr, rp->rcb_faddr, sizeof(struct sockaddr));
      } else
	rp->rcb_faddr = NULL;
    }
#endif
    soisconnected(so);   /* Key socket, like routing socket, must be
			    connected. */

    /* Possibly set other needed flags/options at creation time in here. */
    so->so_options |= SO_USELOOPBACK; /* Like routing socket, we turn this */
                                      /* on by default                     */
  }
  splx(s);
  return error;
}

static int
key_detach(struct socket *so)
{
  register int error = 0;
  register struct rawcb *rp;
  int s;

  DPRINTF(IDL_EVENT,("Entering key_detach\n"));

  rp = sotorawcb(so);
  if (rp) {
    int af = rp->rcb_proto.sp_protocol;
    if (af == AF_INET)
      keyso_cb.ip4_count--;
#ifdef INET6
    else if (af == AF_INET6)
      keyso_cb.ip6_count--;
#endif /* INET6 */
    keyso_cb.any_count--;
  }
  s = splnet();
  error = (raw_usrreqs.pru_detach)(so);
  splx(s);
  return error;

}

static int
key_abort(struct socket *so)
{
  DPRINTF(IDL_EVENT,("Entering key_abort\n"));

  return (raw_usrreqs.pru_abort)(so);
}

static int
key_bind(struct socket *so, struct sockaddr *nam, struct proc *p)
{
  DPRINTF(IDL_EVENT,("Entering key_bind\n"));

  return (raw_usrreqs.pru_bind)(so, nam, p);
}

static int
key_connect(struct socket *so, struct sockaddr *nam, struct proc *p)
{
  DPRINTF(IDL_EVENT,("Entering key_connect\n"));

  return (raw_usrreqs.pru_connect)(so, nam, p);
}

static int
key_disconnect(struct socket *so)
{
  DPRINTF(IDL_EVENT,("Entering key_disconnect\n"));

  return (raw_usrreqs.pru_disconnect)(so);
}

static int
key_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
	 struct mbuf *control, struct proc *p)
{
  DPRINTF(IDL_EVENT,("Entering key_send\n"));

  return (raw_usrreqs.pru_send)(so, flags, m, nam, control, p);
}

static int
key_shutdown(struct socket *so)
{
  DPRINTF(IDL_EVENT,("Entering key_shutdown\n"));

  return (raw_usrreqs.pru_shutdown)(so);
}

/*----------------------------------------------------------------------
 * key_cbinit():
 *      Control block init routine for key socket
 ----------------------------------------------------------------------*/
static void
key_cbinit()
{
  /*
   *  This is equivalent to raw_init for the routing socket. 
   *  The key socket uses the same control block as the routing 
   *  socket.
   */
  DPRINTF(IDL_EVENT,("Called key_cbinit().\n"));
}

/*
 * Protoswitch entry for pf_key 
 */

extern struct domain keydomain;		/* or at least forward */

struct pr_usrreqs key_usrreqs = {
  key_abort, pru_accept_notsupp, key_attach, key_bind, key_connect,
  pru_connect2_notsupp, in_control, key_detach, key_disconnect,
  pru_listen_notsupp, in_setpeeraddr, pru_rcvd_notsupp,
  pru_rcvoob_notsupp, key_send, pru_sense_null, key_shutdown, 
  in_setsockaddr, sosend, soreceive, sopoll
};


struct protosw keysw[] = {
{ SOCK_RAW,	&keydomain,	0,	PR_ATOMIC|PR_ADDR,
  0,		key_output,	raw_ctlinput, 0,
  0,
  key_cbinit,	0,		0,		0,
  &key_usrreqs,
},
};

struct domain keydomain =
    { PF_KEY, "key", key_init, 0, 0,
      keysw, &keysw[sizeof(keysw)/sizeof(keysw[0])] };

#ifdef __FreeBSD__
DOMAIN_SET(key);
#endif

#endif /*KEY*/
