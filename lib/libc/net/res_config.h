#define	DEBUG	1	/* enable debugging code (needed for dig) */
#undef	ALLOW_T_UNSPEC	/* enable the "unspec" RR type for old athena */
#define	RESOLVSORT	/* allow sorting of addresses in gethostbyname */
#define	RFC1535		/* comply with RFC1535 (STRONGLY reccomended by vixie)*/
#undef	ALLOW_UPDATES	/* destroy your system security */
#undef	USELOOPBACK	/* res_init() bind to localhost */
#undef	SUNSECURITY	/* verify gethostbyaddr() calls - WE DONT NEED IT  */
#define MULTI_PTRS_ARE_ALIASES	/* fold multiple PTR records into aliases */
