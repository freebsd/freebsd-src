/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: ctm.c,v 1.6 1994/11/26 08:57:40 phk Exp $
 *
 * This is the client program of 'CTM'.  It will apply a CTM-patch to a 
 * collection of files.
 *
 * Options we'd like to see:
 *
 * -a 			Attempt best effort.
 * -b <dir>		Base-dir	
 * -B <file>		Backup to tar-file.
 * -d <int>		Debug TBD.
 * -m <mail-addr>	Email me instead.
 * -r <name>		Reconstruct file.
 * -R <file>		Read list of files to reconstruct.
 *
 * Options we have:
 * -c			Check it out, don't do anything.
 * -F      		Force
 * -p			Less paranoid.
 * -P			Paranoid.
 * -q 			Tell us less.
 * -T <tmpdir>.		Temporary files.
 * -v 			Tell us more.
 *
 */

#define EXTERN /* */
#include "ctm.h"

extern int Proc(char *);

int
main(int argc, char **argv) 
{
    int stat=0;
    int c;
    extern int optopt,optind;
    extern char * optarg;
    
    Verbose = 1;
    Paranoid = 1;
    setbuf(stderr,0);
    setbuf(stdout,0);
    
    while((c=getopt(argc,argv,"ab:B:cd:Fm:pPqr:R:T:Vv")) != -1) {
	switch (c) {
	    case 'c': CheckIt++;	break; /* Only check it */
	    case 'p': Paranoid--;	break; /* Less Paranoid */
	    case 'P': Paranoid++;	break; /* More Paranoid */
	    case 'q': Verbose--;	break; /* Quiet */
	    case 'v': Verbose++;	break; /* Verbose */
	    case 'T': TmpDir = optarg;	break;
	    case 'F': Force = 1;	break;
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

    if(!argc)
	stat |= Proc("-");

    while(argc-- && stat == Exit_Done) {
	stat |= Proc(*argv++);
    }

    if(stat == Exit_Done)
	stat = Exit_OK;

    if(Verbose)
	fprintf(stderr,"Exit(%d)\n",stat);
    return stat;
}

int
Proc(char *filename)
{
    FILE *f;
    int i;
    char *p = strrchr(filename,'.');

    if(!strcmp(filename,"-")) {
	p = 0;
	f = stdin;
    } else if(p && (!strcmp(p,".gz") || !strcmp(p,".Z"))) {
	p = Malloc(100);
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

    if(FileName) Free(FileName);
    FileName = String(filename);

    /* If we cannot seek, we're doomed, so copy to a tmp-file in that case */
    if(!p &&  -1 == fseek(f,0,SEEK_END)) {
	char *fn = tempnam(TmpDir,"CMTclient");
	FILE *f2 = fopen(fn,"w+");
	int i;

	if(!f2) {
	    perror(fn);
	    fclose(f);
	    return Exit_Broke;
	}
	unlink(fn);
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

    if((i=Pass1(f)))
	return i;

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
	    return i;
    }

    if(CheckIt) {
        fprintf(stderr,"All checks out ok.\n");
	return Exit_Done;
    }

    i=Pass3(f);

    if(!p) {
        fclose(f);
    } else {
	pclose(f);
	Free(p);
    }
    if(i)
	return i;

    fprintf(stderr,"All done ok\n");
    return Exit_Done;
}
