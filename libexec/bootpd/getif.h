/* getif.h */

#ifdef	__STDC__
extern struct ifreq *getif(int, struct in_addr *);
#else
extern struct ifreq *getif();
#endif
