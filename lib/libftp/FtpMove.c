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

STATUS FtpMove(FTP *con,char * oldname , char * newname )
{
  STATUS i;

  if ((i=FtpCommand(con,"RNFR %s",oldname,200,350,EOF)) > 1 )
    return FtpCommand(con,"RNTO %s",newname,200,250,EOF);
  else
    return i;
}
