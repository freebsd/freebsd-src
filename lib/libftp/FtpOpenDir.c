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

STATUS FtpOpenDir(FTP * con,char * file)
{
  String command;

  if ( file == NULL || *file == '\0' )
    strcpy(command,"NLST");
  else
    sprintf(command,"NLST %s",file);

  return FtpCommand(con,command,"",120,150,200,EOF);
}
