/*
		      Library for ftpd clients.(libftp)
			Copyright by Oleg Orel
			 All rights reserved.
			
This  library is desined  for  free,  non-commercial  software  creation. 
It is changeable and can be improved. The author would greatly appreciate 
any  advises, new  components  and  patches  of  the  existing  programs.
Commercial  usage is  also  possible  with  participation of it's author.


*/

  
#ifndef __FTPLIBRARY_H
#define __FTPLIBRARY_H


#ifdef __cplusplus
    extern "C" {
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <arpa/ftp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/telnet.h>
#include <sys/stat.h>


#ifndef __FreeBSD__
extern char *sys_errlist[];
extern int errno;
#else
#include <errno.h>
#endif


		/* Standard Macros & Definitions */

		
      
#define EXIT(con,e) \
  ( con -> errno = e,  \
    ((e==QUIT||e==LQUIT)&&(con->IO != NULL))?(*(con->IO))(con,e,sys_errlist[errno]):0,\
   ((con->error != NULL) && (e < 0) )?(*(con->error))(con,e,FtpMessage(e)):0,\
   e)
	
   
#define MAX_ANSWERS 10 /* Number of known goodest answers for reqest */
#define NFDS 64
#define FTPBUFSIZ BUFSIZ
#define LQUIT (-6)
#define QUIT (-5) /* Few time ago QUIT character been 
		     equivalence to zero, changed for clear
		     conflicts with reading functions */
#define Ctrl(x) ((x) - '@')
#define FREE(x) memset ( &(x) , '\0' , sizeof (x) )
#define CUT(x) ((x)&0xff)
#define FtpError(x) ((x)<0)
#define FtpAssert(ftp,x) if (FtpError(x)) return EXIT((ftp),(ftp)->errno);

typedef int STATUS;
typedef char String[256];

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif



		/* Common Information Structure */

typedef struct/* All structure initialize from edited struct FtpInit */
{
  FILE *sock;       /* Command stream to server */

#define FTPDATA(x) ((x)->data)
#define FTPCMD(x) ((x)->sock)


  FILE *data;       /* Data stream to server */
  char mode;        /* Binary, Ascii, ......... */
  int errno;        /* Last error code */
  int ch;           /* Help character for ascii streams */
  
  STATUS (*error)();
  STATUS (*debug)();
  STATUS (*IO)();
  STATUS (*hash)(); /* Call with reading/writing next "int" characters
			 from stream */  
  int seek;        /*
		     Warning! If server not supported REST-command,
		     then seek variable automaticaly turn to zero
		   */
  int flags;       /* FTP_REST, 
		      FTP_NOEXIT */
  struct timeval timeout;
                   /* How long must be waiting next character 
		      from server */
  int port;
  String title;    /* Using for FtpLog, FtpConnect lets hostname */
  unsigned long counter; 
                   /* Using by FtpHash */
} FTP;

typedef struct 
{
  struct tm createtime;
  unsigned long size;
  String host;
  String file;
} ARCHIE;


enum {FTP_REST=1,FTP_NOEXIT=2};
enum {no,yes};
enum {off,on};
enum {false,true};

extern FTP FtpInit;

/* Options defines */

#define FtpSetFlag(ftp,flag) ((ftp)->flags|=(flag))
#define FtpClearFlag(ftp,flag) ((ftp)->flags &= (~(flag)) )
#define FtpTestFlag(ftp,flag) ((ftp)->flags&(flag)==flag)
#define FtpSetTimeout(ftp,tim) \
  ((ftp)->timeout.tv_sec=tim,(ftp)->timeout.tv_usec=0)
#define FtpSetPort(ftp,prt) ((ftp)->port=(prt))

/* Connect & disconnect */ 

STATUS  FtpConnect(FTP **con,char *hostname);
#define FtpUser(ftp,user)           FtpCommand(ftp,"USER %s",user,230,331,332,EOF)
#define FtpPassword(ftp,pas)        FtpCommand(ftp,"PASS %s",pas,230,332,EOF)
#define FtpAccount(ftp,acc)         FtpCommand(ftp,"ACCT %s",acc,230,EOF)
STATUS  FtpLogin(FTP **con,char *host ,char *user,char *pass,char *acct);
STATUS  FtpBye (FTP * con);
STATUS  FtpQuickBye (FTP * con);
STATUS  FtpAbort(FTP *ftp);

/* Set type of transfer */

STATUS FtpType(FTP *ftp,char type);
#define FtpAscii(ftp)               FtpType(ftp,'A')
#define FtpBinary(ftp)              FtpType(ftp,'I')


/* Send/Receive and handling Procedure(s) */

STATUS  FtpCopy(FTP *ftp1, FTP *ftp2, char *in, char *out);
STATUS  FtpPassiveTransfer(FTP *ftp1, FTP *ftp2, char *in, char *out);

STATUS  FtpRetr(FTP *con, char *command,char *inp,char *out);
#define FtpGet(ftp,in,out)          FtpRetr(ftp,"RETR %s",in,out)
#define FtpDirectory(ftp,pat,out)   FtpRetr(ftp,"LIST %s",pat,out)
#define FtpDir(ftp,out)             FtpRetr(ftp,"LIST","",out)

STATUS  FtpStor(FTP *con ,char*command ,char *inp,char *out);
#define FtpPut(ftp,in,out)          FtpStor(ftp,"STOR %s",in,out)

STATUS  FtpData( FTP * con , char * command , char * param , char *mode);
STATUS  FtpPort ( FTP *con ,int ,int ,int ,int ,int ,int );
#define FtpOpenRead(ftp,file)       FtpData(ftp,"RETR %s",file,"r")
#define FtpOpenWrite(ftp,file)      FtpData(ftp,"STOR %s",file,"w")
#define FtpOpenAppend(ftp,file)     FtpData(ftp,"APPE %s",file,"r")
STATUS  FtpOpenDir( FTP * con , char * files );
STATUS  FtpClose ( FTP *);

STATUS	FtpReadBlock(FTP *ftp, char *buffer, int size);
STATUS FtpWriteBlock(FTP *ftp, char *buffer, int size);

/* Command for hand transfer */

STATUS  FtpRead ( FTP * con);
STATUS  FtpWrite ( FTP * con , char c);
int     FtpGetc ( FTP * ftp, FILE *fp );
STATUS  FtpPutc (FTP *ftp, FILE *fp, char c);

/* Manipulation commands for remote server */

STATUS  FtpCommand ();
#define FtpChdir(ftp,dir)           FtpCommand(ftp,"CWD %s",dir,200,250,EOF)
#define FtpMkdir(ftp,dir)           FtpCommand(ftp,"MKD %s",dir,200,257,EOF)
#define FtpRm(ftp,dir)              FtpCommand(ftp,"DELE %s",dir,200,250,EOF)
char   *FtpPwd(FTP *con);
int     FtpSize(FTP *con,char *filename);
STATUS  FtpMove ( FTP *con,char * old,char *new);

/* Procedures for dialog with remote server */

STATUS  FtpInitMessageList();
STATUS  FtpSendMessage( FTP * con , char * Message );
int     FtpGetMessage( FTP * con , char * Message);
char   *FtpMessage(int Number);
int     FtpNumber ( char * Message );


/* Debug */

#define FtpSetErrorHandler(con,f) (con)->error = f
#define FtpSetDebugHandler(con,f) (con)->debug = f 
#define FtpSetIOHandler(con,f) (con)->IO =f
#define FtpSetHashHandler(con,f) (con)->hash =f
#define FtplibDebug(t) FtpDebug(&FtpInit)

STATUS  FtpDebugDebug ( FTP *con, int errno, char * Message);
STATUS  FtpDebugError ( FTP *con, int errno, char * Message);
STATUS  FtpDebugIO    ( FTP *con, int errno, char * Message);
STATUS  FtpLog(char *progtitle, char *msg);
STATUS  FtpHash ( FTP *con, unsigned long number_of_bytes );
void    FtpDebug ( FTP * con );
STATUS  FtpBadReply550 (char *message);


/* Other Procedures */

FTP    *FtpCreateObject();
FILE   *FtpFullOpen(char * file,char * mode );
STATUS  FtpFullSyntax(String,String,String,String,String);
FILE   *Ftpfopen(char *filename,char *mode);
STATUS  Ftpfclose(FILE *);
STATUS  FtpFullClose(FILE *);
STATUS  FtpGood ();
STATUS  FtpGood1 (int, int *);
struct  hostent *FtpGetHost(char *host);
STATUS  FtpFilenameChecher(char *input, char *output);
STATUS  FtpLink(FTP *,FTP *);
int     FtpArchie(char *what, ARCHIE *, int number);

#ifdef __cplusplus
}
#endif
      

/* Additional definitions */

#ifdef _AIX
int accept (int, struct sockaddr_in*, int*);
char *bcopy (char*, char*, size_t);
int bind (int, const void*, int);
int connect (int, struct sockaddr_in*, int);
int gethostname (char*, size_t);
int getsockname (int, struct sockaddr_in*, int*);
int getpeername (int, struct sockaddr_in*, int*);
int getsockopt (int, int, int, void*, int*);
int listen(int, int);
int setsockopt (int, int, int, void*, int);
int socket (int, int, int);
void free (void*);
void *malloc (size_t);
#endif




#endif /* __FTPLIBRARYH_ */



