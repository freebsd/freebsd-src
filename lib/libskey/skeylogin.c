/*   Login code for S/KEY Authentication.  S/KEY is a trademark
 *   of Bellcore.
 *
 *   Mink is the former name of the S/KEY authentication system.
 *   Many references for mink  may still be found in this program.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#include "skey.h"
#include "pathnames.h"

char *skipspace();
int skeylookup __P((struct skey *mp,char *name));

#define setpriority(x,y,z)	/* nothing */

static char *month[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* Look up skey info for user 'name'. If successful, fill in the caller's
 * skey structure and return 0. If unsuccessful (e.g., if name is unknown)
 * return -1. If an optional challenge string buffer is given, update it.
 *
 * The file read/write pointer is left at the start of the
 * record.
 */
int
skeyinfo(mp,name,ss)
struct skey *mp;
char *name;
char *ss;
{
	int rval;

	rval = skeylookup(mp,name);
	switch(rval){
	case -1:	/* File error */
		return -1;
	case 0:		/* Lookup succeeded */
		if (ss != 0) {
			sprintf(ss, "s/key %d %s",mp->n - 1,mp->seed);
			fclose(mp->keyfile);
		}
		return 0;
	case 1:		/* User not found */
		fclose(mp->keyfile);
		return -1;
	}
	return -1;	/* Can't happen */
}

/* Return  a skey challenge string for user 'name'. If successful,
 * fill in the caller's skey structure and return 0. If unsuccessful
 * (e.g., if name is unknown) return -1.
 *
 * The file read/write pointer is left at the start of the
 * record.
 */
int
skeychallenge(mp,name, ss)
struct skey *mp;
char *name;
char *ss;
{
	int rval;

	rval = skeylookup(mp,name);
	switch(rval){
	case -1:	/* File error */
		return -1;
	case 0:		/* Lookup succeeded, issue challenge */
                sprintf(ss, "s/key %d %s",mp->n - 1,mp->seed);
		return 0;
	case 1:		/* User not found */
		fclose(mp->keyfile);
		return -1;
	}
	return -1;	/* Can't happen */
}

/* Find an entry in the One-time Password database.
 * Return codes:
 * -1: error in opening database
 *  0: entry found, file R/W pointer positioned at beginning of record
 *  1: entry not found, file R/W pointer positioned at EOF
 */
int
skeylookup(mp,name)
struct skey *mp;
char *name;
{
	int found;
	int len;
	long recstart;
	char *cp, *p;
	struct stat statbuf;
	mode_t oldmask;

	/* See if the _PATH_SKEYFILE exists, and create it if not */
	if(stat(_PATH_SKEYFILE,&statbuf) == -1 && errno == ENOENT){
		oldmask = umask(S_IRWXG|S_IRWXO);
		mp->keyfile = fopen(_PATH_SKEYFILE,"w+");
		(void)umask(oldmask);
	} else {
		/* Otherwise open normally for update */
		mp->keyfile = fopen(_PATH_SKEYFILE,"r+");
	}
	if(mp->keyfile == NULL)
		return -1;

	/* Look up user name in database */
	len = strlen(name);
	if( len > 8 ) len = 8;		/*  Added 8/2/91  -  nmh */
	found = 0;
	while(!feof(mp->keyfile)){
		recstart = ftell(mp->keyfile);
		mp->recstart = recstart;
		if(fgets(mp->buf,sizeof(mp->buf),mp->keyfile) != mp->buf){
			break;
		}
		rip(mp->buf);
		if(mp->buf[0] == '#')
			continue;	/* Comment */
		p = mp->buf;
		while ((cp = strsep(&p, " \t")) != NULL && *cp == '\0')
			;
		if((mp->logname = cp) == NULL)
			continue;
		while ((cp = strsep(&p, " \t")) != NULL && *cp == '\0')
			;
		if(cp == NULL)
			continue;
		mp->n = atoi(cp);
		while ((cp = strsep(&p, " \t")) != NULL && *cp == '\0')
			;
		if((mp->seed = cp) == NULL)
			continue;
		while ((cp = strsep(&p, " \t")) != NULL && *cp == '\0')
			;
		if((mp->val = cp) == NULL)
			continue;
		if(strlen(mp->logname) == len
		 && strncmp(mp->logname,name,len) == 0){
			found = 1;
			break;
		}
	}
	if(found){
		fseek(mp->keyfile,recstart,0);
		return 0;
	} else
		return 1;
}
/* Verify response to a s/key challenge.
 *
 * Return codes:
 * -1: Error of some sort; database unchanged
 *  0:  Verify successful, database updated
 *  1:  Verify failed, database unchanged
 *
 * The database file is always closed by this call.
 */
int
skeyverify(mp,response)
struct skey *mp;
char *response;
{
	char key[8];
	char fkey[8];
	char filekey[8];
	time_t now;
	struct tm *tm;
	char tbuf[27], fbuf[20];
	char *cp, *p;

	time(&now);
	tm = localtime(&now);
/* can't use %b here, because it can be in national form */
	strftime(fbuf, sizeof(fbuf), "%d,%Y %T", tm);
	sprintf(tbuf, " %s %s", month[tm->tm_mon], fbuf);

	if(response == NULL){
		fclose(mp->keyfile);
		return -1;
	}
	rip(response);

	/* Convert response to binary */
	if(etob(key,response) != 1 && atob8(key,response) != 0){
		/* Neither english words or ascii hex */
		fclose(mp->keyfile);
		return -1;
	}

	/* Compute fkey = f(key) */
	memcpy(fkey,key,sizeof(key));
	f(fkey);
	/* in order to make the window of update as short as possible
           we must do the comparison here and if OK write it back
           other wise the same password can be used twice to get in
  	   to the system
	*/

	setpriority(PRIO_PROCESS, 0, -4);

	/* reread the file record NOW*/

	fseek(mp->keyfile,mp->recstart,0);
	if(fgets(mp->buf,sizeof(mp->buf),mp->keyfile) != mp->buf){
		setpriority(PRIO_PROCESS, 0, 0);
		fclose(mp->keyfile);
		return -1;
	}
	rip(mp->buf);
	p = mp->buf;
	while ((cp = strsep(&p, " \t")) != NULL && *cp == '\0')
		;
	mp->logname = cp;
	while ((cp = strsep(&p, " \t")) != NULL && *cp == '\0')
		;
	while ((cp = strsep(&p, " \t")) != NULL && *cp == '\0')
		;
	mp->seed = cp;
	while ((cp = strsep(&p, " \t")) != NULL && *cp == '\0')
		;
	mp->val = cp;
	/* And convert file value to hex for comparison */
	atob8(filekey,mp->val);

	/* Do actual comparison */
	if(memcmp(filekey,fkey,8) != 0){
		/* Wrong response */
		setpriority(PRIO_PROCESS, 0, 0);
		fclose(mp->keyfile);
		return 1;
	}

	/* Update key in database by overwriting entire record. Note
	 * that we must write exactly the same number of bytes as in
	 * the original record (note fixed width field for N)
	 */
	btoa8(mp->val,key);
	mp->n--;
	fseek(mp->keyfile,mp->recstart,0);
	fprintf(mp->keyfile,"%s %04d %-16s %s %-21s\n",mp->logname,mp->n,mp->seed,
	 mp->val, tbuf);

	fclose(mp->keyfile);

	setpriority(PRIO_PROCESS, 0, 0);
	return 0;
}


/* Convert 8-byte hex-ascii string to binary array
 * Returns 0 on success, -1 on error
 */
int
atob8(out,in)
register char *out,*in;
{
	register int i;
	register int val;

	if(in == NULL || out == NULL)
		return -1;

	for(i=0;i<8;i++){
		if((in = skipspace(in)) == NULL)
			return -1;
		if((val = htoi(*in++)) == -1)
			return -1;
		*out = val << 4;

		if((in = skipspace(in)) == NULL)
			return -1;
		if((val = htoi(*in++)) == -1)
			return -1;
		*out++ |= val;
	}
	return 0;
}

char *
skipspace(cp)
register char *cp;
{
	while(*cp == ' ' || *cp == '\t')
		cp++;

	if(*cp == '\0')
		return NULL;
	else
		return cp;
}

/* Convert 8-byte binary array to hex-ascii string */
int
btoa8(out,in)
register char *out,*in;
{
	register int i;

	if(in == NULL || out == NULL)
		return -1;

	for(i=0;i<8;i++){
		sprintf(out,"%02x",*in++ & 0xff);
		out += 2;
	}
	return 0;
}


/* Convert hex digit to binary integer */
int
htoi(c)
register char c;
{
	if('0' <= c && c <= '9')
		return c - '0';
	if('a' <= c && c <= 'f')
		return 10 + c - 'a';
	if('A' <= c && c <= 'F')
		return 10 + c - 'A';
	return -1;
}
