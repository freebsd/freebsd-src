#define	DEBUG	1	/* enable debugging code (needed for dig) */
#undef	ALLOW_T_UNSPEC	/* enable the "unspec" RR type for old athena */
#define	RESOLVSORT	/* allow sorting of addresses in gethostbyname */
#undef	RFC1535		/* comply with RFC1535 */
#undef	ALLOW_UPDATES	/* destroy your system security */
#undef	USELOOPBACK	/* res_init() bind to localhost */
#undef	SUNSECURITY	/* verify gethostbyaddr() calls - WE DONT NEED IT  */
