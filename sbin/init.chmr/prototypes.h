/*
 * prototypes
 */

/* from init.c: */
void		main(int, char **);

void		sig_tstp(int);
void		sig_term(int);
void		sig_hup(int);
void		sig_alrm(int);
#ifdef DEBUG
void		sig_usr1(int);
void		sig_usr2(int);
#endif
#if defined (UNTRUSTED) && !defined (TESTRUN)
void		sig_int(int);
#endif
#ifdef CONFIGURE
void		sig_ttin(int);
#endif

void		singleuser(void);
void		single2multi(void);
void		waitforboot(void);
void		multiuser(void);
void		multi2single(void);

void		callout(unsigned int, retr_t, void *);
void		allocate_callout(void);
void		clear_callout(void);
void		do_callout(void);
void		signalsforchile(void);
void		no_autoboot(void);


/* from ttytab.c: */
ttytab_t	*free_tty(ttytab_t *, ttytab_t *);
#ifdef _TTYENT_H_
ttytab_t	*ent_to_tab(const struct ttyent *, ttytab_t *, ttytab_t *, int);
#endif
int		argv_changed(const char *, const char **);
char		**string_to_argv(const char *, int *, char **);
int		do_getty(ttytab_t *, int);


/* from configure.c: */
#ifdef CONFIGURE
void		configure(char *);
void		getconf(void);
void		setconf(void);
void		checkconf(void);
#endif


/* from utils.c: */
void		iputenv(const char *, const char *);
void		Debug(int, const char *, ...);
int		strCcmp(char *, char *);
char		*newstring(const char *);
long		str2u(const char *);


#ifdef TESTRUN
/* from fake_syslog.c: */
void		openlog();
void		syslog(int, const char *, ...);
void		vsyslog(int, const char *, va_list);
void		closelog(void);
#endif /* TESTRUN */
