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

STATUS FtpPort(FTP *con,int a,int b,int c,int d,int e,int f)
{
  String cmd;
  int i;

  sprintf(cmd,"PORT %d,%d,%d,%d,%d,%d",a,b,c,d,e,f);
  if ( FtpSendMessage(con,cmd) == QUIT)
    return QUIT;
  if ( (i=FtpGetMessage(con,cmd)) == QUIT)
    return QUIT;
  
  if ( ! FtpGood ( i , 200 , EOF ))
    return EXIT(con,-i);

  return EXIT(con,i);
}
