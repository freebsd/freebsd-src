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

int FtpSize(FTP * con, char *filename)
{
  String tmp;
  int i,size;

  strcpy(tmp,"SIZE ");
  strcat(tmp,filename);

  if ( FtpSendMessage(con,tmp) == QUIT )
    return EXIT(con,QUIT);
  if ( (i=FtpGetMessage(con,tmp)) == QUIT )
    return EXIT(con,QUIT);

  if ( i != 213 )
    return con -> errno = (-i);

  sscanf(tmp,"%*d %d",&size);
  return size;
}
