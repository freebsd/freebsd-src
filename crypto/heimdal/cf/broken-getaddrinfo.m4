dnl $Id: broken-getaddrinfo.m4,v 1.2 2001/08/22 01:05:29 assar Exp $
dnl
dnl test if getaddrinfo can handle numeric services

AC_DEFUN(rk_BROKEN_GETADDRINFO,[
AC_CACHE_CHECK([if getaddrinfo handles numeric services], ac_cv_func_getaddrinfo_numserv,
AC_TRY_RUN([[#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int
main(int argc, char **argv)
{
	struct addrinfo hints, *ai;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = PF_UNSPEC;
	if(getaddrinfo(NULL, "17", &hints, &ai) == EAI_SERVICE)
		return 1;
	return 0;
}
]], ac_cv_func_getaddrinfo_numserv=yes, ac_cv_func_getaddrinfo_numserv=no))])
