/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: ctm_pass2.c,v 1.9 1995/07/12 09:16:10 phk Exp $
 *
 */

#include "ctm.h"
#define BADREAD 32

/*---------------------------------------------------------------------------*/
/* Pass2 -- Validate the incoming CTM-file.
 */

int
Pass2(FILE *fd)
{
    u_char *p,*q,*md5=0;
    MD5_CTX ctx;
    int i,j,sep,cnt;
    u_char *trash=0,*name=0;
    struct CTM_Syntax *sp;
    struct stat st;
    int ret = 0;
    char md5_1[33];

    if(Verbose>3)
	printf("Pass2 -- Checking if CTM-patch will apply\n");
    MD5Init (&ctx);

    GETFIELD(p,' '); if(strcmp("CTM_BEGIN",p)) WRONG
    GETFIELD(p,' '); if(strcmp(Version,p)) WRONG
    GETFIELD(p,' '); if(strcmp(Name,p)) WRONG
    /* XXX Lookup name in /etc/ctm,conf, read stuff */
    GETFIELD(p,' '); if(strcmp(Nbr,p)) WRONG
    /* XXX Verify that this is the next patch to apply */
    GETFIELD(p,' '); if(strcmp(TimeStamp,p)) WRONG
    GETFIELD(p,'\n'); if(strcmp(Prefix,p)) WRONG
    /* XXX drop or use ? */

    for(;;) {
	if(trash)	{Free(trash), trash = 0;}
	if(name)	{Free(name), name = 0;}
	if(md5)		{Free(md5), md5 = 0;}
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
		case CTM_F_Name:
		    GETFIELDCOPY(name,sep);
		    /* XXX Check DR DM rec's for parent-dir */
		    if(j & CTM_Q_Name_New) {
			/* XXX Check DR FR rec's for item */
			if(-1 != stat(name,&st)) {
			    fprintf(stderr,"  %s: %s exists.\n",
				sp->Key,name);
			    ret |= Exit_Forcible;
			}
			break;
		    }
		    if(-1 == stat(name,&st)) {
			fprintf(stderr,"  %s: %s doesn't exist.\n",
			    sp->Key,name);
		        if (sp->Key[1] == 'R')
			    ret |= Exit_Forcible;
			else
			    ret |= Exit_NotOK;
			break;
		    }
		    if (j & CTM_Q_Name_Dir) {
			if((st.st_mode & S_IFMT) != S_IFDIR) {
			    fprintf(stderr,
				"  %s: %s exist, but isn't dir.\n",
				sp->Key,name);
			    ret |= Exit_NotOK;
			}
			break;
		    }
		    if (j & CTM_Q_Name_File) {
			if((st.st_mode & S_IFMT) != S_IFREG) {
			    fprintf(stderr,
				"  %s: %s exist, but isn't file.\n",
				sp->Key,name);
			    ret |= Exit_NotOK;
			}
			break;
		    }
		    break;
		case CTM_F_Uid:
		case CTM_F_Gid:
		case CTM_F_Mode:
		    GETFIELD(p,sep);
		    break;
		case CTM_F_MD5:
		    if(!name) WRONG
		    if(j & CTM_Q_MD5_Before) {
		        char *tmp;
			GETFIELD(p,sep);
			if((st.st_mode & S_IFMT) == S_IFREG &&
			  (tmp = MD5File(name,md5_1)) != NULL &&
			  strcmp(tmp,p)) {
			    fprintf(stderr,"  %s: %s md5 mismatch.\n",
				sp->Key,name);
			    if(j & CTM_Q_MD5_Force) {
				if(Force)
				    fprintf(stderr,"  Can and will force.\n");
				else
				    fprintf(stderr,"  Could have forced.\n");
				ret |= Exit_Forcible;
			    } else {
				ret |= Exit_NotOK;
			    }
			}
			break;
		    }
		    if(j & CTM_Q_MD5_After) {
			GETFIELDCOPY(md5,sep);
			break;
		    }
		    /* Unqualified MD5 */
		    WRONG
		    break;
		case CTM_F_Count:
		    GETBYTECNT(cnt,sep);
		    break;
		case CTM_F_Bytes:
		    if(cnt < 0) WRONG
		    GETDATA(trash,cnt);
		    if(!strcmp(sp->Key,"FN")) {
			p = tempnam(TmpDir,"CTMclient");
			j = ctm_edit(trash,cnt,name,p);
			if(j) {
			    fprintf(stderr,"  %s: %s edit returned %d.\n",
				sp->Key,name,j);
			    ret |= j;
			    return ret;
			} else if(strcmp(md5,MD5File(p,md5_1))) {
			    fprintf(stderr,"  %s: %s edit fails.\n",
				sp->Key,name);
			    ret |= Exit_Mess;
			    return ret;
			}
			unlink(p);
			free(p);
		    }

		    break;
		default: WRONG
	    }
        }
    }
    q = MD5End (&ctx,md5_1);
    GETFIELD(p,'\n');			/* <MD5> */
    if(strcmp(q,p)) WRONG
    if (-1 != getc(fd)) WRONG
    return ret;
}
