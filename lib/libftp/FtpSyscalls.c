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
#include <unistd.h>
#include <errno.h>

#define DEF(syscal,name) int name(void *a, void *b, void *c) \
{\
   register int status;\
   while (((status=syscal(a,b,c))==-1) && (errno==EINTR));\
   return status;\
}

DEF(open,nointr_open)
DEF(close,nointr_close)
DEF(select,nointr_select)
DEF(read,nointr_read)
DEF(write,nointr_write)
DEF(dup,nointr_dup)
DEF(wait,nointr_wait)
DEF(connect,nointr_connect)
DEF(listen,nointr_listen)
DEF(accept,nointr_accept)
