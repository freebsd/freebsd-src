/*
 * Defaults
 */
#define RETRYTIME		5*60
#define INIT_M2S_TERMTO		10
#define INIT_M2S_KILLTO		30
#define DEF_CHECKTIME		5
#define DEF_CHECKSTATUS		1
#define ALLOC_ARGV		4
#define CALLOUT_MINFREE		5
#define CALLOUT_CHUNK		10


#ifndef TESTRUN
#   define DEBUG_LEVEL		0
#   define INIT_CONFIG		"/etc/init.conf"
#else /* TESTRUN */
#   define DEBUG_LEVEL		5
#   define INIT_CONFIG		"./init.conf"
#endif /* TESTRUN */


#ifdef DEBUG
extern int	debug;
#endif
extern int	retrytime;
extern char	**ienviron;
extern sigset_t	block_set;


#define blocksig()	sigprocmask(SIG_BLOCK, &block_set, 0)
#define unblocksig()	sigprocmask(SIG_UNBLOCK, &block_set, 0)



/* internal representation of getty table */
typedef struct ttytab	{
	struct ttytab	*next;
	struct ttytab	*prev;
	char		*name;		/* device name */
	char		**argv;		/* argv for execve() */
	char		*type;		/* terminal type */
	int		intflags;	/* internal flags, see below */
	pid_t		pid;		/* PID of spawned process */
	int		failcount;	/* how often getty exited with error */
	time_t		starttime;	/* when it was started */
	}	ttytab_t;


/* Values for intflags: */
#define INIT_SEEN	0x001
#define INIT_CHANGED	0x002
#define INIT_NEW	0x004
#define INIT_FAILED	0x008	/* process exited with error code last time */
#define INIT_OPEN	0x010	/* Init has to do the open() */
#define INIT_NODEV	0x020	/* do not append device to argv */
#define INIT_DONTSPAWN	0x040	/* do not respawn a process on this line */
#define INIT_ARG0	0x080	/* don't pass command as argv[0] */
#define INIT_FAILSLEEP	0x100	/* getty is asleep before it is retried */


/* type field for callout table */
typedef enum	{
	CO_ENT2TAB,		/* retry multiuser() */
	CO_FORK,		/* retry do_getty(tt) */
	CO_GETTY,		/* retry do_getty(tt) */
	CO_MUL2SIN,		/* timeout in multi2single */
	} retr_t;

/* format of callout table */
typedef struct callout {
	struct callout	*next;
	unsigned int	sleept;
	retr_t		what;
	void		*arg;
	}	callout_t;
