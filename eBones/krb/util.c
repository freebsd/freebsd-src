/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Miscellaneous debug printing utilities
 *
 *	from: util.c,v 4.8 89/01/17 22:02:08 wesommer Exp $
 *	$Id: util.c,v 1.2 1994/07/19 19:26:31 g89r4222 Exp $
 */

#ifndef	lint
static char rcsid[] =
"$Id: util.c,v 1.2 1994/07/19 19:26:31 g89r4222 Exp $";
#endif	lint

#include <krb.h>
#include <des.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

/*
 * Print some of the contents of the given authenticator structure
 * (AUTH_DAT defined in "krb.h").  Fields printed are:
 *
 * pname, pinst, prealm, netaddr, flags, cksum, timestamp, session
 */

ad_print(x)
AUTH_DAT	*x;
{
	struct in_addr	in;

	/* Print the contents of an auth_dat struct. */
	in.s_addr = x->address;
	printf("\n%s %s %s %s flags %u cksum 0x%X\n\ttkt_tm 0x%X sess_key",
           x->pname, x->pinst, x->prealm, inet_ntoa(in), x->k_flags,
           x->checksum, x->time_sec);

	printf("[8] =");
#ifdef NOENCRYPTION
	placebo_cblock_print(x->session);
#else
	des_cblock_print_file(x->session,stdout);
#endif
	/* skip reply for now */
}

/*
 * Print in hex the 8 bytes of the given session key.
 *
 * Printed format is:  " 0x { x, x, x, x, x, x, x, x }"
 */

#ifdef NOENCRYPTION
placebo_cblock_print(x)
    des_cblock x;
{
    unsigned char *y = (unsigned char *) x;
    register int i = 0;

    printf(" 0x { ");

    while (i++ <8) {
        printf("%x",*y++);
        if (i<8) printf(", ");
    }
    printf(" }");
}
#endif
