/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include "ctm.h"
#define BADREAD 1

/*---------------------------------------------------------------------------*/
/* Pass1 -- Validate the incoming CTM-file.
 */

int
Pass1(FILE *fd, unsigned applied)
{
    u_char *p,*q;
    MD5_CTX ctx;
    int i,j,sep,cnt;
    u_char *md5=0,*name=0,*trash=0;
    struct CTM_Syntax *sp;
    int slashwarn=0, match=0, total_matches=0;
    unsigned current;
    char md5_1[33];

    if(Verbose>3)
	printf("Pass1 -- Checking integrity of incoming CTM-patch\n");
    MD5Init (&ctx);

    GETFIELD(p,' ');				/* CTM_BEGIN */
    if(strcmp(p,"CTM_BEGIN")) {
	Fatal("Probably not a CTM-patch at all.");
	if(Verbose>3)
	    fprintf(stderr,"Expected \"CTM_BEGIN\" got \"%s\".\n",p);
	return 1;
    }

    GETFIELDCOPY(Version,' ');				/* <Version> */
    if(strcmp(Version,VERSION)) {
	Fatal("CTM-patch is wrong version.");
	if(Verbose>3)
	    fprintf(stderr,"Expected \"%s\" got \"%s\".\n",VERSION,p);
	return 1;
    }

    GETFIELDCOPY(Name,' ');				/* <Name> */
    GETFIELDCOPY(Nbr,' ');				/* <Nbr> */
    GETFIELDCOPY(TimeStamp,' ');			/* <TimeStamp> */
    GETFIELDCOPY(Prefix,'\n');				/* <Prefix> */

    sscanf(Nbr, "%u", &current);
    if (FilterList || ListIt)
	current = 0;	/* ignore if -l or if filters are present */
    if(current && current <= applied) {
	if(Verbose > 0)
	    fprintf(stderr,"Delta number %u is already applied; ignoring.\n",
		    current);
	return Exit_Version;
    }

    for(;;) {
	Delete(md5);
	Delete(name);
	Delete(trash);
	cnt = -1;
	/* if a filter list is defined we assume that all pathnames require
	   an action opposite to that requested by the first filter in the
	   list.
	   If no filter is defined, all pathnames are assumed to match. */
	match = (FilterList ? !(FilterList->Action) : CTM_FILTER_ENABLE);

	GETFIELD(p,' ');			/* CTM_something */

	if (p[0] != 'C' || p[1] != 'T' || p[2] != 'M') {
	    Fatal("Expected CTM keyword.");
	    fprintf(stderr,"Got [%s]\n",p);
	    return 1;
	}

	if(!strcmp(p+3,"_END"))
	    break;

	for(sp=Syntax;sp->Key;sp++)
	    if(!strcmp(p+3,sp->Key))
		goto found;
	Fatal("Expected CTM keyword.");
	fprintf(stderr,"Got [%s]\n",p);
	return 1;
    found:
	if(Verbose > 5)
	    fprintf(stderr,"%s ",sp->Key);
	for(i=0;(j = sp->List[i]);i++) {
	    if (sp->List[i+1] && (sp->List[i+1] & CTM_F_MASK) != CTM_F_Bytes)
		sep = ' ';
	    else
		sep = '\n';

	    if(Verbose > 5)
	        fprintf(stderr," %x(%d)",sp->List[i],sep);

	    switch (j & CTM_F_MASK) {
		case CTM_F_Name: /* XXX check for garbage and .. */
		    GETFIELDCOPY(name,sep);
		    j = strlen(name);
		    if(name[j-1] == '/' && !slashwarn)  {
			fprintf(stderr,"Warning: contains trailing slash\n");
			slashwarn++;
		    }
		    if (name[0] == '/') {
			Fatal("Absolute paths are illegal.");
			return Exit_Mess;
		    }
		    q = name;
		    for (;;) {
			if (q[0] == '.' && q[1] == '.')
			    if (q[2] == '/' || q[2] == '\0') {
				Fatal("Paths containing '..' are illegal.");
				return Exit_Mess;
			    }
			if ((q = strchr(q, '/')) == NULL)
			    break;
			q++;
		    }

		    /* if we have been asked to `keep' files then skip
		       removes; i.e. we don't match these entries at
		       all. */
		    if (KeepIt &&
			(!strcmp(sp->Key,"DR") || !strcmp(sp->Key,"FR"))) {
			match = CTM_FILTER_DISABLE;
			break;
		    }

		    /* If filter expression have been defined, match the
		       path name against the expression list.  */
		    
		    if (FilterList) {
			struct CTM_Filter *filter;

			for (filter = FilterList; filter; 
			     filter = filter->Next) {
				if (0 == regexec(&filter->CompiledRegex, name,
					0, 0, 0))
					/* if the name matches, adopt the 
					   action */
					match = filter->Action;
			}
		    }

		    /* Add up the total number of matches */
		    total_matches += match;
		    break;
		case CTM_F_Uid:
		    GETFIELD(p,sep);
		    while(*p) {
			if(!isdigit(*p)) {
			    Fatal("Non-digit in uid.");
			    return 32;
			}
			p++;
		    }
		    break;
		case CTM_F_Gid:
		    GETFIELD(p,sep);
		    while(*p) {
			if(!isdigit(*p)) {
			    Fatal("Non-digit in gid.");
			    return 32;
			}
			p++;
		    }
		    break;
		case CTM_F_Mode:
		    GETFIELD(p,sep);
		    while(*p) {
			if(!isdigit(*p)) {
			    Fatal("Non-digit in mode.");
			    return 32;
			}
			p++;
		    }
		    break;
		case CTM_F_MD5:
		    if(j & CTM_Q_MD5_Chunk) {
			GETFIELDCOPY(md5,sep);  /* XXX check for garbage */
		    } else if(j & CTM_Q_MD5_Before) {
			GETFIELD(p,sep);  /* XXX check for garbage */
		    } else if(j & CTM_Q_MD5_After) {
			GETFIELD(p,sep);  /* XXX check for garbage */
		    } else {
			fprintf(stderr,"List = 0x%x\n",j);
			Fatal("Unqualified MD5.");
			return 32;
		    }
		    break;
		case CTM_F_Count:
		    GETBYTECNT(cnt,sep);
		    break;
		case CTM_F_Bytes:
		    if(cnt < 0) WRONG
		    GETDATA(trash,cnt);
		    p = MD5Data(trash,cnt,md5_1);
		    if(md5 && strcmp(md5,p)) {
			Fatal("Internal MD5 failed.");
			return Exit_Garbage;
		default:
			fprintf(stderr,"List = 0x%x\n",j);
			Fatal("List had garbage.");
			return Exit_Garbage;
		    }
	    }
	}
	if(Verbose > 5)
	    putc('\n',stderr);
	if(ListIt && match)
	    printf("> %s %s\n", sp->Key, name);
    }

    Delete(md5);
    Delete(name);
    Delete(trash);

    q = MD5End (&ctx,md5_1);
    if(Verbose > 2)
	printf("Expecting Global MD5 <%s>\n",q);
    GETFIELD(p,'\n');			/* <MD5> */
    if(Verbose > 2)
	printf("Reference Global MD5 <%s>\n",p);
    if(strcmp(q,p)) {
	Fatal("MD5 sum doesn't match.");
	fprintf(stderr,"\tI have:<%s>\n",q);
	fprintf(stderr,"\tShould have been:<%s>\n",p);
	return Exit_Garbage;
    }
    if (-1 != getc(fd)) {
	if(!Force) {
	    Fatal("Trailing junk in CTM-file.  Can Force with -F.");
	    return 16;
	}
    }
    if ((Verbose > 1) && (0 == total_matches))
	printf("No matches in \"%s\"\n", FileName);
    return (total_matches ? Exit_OK : Exit_NoMatch);
}
