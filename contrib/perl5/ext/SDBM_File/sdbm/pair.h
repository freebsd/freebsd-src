/* Mini EMBED (pair.c) */
#define chkpage sdbm__chkpage
#define delpair sdbm__delpair
#define duppair sdbm__duppair
#define exipair sdbm__exipair
#define fitpair sdbm__fitpair
#define getnkey sdbm__getnkey
#define getpair sdbm__getpair
#define putpair sdbm__putpair
#define splpage sdbm__splpage

extern int fitpair proto((char *, int));
extern void  putpair proto((char *, datum, datum));
extern datum	getpair proto((char *, datum));
extern int  exipair proto((char *, datum));
extern int  delpair proto((char *, datum));
extern int  chkpage proto((char *));
extern datum getnkey proto((char *, int));
extern void splpage proto((char *, char *, long));
#ifdef SEEDUPS
extern int duppair proto((char *, datum));
#endif
