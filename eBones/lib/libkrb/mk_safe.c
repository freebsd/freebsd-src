/*
 * Copyright 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * This routine constructs a Kerberos 'safe msg', i.e. authenticated
 * using a private session key to seed a checksum. Msg is NOT
 * encrypted.
 *
 *      Note-- bcopy is used to avoid alignment problems on IBM RT
 *
 *      Returns either <0 ===> error, or resulting size of message
 *
 * Steve Miller    Project Athena  MIT/DEC
 *
 *	from: mk_safe.c,v 4.12 89/03/22 14:50:49 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char rcsid[] =
"$FreeBSD$";
#endif /* lint */
#endif

/* system include files */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>

/* application include files */
#include <des.h>
#include <krb.h>
#include <prot.h>
#include "lsb_addr_comp.h"

extern char *errmsg();
extern int errno;
extern int krb_debug;

/* static storage */

static u_long cksum;
static C_Block big_cksum[2];
static struct timeval msg_time;
static u_char msg_time_5ms;
static long msg_time_sec;

/*
 * krb_mk_safe() constructs an AUTH_MSG_SAFE message.  It takes some
 * user data "in" of "length" bytes and creates a packet in "out"
 * consisting of the user data, a timestamp, and the sender's network
 * address, followed by a checksum computed on the above, using the
 * given "key".  The length of the resulting packet is returned.
 *
 * The "out" packet consists of:
 *
 * Size			Variable		Field
 * ----			--------		-----
 *
 * 1 byte		KRB_PROT_VERSION	protocol version number
 * 1 byte		AUTH_MSG_SAFE |		message type plus local
 *			HOST_BYTE_ORDER		byte order in low bit
 *
 * ===================== begin checksum ================================
 *
 * 4 bytes		length			length of user data
 * length		in			user data
 * 1 byte		msg_time_5ms		timestamp milliseconds
 * 4 bytes		sender->sin.addr.s_addr	sender's IP address
 *
 * 4 bytes		msg_time_sec or		timestamp seconds with
 *			-msg_time_sec		direction in sign bit
 *
 * ======================= end checksum ================================
 *
 * 16 bytes		big_cksum		quadratic checksum of
 *						above using "key"
 */

long krb_mk_safe(in,out,length,key,sender,receiver)
    u_char *in;			/* application data */
    u_char *out;		/*
				 * put msg here, leave room for header!
				 * breaks if in and out (header stuff)
				 * overlap
				 */
    u_long length;		/* of in data */
    C_Block *key;		/* encryption key for seed and ivec */
    struct sockaddr_in *sender;	/* sender address */
    struct sockaddr_in *receiver; /* receiver address */
{
    register u_char     *p,*q;

    /*
     * get the current time to use instead of a sequence #, since
     * process lifetime may be shorter than the lifetime of a session
     * key.
     */
    if (gettimeofday(&msg_time,(struct timezone *)0)) {
        return  -1;
    }
    msg_time_sec = (long) msg_time.tv_sec;
    msg_time_5ms = msg_time.tv_usec/5000; /* 5ms quanta */

    p = out;

    *p++ = KRB_PROT_VERSION;
    *p++ = AUTH_MSG_SAFE | HOST_BYTE_ORDER;

    q = p;			/* start for checksum stuff */
    /* stuff input length */
    bcopy((char *)&length,(char *)p,sizeof(length));
    p += sizeof(length);

    /* make all the stuff contiguous for checksum */
    bcopy((char *)in,(char *)p,(int) length);
    p += length;

    /* stuff time 5ms */
    bcopy((char *)&msg_time_5ms,(char *)p,sizeof(msg_time_5ms));
    p += sizeof(msg_time_5ms);

    /* stuff source address */
    bcopy((char *) &sender->sin_addr.s_addr,(char *)p,
          sizeof(sender->sin_addr.s_addr));
    p += sizeof(sender->sin_addr.s_addr);

    /*
     * direction bit is the sign bit of the timestamp.  Ok until
     * 2038??
     */
    /* For compatibility with broken old code, compares are done in VAX
       byte order (LSBFIRST) */
    if (lsb_net_ulong_less(sender->sin_addr.s_addr, /* src < recv */
			  receiver->sin_addr.s_addr)==-1)
        msg_time_sec =  -msg_time_sec;
    else if (lsb_net_ulong_less(sender->sin_addr.s_addr,
				receiver->sin_addr.s_addr)==0)
        if (lsb_net_ushort_less(sender->sin_port,receiver->sin_port) == -1)
            msg_time_sec = -msg_time_sec;
    /*
     * all that for one tiny bit!  Heaven help those that talk to
     * themselves.
     */

    /* stuff time sec */
    bcopy((char *)&msg_time_sec,(char *)p,sizeof(msg_time_sec));
    p += sizeof(msg_time_sec);

#ifdef NOENCRYPTION
    cksum = 0;
    bzero(big_cksum, sizeof(big_cksum));
#else
    cksum=quad_cksum((C_Block *)q,big_cksum,p-q,2,key);
#endif
    if (krb_debug)
        printf("\ncksum = %lu",cksum);

    /* stuff checksum */
    bcopy((char *)big_cksum,(char *)p,sizeof(big_cksum));
    p += sizeof(big_cksum);

    return ((long)(p - out));	/* resulting size */

}
