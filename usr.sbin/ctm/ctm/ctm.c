/* $Id$
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 * This is the client program of 'CTM'.  It will apply a CTM-patch to a 
 * collection of files.
 *
 * Options we'd like to see:
 *
 * -a 			Attempt best effort.
 * -b <dir>		Base-dir	
 * -B <file>		Backup to tar-file.
 * -c			Check it out, "ma non troppo"
 * -d <int>		Debug TBD.
 * -m <mail-addr>	Email me instead.
 * -p			Less paranoid.
 * -P			Paranoid.
 * -q 			Be quiet.
 * -r <name>		Reconstruct file.
 * -R <file>		Read list of files to reconstruct.
 * -T <tmpdir>.		Temporary files.
 * -v 			Tell about each file.
 *
 * Exit-codes, bitmap, logical or of:
 * 1	Couldn't do something we wanted to, not fatal.
 * 2	Couldn't do something we wanted to, fatal.
 * 4	Input file corrupt.
 * 8	Cannot apply input file.
 * 16   Corruption while applying input file.
 *
 */

#define EXTERN /* */
#include "ctm.h"

extern int Proc(char *);

int
main(int argc, char **argv) 
{
    int stat=0;
    int i,j,c;
    extern int optopt,optind;
    extern char * optarg;
    
    Verbose = 1;
    Paranoid = 1;
    setbuf(stderr,0);
    setbuf(stdout,0);
    
    while((c=getopt(argc,argv,"ab:B:cd:m:pPqr:R:T:Vv")) != -1) {
	switch (c) {
	    case 'p': Paranoid--; break; /* Less Paranoid */
	    case 'P': Paranoid++; break; /* More Paranoid */
	    case 'q': Verbose--;    break; /* Quiet */
	    case 'v': Verbose++;    break; /* Verbose */
	    case 'T': TmpDir = optarg; break;
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
	exit(1);
    }
    stat = 0;
    argc -= optind;
    argv += optind;

    if(!argc)
	stat |= Proc("-");

    while(argc--)
	stat |= Proc(*argv++);

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
    } else if(!strcmp(p,".gz") || !strcmp(p,".Z")) {
	p = Malloc(100);
	strcpy(p,"gunzip < ");
	strcat(p,filename);
	f = popen(p,"r");
    } else {
	p = 0;
	f = fopen(filename,"r");
    }
    if(!f) {
	perror(filename);
	return 1;
    }

    if(Verbose > 1)
	fprintf(stderr,"Working on <%s>\n",filename);

    if(FileName) Free(FileName);
    FileName = String(filename);

    /* If we cannot seek, we're doomed, so copy to a tmp-file in that case */
    if(!p &&  -1 == fseek(f,0,SEEK_END)) {
	char *fn = tempnam(NULL,"CMTclient");
	FILE *f2 = fopen(fn,"w+");
	int i;

	if(!f2) {
	    perror(fn);
	    fclose(f);
	    return 2;
	}
	unlink(fn);
	fprintf(stderr,"Writing tmp-file \"%s\"\n",fn);
	while(EOF != (i=getc(f)))
	    putc(i,f2);
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
    }
    if((i=Pass2(f)))
	return i;
    if(!p) {
        rewind(f);
    } else {
	pclose(f);
	f = popen(p,"r");
    }
    if((i=Pass3(f)))
	return i;
    if(!p) {
        fclose(f);
    } else {
	pclose(f);
    }
    return 0;
}
