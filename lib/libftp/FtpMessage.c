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
#include <ctype.h>

static char * FtpMessageList[1000];

INLINE static char *___gets(char *s, int maxchars, FTP *ftp)
{
  char *p=s;
  int c;

  while (1)
    {
      if ((c = FtpGetc(ftp,FTPCMD(ftp))) == EOF)
	return NULL;
      
      if ( c == '\n' && *(p-1) == '\r' )
	{
	  p--;
	  *p='\0';
	  return s;
	}
      
      if ( (p-s) > maxchars ) return NULL;
      
      *p++=(char)c;
    }
}
    


     
int FtpGetMessage(FTP *con , char * Message )
{
  int n;
  char tmp[1024];
  
  while(1)
    {
      if (___gets(tmp,sizeof tmp,con)==NULL)
	return EXIT(con,QUIT);
      if (isdigit(tmp[0]) &&
	  isdigit(tmp[1]) &&
	  isdigit(tmp[2]) &&
	  tmp[3]!='-') break;
      if ( con -> debug != NULL )
	(*con->debug)(con,0,tmp);
    }

  strcpy(Message,tmp); 
  FtpInitMessageList();
  FtpMessageList[n=FtpNumber(Message)] =
    ( char * ) malloc ( strlen(Message) + 1);
  strcpy(FtpMessageList[n] , Message );
  if ( con -> debug != NULL )
    (*con->debug)(con,n,Message);

  return n;
}

STATUS FtpSendMessage(FTP *ftp,char * Message )
{
  char *msg=Message;
  
  while (*Message) 
    FtpAssert(ftp,FtpPutc(ftp,FTPCMD(ftp),*Message++));
  
  FtpAssert(ftp,FtpPutc(ftp,FTPCMD(ftp),'\015'));
  FtpAssert(ftp,FtpPutc(ftp,FTPCMD(ftp),'\012'));

  if ( ftp -> debug != NULL )
    (*ftp->debug)(ftp,0,msg);
  return 1;
}

char *FtpMessage(int number)
{
  extern int sys_nerr,errno;
#ifndef __FreeBSD__
  extern char *sys_errlist[]; 
#endif
  
  FtpInitMessageList();
  if ( number == 0 )
    return (char *)sys_errlist[errno];
  return (FtpMessageList[abs(number)]==NULL)?
    "":FtpMessageList[abs(number)];
}


STATUS FtpInitMessageList()
{
  int i;
  static u = 0;
  
  if ( u )
    return 1;

  u = 1;

  for (i=0;i<1000;i++)
    FtpMessageList[i]=NULL;

  return 1;
}

int FtpNumber(char *Message)
{
  return (Message[0] - '0') * 100 +
         (Message[1] - '0') * 10  +
	 (Message[2] - '0') ;
}





