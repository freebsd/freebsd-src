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



struct hostent *FtpGetHost(char *host)
{

  static struct in_addr addr;
  static struct hostent _host;
  static char *point[2];
  static char *alias[1];

  bzero(&_host,sizeof _host);
  if ( (addr.s_addr=inet_addr(host)) != -1 )
    {
      _host.h_addr_list = point;
      _host.h_addr_list[0] = (char *) &addr;
      _host.h_addr_list[1] = (char *) 0;
      alias[0]=NULL;
      _host.h_aliases=alias;
      _host.h_name=host;
      _host.h_length=sizeof(unsigned long);
      _host.h_addrtype=AF_INET;
      return &_host;
    }
  
  return gethostbyname(host);
}


