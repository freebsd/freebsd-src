/* db_proc.h - prototypes for functions in db_*.c
 *
 * $Id: db_func.h,v 1.1.1.1 1994/09/22 19:46:13 pst Exp $
 */

/* ++from db_update.c++ */
extern int		db_update __P((char name[],
				       struct databuf *odp,
				       struct databuf *newdp,
				       int flags,
				       struct hashbuf *htp));
/* --from db_update.c-- */

/* ++from db_reload.c++ */
extern void		db_reload __P((void));
/* --from db_reload.c-- */

/* ++from db_save.c++ */
extern struct namebuf	*savename __P((char *));
#ifdef DMALLOC
extern struct databuf	*savedata_tagged __P((char *, int,
					      int, int, u_int32_t,
					      u_char *, int));
#define savedata(class, type, ttl, data, size) \
	savedata_tagged(__FILE__, __LINE__, class, type, ttl, data, size)
#else
extern struct databuf	*savedata __P((int, int, u_int32_t,
				       u_char *, int));
#endif
extern struct hashbuf	*savehash __P((struct hashbuf *));
/* --from db_save.c-- */

/* ++from db_dump.c++ */
extern int		db_dump __P((struct hashbuf *, FILE *, int, char *)),
			zt_dump __P((FILE *)),
			atob __P((char *, int, char *, int, int *));
extern void		doachkpt __P((void)),
			doadump __P((void));
#ifdef ALLOW_UPDATES
extern void		zonedump __P((struct zoneinfo *));
#endif
/* --from db_dump.c-- */

/* ++from db_load.c++ */
extern void		endline __P((FILE *)),
			get_netlist __P((FILE *, struct netinfo **,
					 int, char *)),
			free_netlist __P((struct netinfo **));
extern int		getword __P((char *, int, FILE *)),
			getnum __P((FILE *, char *, int)),
			db_load __P((char *, char *, struct zoneinfo *, int)),
			position_on_netlist __P((struct in_addr,
						 struct netinfo *));
extern struct netinfo	*addr_on_netlist __P((struct in_addr,
					      struct netinfo *));
/* --from db_load.c-- */

/* ++from db_glue.c++ */
extern void		buildservicelist __P((void)),
			buildprotolist __P((void)),
			gettime __P((struct timeval *)),
			getname __P((struct namebuf *, char *, int));
extern int		servicenumber __P((char *)),
			protocolnumber __P((char *)),
			my_close __P((int)),
			my_fclose __P((FILE *)),
#ifdef GEN_AXFR
			get_class __P((char *)),
#endif
			writemsg __P((int, u_char *, int)),
			dhash __P((u_char *, int)),
			samedomain __P((const char *, const char *));
extern char		*protocolname __P((int)),
			*servicename __P((u_int16_t, char *)),
			*savestr __P((char *));
#ifndef BSD
extern int		getdtablesize __P((void));
#endif
extern struct databuf	*rm_datum __P((struct databuf *,
				       struct namebuf *,
				       struct databuf *));
extern struct namebuf	*rm_name __P((struct namebuf *,
				      struct namebuf **,
				      struct namebuf *));
#ifdef INVQ
extern void		addinv __P((struct namebuf *, struct databuf *)),
			rminv __P((struct databuf *));
struct invbuf		*saveinv __P((void));
#endif
/* --from db_glue.c-- */

/* ++from db_lookup.c++ */
extern struct namebuf	*nlookup __P((char *, struct hashbuf **,
				      char **, int));
extern int		match __P((struct databuf *, int, int));
/* --from db_lookup.c-- */

/* ++from db_secure.c++ */
#ifdef SECURE_ZONES
extern int		build_secure_netlist __P((struct zoneinfo *));
#endif
/* --from db_secure.c-- */
