#define	DEBUG	1	/* enable debugging code (needed for dig) */
#undef	ALLOW_T_UNSPEC	/* enable the "unspec" RR type for old athena */
#define	RESOLVSORT	/* allow sorting of addresses in gethostbyname */
#define	RFC1535		/* comply with RFC1535 (STRONGLY reccomended by vixie)*/
#undef	USELOOPBACK	/* res_init() bind to localhost */
#undef	SUNSECURITY	/* verify gethostbyaddr() calls - WE DONT NEED IT  */
#undef	ALLOW_UPDATES	/* compile support for update REQUESTS - harmless */
#define MULTI_PTRS_ARE_ALIASES 1 /* fold multiple PTR records into aliases */
