/*
		      Library for ftpd clients.(libftp)
			Copyright by Oleg Orel
			 All rights reserved.

This  library is desined  for  free,  non-commercial  software  creation.
It is changeable and can be improved. The author would greatly appreciate
any  advises, new  components  and  patches  of  the  existing  programs.
Commercial  usage is  also  possible  with  participation of it's author.



*/

#include "FtpLibrary.h"

void FtpDebug(FTP *ftp)
{
  STATUS FtpDebugDebug(),
         FtpDebugError(),
         FtpDebugIO();

  FtpSetDebugHandler(ftp,FtpDebugDebug);
  FtpSetErrorHandler(ftp,FtpDebugError);
  FtpSetIOHandler(ftp,FtpDebugIO);
}

STATUS FtpDebugDebug(FTP *ftp,int n, char * Message)
{
  String tmp;


  strcpy(tmp,Message);

  if (strncmp(tmp,"PASS ",5)==0)
    {
      char *p=tmp+5;
      while ( *p != '\0') *p++='*';
    };

  FtpLog(ftp->title,tmp);
  return 1;
}

STATUS FtpDebugError(FTP *ftp,int n, char * Message)
{

  FtpLog("FtpDebugError","");
  FtpLog(ftp->title,Message);
  if ( ! FtpTestFlag(ftp,FTP_NOEXIT))
    exit(1);
  return 0;
}

STATUS FtpDebugIO(FTP *ftp,int n, char * Message)
{
  FtpLog("FtpDebugIO","");
  FtpLog(ftp->title,Message);
  if ( ! FtpTestFlag(ftp,FTP_NOEXIT))
    exit(1);
  return 0;
}

STATUS FtpLog(char *name,char *str)
{
  time_t t=time((time_t *)0);
  struct tm *lt=localtime(&t);
  fprintf(stderr,"%02d:%02d:%02d %s %s\n",lt->tm_hour,
	  lt->tm_min,lt->tm_sec,name,str);
  fflush(stderr);
  return 0;
}

int
FtpHash(FTP *ftp, unsigned long chars)
{

  if (chars==0) return ftp->counter=0;
  ftp->counter+=chars;
  fprintf(stdout,"%10u bytes transfered\r",(unsigned int)ftp->counter);
  fflush(stdout);
  return ftp->counter;
}


STATUS FtpBadReply550(char *s)
{
  if(
     (strstr(s,"unreachable")!=NULL) ||
     (strstr(s,"Broken pipe")!=NULL)
     )
    return 0;
  return 1;
}
