 /*
  * @(#) scaffold.h 1.3 94/12/31 18:19:19
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  *
  * $FreeBSD: src/contrib/tcp_wrappers/scaffold.h,v 1.2.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $
  */

#ifdef INET6
extern struct addrinfo *find_inet_addr();
#else
extern struct hostent *find_inet_addr();
#endif
extern int check_dns();
extern int check_path();
