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

STATUS FtpCopy (FTP * ftp1 , FTP * ftp2 ,char *in , char * out)
{
  int size;
  char buffer[FTPBUFSIZ];

  if (!*out) out=in;

  if ( FtpTestFlag(ftp1,FTP_REST) &&  FtpTestFlag(ftp2,FTP_REST)
      && (size=FtpSize(ftp1,in))>0
      && FtpCommand(ftp1,"REST 0",0,0,EOF)==350 && FtpCommand(ftp2,"REST 0",0,0,EOF)==350 )
    ftp1->seek=ftp2->seek=size;
  else
    ftp1->seek=ftp2->seek=0;

  FtpAssert(ftp1,FtpData(ftp1,"RETR %s",in,"r"));
  FtpAssert(ftp2,FtpData(ftp2,"STOR %s",out,"w"));

  while ((size=FtpReadBlock(ftp1,buffer,FTPBUFSIZ))>0)
    {
      if (FtpWriteBlock(ftp2,buffer,size)!=size)
	return EXIT(ftp2,QUIT);
    }

  FtpAssert(ftp1,FtpClose(ftp1));
  FtpAssert(ftp2,FtpClose(ftp2));
  return 0;
}







