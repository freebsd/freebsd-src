/*
 * ntpq_ops.c - subroutines which are called to perform operations by ntpq
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>

#include "ntpq.h"
#include "ntp_stdlib.h"

extern char *	chosts[];
extern char currenthost[];
extern int	numhosts;
int 	maxhostlen;

/*
 * Declarations for command handlers in here
 */
static	int checkassocid	P((u_int32));
static	char *	strsave 	P((char *));
static	struct varlist *findlistvar P((struct varlist *, char *));
static	void	doaddvlist	P((struct varlist *, char *));
static	void	dormvlist	P((struct varlist *, char *));
static	void	doclearvlist	P((struct varlist *));
static	void	makequerydata	P((struct varlist *, int *, char *));
static	int doquerylist P((struct varlist *, int, int, int, u_short *, int *, char **));
static	void	doprintvlist	P((struct varlist *, FILE *));
static	void	addvars 	P((struct parse *, FILE *));
static	void	rmvars		P((struct parse *, FILE *));
static	void	clearvars	P((struct parse *, FILE *));
static	void	showvars	P((struct parse *, FILE *));
static	int dolist		P((struct varlist *, int, int, int, FILE *));
static	void	readlist	P((struct parse *, FILE *));
static	void	writelist	P((struct parse *, FILE *));
static	void	readvar 	P((struct parse *, FILE *));
static	void	writevar	P((struct parse *, FILE *));
static	void	clocklist	P((struct parse *, FILE *));
static	void	clockvar	P((struct parse *, FILE *));
static	int findassidrange	P((u_int32, u_int32, int *, int *));
static	void	mreadlist	P((struct parse *, FILE *));
static	void	mreadvar	P((struct parse *, FILE *));
static	int dogetassoc	P((FILE *));
static	void	printassoc	P((int, FILE *));
static	void	associations	P((struct parse *, FILE *));
static	void	lassociations	P((struct parse *, FILE *));
static	void	passociations	P((struct parse *, FILE *));
static	void	lpassociations	P((struct parse *, FILE *));

#ifdef	UNUSED
static	void	radiostatus P((struct parse *, FILE *));
#endif	/* UNUSED */

static	void	pstatus 	P((struct parse *, FILE *));
static	long	when		P((l_fp *, l_fp *, l_fp *));
static	char *	prettyinterval	P((char *, long));
static	int doprintpeers	P((struct varlist *, int, int, int, char *, FILE *, int));
static	int dogetpeers	P((struct varlist *, int, FILE *, int));
static	void	dopeers 	P((int, FILE *, int));
static	void	peers		P((struct parse *, FILE *));
static	void	lpeers		P((struct parse *, FILE *));
static	void	doopeers	P((int, FILE *, int));
static	void	opeers		P((struct parse *, FILE *));
static	void	lopeers 	P((struct parse *, FILE *));


/*
 * Commands we understand.	Ntpdc imports this.
 */
struct xcmd opcmds[] = {
	{ "associations", associations, {  NO, NO, NO, NO },
	  { "", "", "", "" },
	  "print list of association ID's and statuses for the server's peers" },
	{ "passociations", passociations,   {  NO, NO, NO, NO },
	  { "", "", "", "" },
	  "print list of associations returned by last associations command" },
	{ "lassociations", lassociations,   {  NO, NO, NO, NO },
	  { "", "", "", "" },
	  "print list of associations including all client information" },
	{ "lpassociations", lpassociations, {  NO, NO, NO, NO },
	  { "", "", "", "" },
	  "print last obtained list of associations, including client information" },
	{ "addvars",    addvars,    { NTP_STR, NO, NO, NO },
	  { "name[=value][,...]", "", "", "" },
	  "add variables to the variable list or change their values" },
	{ "rmvars", rmvars,     { NTP_STR, NO, NO, NO },
	  { "name[,...]", "", "", "" },
	  "remove variables from the variable list" },
	{ "clearvars",  clearvars,  { NO, NO, NO, NO },
	  { "", "", "", "" },
	  "remove all variables from the variable list" },
	{ "showvars",   showvars,   { NO, NO, NO, NO },
	  { "", "", "", "" },
	  "print variables on the variable list" },
	{ "readlist",   readlist,   { OPT|NTP_UINT, NO, NO, NO },
	  { "assocID", "", "", "" },
	  "read the system or peer variables included in the variable list" },
	{ "rl",     readlist,   { OPT|NTP_UINT, NO, NO, NO },
	  { "assocID", "", "", "" },
	  "read the system or peer variables included in the variable list" },
	{ "writelist",  writelist,  { OPT|NTP_UINT, NO, NO, NO },
	  { "assocID", "", "", "" },
	  "write the system or peer variables included in the variable list" },
	{ "readvar",    readvar,    { OPT|NTP_UINT, OPT|NTP_STR, NO, NO },
	  { "assocID", "name=value[,...]", "", "" },
	  "read system or peer variables" },
	{ "rv",     readvar,    { OPT|NTP_UINT, OPT|NTP_STR, NO, NO },
	  { "assocID", "name=value[,...]", "", "" },
	  "read system or peer variables" },
	{ "writevar",   writevar,   { NTP_UINT, NTP_STR, NO, NO },
	  { "assocID", "name=value,[...]", "", "" },
	  "write system or peer variables" },
	{ "mreadlist",  mreadlist,  { NTP_UINT, NTP_UINT, NO, NO },
	  { "assocID", "assocID", "", "" },
	  "read the peer variables in the variable list for multiple peers" },
	{ "mrl",    mreadlist,  { NTP_UINT, NTP_UINT, NO, NO },
	  { "assocID", "assocID", "", "" },
	  "read the peer variables in the variable list for multiple peers" },
	{ "mreadvar",   mreadvar,   { NTP_UINT, NTP_UINT, OPT|NTP_STR, NO },
	  { "assocID", "assocID", "name=value[,...]", "" },
	  "read peer variables from multiple peers" },
	{ "mrv",    mreadvar,   { NTP_UINT, NTP_UINT, OPT|NTP_STR, NO },
	  { "assocID", "assocID", "name=value[,...]", "" },
	  "read peer variables from multiple peers" },
	{ "clocklist",  clocklist,  { OPT|NTP_UINT, NO, NO, NO },
	  { "assocID", "", "", "" },
	  "read the clock variables included in the variable list" },
	{ "cl",     clocklist,  { OPT|NTP_UINT, NO, NO, NO },
	  { "assocID", "", "", "" },
	  "read the clock variables included in the variable list" },
	{ "clockvar",   clockvar,   { OPT|NTP_UINT, OPT|NTP_STR, NO, NO },
	  { "assocID", "name=value[,...]", "", "" },
	  "read clock variables" },
	{ "cv",     clockvar,   { OPT|NTP_UINT, OPT|NTP_STR, NO, NO },
	  { "assocID", "name=value[,...]", "", "" },
	  "read clock variables" },
	{ "pstatus",    pstatus,    { NTP_UINT, NO, NO, NO },
	  { "assocID", "", "", "" },
	  "print status information returned for a peer" },
	{ "peers",  peers,      { OPT|IP_VERSION, NO, NO, NO },
	  { "-4|-6", "", "", "" },
	  "obtain and print a list of the server's peers [IP version]" },
	{ "lpeers", lpeers,     { OPT|IP_VERSION, NO, NO, NO },
	  { "-4|-6", "", "", "" },
	  "obtain and print a list of all peers and clients [IP version]" },
	{ "opeers", opeers,     { OPT|IP_VERSION, NO, NO, NO },
	  { "-4|-6", "", "", "" },
	  "print peer list the old way, with dstadr shown rather than refid [IP version]" },
	{ "lopeers", lopeers,   { OPT|IP_VERSION, NO, NO, NO },
	  { "-4|-6", "", "", "" },
	  "obtain and print a list of all peers and clients showing dstadr [IP version]" },
	{ 0,		0,		{ NO, NO, NO, NO },
	  { "-4|-6", "", "", "" }, "" }
};


/*
 * Variable list data space
 */
#define MAXLIST 	64	/* maximum number of variables in list */
#define LENHOSTNAME 256 /* host name is 256 characters long */
/*
 * Old CTL_PST defines for version 2.
 */
#define OLD_CTL_PST_CONFIG			0x80
#define OLD_CTL_PST_AUTHENABLE		0x40
#define OLD_CTL_PST_AUTHENTIC		0x20
#define OLD_CTL_PST_REACH			0x10
#define OLD_CTL_PST_SANE			0x08
#define OLD_CTL_PST_DISP			0x04
#define OLD_CTL_PST_SEL_REJECT		0
#define OLD_CTL_PST_SEL_SELCAND 	1
#define OLD_CTL_PST_SEL_SYNCCAND	2
#define OLD_CTL_PST_SEL_SYSPEER 	3


char flash2[] = " .+*    "; /* flash decode for version 2 */
char flash3[] = " x.-+#*o"; /* flash decode for peer status version 3 */

struct varlist {
	char *name;
	char *value;
} varlist[MAXLIST] = { { 0, 0 } };

/*
 * Imported from ntpq.c
 */
extern int showhostnames;
extern int rawmode;
extern struct servent *server_entry;
extern struct association assoc_cache[];
extern int numassoc;
extern u_char pktversion;
extern struct ctl_var peer_var[];

/*
 * For quick string comparisons
 */
#define STREQ(a, b) (*(a) == *(b) && strcmp((a), (b)) == 0)


/*
 * checkassocid - return the association ID, checking to see if it is valid
 */
static int
checkassocid(
	u_int32 value
	)
{
	if (value == 0 || value >= 65536) {
		(void) fprintf(stderr, "***Invalid association ID specified\n");
		return 0;
	}
	return (int)value;
}


/*
 * strsave - save a string
 * XXX - should be in libntp.a
 */
static char *
strsave(
	char *str
	)
{
	char *cp;
	u_int len;

	len = strlen(str) + 1;
	if ((cp = (char *)malloc(len)) == NULL) {
		(void) fprintf(stderr, "Malloc failed!!\n");
		exit(1);
	}

	memmove(cp, str, len);
	return (cp);
}


/*
 * findlistvar - look for the named variable in a list and return if found
 */
static struct varlist *
findlistvar(
	struct varlist *list,
	char *name
	)
{
	register struct varlist *vl;

	for (vl = list; vl < list + MAXLIST && vl->name != 0; vl++)
		if (STREQ(name, vl->name))
		return vl;
	if (vl < list + MAXLIST)
		return vl;
	return (struct varlist *)0;
}


/*
 * doaddvlist - add variable(s) to the variable list
 */
static void
doaddvlist(
	struct varlist *vlist,
	char *vars
	)
{
	register struct varlist *vl;
	int len;
	char *name;
	char *value;

	len = strlen(vars);
	while (nextvar(&len, &vars, &name, &value)) {
		vl = findlistvar(vlist, name);
		if (vl == 0) {
			(void) fprintf(stderr, "Variable list full\n");
			return;
		}

		if (vl->name == 0) {
			vl->name = strsave(name);
		} else if (vl->value != 0) {
			free(vl->value);
			vl->value = 0;
		}

		if (value != 0)
			vl->value = strsave(value);
	}
}


/*
 * dormvlist - remove variable(s) from the variable list
 */
static void
dormvlist(
	struct varlist *vlist,
	char *vars
	)
{
	register struct varlist *vl;
	int len;
	char *name;
	char *value;

	len = strlen(vars);
	while (nextvar(&len, &vars, &name, &value)) {
		vl = findlistvar(vlist, name);
		if (vl == 0 || vl->name == 0) {
			(void) fprintf(stderr, "Variable `%s' not found\n",
				       name);
		} else {
			free((void *)vl->name);
			if (vl->value != 0)
			    free(vl->value);
			for ( ; (vl+1) < (varlist+MAXLIST)
				      && (vl+1)->name != 0; vl++) {
				vl->name = (vl+1)->name;
				vl->value = (vl+1)->value;
			}
			vl->name = vl->value = 0;
		}
	}
}


/*
 * doclearvlist - clear a variable list
 */
static void
doclearvlist(
	struct varlist *vlist
	)
{
	register struct varlist *vl;

	for (vl = vlist; vl < vlist + MAXLIST && vl->name != 0; vl++) {
		free((void *)vl->name);
		vl->name = 0;
		if (vl->value != 0) {
			free(vl->value);
			vl->value = 0;
		}
	}
}


/*
 * makequerydata - form a data buffer to be included with a query
 */
static void
makequerydata(
	struct varlist *vlist,
	int *datalen,
	char *data
	)
{
	register struct varlist *vl;
	register char *cp, *cpend;
	register int namelen, valuelen;
	register int totallen;

	cp = data;
	cpend = data + *datalen;

	for (vl = vlist; vl < vlist + MAXLIST && vl->name != 0; vl++) {
		namelen = strlen(vl->name);
		if (vl->value == 0)
			valuelen = 0;
		else
			valuelen = strlen(vl->value);
		totallen = namelen + valuelen + (valuelen != 0) + (cp != data);
		if (cp + totallen > cpend)
			break;

		if (cp != data)
			*cp++ = ',';
		memmove(cp, vl->name, (unsigned)namelen);
		cp += namelen;
		if (valuelen != 0) {
			*cp++ = '=';
			memmove(cp, vl->value, (unsigned)valuelen);
			cp += valuelen;
		}
	}
	*datalen = cp - data;
}


/*
 * doquerylist - send a message including variables in a list
 */
static int
doquerylist(
	struct varlist *vlist,
	int op,
	int associd,
	int auth,
	u_short *rstatus,
	int *dsize,
	char **datap
	)
{
	char data[CTL_MAX_DATA_LEN];
	int datalen;

	datalen = sizeof(data);
	makequerydata(vlist, &datalen, data);

	return doquery(op, associd, auth, datalen, data, rstatus,
			   dsize, datap);
}


/*
 * doprintvlist - print the variables on a list
 */
static void
doprintvlist(
	struct varlist *vlist,
	FILE *fp
	)
{
	register struct varlist *vl;

	if (vlist->name == 0) {
		(void) fprintf(fp, "No variables on list\n");
	} else {
		for (vl = vlist; vl < vlist + MAXLIST && vl->name != 0; vl++) {
			if (vl->value == 0) {
				(void) fprintf(fp, "%s\n", vl->name);
			} else {
				(void) fprintf(fp, "%s=%s\n",
						   vl->name, vl->value);
			}
		}
	}
}


/*
 * addvars - add variables to the variable list
 */
/*ARGSUSED*/
static void
addvars(
	struct parse *pcmd,
	FILE *fp
	)
{
	doaddvlist(varlist, pcmd->argval[0].string);
}


/*
 * rmvars - remove variables from the variable list
 */
/*ARGSUSED*/
static void
rmvars(
	struct parse *pcmd,
	FILE *fp
	)
{
	dormvlist(varlist, pcmd->argval[0].string);
}


/*
 * clearvars - clear the variable list
 */
/*ARGSUSED*/
static void
clearvars(
	struct parse *pcmd,
	FILE *fp
	)
{
	doclearvlist(varlist);
}


/*
 * showvars - show variables on the variable list
 */
/*ARGSUSED*/
static void
showvars(
	struct parse *pcmd,
	FILE *fp
	)
{
	doprintvlist(varlist, fp);
}


/*
 * dolist - send a request with the given list of variables
 */
static int
dolist(
	struct varlist *vlist,
	int associd,
	int op,
	int type,
	FILE *fp
	)
{
	char *datap;
	int res;
	int dsize;
	u_short rstatus;

	res = doquerylist(vlist, op, associd, 0, &rstatus, &dsize, &datap);

	if (res != 0)
		return 0;

	if (numhosts > 1)
		(void) fprintf(fp, "server=%s ", currenthost);
	if (dsize == 0) {
		if (associd == 0)
			(void) fprintf(fp, "No system%s variables returned\n",
				   (type == TYPE_CLOCK) ? " clock" : "");
		else
			(void) fprintf(fp,
				   "No information returned for%s association %u\n",
				   (type == TYPE_CLOCK) ? " clock" : "", associd);
		return 1;
	}

	(void) fprintf(fp,"assID=%d ",associd);
	printvars(dsize, datap, (int)rstatus, type, fp);
	return 1;
}


/*
 * readlist - send a read variables request with the variables on the list
 */
static void
readlist(
	struct parse *pcmd,
	FILE *fp
	)
{
	int associd;

	if (pcmd->nargs == 0) {
		associd = 0;
	} else {
	  /* HMS: I think we want the u_int32 target here, not the u_long */
		if (pcmd->argval[0].uval == 0)
			associd = 0;
		else if ((associd = checkassocid(pcmd->argval[0].uval)) == 0)
			return;
	}

	(void) dolist(varlist, associd, CTL_OP_READVAR,
			  (associd == 0) ? TYPE_SYS : TYPE_PEER, fp);
}


/*
 * writelist - send a write variables request with the variables on the list
 */
static void
writelist(
	struct parse *pcmd,
	FILE *fp
	)
{
	char *datap;
	int res;
	int associd;
	int dsize;
	u_short rstatus;

	if (pcmd->nargs == 0) {
		associd = 0;
	} else {
		/* HMS: Do we really want uval here? */
		if (pcmd->argval[0].uval == 0)
			associd = 0;
		else if ((associd = checkassocid(pcmd->argval[0].uval)) == 0)
			return;
	}

	res = doquerylist(varlist, CTL_OP_WRITEVAR, associd, 1, &rstatus,
			  &dsize, &datap);

	if (res != 0)
		return;

	if (numhosts > 1)
		(void) fprintf(fp, "server=%s ", currenthost);
	if (dsize == 0)
		(void) fprintf(fp, "done! (no data returned)\n");
	else {
		(void) fprintf(fp,"assID=%d ",associd);
		printvars(dsize, datap, (int)rstatus,
			  (associd != 0) ? TYPE_PEER : TYPE_SYS, fp);
	}
	return;
}


/*
 * readvar - send a read variables request with the specified variables
 */
static void
readvar(
	struct parse *pcmd,
	FILE *fp
	)
{
	int associd;
	struct varlist tmplist[MAXLIST];

	/* HMS: uval? */
	if (pcmd->nargs == 0 || pcmd->argval[0].uval == 0)
		associd = 0;
	else if ((associd = checkassocid(pcmd->argval[0].uval)) == 0)
		return;

	memset((char *)tmplist, 0, sizeof(tmplist));
	if (pcmd->nargs >= 2)
		doaddvlist(tmplist, pcmd->argval[1].string);

	(void) dolist(tmplist, associd, CTL_OP_READVAR,
			  (associd == 0) ? TYPE_SYS : TYPE_PEER, fp);

	doclearvlist(tmplist);
}


/*
 * writevar - send a write variables request with the specified variables
 */
static void
writevar(
	struct parse *pcmd,
	FILE *fp
	)
{
	char *datap;
	int res;
	int associd;
	int dsize;
	u_short rstatus;
	struct varlist tmplist[MAXLIST];

	/* HMS: uval? */
	if (pcmd->argval[0].uval == 0)
		associd = 0;
	else if ((associd = checkassocid(pcmd->argval[0].uval)) == 0)
		return;

	memset((char *)tmplist, 0, sizeof(tmplist));
	doaddvlist(tmplist, pcmd->argval[1].string);

	res = doquerylist(tmplist, CTL_OP_WRITEVAR, associd, 1, &rstatus,
			  &dsize, &datap);

	doclearvlist(tmplist);

	if (res != 0)
		return;

	if (numhosts > 1)
		(void) fprintf(fp, "server=%s ", currenthost);
	if (dsize == 0)
		(void) fprintf(fp, "done! (no data returned)\n");
	else {
		(void) fprintf(fp,"assID=%d ",associd);
		printvars(dsize, datap, (int)rstatus,
			  (associd != 0) ? TYPE_PEER : TYPE_SYS, fp);
	}
	return;
}


/*
 * clocklist - send a clock variables request with the variables on the list
 */
static void
clocklist(
	struct parse *pcmd,
	FILE *fp
	)
{
	int associd;

	/* HMS: uval? */
	if (pcmd->nargs == 0) {
		associd = 0;
	} else {
		if (pcmd->argval[0].uval == 0)
			associd = 0;
		else if ((associd = checkassocid(pcmd->argval[0].uval)) == 0)
			return;
	}

	(void) dolist(varlist, associd, CTL_OP_READCLOCK, TYPE_CLOCK, fp);
}


/*
 * clockvar - send a clock variables request with the specified variables
 */
static void
clockvar(
	struct parse *pcmd,
	FILE *fp
	)
{
	int associd;
	struct varlist tmplist[MAXLIST];

	/* HMS: uval? */
	if (pcmd->nargs == 0 || pcmd->argval[0].uval == 0)
		associd = 0;
	else if ((associd = checkassocid(pcmd->argval[0].uval)) == 0)
		return;

	memset((char *)tmplist, 0, sizeof(tmplist));
	if (pcmd->nargs >= 2)
		doaddvlist(tmplist, pcmd->argval[1].string);

	(void) dolist(tmplist, associd, CTL_OP_READCLOCK, TYPE_CLOCK, fp);

	doclearvlist(tmplist);
}


/*
 * findassidrange - verify a range of association ID's
 */
static int
findassidrange(
	u_int32 assid1,
	u_int32 assid2,
	int *from,
	int *to
	)
{
	register int i;
	int f, t;

	if (assid1 == 0 || assid1 > 65535) {
		(void) fprintf(stderr,
				   "***Invalid association ID %lu specified\n", (u_long)assid1);
		return 0;
	}

	if (assid2 == 0 || assid2 > 65535) {
		(void) fprintf(stderr,
				   "***Invalid association ID %lu specified\n", (u_long)assid2);
		return 0;
	}

	f = t = -1;
	for (i = 0; i < numassoc; i++) {
		if (assoc_cache[i].assid == assid1) {
			f = i;
			if (t != -1)
				break;
		}
		if (assoc_cache[i].assid == assid2) {
			t = i;
			if (f != -1)
				break;
		}
	}

	if (f == -1 || t == -1) {
		(void) fprintf(stderr,
				   "***Association ID %lu not found in list\n",
				   (f == -1) ? (u_long)assid1 : (u_long)assid2);
		return 0;
	}

	if (f < t) {
		*from = f;
		*to = t;
	} else {
		*from = t;
		*to = f;
	}
	return 1;
}



/*
 * mreadlist - send a read variables request for multiple associations
 */
static void
mreadlist(
	struct parse *pcmd,
	FILE *fp
	)
{
	int i;
	int from;
	int to;

	/* HMS: uval? */
	if (!findassidrange(pcmd->argval[0].uval, pcmd->argval[1].uval,
				&from, &to))
		return;

	for (i = from; i <= to; i++) {
		if (i != from)
			(void) fprintf(fp, "\n");
		if (!dolist(varlist, (int)assoc_cache[i].assid,
				CTL_OP_READVAR, TYPE_PEER, fp))
			return;
	}
	return;
}


/*
 * mreadvar - send a read variables request for multiple associations
 */
static void
mreadvar(
	struct parse *pcmd,
	FILE *fp
	)
{
	int i;
	int from;
	int to;
	struct varlist tmplist[MAXLIST];

	/* HMS: uval? */
	if (!findassidrange(pcmd->argval[0].uval, pcmd->argval[1].uval,
				&from, &to))
		return;

	memset((char *)tmplist, 0, sizeof(tmplist));
	if (pcmd->nargs >= 3)
		doaddvlist(tmplist, pcmd->argval[2].string);

	for (i = from; i <= to; i++) {
		if (i != from)
			(void) fprintf(fp, "\n");
		if (!dolist(varlist, (int)assoc_cache[i].assid,
				CTL_OP_READVAR, TYPE_PEER, fp))
			break;
	}
	doclearvlist(tmplist);
	return;
}


/*
 * dogetassoc - query the host for its list of associations
 */
static int
dogetassoc(
	FILE *fp
	)
{
	char *datap;
	int res;
	int dsize;
	u_short rstatus;

	res = doquery(CTL_OP_READSTAT, 0, 0, 0, (char *)0, &rstatus,
			  &dsize, &datap);

	if (res != 0)
		return 0;

	if (dsize == 0) {
		if (numhosts > 1)
			(void) fprintf(fp, "server=%s ", currenthost);
		(void) fprintf(fp, "No association ID's returned\n");
		return 0;
	}

	if (dsize & 0x3) {
		if (numhosts > 1)
			(void) fprintf(stderr, "server=%s ", currenthost);
		(void) fprintf(stderr,
				   "***Server returned %d octets, should be multiple of 4\n",
				   dsize);
		return 0;
	}

	numassoc = 0;
	while (dsize > 0) {
		assoc_cache[numassoc].assid = ntohs(*((u_short *)datap));
		datap += sizeof(u_short);
		assoc_cache[numassoc].status = ntohs(*((u_short *)datap));
		datap += sizeof(u_short);
		if (++numassoc >= MAXASSOC)
			break;
		dsize -= sizeof(u_short) + sizeof(u_short);
	}
	sortassoc();
	return 1;
}


/*
 * printassoc - print the current list of associations
 */
static void
printassoc(
	int showall,
	FILE *fp
	)
{
	register char *bp;
	int i;
	u_char statval;
	int event;
	u_long event_count;
	const char *conf;
	const char *reach;
	const char *auth;
	const char *condition = "";
	const char *last_event;
	const char *cnt;
	char buf[128];

	if (numassoc == 0) {
		(void) fprintf(fp, "No association ID's in list\n");
		return;
	}

	/*
	 * Output a header
	 */
	(void) fprintf(fp,
			   "\nind assID status  conf reach auth condition  last_event cnt\n");
	(void) fprintf(fp,
			   "===========================================================\n");
	for (i = 0; i < numassoc; i++) {
		statval = (u_char) CTL_PEER_STATVAL(assoc_cache[i].status);
		if (!showall && !(statval & (CTL_PST_CONFIG|CTL_PST_REACH)))
			continue;
		event = CTL_PEER_EVENT(assoc_cache[i].status);
		event_count = CTL_PEER_NEVNT(assoc_cache[i].status);
		if (statval & CTL_PST_CONFIG)
			conf = "yes";
		else
			conf = "no";
		if (statval & CTL_PST_REACH || 1) {
			reach = "yes";
			if (statval & CTL_PST_AUTHENABLE) {
				if (statval & CTL_PST_AUTHENTIC)
					auth = "ok ";
				else
					auth = "bad";
			} else
				auth = "none";

			if (pktversion > NTP_OLDVERSION)
				switch (statval & 0x7) {
				case CTL_PST_SEL_REJECT:
					condition = "reject";
					break;
				case CTL_PST_SEL_SANE:
					condition = "falsetick";
					break;
				case CTL_PST_SEL_CORRECT:
					condition = "excess";
					break;
				case CTL_PST_SEL_SELCAND:
					condition = "outlyer";
					break;
				case CTL_PST_SEL_SYNCCAND:
					condition = "candidat";
					break;
				case CTL_PST_SEL_DISTSYSPEER:
					condition = "selected";
					break;
				case CTL_PST_SEL_SYSPEER:
					condition = "sys.peer";
					break;
				case CTL_PST_SEL_PPS:
					condition = "pps.peer";
					break;
				}
			else
				switch (statval & 0x3) {
				case OLD_CTL_PST_SEL_REJECT:
					if (!(statval & OLD_CTL_PST_SANE))
					condition = "insane";
					else if (!(statval & OLD_CTL_PST_DISP))
					condition = "hi_disp";
					else
					condition = "";
					break;
				case OLD_CTL_PST_SEL_SELCAND:
					condition = "sel_cand";
					break;
				case OLD_CTL_PST_SEL_SYNCCAND:
					condition = "sync_cand";
					break;
				case OLD_CTL_PST_SEL_SYSPEER:
					condition = "sys_peer";
					break;
				}

		} else {
			reach = "no";
			auth = condition = "";
		}

		switch (PEER_EVENT|event) {
			case EVNT_PEERIPERR:
			last_event = "IP error";
			break;
			case EVNT_PEERAUTH:
			last_event = "auth fail";
			break;
			case EVNT_UNREACH:
			last_event = "lost reach";
			break;
			case EVNT_REACH:
			last_event = "reachable";
			break;
			case EVNT_PEERCLOCK:
			last_event = "clock expt";
			break;
#if 0
			case EVNT_PEERSTRAT:
			last_event = "stratum chg";
			break;
#endif
			default:
			last_event = "";
			break;
		}

		if (event_count != 0)
			cnt = uinttoa(event_count);
		else
			cnt = "";
		(void) sprintf(buf,
				   "%3d %5u  %04x   %3.3s  %4s  %4.4s %9.9s %11s %2s",
				   i+1, assoc_cache[i].assid, assoc_cache[i].status,
				   conf, reach, auth, condition, last_event, cnt);
		bp = &buf[strlen(buf)];
		while (bp > buf && *(bp-1) == ' ')
			*(--bp) = '\0';
		(void) fprintf(fp, "%s\n", buf);
	}
}



/*
 * associations - get, record and print a list of associations
 */
/*ARGSUSED*/
static void
associations(
	struct parse *pcmd,
	FILE *fp
	)
{
	if (dogetassoc(fp))
		printassoc(0, fp);
}


/*
 * lassociations - get, record and print a long list of associations
 */
/*ARGSUSED*/
static void
lassociations(
	struct parse *pcmd,
	FILE *fp
	)
{
	if (dogetassoc(fp))
		printassoc(1, fp);
}


/*
 * passociations - print the association list
 */
/*ARGSUSED*/
static void
passociations(
	struct parse *pcmd,
	FILE *fp
	)
{
	printassoc(0, fp);
}


/*
 * lpassociations - print the long association list
 */
/*ARGSUSED*/
static void
lpassociations(
	struct parse *pcmd,
	FILE *fp
	)
{
	printassoc(1, fp);
}


#ifdef	UNUSED
/*
 * radiostatus - print the radio status returned by the server
 */
/*ARGSUSED*/
static void
radiostatus(
	struct parse *pcmd,
	FILE *fp
	)
{
	char *datap;
	int res;
	int dsize;
	u_short rstatus;

	res = doquery(CTL_OP_READCLOCK, 0, 0, 0, (char *)0, &rstatus,
			  &dsize, &datap);

	if (res != 0)
		return;

	if (numhosts > 1)
		(void) fprintf(fp, "server=%s ", currenthost);
	if (dsize == 0) {
		(void) fprintf(fp, "No radio status string returned\n");
		return;
	}

	asciize(dsize, datap, fp);
}
#endif	/* UNUSED */

/*
 * pstatus - print peer status returned by the server
 */
static void
pstatus(
	struct parse *pcmd,
	FILE *fp
	)
{
	char *datap;
	int res;
	int associd;
	int dsize;
	u_short rstatus;

	/* HMS: uval? */
	if ((associd = checkassocid(pcmd->argval[0].uval)) == 0)
		return;

	res = doquery(CTL_OP_READSTAT, associd, 0, 0, (char *)0, &rstatus,
			  &dsize, &datap);

	if (res != 0)
		return;

	if (numhosts > 1)
		(void) fprintf(fp, "server=%s ", currenthost);
	if (dsize == 0) {
		(void) fprintf(fp,
				   "No information returned for association %u\n",
				   associd);
		return;
	}

	(void) fprintf(fp,"assID=%d ",associd);
	printvars(dsize, datap, (int)rstatus, TYPE_PEER, fp);
}


/*
 * when - print how long its been since his last packet arrived
 */
static long
when(
	l_fp *ts,
	l_fp *rec,
	l_fp *reftime
	)
{
	l_fp *lasttime;

	if (rec->l_ui != 0)
		lasttime = rec;
	else if (reftime->l_ui != 0)
		lasttime = reftime;
	else
		return 0;

	return (ts->l_ui - lasttime->l_ui);
}


/*
 * Pretty-print an interval into the given buffer, in a human-friendly format.
 */
static char *
prettyinterval(
	char *buf,
	long diff
	)
{
	if (diff <= 0) {
		buf[0] = '-';
		buf[1] = 0;
		return buf;
	}

	if (diff <= 2048) {
		(void) sprintf(buf, "%ld", (long int)diff);
		return buf;
	}

	diff = (diff + 29) / 60;
	if (diff <= 300) {
		(void) sprintf(buf, "%ldm", (long int)diff);
		return buf;
	}

	diff = (diff + 29) / 60;
	if (diff <= 96) {
		(void) sprintf(buf, "%ldh", (long int)diff);
		return buf;
	}

	diff = (diff + 11) / 24;
	(void) sprintf(buf, "%ldd", (long int)diff);
	return buf;
}

static char
decodeaddrtype(
	struct sockaddr_storage *sock
	)
{
	char ch = '-';
	u_int32 dummy;
	struct sockaddr_in6 *sin6;

	switch(sock->ss_family) {
	case AF_INET:
		dummy = ((struct sockaddr_in *)sock)->sin_addr.s_addr;
		dummy = ntohl(dummy);
		ch = (char)(((dummy&0xf0000000)==0xe0000000) ? 'm' :
			((dummy&0x000000ff)==0x000000ff) ? 'b' :
			((dummy&0xffffffff)==0x7f000001) ? 'l' :
			((dummy&0xffffffe0)==0x00000000) ? '-' :
			'u');
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sock;
		if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
			ch = 'm';
		else
			ch = 'u';
		break;
	default:
		ch = '-';
		break;
	}
	return ch;
}

/*
 * A list of variables required by the peers command
 */
struct varlist opeervarlist[] = {
	{ "srcadr", 0 },    /* 0 */
	{ "dstadr", 0 },    /* 1 */
	{ "stratum",    0 },    /* 2 */
	{ "hpoll",  0 },    /* 3 */
	{ "ppoll",  0 },    /* 4 */
	{ "reach",  0 },    /* 5 */
	{ "delay",  0 },    /* 6 */
	{ "offset", 0 },    /* 7 */
	{ "jitter", 0 },    /* 8 */
	{ "dispersion", 0 },    /* 9 */
	{ "rec",    0 },    /* 10 */
	{ "reftime",    0 },    /* 11 */
	{ "srcport",    0 },    /* 12 */
	{ 0,		0 }
};

struct varlist peervarlist[] = {
	{ "srcadr", 0 },    /* 0 */
	{ "refid",  0 },    /* 1 */
	{ "stratum",    0 },    /* 2 */
	{ "hpoll",  0 },    /* 3 */
	{ "ppoll",  0 },    /* 4 */
	{ "reach",  0 },    /* 5 */
	{ "delay",  0 },    /* 6 */
	{ "offset", 0 },    /* 7 */
	{ "jitter", 0 },    /* 8 */
	{ "dispersion", 0 },    /* 9 */
	{ "rec",    0 },    /* 10 */
	{ "reftime",    0 },    /* 11 */
	{ "srcport",    0 },    /* 12 */
	{ 0,		0 }
};

#define HAVE_SRCADR 0
#define HAVE_DSTADR 1
#define HAVE_REFID	1
#define HAVE_STRATUM	2
#define HAVE_HPOLL	3
#define HAVE_PPOLL	4
#define HAVE_REACH	5
#define HAVE_DELAY	6
#define HAVE_OFFSET 7
#define HAVE_JITTER 8
#define HAVE_DISPERSION 9
#define HAVE_REC	10
#define HAVE_REFTIME	11
#define HAVE_SRCPORT	12
#define MAXHAVE 	13

/*
 * Decode an incoming data buffer and print a line in the peer list
 */
static int
doprintpeers(
	struct varlist *pvl,
	int associd,
	int rstatus,
	int datalen,
	char *data,
	FILE *fp,
	int af
	)
{
	char *name;
	char *value = NULL;
	int i;
	int c;

	struct sockaddr_storage srcadr;
	struct sockaddr_storage dstadr;
	u_long srcport = 0;
	char *dstadr_refid = "0.0.0.0";
	u_long stratum = 0;
	long ppoll = 0;
	long hpoll = 0;
	u_long reach = 0;
	l_fp estoffset;
	l_fp estdelay;
	l_fp estjitter;
	l_fp estdisp;
	l_fp reftime;
	l_fp rec;
	l_fp ts;
	u_char havevar[MAXHAVE];
	u_long poll_sec;
	char type = '?';
	char refid_string[10];
	char whenbuf[8], pollbuf[8];
	char clock_name[LENHOSTNAME];

	memset((char *)havevar, 0, sizeof(havevar));
	get_systime(&ts);
	
	memset((char *)&srcadr, 0, sizeof(struct sockaddr_storage));
	memset((char *)&dstadr, 0, sizeof(struct sockaddr_storage));

	/* Initialize by zeroing out estimate variables */
	memset((char *)&estoffset, 0, sizeof(l_fp));
	memset((char *)&estdelay, 0, sizeof(l_fp));
	memset((char *)&estjitter, 0, sizeof(l_fp));
	memset((char *)&estdisp, 0, sizeof(l_fp));

	while (nextvar(&datalen, &data, &name, &value)) {
		struct sockaddr_storage dum_store;

		i = findvar(name, peer_var, 1);
		if (i == 0)
			continue;	/* don't know this one */
		switch (i) {
			case CP_SRCADR:
			if (decodenetnum(value, &srcadr))
				havevar[HAVE_SRCADR] = 1;
			break;
			case CP_DSTADR:
			if (decodenetnum(value, &dum_store))
				type = decodeaddrtype(&dum_store);
			if (pvl == opeervarlist) {
				if (decodenetnum(value, &dstadr)) {
					havevar[HAVE_DSTADR] = 1;
					dstadr_refid = stoa(&dstadr);
				}
			}
			break;
			case CP_REFID:
			if (pvl == peervarlist) {
				havevar[HAVE_REFID] = 1;
				if (*value == '\0') {
					dstadr_refid = "0.0.0.0";
				} else if ((int)strlen(value) <= 4) {
					refid_string[0] = '.';
					(void) strcpy(&refid_string[1], value);
					i = strlen(refid_string);
					refid_string[i] = '.';
					refid_string[i+1] = '\0';
					dstadr_refid = refid_string;
				} else if (decodenetnum(value, &dstadr)) {
					if (SOCKNUL(&dstadr))
						dstadr_refid = "0.0.0.0";
					else if ((dstadr.ss_family == AF_INET)
					    && ISREFCLOCKADR(&dstadr))
    						dstadr_refid =
						    refnumtoa(&dstadr);
					else
						dstadr_refid =
						    stoa(&dstadr);
				} else {
					havevar[HAVE_REFID] = 0;
				}
			}
			break;
			case CP_STRATUM:
			if (decodeuint(value, &stratum))
				havevar[HAVE_STRATUM] = 1;
			break;
			case CP_HPOLL:
			if (decodeint(value, &hpoll)) {
				havevar[HAVE_HPOLL] = 1;
				if (hpoll < 0)
					hpoll = NTP_MINPOLL;
			}
			break;
			case CP_PPOLL:
			if (decodeint(value, &ppoll)) {
				havevar[HAVE_PPOLL] = 1;
				if (ppoll < 0)
					ppoll = NTP_MINPOLL;
			}
			break;
			case CP_REACH:
			if (decodeuint(value, &reach))
				havevar[HAVE_REACH] = 1;
			break;
			case CP_DELAY:
			if (decodetime(value, &estdelay))
				havevar[HAVE_DELAY] = 1;
			break;
			case CP_OFFSET:
			if (decodetime(value, &estoffset))
				havevar[HAVE_OFFSET] = 1;
			break;
			case CP_JITTER:
			if (pvl == peervarlist)
				if (decodetime(value, &estjitter))
					havevar[HAVE_JITTER] = 1;
			break;
			case CP_DISPERSION:
			if (decodetime(value, &estdisp))
				havevar[HAVE_DISPERSION] = 1;
			break;
			case CP_REC:
			if (decodets(value, &rec))
				havevar[HAVE_REC] = 1;
			break;
			case CP_SRCPORT:
			if (decodeuint(value, &srcport))
				havevar[HAVE_SRCPORT] = 1;
			break;
			case CP_REFTIME:
			havevar[HAVE_REFTIME] = 1;
			if (!decodets(value, &reftime))
				L_CLR(&reftime);
			break;
			default:
			break;
		}
	}

	/*
	 * Check to see if the srcport is NTP's port.  If not this probably
	 * isn't a valid peer association.
	 */
	if (havevar[HAVE_SRCPORT] && srcport != NTP_PORT)
		return (1);

	/*
	 * Got everything, format the line
	 */
	poll_sec = 1<<max(min3(ppoll, hpoll, NTP_MAXPOLL), NTP_MINPOLL);
	if (pktversion > NTP_OLDVERSION)
		c = flash3[CTL_PEER_STATVAL(rstatus) & 0x7];
	else
		c = flash2[CTL_PEER_STATVAL(rstatus) & 0x3];
	if (numhosts > 1)
		(void) fprintf(fp, "%-*s ", maxhostlen, currenthost);
	if (af == 0 || srcadr.ss_family == af){
		strcpy(clock_name, nntohost(&srcadr));
		
		(void) fprintf(fp,
			"%c%-15.15s %-15.15s %2ld %c %4.4s %4.4s  %3lo  %7.7s %8.7s %7.7s\n",
			c, clock_name, dstadr_refid, stratum, type,
			prettyinterval(whenbuf, when(&ts, &rec, &reftime)),
			prettyinterval(pollbuf, (int)poll_sec), reach,
			lfptoms(&estdelay, 3), lfptoms(&estoffset, 3),
			havevar[HAVE_JITTER] ? lfptoms(&estjitter, 3) :
			lfptoms(&estdisp, 3));
		return (1);
	}
	else
		return(1);
}

#undef	HAVE_SRCADR
#undef	HAVE_DSTADR
#undef	HAVE_STRATUM
#undef	HAVE_PPOLL
#undef	HAVE_HPOLL
#undef	HAVE_REACH
#undef	HAVE_ESTDELAY
#undef	HAVE_ESTOFFSET
#undef	HAVE_JITTER
#undef	HAVE_ESTDISP
#undef	HAVE_REFID
#undef	HAVE_REC
#undef	HAVE_SRCPORT
#undef	HAVE_REFTIME
#undef	MAXHAVE


/*
 * dogetpeers - given an association ID, read and print the spreadsheet
 *		peer variables.
 */
static int
dogetpeers(
	struct varlist *pvl,
	int associd,
	FILE *fp,
	int af
	)
{
	char *datap;
	int res;
	int dsize;
	u_short rstatus;

#ifdef notdef
	res = doquerylist(pvl, CTL_OP_READVAR, associd, 0, &rstatus,
			  &dsize, &datap);
#else
	/*
	 * Damn fuzzballs
	 */
	res = doquery(CTL_OP_READVAR, associd, 0, 0, (char *)0, &rstatus,
			  &dsize, &datap);
#endif

	if (res != 0)
		return 0;

	if (dsize == 0) {
		if (numhosts > 1)
			(void) fprintf(stderr, "server=%s ", currenthost);
		(void) fprintf(stderr,
				   "***No information returned for association %d\n",
				   associd);
		return 0;
	}

	return doprintpeers(pvl, associd, (int)rstatus, dsize, datap, fp, af);
}


/*
 * peers - print a peer spreadsheet
 */
static void
dopeers(
	int showall,
	FILE *fp,
	int af
	)
{
	register int i;
	char fullname[LENHOSTNAME];
	struct sockaddr_storage netnum;

	if (!dogetassoc(fp))
		return;

	for (i = 0; i < numhosts; ++i) {
		if (getnetnum(chosts[i], &netnum, fullname, af))
			if ((int)strlen(fullname) > maxhostlen)
				maxhostlen = strlen(fullname);
	}
	if (numhosts > 1)
		(void) fprintf(fp, "%-*.*s ", maxhostlen, maxhostlen, "server");
	(void) fprintf(fp,
			   "     remote           refid      st t when poll reach   delay   offset  jitter\n");
	if (numhosts > 1)
		for (i = 0; i <= maxhostlen; ++i)
		(void) fprintf(fp, "=");
	(void) fprintf(fp,
			   "==============================================================================\n");

	for (i = 0; i < numassoc; i++) {
		if (!showall &&
			!(CTL_PEER_STATVAL(assoc_cache[i].status)
			  & (CTL_PST_CONFIG|CTL_PST_REACH)))
			continue;
		if (!dogetpeers(peervarlist, (int)assoc_cache[i].assid, fp, af)) {
			return;
		}
	}
	return;
}


/*
 * peers - print a peer spreadsheet
 */
/*ARGSUSED*/
static void
peers(
	struct parse *pcmd,
	FILE *fp
	)
{
	int af = 0;

	if (pcmd->nargs == 1) {
		if (pcmd->argval->ival == 6)
			af = AF_INET6;
		else
			af = AF_INET;
	}
	dopeers(0, fp, af);
}


/*
 * lpeers - print a peer spreadsheet including all fuzzball peers
 */
/*ARGSUSED*/
static void
lpeers(
	struct parse *pcmd,
	FILE *fp
	)
{
	int af = 0;

	if (pcmd->nargs == 1) {
		if (pcmd->argval->ival == 6)
			af = AF_INET6;
		else
			af = AF_INET;
	}
	dopeers(1, fp, af);
}


/*
 * opeers - print a peer spreadsheet
 */
static void
doopeers(
	int showall,
	FILE *fp,
	int af
	)
{
	register int i;
	char fullname[LENHOSTNAME];
	struct sockaddr_storage netnum;

	if (!dogetassoc(fp))
		return;

	for (i = 0; i < numhosts; ++i) {
		if (getnetnum(chosts[i], &netnum, fullname, af))
			if ((int)strlen(fullname) > maxhostlen)
				maxhostlen = strlen(fullname);
	}
	if (numhosts > 1)
		(void) fprintf(fp, "%-*.*s ", maxhostlen, maxhostlen, "server");
	(void) fprintf(fp,
			   "     remote           local      st t when poll reach   delay   offset    disp\n");
	if (numhosts > 1)
		for (i = 0; i <= maxhostlen; ++i)
		(void) fprintf(fp, "=");
	(void) fprintf(fp,
			   "==============================================================================\n");

	for (i = 0; i < numassoc; i++) {
		if (!showall &&
			!(CTL_PEER_STATVAL(assoc_cache[i].status)
			  & (CTL_PST_CONFIG|CTL_PST_REACH)))
			continue;
		if (!dogetpeers(opeervarlist, (int)assoc_cache[i].assid, fp, af)) {
			return;
		}
	}
	return;
}


/*
 * opeers - print a peer spreadsheet the old way
 */
/*ARGSUSED*/
static void
opeers(
	struct parse *pcmd,
	FILE *fp
	)
{
	int af = 0;

	if (pcmd->nargs == 1) {
		if (pcmd->argval->ival == 6)
			af = AF_INET6;
		else
			af = AF_INET;
	}
	doopeers(0, fp, af);
}


/*
 * lopeers - print a peer spreadsheet including all fuzzball peers
 */
/*ARGSUSED*/
static void
lopeers(
	struct parse *pcmd,
	FILE *fp
	)
{
	int af = 0;

	if (pcmd->nargs == 1) {
		if (pcmd->argval->ival == 6)
			af = AF_INET6;
		else
			af = AF_INET;
	}
	doopeers(1, fp, af);
}
