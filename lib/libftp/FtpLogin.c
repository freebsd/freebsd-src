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

STATUS FtpLogin ( FTP ** con, char * host , char * user , 
		  char * password , char * account)
{
  
  FtpAssert((*con),FtpConnect(con,host));
  FtpAssert((*con),FtpUser((*con),user));
  if (((*con)->errno)==230 )
    return ((*con)->errno);
  if (((*con)->errno)==332)
    {
      if ( account == NULL )
	return EXIT(((*con)),(*con)->errno);
      FtpAssert((*con),FtpAccount( (*con) , account ));
      if ( ((*con)->errno)==230 )
	return (*con)->errno;
    }
  return FtpPassword((*con),password);
}
