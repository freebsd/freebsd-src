/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: ctm.c,v 1.13 1996/04/29 21:02:28 phk Exp $
 *
 * This is the client program of 'CTM'.  It will apply a CTM-patch to a
 * collection of files.
 *
 * Options we'd like to see:
 *
 * -a 			Attempt best effort.
 * -d <int>		Debug TBD.
 * -m <mail-addr>	Email me instead.
 * -r <name>		Reconstruct file.
 * -R <file>		Read list of files to reconstruct.
 *
 * Options we have:
 * -b <dir>		Base-dir
 * -B <file>		Backup to tar-file.
 * -t 			Tar command (default as in TARCMD).
 * -c			Check it out, don't do anything.
 * -F      		Force
 * -q 			Tell us less.
 * -T <tmpdir>.		Temporary files.
 * -u			Set all file modification times to the timestamp
 * -v 			Tell us more.
 * -V <level>		Tell us more level = number of -v
 * -k			Keep files and directories that would have been removed.
 * -l			List actions.
 *
 * Options we don't actually use:
 * -p			Less paranoid.
 * -P			Paranoid.
 */

#define EXTERN /* */
#include "ctm.h"

#define CTM_STATUS ".ctm_status"

extern int Proc(char *, unsigned applied);

int
main(int argc, char **argv)
{
    int stat=0, err=0;
    int c;
    extern int optopt,optind;
    extern char * optarg;
    unsigned applied = 0;
    FILE *statfile;
    struct CTM_Filter *nfilter = NULL;	/* new filter */
    u_char * basedir;

    basedir = NULL;
    Verbose = 1;
    Paranoid = 1;
    SetTime = 0;
    KeepIt = 0;
    ListIt = 0;
    BackupFile = NULL;
    TarCmd = TARCMD;
    LastFilter = FilterList = NULL;
    setbuf(stderr,0);
    setbuf(stdout,0);

    while((c=getopt(argc,argv,"ab:B:cd:e:Fklm:pPqr:R:t:T:uV:vx:")) != -1) {
	switch (c) {
	    case 'b': basedir = optarg;	break; /* Base Directory */
	    case 'B': BackupFile = optarg;	break;
	    case 'c': CheckIt++;	break; /* Only check it */
	    case 'F': Force = 1;	break; 
	    case 'k': KeepIt++;		break; /* Don't do removes */
	    case 'l': ListIt++;		break; /* Only list actions and files */
	    case 'p': Paranoid--;	break; /* Less Paranoid */
	    case 'P': Paranoid++;	break; /* More Paranoid */
	    case 'q': Verbose--;	break; /* Quiet */
	    case 't': TarCmd = optarg;	break; /* archiver command */
	    case 'T': TmpDir = optarg;	break; /* set temporary directory */
	    case 'u': SetTime++;	break; /* Set timestamp on files */
	    case 'v': Verbose++;	break; /* Verbose */
	    case 'V': sscanf(optarg,"%d", &c); /* Verbose */
		      Verbose += c;
		      break;
	    case 'e':				/* filter expressions */
	    case 'x':
		if (NULL == (nfilter =  Malloc(sizeof(struct CTM_Filter)))) {
			fprintf(stderr, 
				"Out of memory for expressions: \"%s\"\n",
			  	optarg);
			 stat++;
			break;
		}

		(void) memset(nfilter, 0, sizeof(struct CTM_Filter));

		if (0 != (err = 
			regcomp(&nfilter->CompiledRegex, optarg, REG_NOSUB))) {

			char errmsg[128];

			regerror(err, &nfilter->CompiledRegex, errmsg, 
				sizeof(errmsg));
			fprintf(stderr, "Regular expression: \"%s\"\n", errmsg);
			stat++;
			break;
		}

		/* note whether the filter enables or disables on match */
		nfilter->Action = 
			(('e' == c) ? CTM_FILTER_ENABLE : CTM_FILTER_DISABLE);

		/* link in the expression into the list */
	        nfilter->Next = NULL;
		if (NULL == FilterList) {
		  LastFilter = FilterList = nfilter; /* init head and tail */
		} else {    /* place at tail */
		  LastFilter->Next = nfilter;
		  LastFilter = nfilter;	
		}
		break;
	    case ':':
		fprintf(stderr,"Option '%c' requires an argument.\n",optopt);
		stat++;
		break;
	    case '?':
		fprintf(stderr,"Option '%c' not supported.\n",optopt);
		stat++;
		break;
	    default:
		fprintf(stderr,"Option '%c' not yet implemented.\n",optopt);
		break;
	}
    }

    if(stat) {
	fprintf(stderr,"%d errors during option processing\n",stat);
	return Exit_Pilot;
    }
    stat = Exit_Done;
    argc -= optind;
    argv += optind;

    if (basedir == NULL) {
	Buffer = (u_char *)Malloc(BUFSIZ + strlen(SUBSUFF) +1);
	CatPtr = Buffer;
	*Buffer  = '\0';
    } else {
	Buffer = (u_char *)Malloc(strlen(basedir)+ BUFSIZ + strlen(SUBSUFF) +1);
	strcpy(Buffer, basedir);
	CatPtr = Buffer + strlen(basedir);
	if (CatPtr[-1] != '/') {
		strcat(Buffer, "/");
		CatPtr++;
	}
    }
    strcat(Buffer, CTM_STATUS);

    if(ListIt) 
	applied = 0;
    else
	if((statfile = fopen(Buffer, "r")) == NULL) {
	    if (Verbose > 0)
	    	fprintf(stderr, "Warning: %s not found.\n", Buffer);
	} else {
	    fscanf(statfile, "%*s %u", &applied);
	    fclose(statfile);
	}

    if(!argc)
	stat |= Proc("-", applied);

    while(argc-- && stat == Exit_Done) {
	stat |= Proc(*argv++, applied);
	stat &= ~(Exit_Version | Exit_NoMatch);
    }

    if(stat == Exit_Done)
	stat = Exit_OK;

    if(Verbose > 0)
	fprintf(stderr,"Exit(%d)\n",stat);

    if (FilterList)
	for (nfilter = FilterList; nfilter; ) {
	    struct CTM_Filter *tmp = nfilter->Next;
	    Free(nfilter);
	    nfilter = tmp;
	}
    return stat;
}

int
Proc(char *filename, unsigned applied)
{
    FILE *f;
    int i;
    char *p = strrchr(filename,'.');

    if(!strcmp(filename,"-")) {
	p = 0;
	f = stdin;
    } else if(p && (!strcmp(p,".gz") || !strcmp(p,".Z"))) {
	p = alloca(20 + strlen(filename));
	strcpy(p,"gunzip < ");
	strcat(p,filename);
	f = popen(p,"r");
	if(!f) { perror(p); return Exit_Garbage; }
    } else {
	p = 0;
	f = fopen(filename,"r");
    }
    if(!f) {
	perror(filename);
	return Exit_Garbage;
    }

    if(Verbose > 1)
	fprintf(stderr,"Working on <%s>\n",filename);

    Delete(FileName);
    FileName = String(filename);

    /* If we cannot seek, we're doomed, so copy to a tmp-file in that case */
    if(!p &&  -1 == fseek(f,0,SEEK_END)) {
	char *fn = tempnam(TmpDir,"CTMclient");
	FILE *f2 = fopen(fn,"w+");
	int i;

	if(!f2) {
	    perror(fn);
	    fclose(f);
	    return Exit_Broke;
	}
	unlink(fn);
	if (Verbose > 0)
	    fprintf(stderr,"Writing tmp-file \"%s\"\n",fn);
	while(EOF != (i=getc(f)))
	    if(EOF == putc(i,f2)) {
		fclose(f2);
		return Exit_Broke;
	    }
	fclose(f);
	f = f2;
    }

    if(!p)
	rewind(f);

    if((i=Pass1(f, applied)))
	goto exit_and_close;

    if(ListIt) {
	i = Exit_Done;
	goto exit_and_close;
    }

    if(!p) {
        rewind(f);
    } else {
	pclose(f);
	f = popen(p,"r");
	if(!f) { perror(p); return Exit_Broke; }
    }

    i=Pass2(f);

    if(!p) {
        rewind(f);
    } else {
	pclose(f);
	f = popen(p,"r");
	if(!f) { perror(p); return Exit_Broke; }
    }

    if(i) {
	if((!Force) || (i & ~Exit_Forcible))
	    goto exit_and_close;
    }

    if(CheckIt) {
	if (Verbose > 0) 
	    fprintf(stderr,"All checks out ok.\n");
	i = Exit_Done;
	goto exit_and_close;
    }
   
    /* backup files if requested */
    if(BackupFile) {

	i = PassB(f);

	if(!p) {
	    rewind(f);
	} else {
	    pclose(f);
	    f = popen(p,"r");
	    if(!f) { perror(p); return Exit_Broke; }
	}
    }

    i=Pass3(f);

exit_and_close:
    if(!p)
        fclose(f);
    else
	pclose(f);

    if(i)
	return i;

    if (Verbose > 0)
	fprintf(stderr,"All done ok\n");

    return Exit_Done;
}
