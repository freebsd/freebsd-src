/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include "ctm.h"
#define BADREAD 32

/*---------------------------------------------------------------------------*/
/* Pass3 -- Validate the incoming CTM-file.
 */

int
settime(const char *name, const struct timeval *times)
{
	if (SetTime)
	    if (utimes(name,times)) {
		warn("utimes(): %s", name);
		return -1;
	    }
	return 0;
}

int
Pass3(FILE *fd)
{
    u_char *p,*q,buf[BUFSIZ];
    MD5_CTX ctx;
    int i,j,sep,cnt;
    u_char *md5=0,*md5before=0,*trash=0,*name=0,*uid=0,*gid=0,*mode=0;
    struct CTM_Syntax *sp;
    FILE *ed=0;
    struct stat st;
    char md5_1[33];
    int match=0;
    struct timeval times[2];
    struct CTM_Filter *filter = NULL;
    if(Verbose>3)
	printf("Pass3 -- Applying the CTM-patch\n");
    MD5Init (&ctx);

    GETFIELD(p,' '); if(strcmp("CTM_BEGIN",p)) WRONG
    GETFIELD(p,' '); if(strcmp(Version,p)) WRONG
    GETFIELD(p,' '); if(strcmp(Name,p)) WRONG
    GETFIELD(p,' '); if(strcmp(Nbr,p)) WRONG
    GETFIELD(p,' '); if(strcmp(TimeStamp,p)) WRONG
    GETFIELD(p,'\n'); if(strcmp(Prefix,p)) WRONG

    /*
     * This would be cleaner if mktime() worked in UTC rather than
     * local time.
     */
    if (SetTime) {
        struct tm tm;
        char *tz;
        char buf[5];
        int i;

#define SUBSTR(off,len)	strncpy(buf, &TimeStamp[off], len), buf[len] = '\0'
#define WRONGDATE { fprintf(stderr, " %s failed date validation\n",\
	TimeStamp); WRONG}

        if (strlen(TimeStamp) != 15 || TimeStamp[14] != 'Z') WRONGDATE
	for (i = 0; i < 14; i++)
	    if (!isdigit(TimeStamp[i])) WRONGDATE

        tz = getenv("TZ");
	if (setenv("TZ", "UTC", 1) < 0) WRONG
	tzset();

	tm.tm_isdst = tm.tm_gmtoff = 0;

        SUBSTR(0, 4);
        tm.tm_year = atoi(buf) - 1900;
        SUBSTR(4, 2);
        tm.tm_mon = atoi(buf) - 1;
        if (tm.tm_mon < 0 || tm.tm_mon > 11) WRONGDATE
        SUBSTR(6, 2);
        tm.tm_mday = atoi(buf);
        if (tm.tm_mday < 1 || tm.tm_mday > 31) WRONG;
        SUBSTR(8, 2);
        tm.tm_hour = atoi(buf);
        if (tm.tm_hour > 24) WRONGDATE
        SUBSTR(10, 2);
        tm.tm_min = atoi(buf);
        if (tm.tm_min > 59) WRONGDATE
        SUBSTR(12, 2);
        tm.tm_sec = atoi(buf);
        if (tm.tm_min > 62) WRONGDATE	/* allow leap seconds */
    
        times[0].tv_sec = times[1].tv_sec = mktime(&tm);
        if (times[0].tv_sec == -1) WRONGDATE
	times[0].tv_usec = times[1].tv_usec = 0;

        if (tz) {
            if (setenv("TZ", tz, 1) < 0) WRONGDATE
         } else {
            unsetenv("TZ");
        }
    }

    for(;;) {
	Delete(md5);
	Delete(uid);
	Delete(gid);
	Delete(mode);
	Delete(md5before);
	Delete(trash);
	Delete(name);
	cnt = -1;

	GETFIELD(p,' ');

	if (p[0] != 'C' || p[1] != 'T' || p[2] != 'M') WRONG

	if(!strcmp(p+3,"_END"))
	    break;

	for(sp=Syntax;sp->Key;sp++)
	    if(!strcmp(p+3,sp->Key))
		goto found;
	WRONG
    found:
	for(i=0;(j = sp->List[i]);i++) {
	    if (sp->List[i+1] && (sp->List[i+1] & CTM_F_MASK) != CTM_F_Bytes)
		sep = ' ';
	    else
		sep = '\n';

	    switch (j & CTM_F_MASK) {
		case CTM_F_Name: GETNAMECOPY(name,sep,j, Verbose); break;
		case CTM_F_Uid:  GETFIELDCOPY(uid,sep); break;
		case CTM_F_Gid:  GETFIELDCOPY(gid,sep); break;
		case CTM_F_Mode: GETFIELDCOPY(mode,sep); break;
		case CTM_F_MD5:
		    if(j & CTM_Q_MD5_Before)
			GETFIELDCOPY(md5before,sep);
		    else
			GETFIELDCOPY(md5,sep);
		    break;
		case CTM_F_Count: GETBYTECNT(cnt,sep); break;
		case CTM_F_Bytes: GETDATA(trash,cnt); break;
		default: WRONG
		}
	    }
	/* XXX This should go away.  Disallow trailing '/' */
	j = strlen(name)-1;
	if(name[j] == '/') name[j] = '\0';

	/*
	 * If a filter list is specified, run thru the filter list and
	 * match `name' against filters.  If the name matches, set the
	 * required action to that specified in the filter.
	 * The default action if no filterlist is given is to match
	 * everything.  
	 */

	match = (FilterList ? !(FilterList->Action) : CTM_FILTER_ENABLE);
	for (filter = FilterList; filter; filter = filter->Next) {
	    if (0 == regexec(&filter->CompiledRegex, name,
		0, 0, 0)) {
		match = filter->Action;
	    }
	}

	if (CTM_FILTER_DISABLE == match) /* skip file if disabled */
		continue;

	if (Verbose > 0)
		fprintf(stderr,"> %s %s\n",sp->Key,name);
	if(!strcmp(sp->Key,"FM") || !strcmp(sp->Key, "FS")) {
	    i = open(name,O_WRONLY|O_CREAT|O_TRUNC,0666);
	    if(i < 0) {
		warn("%s", name);
		WRONG
	    }
	    if(cnt != write(i,trash,cnt)) {
		warn("%s", name);
		WRONG
	    }
	    close(i);
	    if(strcmp(md5,MD5File(name,md5_1))) {
		fprintf(stderr,"  %s %s MD5 didn't come out right\n",
		   sp->Key,name);
		WRONG
	    }
	    if (settime(name,times)) WRONG
	    continue;
	}
	if(!strcmp(sp->Key,"FE")) {
	    ed = popen("ed","w");
	    if(!ed) {
		WRONG
	    }
	    fprintf(ed,"e %s\n",name);
	    if(cnt != fwrite(trash,1,cnt,ed)) {
		warn("%s", name);
		pclose(ed);
		WRONG
	    }
	    fprintf(ed,"w %s\n",name);
	    if(pclose(ed)) {
		warn("ed");
		WRONG
	    }
	    if(strcmp(md5,MD5File(name,md5_1))) {
		fprintf(stderr,"  %s %s MD5 didn't come out right\n",
		   sp->Key,name);
		WRONG
	    }
	    if (settime(name,times)) WRONG
	    continue;
	}
	if(!strcmp(sp->Key,"FN")) {
	    strcpy(buf,name);
	    strcat(buf,TMPSUFF);
	    i = ctm_edit(trash,cnt,name,buf);
	    if(i) {
		fprintf(stderr," %s %s Edit failed with code %d.\n",
		    sp->Key,name,i);
	        WRONG
	    }
	    if(strcmp(md5,MD5File(buf,md5_1))) {
		fprintf(stderr," %s %s Edit failed MD5 check.\n",
		    sp->Key,name);
	        WRONG
	    }
	    if (rename(buf,name) == -1)
		WRONG
	    if (settime(name,times)) WRONG
	    continue;
	}
	if(!strcmp(sp->Key,"DM")) {
	    if(0 > mkdir(name,0777)) {
		sprintf(buf,"mkdir -p %s",name);
		system(buf);
	    }
	    if(0 > stat(name,&st) || ((st.st_mode & S_IFMT) != S_IFDIR)) {
		fprintf(stderr,"<%s> mkdir failed\n",name);
		WRONG
	    }
	    if (settime(name,times)) WRONG
	    continue;
	}
	if(!strcmp(sp->Key,"FR")) {
	    if (KeepIt) { 
		if (Verbose > 1) 
			printf("<%s> not removed\n", name);
	    }
	    else if (0 != unlink(name)) {
		fprintf(stderr,"<%s> unlink failed\n",name);
		if (!Force)
		    WRONG
	    }
	    continue;
	}
	if(!strcmp(sp->Key,"DR")) {
	    /*
	     * We cannot use rmdir() because we do not get the directories
	     * in '-depth' order (cvs-cur.0018.gz for examples)
	     */
	    if (KeepIt) {
		if (Verbose > 1) {
			printf("<%s> not removed\n", name);
		}
	    } else {
		    sprintf(buf,"rm -rf %s",name);
		    system(buf);
	    }
	    continue;
	}
	WRONG
    }

    Delete(md5);
    Delete(uid);
    Delete(gid);
    Delete(mode);
    Delete(md5before);
    Delete(trash);
    Delete(name);

    q = MD5End (&ctx,md5_1);
    GETFIELD(p,'\n');
    if(strcmp(q,p)) WRONG
    if (-1 != getc(fd)) WRONG
    return 0;
}
