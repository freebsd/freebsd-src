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

STATUS FtpType(FTP *ftp, char type)
{
  STATUS p;

  if ((p=FtpCommand(ftp,"TYPE %c",type,200,EOF))>0)
    ftp->mode=(int)type;
  return p;
}


