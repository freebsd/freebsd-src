/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: decomp_ticket.c,v 4.12 89/05/16 18:44:46 jtkohl Exp $
 *	$Id: decomp_ticket.c,v 1.3 1995/07/18 16:38:15 mark Exp $
 */

#if 0
#ifndef lint
static char *rcsid =
"$Id: decomp_ticket.c,v 1.3 1995/07/18 16:38:15 mark Exp $";
#endif /* lint */
#endif

#include <stdio.h>
#include <des.h>
#include <krb.h>
#include <prot.h>
#include <strings.h>

/*
 * This routine takes a ticket and pointers to the variables that
 * should be filled in based on the information in the ticket.  It
 * fills in values for its arguments.
 *
 * Note: if the client realm field in the ticket is the null string,
 * then the "prealm" variable is filled in with the local realm (as
 * defined by KRB_REALM).
 *
 * If the ticket byte order is different than the host's byte order
 * (as indicated by the byte order bit of the "flags" field), then
 * the KDC timestamp "time_sec" is byte-swapped.  The other fields
 * potentially affected by byte order, "paddress" and "session" are
 * not byte-swapped.
 *
 * The routine returns KFAILURE if any of the "pname", "pinstance",
 * or "prealm" fields is too big, otherwise it returns KSUCCESS.
 *
 * The corresponding routine to generate tickets is create_ticket.
 * When changes are made to this routine, the corresponding changes
 * should also be made to that file.
 *
 * See create_ticket.c for the format of the ticket packet.
 */

int decomp_ticket(KTEXT tkt, unsigned char *flags, char *pname,
    char *pinstance, char *prealm, unsigned long *paddress, des_cblock session,
    int *life, unsigned long *time_sec, char *sname, char *sinstance,
    des_cblock key, des_key_schedule key_s)
{
    static int tkt_swap_bytes;
    unsigned char *uptr;
    char *ptr = (char *)tkt->dat;

#ifndef NOENCRYPTION
    pcbc_encrypt((des_cblock *)tkt->dat,(des_cblock *)tkt->dat,
	(long)tkt->length,key_s,(des_cblock *)key,DECRYPT);
#endif /* ! NOENCRYPTION */

    *flags = *ptr;              /* get flags byte */
    ptr += sizeof(*flags);
    tkt_swap_bytes = 0;
    if (HOST_BYTE_ORDER != ((*flags >> K_FLAG_ORDER)& 1))
        tkt_swap_bytes++;

    if (strlen(ptr) > ANAME_SZ)
        return(KFAILURE);
    (void) strcpy(pname,ptr);   /* pname */
    ptr += strlen(pname) + 1;

    if (strlen(ptr) > INST_SZ)
        return(KFAILURE);
    (void) strcpy(pinstance,ptr); /* instance */
    ptr += strlen(pinstance) + 1;

    if (strlen(ptr) > REALM_SZ)
        return(KFAILURE);
    (void) strcpy(prealm,ptr);  /* realm */
    ptr += strlen(prealm) + 1;
    /* temporary hack until realms are dealt with properly */
    if (*prealm == 0)
        (void) strcpy(prealm,KRB_REALM);

    bcopy(ptr,(char *)paddress,4); /* net address */
    ptr += 4;

    bcopy(ptr,(char *)session,8); /* session key */
    ptr+= 8;
#ifdef notdef /* DONT SWAP SESSION KEY spm 10/22/86 */
    if (tkt_swap_bytes)
        swap_C_Block(session);
#endif

    /* get lifetime, being certain we don't get negative lifetimes */
    uptr = (unsigned char *) ptr++;
    *life = (int) *uptr;

    bcopy(ptr,(char *) time_sec,4); /* issue time */
    ptr += 4;
    if (tkt_swap_bytes)
        swap_u_long(*time_sec);

    (void) strcpy(sname,ptr);   /* service name */
    ptr += 1 + strlen(sname);

    (void) strcpy(sinstance,ptr); /* instance */
    ptr += 1 + strlen(sinstance);
    return(KSUCCESS);
}
