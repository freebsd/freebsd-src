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

STATUS FtpConnect(FTP **con,char * hostname)
{
  struct sockaddr_in unit;
  register struct hostent *host;
  register int new_socket;
  String S1;
  STATUS x;
  
  *con = FtpCreateObject();
  
  if ((host=FtpGetHost(hostname))==NULL)
    return EXIT((*con),QUIT);
  
  strcpy((*con) -> title,host -> h_name); /* Default title for FtpLog */
  
  unit.sin_family = host -> h_addrtype; /* AF_INET */
  
  bcopy(host-> h_addr_list[0],(char *)&unit.sin_addr,host->h_length);
  if ( ( new_socket = socket ( unit.sin_family , SOCK_STREAM , 0)) < 0)
    return EXIT((*con),QUIT);

  unit.sin_port = htons((*con)->port);
  
  while ( connect ( new_socket , (struct sockaddr *)&unit , sizeof unit ) < 0 )
    {
      host -> h_addr_list ++;
      if (host -> h_addr_list[0]==NULL) {
	close(new_socket);
	return EXIT((*con),QUIT);
      }
      bcopy(host -> h_addr_list[0],(char *)&unit,host->h_length);
      close(new_socket);
      if ( ( new_socket = socket ( unit.sin_family , SOCK_STREAM , 0)) < 0)
	{
	  close(new_socket);
	  return EXIT((*con),QUIT);
	}
    }
  
  FTPCMD(*con) = fdopen(new_socket,"r+");

  if ( (x=FtpGetMessage(*con,S1)) == QUIT )
    return EXIT((*con),QUIT);
  if ( ! FtpGood(x,120,220,EOF))
    {
      close(new_socket);
      return EXIT((*con),-x);
    }
  if ( (*con)->mode != 'A' ) FtpType(*con,(*con)->mode);
  
  return EXIT((*con),x);
}



