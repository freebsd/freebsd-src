/*
Library for ftpd clients.(libftp)
			Copyright by Oleg Orel
			 All rights reserved.

This  library is desined  for  free,  non-commercial  software  creation.
It is changeable and can be improved. The author would greatly appreciate
any  advises, new  components  and  patches  of  the  existing  programs.
Commercial  usage is  also  possible  with  participation of it's author.



*/

char intro[]="\
		    Ftptry - try transfer via FTP.\n\
		 Copyright by Oleg Orel is Reserved.\n\
\n\
This program is writen using \"libftp\".The main orientation for this\n\
program  is  FTPing  via  bad-working  network. Many network  links are\n\
down-up  switched  and  networks  are  broaken,   so  the  problem  of\n\
transfering  large  files  exists.   The  main   method,  used  by this\n\
software  is  repetition  until  successfull  transfer.  There are some\n\
keys for  setting  repetition and timeout  intervals, modes of transfer\n\
(binary and ascii), types of transfer  (get,put,directory). All options\n\
will be described in usage, if the  program is started without them.\n\
\n\
  The libftp you may transfer from host lpuds.oea.ihep.su via ftp-anonymous.\n\
  All question are sent to author via e-mail (orel@oea.ihep.su)\n\
  ";

#include <FtpLibrary.h>
#include <sys/types.h>
#include <sys/file.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>

#ifdef __GNUC__
#define INLINE inline
#else
#define inline
#endif

#define FTPTRY "FTPTRY" /* The name of enviroment
                           variable with default options*/
#define log(x) FtpLog("ftptry",x)
#define DEBUG(x) (debug?log(x):0)
#define USERNAME (getenv("USER")==NULL?getenv("LOGNAME"):getenv("USER"))
#define DEFAULT_TIMEOUT 600


enum __type__ {ascii=1,binary};
enum __mode__ {get=1,put,dir,multiget};
enum __logmode__ {lm_tty,lm_file,lm_mail};
enum __rcode__ {OK_, BREAK_, CANCEL_};

char *gethost();
char *date();
int my_abort();
int my_IO();
int my_debug();
void done(),leave(),sighup();

char
  *machine ="localhost",
  *user="anonymous",
  *password,
  *localfile=NULL,
  *progname="ftptry";

jmp_buf stack,trap;
int counter=0;
String tmp;
FTP *ftp;
int type=ascii;
int sleeptime=600;
int debug=false;
int mode=get;
int logmode=lm_tty;
char *logfile=NULL;
FILE *LIST=NULL;
extern int errno;
extern char *optarg;
extern int optind, opterr;
String PASSWORD;


/* Setup enviroment */

main(int argc,char **argv)
{
  FILE *out,*in;
  int i;

  if (setjmp(trap)!=0)
    exit(1);

  signal(SIGHUP,sighup);
  signal(SIGTRAP,done);
  signal(SIGINT,done);
  signal(SIGQUIT,done);

  sprintf(password=PASSWORD,"%s@%s",
	  USERNAME,
	  gethost());

  progname=argv[0];

  FtpDebug(&FtpInit);
  FtpSetErrorHandler(&FtpInit,my_abort);
  FtpSetIOHandler(&FtpInit,my_IO);
  FtpSetFlag(&FtpInit,FTP_REST);
  FtpSetTimeout(&FtpInit,DEFAULT_TIMEOUT);

  setoptions();
  optind=1;
  options(argc,argv);
  if ( argc<2 ) usage();

  switch(logmode)
    {

    case lm_file:

      if (fork()) quit("Deattaching.....");
      close(0);close(1);close(2);

      if (logfile==NULL)
	{
	  sprintf(tmp,"/tmp/ftptry-%s.XXXXXX",USERNAME);
	  mktemp(tmp);
	}
      else
	strcpy(tmp,logfile);

      open(tmp,O_TRUNC|O_CREAT|O_WRONLY,0600);
      dup(0);
      dup(0);
      break;

    case lm_mail:



      if (fork()) quit("Deattaching.....");

      close(0);close(1);close(2);
      if (popen("/usr/lib/sendmail -t","w")==NULL)
	perror("sendmail"),
	quit("");

      dup(0);
      dup(0);

      printf("From: %s@%s\n",USERNAME,gethost());
      printf("To: %s@%s\n",USERNAME,gethost());
      printf("Subject: ftptry session log\n\n");

      fflush(stdout);

      break;
    }


  signal(SIGHUP,sighup);
  if (isatty(fileno(stdout))) FtpSetHashHandler(&FtpInit,FtpHash);
  loop(argc,argv,optind);
  exit(0);
}



INLINE noargs(int argc, char **argv, int optind)
{
  int i;

  for (i=optind;i<argc;i++)
    if (argv[i]!=NULL) return 0;
  return 1;
}



/* Main loop */

loop(int argc, char **argv, int optind /* First args as filename */ )
{
  int i,rcode;
  String tmp;
  String machine;
  String filename;
  String localfilename;
  char *p1;

  for(i=optind;;(i==argc-1)?i=optind,
      sprintf(tmp,"Sleeping %d secs",sleeptime),log(tmp),sleep(sleeptime):
      i++)
    {
      if (noargs(argc,argv,optind))
	quit("Nothing doing");

      if (strchr(argv[i],':')==NULL)
	{
	  if (find_archie(argv[i],machine,filename,localfilename)==0)
	    argv[i]=NULL;
	}
      else
	{
	  sscanf(argv[i],"%[^:]:%s",machine,filename);
	  if ((p1=strrchr(filename,'/'))!=NULL)
	    strcpy(localfilename,p1+1);
	  else
	    strcpy(localfilename,filename);
	}
      if (localfile == NULL ) localfile=localfilename;
      if ((rcode=transfer(machine, filename, localfile))==OK_)
	DEBUG("Transfer complete"),
	exit(0);
      if (rcode==CANCEL_ && strchr(argv[i],':')!=NULL)
	argv[i]=NULL;
    }
}

transfer(char *machine, char *file, char *localfile)
{
  int r;
  String tmp;

  if ((r=setjmp(stack))!=0)
    return r;

  sprintf(tmp,"Start transfer, machine is %s, remote file is %s, local file is %s",
	  machine, file, localfile);
  DEBUG(tmp);

  FtpLogin(&ftp,machine,user,password,NULL);

  if (type==binary)
    FtpBinary(ftp);

  switch (mode)
    {

    case get:

      FtpRetr(ftp,"RETR %s",file,localfile);
      break;

    case put:

      FtpStor(ftp,"STOR %s",localfile,file);
      break;

    case dir:

      FtpRetr(ftp,"LIST %s",file,localfile);
      break;

    case multiget:
      domultiget(file);
      break;

    }

  FtpBye(ftp);

  DEBUG("Transfer complete");
  return OK_;
}


void leave(int code)
{
  FtpQuickBye(ftp);
  DEBUG("Leaving transfering");
  longjmp(stack,code);
}

void sighup()
{
  leave(BREAK_);
}


quit(char *s)
{
  log(s);
  longjmp(trap,1);
}


my_IO(FTP *ftp, int n, char *s )
{

  DEBUG(s);
  leave(BREAK_);
}

my_abort(FTP *ftp, int n, char *s )
{

  log(s);
  /* No access or not found, exclude network or host unreachable */;
  if (abs(n) == 550 && FtpBadReply550(s))
    leave(CANCEL_);
  leave(BREAK_);
}


domultiget(char *file)
{
  String list,localname,tmp_name;
  char *p1;

  sprintf(list,"/tmp/ftptry-%s-multiget.XXXXXX",USERNAME);
  mktemp(list);

  FtpRetr(ftp,"NLST %s",file,list);

  if ((LIST=fopen(list,"r"))==NULL) quit((char *)sys_errlist[errno]);

  while ( fgets (tmp, sizeof tmp, LIST) != NULL )
    {
      tmp[strlen(tmp)-1]=0;
      if ((p1=strrchr(tmp,'/'))!=NULL)
	strcpy(localname,p1+1);
      else
	strcpy(localname,tmp);

      DEBUG(localname);
      FtpGet(ftp,tmp,localname);
    }

  fclose(LIST);
  LIST=NULL;
  return;
}

usage()
{
  fprintf(stderr,"\
Usage: %s [optins] [host:file]\n\
        (default host \"localhost\",\n\
         default file \"README\" or \".\" in directory mode)\n\
\n\
Valid options:\n\
\n\
      -u user              default anonymous\n\
      -p password          default %s\n\
      -P                   inquire password from your terminal\n\
      -l local_file        use only if remote and local file differ\n\
      -c                   direct output to stdout(like cat)\n\
      -r                   reverse mode, i.e. send file to remote host\n\
      -d                   directory mode, remote file is patern or \"ls\" options\n\
                           default output is stdout.\n\
      -G                   multiget mode, file is patern for \"ls\" command\n\
                           again at next try.\n\
      -b                   binary transfer mode\n\
      -s seconds           Retry interval, default 10 minutes\n\
      -t seconds           Timeout, default 10 minutes\n\
      -D                   Turn on debugging mode\n\
      -B                   Run in background and direct output to\n\
                           /tmp/ftptry-%s.XXXXXX\n\
      -o file              Run in background and direct output\n\
                           to file\n\
      -m                   Send output to orel via e-mail\n\
      -I                   Print short introduction\n\
\n\
Example:\n\
      %s  -Dbs 300 garbo.uwasa.fi:ls-lR.Z\n\
                   Retrive file ls-lR.Z from garbo.uwasa.fi in binary mode\n\
                   trying to reestablish connection every 5 minutes\n\
                   on failure. Print debugging information.\n\
\n\
      You can set default options to %s enviroment variable\n\
",progname,password,USERNAME,progname,FTPTRY);
  exit(-1);
}


char *gethost()
{
  static String tmp;
  String tmp2;

  gethostname(tmp2,sizeof tmp2);

  strcpy(tmp,FtpGetHost(tmp2)->h_name);
  return tmp;
}


void done(sig)
{
  String x;

  signal(sig,done);
  sprintf(x,"interputed by signal %d",sig);
  log(x);
  longjmp(trap,1);
}


options(int argc,char **argv)
{
  char c;

  while((c=getopt(argc,argv,"GOIBDru:p:Pdbs:o:l:t:cm"))!=EOF)
    {
      switch(c)
	{

	case 'G':
	  mode=multiget;
	  break;

	case 'c':

	  if (localfile==NULL) localfile="*STDOUT*";
	  break;

	case 'I':
	  fprintf(stderr,intro);
	  exit(0);

	case 'r':

	  mode=put;
	  break;

	case 'd':

	  mode=dir;
	  if (localfile==NULL) localfile="*STDOUT*";
	  break;

	case 't':

	  FtpSetTimeout(&FtpInit,atoi(optarg));
	  break;

	case 'l':

	  localfile=optarg;
	  break;

	case 'D':

	  debug=true;
	  break;

	case 'u':

	  user=optarg;
	  break;

	case 'p':

	  password=optarg;
	  break;

	case 'P':

	  password=(char *)getpass("Password:");
	  break;

	case 'b':

	  type=binary;
	  break;

	case 's':

	  sleeptime=atoi(optarg);
	  break;

	case 'o':

	  logmode=lm_file;
	  logfile=optarg;
	  break;

	case 'm':

	  logmode=lm_mail;
	  break;

	case 'B':

	  logmode=lm_file;
	  logfile=NULL;
	  break;

	default:

	  usage();

	}
    }
}


setoptions()
{
  String x;
  char *p,*sp;
  int argc;
  char *argv[100];




  if ((p=(char *)getenv(FTPTRY))!=NULL && *p!=0)
    strcpy(x,p);
  else
    return;



  for (argv[0]="",p=x,sp=x,argc=1; *p!=0 ; p++)
    {
      if (*p==' ')
	{
	  *p=0;
	  argv[argc++] = sp;
	  sp = p+1;
	}
    }

  argv[argc++]=sp;

  options(argc,argv);
}


INLINE unsigned long maxsize(String *result, int hops)
{
  unsigned long maxsiz=0,cursiz=0;
  int i;

  for (i=0;i<hops;i++)
    {
      sscanf(result[i],"%*[^ ] %u %*s",&cursiz);
      if (cursiz>maxsiz)maxsiz=cursiz;
    }
  return maxsiz;
}


find_archie(char *what,char *machine, char *filename, char *localfilename)
{

#define MAXHOPS 15

  static String result[MAXHOPS];
  static int last=0;
  static int size=0;
  static int first=0;
  int rnd;
  static int init=0;
  static String oldwhat={'\0'};
  char *p1;
  int i;

  if (!init || strcmp(oldwhat,what)!=0 || last==0)
    {
      String cmd;
      FILE *archie;

      for (i=0;i<MAXHOPS;i++) result[i][0]=0;
      sprintf(cmd,"archie -l -m %d %s",MAXHOPS,what);
      DEBUG(cmd);

      if ((archie = popen(cmd,"r"))==NULL)
	quit("Archie can't execute");

      for(i=0;i<MAXHOPS;i++)
	{
	  if (fgets(result[i],sizeof (result[i]), archie)==NULL)
	    break;
	  result[i][strlen(result[i])-1]=0;
	  DEBUG(result[i]);
	}


      last=i;

      if ( last==0 )
	{
	  DEBUG("archie not found need file");
	  return(0);
	}


      init=1;
      first=0;
      strcpy(oldwhat,what);
      size=maxsize(result,MAXHOPS);
  }

  if (first >= last-1) first=0;
  for ( i=first; i<last; i++)
    if ( atoi ( strchr(result[i],' ') + 1 ) == size)
      {
	first=i+1;
	break;
      }

  DEBUG("Archie select is ... (see next line)");
  DEBUG(result[i]);

  if (sscanf ( result[i] , "%*[^ ] %*[^ ] %[^ ] %s", machine, filename )!=2)
    {
      DEBUG("Bad archie output format");
      last=0;
      return(0);
    }


  if ( (p1=strrchr(filename,'/'))!= NULL)
    strcpy(localfilename,p1+1);
  else
    strcpy(localfilename,filename);

  return(1);
}






