/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"

RCSID("$Id: read_service_key.c,v 1.8 1997/03/23 03:53:16 joda Exp $");

/*
 * The private keys for servers on a given host are stored in a
 * "srvtab" file (typically "/etc/srvtab").  This routine extracts
 * a given server's key from the file.
 *
 * read_service_key() takes the server's name ("service"), "instance",
 * and "realm" and a key version number "kvno", and looks in the given
 * "file" for the corresponding entry, and if found, returns the entry's
 * key field in "key".
 * 
 * If "instance" contains the string "*", then it will match
 * any instance, and the chosen instance will be copied to that
 * string.  For this reason it is important that the there is enough
 * space beyond the "*" to receive the entry.
 *
 * If "kvno" is 0, it is treated as a wild card and the first
 * matching entry regardless of the "vno" field is returned.
 *
 * This routine returns KSUCCESS on success, otherwise KFAILURE.
 *
 * The format of each "srvtab" entry is as follows:
 *
 * Size			Variable		Field in file
 * ----			--------		-------------
 * string		serv			server name
 * string		inst			server instance
 * string		realm			server realm
 * 1 byte		vno			server key version #
 * 8 bytes		key			server's key
 * ...			...			...
 */


int
read_service_key(char *service,	/* Service Name */
		 char *instance, /* Instance name or "*" */
		 char *realm,	/* Realm */
		 int kvno,	/* Key version number */
		 char *file,	/* Filename */
		 char *key)	/* Pointer to key to be filled in */
{
    char serv[SNAME_SZ];
    char inst[INST_SZ];
    char rlm[REALM_SZ];
    unsigned char vno;          /* Key version number */
    int wcard;

    int stab;

    if ((stab = open(file, O_RDONLY, 0)) < 0)
        return(KFAILURE);

    wcard = (instance[0] == '*') && (instance[1] == '\0');

    while (getst(stab,serv,SNAME_SZ) > 0) { /* Read sname */
        getst(stab,inst,INST_SZ); /* Instance */
        getst(stab,rlm,REALM_SZ); /* Realm */
        /* Vers number */
        if (read(stab, &vno, 1) != 1) {
	    close(stab);
            return(KFAILURE);
	}
        /* Key */
        if (read(stab,key,8) != 8) {
	    close(stab);
            return(KFAILURE);
	}
        /* Is this the right service */
        if (strcmp(serv,service))
            continue;
        /* How about instance */
        if (!wcard && strcmp(inst,instance))
            continue;
        if (wcard)
            strncpy(instance,inst,INST_SZ);
        /* Is this the right realm */
        if (strcmp(rlm,realm)) 
	    continue;

        /* How about the key version number */
        if (kvno && kvno != (int) vno)
            continue;

        close(stab);
        return(KSUCCESS);
    }

    /* Can't find the requested service */
    close(stab);
    return(KFAILURE);
}
