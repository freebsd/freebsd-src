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

STATUS FtpCommand(va_alist)
     va_dcl
{
  FTP *con;
  char *command, *param;
  int Answer[MAX_ANSWERS];
  va_list args;
  String S1;
  int i,counter=0;
  
  va_start(args);

  con = va_arg(args,FTP *);
  command = va_arg(args,char *);
  param = va_arg(args, char *);

  while ( 1 )
    {
      if (counter == MAX_ANSWERS)
	return EXIT(con,QUIT);
      Answer[counter] = va_arg(args,int);
      if (Answer[counter] == EOF ) break;
      counter++;
    }

  va_end(args);
  
  
  sprintf(S1,command,param);

  if ( FtpSendMessage(con,S1) == QUIT )
    return EXIT(con,QUIT);
  
  if  ( (i=FtpGetMessage(con,S1)) == QUIT )
    return EXIT(con,QUIT);
  
  if ( ! FtpGood1 ( i , Answer ))
    return EXIT(con,-i);

  return EXIT(con,i);
}
  
  
   
