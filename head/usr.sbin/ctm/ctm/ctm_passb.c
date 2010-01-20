/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <koshy@india.hp.com> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Joseph Koshy
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include "ctm.h"
#define BADREAD 32

/*---------------------------------------------------------------------------*/
/* PassB -- Backup modified files.
 */

int
PassB(FILE *fd)
{
    u_char *p,*q;
    MD5_CTX ctx;
    int i,j,sep,cnt;
    u_char *md5=0,*md5before=0,*trash=0,*name=0,*uid=0,*gid=0,*mode=0;
    struct CTM_Syntax *sp;
    FILE *b = 0;	/* backup command */
    u_char buf[BUFSIZ];
    char md5_1[33];
    int ret = 0;
    int match = 0;
    struct CTM_Filter *filter = NULL;

    if(Verbose>3)
	printf("PassB -- Backing up files which would be changed.\n");

    MD5Init (&ctx);
    snprintf(buf, sizeof(buf), fmtcheck(TarCmd, TARCMD), BackupFile);
    b=popen(buf, "w");
    if(!b) { warn("%s", buf); return Exit_Garbage; }

    GETFIELD(p,' '); if(strcmp("CTM_BEGIN",p)) WRONG
    GETFIELD(p,' '); if(strcmp(Version,p)) WRONG
    GETFIELD(p,' '); if(strcmp(Name,p)) WRONG
    GETFIELD(p,' '); if(strcmp(Nbr,p)) WRONG
    GETFIELD(p,' '); if(strcmp(TimeStamp,p)) WRONG
    GETFIELD(p,'\n'); if(strcmp(Prefix,p)) WRONG

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

	if (KeepIt && 
	    (!strcmp(sp->Key,"DR") || !strcmp(sp->Key,"FR")))
	    continue;
		
	/* match the name against the elements of the filter list.  The
	   action associated with the last matched filter determines whether
	   this file should be ignored or backed up. */
	match = (FilterList ? !(FilterList->Action) : CTM_FILTER_ENABLE);
	for (filter = FilterList; filter; filter = filter->Next) {
	    if (0 == regexec(&filter->CompiledRegex, name, 0, 0, 0))
		match = filter->Action;
	}

	if (CTM_FILTER_DISABLE == match)
		continue;

	if (!strcmp(sp->Key,"FS") || !strcmp(sp->Key,"FN") ||
	    !strcmp(sp->Key,"AS") || !strcmp(sp->Key,"DR") || 
	    !strcmp(sp->Key,"FR")) {
	    /* send name to the archiver for a backup */
	    cnt = strlen(name);
	    if (cnt != fwrite(name,1,cnt,b) || EOF == fputc('\n',b)) {
		warn("%s", name);
		pclose(b);
		WRONG;
	    }
	}
    }

    ret = pclose(b);

    Delete(md5);
    Delete(uid);
    Delete(gid);
    Delete(mode);
    Delete(md5before);
    Delete(trash);
    Delete(name);

    q = MD5End (&ctx,md5_1);
    GETFIELD(p,'\n');			/* <MD5> */
    if(strcmp(q,p)) WRONG
    if (-1 != getc(fd)) WRONG
    return ret;
}
