/*		      Library for ftpd clients.(libftp)
			Copyright by Oleg Orel
			 All rights reserved.
			
This  library is desined  for  free,  non-commercial  software  creation. 
It is changeable and can be improved. The author would greatly appreciate 
any  advises, new  components  and  patches  of  the  exist
ing  programs.
Commercial  usage is  also  possible  with  participation of it's author.



*/

#include "FtpLibrary.h"


char * FtpPasv (FTP *ftp)
{
  char *msg;
  String PORT;
  char *p=PORT;
  
  if FtpError(FtpCommand(ftp,"PASV","",227,EOF)) 
    return "";
  
  msg = FtpMessage (227);

  msg+=3;
  
  while (!isdigit(*msg++));  
  msg--;

  while (isdigit(*msg)||*msg==',') *p++=*msg++;
  *p=0;
  
  return PORT;
}


STATUS FtpLink(FTP *ftp1, FTP *ftp2)
{
  
  String PORT;

  strcpy(PORT,FtpPasv(ftp1));

  FtpCommand(ftp2,"PORT %s",PORT,200,EOF);
}

STATUS FtpPassiveTransfer(FTP *ftp1, FTP *ftp2, char *f1, char *f2)
{
  String tmp;
  fd_set fds;

  FtpAssert(ftp1,FtpLink(ftp1,ftp2));


  if (!*f2) f2=f1;

  FtpAssert(ftp2,FtpCommand(ftp2,"STOR %s",f2, 200, 120 , 150 , 125 , 250 , EOF ));
  FtpAssert(ftp1,FtpCommand(ftp1,"RETR %s",f1, 200, 120 , 150 , 125 , 250 , EOF ));
  
  FD_ZERO(&fds);
  
  FD_SET(fileno(FTPCMD(ftp1)),&fds);
  FD_SET(fileno(FTPCMD(ftp2)),&fds);
  
  if (select(getdtablesize(),&fds,0,0,0)<0)
    {
      if (ftp1->error!=NULL)
	return (*(ftp1->error))(ftp1,QUIT,sys_errlist[errno]);
      if (ftp2->error!=NULL)
	return (*(ftp2->error))(ftp1,QUIT,sys_errlist[errno]);
      return QUIT;
    }
  
  if (FD_ISSET(fileno(FTPCMD(ftp1)),&fds))
    {
      FtpGetMessage(ftp1,tmp);
      FtpLog(ftp1->title,tmp);
      FtpGetMessage(ftp2,tmp);
      FtpLog(ftp2->title,tmp);
    }
  else
    {
      FtpGetMessage(ftp2,tmp);
      FtpLog(ftp2->title,tmp);
      FtpGetMessage(ftp1,tmp);
      FtpLog(ftp1->title,tmp);
    }
}





