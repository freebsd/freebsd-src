/*
 * arlib.h (C)opyright 1992 Darren Reed.
 */

#define	ARES_INITLIST	1
#define	ARES_CALLINIT	2
#define ARES_INITSOCK	4
#define ARES_INITDEBG	8
#define ARES_INITCACH	16

#ifdef	__STDC__
extern	struct	hostent	*ar_answer(char *, int);
extern	void    ar_close();
extern	int     ar_delete(char *, int);
extern	int     ar_gethostbyname(char *, char *, int);
extern	int     ar_gethostbyaddr(char *, char *, int);
extern	int     ar_init(int);
extern	int     ar_open();
extern	long    ar_timeout(time_t, char *, int);
#else
extern	struct	hostent	*ar_answer();
extern	void    ar_close();
extern	int     ar_delete();
extern	int     ar_gethostbyname();
extern	int     ar_gethostbyaddr();
extern	int     ar_init();
extern	int     ar_open();
extern	long    ar_timeout();
#endif
