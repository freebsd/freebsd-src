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
#include <signal.h>

STATUS FtpBye(FTP *ftp)
{
  String S1;
  int i;

  FtpAssert(ftp,FtpCommand(ftp,"QUIT",221,EOF));

  fclose(FTPCMD(ftp));
  free(ftp);
  return 0;
}
      
STATUS FtpQuickBye(FTP *ftp)
{


  if (ftp == NULL) return;
  
  if (FTPDATA(ftp)!=NULL)
    {
      shutdown(fileno(FTPDATA(ftp)),2);
      fclose(FTPDATA(ftp));
    }

  if (FTPCMD(ftp)!=NULL)
    {
      shutdown(fileno(FTPCMD(ftp)),2);
      fclose(FTPCMD(ftp));
    }
  
  free(ftp);
}

