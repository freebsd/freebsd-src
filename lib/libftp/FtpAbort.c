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
#include <unistd.h>

int
FtpAbort(FTP *ftp)
{
  fd_set fds;
  char msgc=IAC;
  String msg;
  
  FD_ZERO(&fds);
  FD_SET(fileno(FTPCMD(ftp)),&fds);
  
  FtpPutc(ftp, FTPCMD(ftp), IAC);
  FtpPutc(ftp, FTPCMD(ftp), IP);
  
  if ( send ( fileno(FTPCMD(ftp)), &msgc , 1 ,MSG_OOB) != 1 )
    return EXIT(ftp,QUIT);
  
  FtpPutc(ftp, FTPCMD(ftp), DM);
  
  FtpSendMessage(ftp,"ABOR");
  
  while (select ( getdtablesize(), &fds, 0,0, &(ftp->timeout) )>0)
    {
      FtpGetMessage(ftp,msg);
      if (FtpGood(FtpNumber(msg),225,226,EOF)) break;
    }
   return 0;
}



