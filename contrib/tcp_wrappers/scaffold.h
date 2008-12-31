 /*
  * @(#) scaffold.h 1.3 94/12/31 18:19:19
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  *
  * $FreeBSD: src/contrib/tcp_wrappers/scaffold.h,v 1.2.32.1 2008/11/25 02:59:29 kensmith Exp $
  */

#ifdef INET6
extern struct addrinfo *find_inet_addr();
#else
extern struct hostent *find_inet_addr();
#endif
extern int check_dns();
extern int check_path();
