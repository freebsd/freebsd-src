/* uusnap.c
   (c) 1992 Heiko W.Rupp hwr@pilhuhn.ka.sub.org
   uusnap is a tool to display the activities of the connected
   systems.

   Put a file uusnap.systems in NEWCONFIGDIR (see Makefile), in which
   the systems, you want to monitor are listed, one on a single line.
   The sequence of the files there determine the sequence of the
   listing.

   At the moment it only works with taylor config and taylor dirs

   compile it form the Makefile or:
   cc -c -g -pipe -O  -I. -I. -DNEWCONFIGLIB=\"/usr/local/lib/uucp\" uusnap.c
   cc  -o uusnap uusnap.o 
   For this, uusnap.[ch] must be in the same directory as uucp.h and so.

   uusnap must have read access to SPOOLDIR/.Status in order to work.
*/

#define MAXSYS 30		/* maximum number of systems */
#define WAIT_NORMAL 10		/* wait period if noone is talking */
#define WAIT_TALKING 2		/* refresh display every second if */
				/* someone is talking with us */

#include "uucp.h"
#if USE_RCS_ID
char uusnap_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/contrib/uusnap.c,v 1.7 1999/08/28 04:58:22 peter Exp $";
#endif

#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/dir.h>

extern char *ctime(time_t*);

struct sysInfo  {
    char sysname[10];		/* name of the system to watch */
    char *statfile;		/* name of its status file */
    char *spooldir;		/* root of its spooldir */
    int in;			/* number of unprocessed in-files */
    int out;			/* number of files to send them */
    time_t last;		/* last poll time */
    time_t next;		/* time of next poll */
    time_t lastidir;		/* time of last in-spooldir access */
    time_t lastodir;		/* time of last outgoing spd acc */
    time_t laststat;		/* time of last status file access */
    int status;			/* status of the system */
    int num_retries;		/* number of retries */
};

struct sysInfo Systems[MAXSYS];    


/*  I have extend the system status. If time for the specified system
    is Never, I say so. To get this to work, one also should extend
    uucico.c. It is not important to do this. With the normal uucico,
    one only gets no status.
*/

const char *azStatus[] =        /* Status codes as defined by uucico  */
{   			        /* listing them here instead of       */
  "Conversation complete",      /* including the appropriate file     */
  "Port unavailable",           /* reduces the size of the executable */
  "Dial failed",
  "Login failed",
  "Handshake failed",
  "Call failed",
  "Talking",
  "Wrong time to call",
  "Time to call = Never !"
};

main()
{
    int i;
    i=get_systems();
    display_info(i);
    
    exit(0);
}

int
get_systems()
{
    char filename[1024];
    char fn[1024];
    char line[80];
    FILE *fp;
    int i=0;
    int j;
    struct stat stbuf;
    struct sysInfo sys;

    strcpy(filename,NEWCONFIGLIB);
    strcat(filename,"/uusnap.systems");
    if ((fp=fopen(filename,"r"))!=NULL) {
	while (fgets(line,80,fp)!=NULL) {
	    *(rindex(line,'\n'))='\0';
	    strcpy(sys.sysname,line); /* get the name of the system */
	    strcpy(fn,SPOOLDIR); /* get the name of the statusfile */
	    strcat(fn,"/.Status/");
	    strcat(fn,line);
	    sys.statfile=malloc(strlen(fn)+1);
	    strcpy(sys.statfile,fn);
	    strcpy(fn,SPOOLDIR); /* get the name of the spooldir */
	    strcat(fn,"/");
	    strcat(fn,line);
	    sys.spooldir=malloc(strlen(fn)+1);
	    strcpy(sys.spooldir,fn);
	    sys.laststat=0;
	    sys.lastidir=sys.lastodir=0;
	    Systems[i]=sys;		/* get_stat_for_system needs it */
	    get_stat_for_system(i);    	/* now get the system status */
	    get_inq_num(i,TRUE);	/* number of unprocessed files */
	    get_outq_num(i,TRUE);       /* number of files to send */
	    i++;	
	}
	fclose(fp);
    }
    else {
	fprintf(stderr,"Can't open %s \n",filename);
	exit(1);
    }
    return i;
}

	    

display_info(int numSys)
{
    char *filename;
    int sysnum;
    FILE *fp;
    char contentline[80];
    char isTalking=FALSE;
    struct stat stbuf;
    struct sysInfo sys;
    time_t time;

    filename = (char*)malloc(1024);
    if (filename == NULL) {
	fprintf(stderr, "Can't malloc 1024 bytes");
	exit(1);
    }
	
    while(TRUE) {
	display_headline();
	for (sysnum=0;sysnum<numSys;sysnum++) {
	    sys = Systems[sysnum];
	    stat(sys.statfile,&stbuf);
	    if ((time=stbuf.st_atime) > sys.laststat) {
		get_stat_for_system(sysnum);
	    }
	    if(display_status_line(sysnum)==1)
		isTalking=TRUE;
	}
	if (isTalking) {
	    sleep(WAIT_TALKING);
	    isTalking = FALSE;
	}
	else
	    sleep(WAIT_NORMAL);            /* wait a bit */
    }
    return 0;
}

int
display_status_line(int sn)
{
    char *time_s;
    
    int sys_stat,num_retries,wait;
    int i;
    time_t last_time;
    time_t next_time;
     
    struct sysInfo sys;
    
    sys = Systems[sn];

    printf("%10s  ",sys.sysname);
    get_inq_num(sn);
    if (sys.in==0)
	printf("     ");
    else 
	printf("%3d  ",sys.in);
    get_outq_num(sn);
    if (sys.out==0)
	printf("     ");
    else
	printf("%3d  ",sys.out);
    time_s = ctime(&sys.last);
    time_s = time_s + 11;
    *(time_s+8)='\0';
    printf("%8s ",time_s);	/* time of last poll */
    time_s = ctime(&sys.next);
    time_s = time_s + 11;
    *(time_s+8)='\0';
    if (sys.last == sys.next) 
	printf("           ");
    else
	printf("%8s   ",time_s);	/* time of next poll */
    if (sys.num_retries==0) 
	printf("   ");
    else 
	printf("%2d ",sys.num_retries);
    if (sys_stat==6) 		/* system is talking */
	printf("\E[7m");	/* reverse video on */
    printf("%s",azStatus[sys.status]);
    if (sys.status==6) {
	printf("\E[m\n");	/* reverse video off */
	return 1;
    }
    else {
	printf("\n");
	return 0;
    }
}


display_headline()
{
    printf("\E[;H\E[2J");	/* clear screen */
    printf("\E[7muusnap (press CTRL-C to escape)\E[m \n\n");
    printf("  System     #in #out   last     next   #ret    Status\n");
    return 0;
}

get_inq_num(int num,char firstTime)
{
    int i=0;
    char filename[1024];
    struct stat stbuf;
    DIR *dirp;
    
    strcpy(filename,Systems[num].spooldir);
    strcat(filename,"/X./.");
    stat(filename,&stbuf);
    if ((stbuf.st_mtime > Systems[num].lastidir) || (firstTime)) {
	if ((dirp=opendir(filename))!=NULL) {
	    while(readdir(dirp))
		i++;
	    closedir(dirp);
	    stat(filename,&stbuf);
	    Systems[num].lastidir=stbuf.st_mtime;
	}
	else {
	    fprintf(stderr,"Can't open %s \n",filename);
	    exit(1);
	}
	if (i>=2)
	    i-=2;			/* correct . and .. */
	Systems[num].in=i;
    }
    return 0;
}

get_outq_num(int sys,char firstTime)
{
    int i=0;
    char filename[1024];
    struct stat stbuf;
    DIR *dirp;
    
    strcpy(filename,Systems[sys].spooldir);
    strcat(filename,"/C./.");
    stat(filename,&stbuf);
    if ((stbuf.st_mtime > Systems[sys].lastodir) || (firstTime)) {
	if ((dirp=opendir(filename))!=NULL) {
	    while(readdir(dirp))
		i++;
	    closedir(dirp);
	    stat(filename,&stbuf);
	    Systems[sys].lastodir=stbuf.st_mtime;
	}
	else {
	    fprintf(stderr,"Can't open %s \n",filename);
	    exit(1);
	}
	if (i>=2)
	    i-=2;			/* correct . and .. */
	Systems[sys].out=i;
    }
    return 0;
}

get_stat_for_system(int i)
{
    char fn[80];
    struct sysInfo sys;
    struct stat stbuf;
    FILE *fp;
    time_t wait;

    sys = Systems[i];
    stat(sys.statfile,&stbuf);
    if (stbuf.st_atime > sys.laststat) {
	if ((fp=fopen(sys.statfile,"r"))!=NULL) {
	    fgets(fn,80,fp);
	    fclose(fp);
	    sscanf(fn,"%d %d %ld %d",
		   &sys.status,
		   &sys.num_retries,
		   &sys.last,
		   &wait);
	    sys.next=sys.last+wait;
	}
	else {
	    sys.status=0;
	    sys.num_retries=0;
	    sys.last=0;
	    sys.next=0;
	}
	stat(sys.statfile,&stbuf);
	sys.laststat=stbuf.st_atime;
    }
    Systems[i] = sys;
    return 0;
}
