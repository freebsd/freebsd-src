/* dovend.h */

#ifdef	__STDC__
#define P(args) args
#else
#define P(args) ()
#endif

extern int dovend_rfc1497 P((struct host *hp, u_char *buf, int len));
extern int insert_ip P((int, struct in_addr_list *, u_char **, int *));
extern void insert_u_long P((u_int32, u_char **));

#undef P
