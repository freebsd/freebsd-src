/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: _service_key.c,v 4.10 90/03/10 19:06:56 jon Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char *rcsid =
"$FreeBSD$";
#endif /* lint */
#endif

#include <krb.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>

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


/*ARGSUSED */
int
read_service_key(service,instance,realm,kvno,file,key)
    char *service;              /* Service Name */
    char *instance;             /* Instance name or "*" */
    char *realm;                /* Realm */
    int kvno;                   /* Key version number */
    char *file;                 /* Filename */
    char *key;                  /* Pointer to key to be filled in */
{
    char serv[SNAME_SZ];
    char inst[INST_SZ];
    char rlm[REALM_SZ];
    unsigned char vno;          /* Key version number */
    int wcard;

    int stab, open();

    if ((stab = open(file, 0, 0)) < NULL)
        return(KFAILURE);

    wcard = (instance[0] == '*') && (instance[1] == '\0');

    while(getst(stab,serv,SNAME_SZ) > 0) { /* Read sname */
        (void) getst(stab,inst,INST_SZ); /* Instance */
        (void) getst(stab,rlm,REALM_SZ); /* Realm */
        /* Vers number */
        if (read(stab,(char *)&vno,1) != 1) {
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
            (void) strncpy(instance,inst,INST_SZ);
        /* Is this the right realm */
#ifdef ATHENA_COMPAT
	/* XXX For backward compatibility:  if keyfile says "Athena"
	   and caller wants "ATHENA.MIT.EDU", call it a match */
        if (strcmp(rlm,realm) &&
	    (strcmp(rlm,"Athena") ||
	     strcmp(realm,"ATHENA.MIT.EDU")))
	    continue;
#else /* ! ATHENA_COMPAT */
        if (strcmp(rlm,realm))
	    continue;
#endif /* ATHENA_COMPAT */

        /* How about the key version number */
        if (kvno && kvno != (int) vno)
            continue;

        (void) close(stab);
        return(KSUCCESS);
    }

    /* Can't find the requested service */
    (void) close(stab);
    return(KFAILURE);
}
