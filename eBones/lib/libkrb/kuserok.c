/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * kuserok: check if a kerberos principal has
 * access to a local account
 *
 *	from: kuserok.c,v 4.5 89/01/23 09:25:21 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$FreeBSD$";
#endif	lint
#endif

#include <krb.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <strings.h>

#define OK 0
#define NOTOK 1
#define MAX_USERNAME 10

/*
 * Given a Kerberos principal "kdata", and a local username "luser",
 * determine whether user is authorized to login according to the
 * authorization file ("~luser/.klogin" by default).  Returns OK
 * if authorized, NOTOK if not authorized.
 *
 * If there is no account for "luser" on the local machine, returns
 * NOTOK.  If there is no authorization file, and the given Kerberos
 * name "kdata" translates to the same name as "luser" (using
 * krb_kntoln()), returns OK.  Otherwise, if the authorization file
 * can't be accessed, returns NOTOK.  Otherwise, the file is read for
 * a matching principal name, instance, and realm.  If one is found,
 * returns OK, if none is found, returns NOTOK.
 *
 * The file entries are in the format:
 *
 *	name.instance@realm
 *
 * one entry per line.
 *
 * The ATHENA_COMPAT code supports old-style Athena ~luser/.klogin
 * file entries.  See the file "kparse.c".
 */

#ifdef ATHENA_COMPAT

#include <kparse.h>

/*
 * The parmtable defines the keywords we will recognize with their
 * default values, and keeps a pointer to the found value.  The found
 * value should be filled in with strsave(), since FreeParameterSet()
 * will release memory for all non-NULL found strings.
 *
*** NOTE WELL! ***
 *
 * The table below is very nice, but we cannot hard-code a default for the
 * realm: we have to get the realm via krb_get_lrealm().  Even though the
 * default shows as "from krb_get_lrealm, below", it gets changed in
 * kuserok to whatever krb_get_lrealm() tells us.  That code assumes that
 * the realm will be the entry number in the table below, so if you
 * change the order of the entries below, you have to change the
 * #definition of REALM_SCRIPT to reflect it.
 */
#define REALM_SUBSCRIPT 1
parmtable kparm[] = {

/* keyword	default 			found value     */
{"user",	"", 				(char *) NULL},
{"realm",	"see krb_get_lrealm, below",	(char *) NULL},
{"instance",	 "",				(char *) NULL},
};
#define KPARMS kparm,PARMCOUNT(kparm)
#endif ATHENA_COMPAT

int
kuserok(kdata, luser)
    AUTH_DAT *kdata;
    char   *luser;
{
    struct stat sbuf;
    struct passwd *pwd;
    char pbuf[MAXPATHLEN];
    int isok = NOTOK, rc;
    FILE *fp;
    char kuser[MAX_USERNAME];
    char principal[ANAME_SZ], inst[INST_SZ], realm[REALM_SZ];
    char linebuf[BUFSIZ];
    char *newline;
    int gobble;
#ifdef ATHENA_COMPAT
    char local_realm[REALM_SZ];
#endif ATHENA_COMPAT

    /* no account => no access */
    if ((pwd = getpwnam(luser)) == NULL) {
	return(NOTOK);
    }
    (void) strcpy(pbuf, pwd->pw_dir);
    (void) strcat(pbuf, "/.klogin");

    if (access(pbuf, F_OK)) {	 /* not accessible */
	/*
	 * if he's trying to log in as himself, and there is no .klogin file,
	 * let him.  To find out, call
	 * krb_kntoln to convert the triple in kdata to a name which we can
	 * string compare.
	 */
	if (!krb_kntoln(kdata, kuser) && (strcmp(kuser, luser) == 0)) {
	    return(OK);
	}
    }
    /* open ~/.klogin */
    if ((fp = fopen(pbuf, "r")) == NULL) {
	return(NOTOK);
    }
    /*
     * security:  if the user does not own his own .klogin file,
     * do not grant access
     */
    if (fstat(fileno(fp), &sbuf)) {
	fclose(fp);
	return(NOTOK);
    }
    if (sbuf.st_uid != pwd->pw_uid) {
	fclose(fp);
	return(NOTOK);
    }

#ifdef ATHENA_COMPAT
    /* Accept old-style .klogin files */

    /*
     * change the default realm from the hard-coded value to the
     * accepted realm that Kerberos specifies.
     */
    rc = krb_get_lrealm(local_realm, 1);
    if (rc == KSUCCESS)
	kparm[REALM_SUBSCRIPT].defvalue = local_realm;
    else
	return (rc);

    /* check each line */
    while ((isok != OK) && (rc = fGetParameterSet(fp, KPARMS)) != PS_EOF) {
	switch (rc) {
	case PS_BAD_KEYWORD:
	case PS_SYNTAX:
	    while (((gobble = fGetChar(fp)) != EOF) && (gobble != '\n'));
	    break;

	case PS_OKAY:
	    isok = (ParmCompare(KPARMS, "user", kdata->pname) ||
		    ParmCompare(KPARMS, "instance", kdata->pinst) ||
		    ParmCompare(KPARMS, "realm", kdata->prealm));
	    break;

	default:
	    break;
	}
	FreeParameterSet(kparm, PARMCOUNT(kparm));
    }
    /* reset the stream for parsing new-style names, if necessary */
    rewind(fp);
#endif ATHENA_COMPAT

    /* check each line */
    while ((isok != OK) && (fgets(linebuf, BUFSIZ, fp) != NULL)) {
	/* null-terminate the input string */
	linebuf[BUFSIZ-1] = '\0';
	newline = NULL;
	/* nuke the newline if it exists */
	if ((newline = index(linebuf, '\n')))
	    *newline = '\0';
	rc = kname_parse(principal, inst, realm, linebuf);
	if (rc == KSUCCESS) {
	    isok = (strncmp(kdata->pname, principal, ANAME_SZ) ||
		    strncmp(kdata->pinst, inst, INST_SZ) ||
		    strncmp(kdata->prealm, realm, REALM_SZ));
	}
	/* clean up the rest of the line if necessary */
	if (!newline)
	    while (((gobble = getc(fp)) != EOF) && gobble != '\n');
    }
    fclose(fp);
    return(isok);
}
