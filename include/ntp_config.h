/*
 * Configuration file name
 */
#ifndef CONFIG_FILE
# ifndef SYS_WINNT
#  define	CONFIG_FILE "/etc/ntp.conf"
# else /* SYS_WINNT */
#  define	CONFIG_FILE	"%windir%\\system32\\drivers\\etc\\ntp.conf"
#  define	ALT_CONFIG_FILE "%windir%\\ntp.conf"
#  define	NTP_KEYSDIR	"%windir%\\system32\\drivers\\etc"
# endif /* SYS_WINNT */
#endif /* not CONFIG_FILE */

/*
 * Types of entries we understand.
 */
#define CONFIG_UNKNOWN		0

/*
 * Command keywords
 */
#define CONFIG_PEER		1
#define CONFIG_SERVER		2
#define CONFIG_AUTOMAX		3
#define CONFIG_DRIFTFILE	4
#define CONFIG_BROADCAST	5
#define CONFIG_BROADCASTCLIENT	6
#define CONFIG_AUTHENTICATE	7
#define CONFIG_KEYS		8
#define CONFIG_REVOKE		9
#define CONFIG_PPS		10
#define CONFIG_RESTRICT		11
#define CONFIG_BDELAY		12
#define CONFIG_TRUSTEDKEY	13
#define CONFIG_REQUESTKEY	14
#define CONFIG_CONTROLKEY	15
#define CONFIG_TRAP		16
#define CONFIG_FUDGE		17
#define CONFIG_TINKER		18
#define CONFIG_STATSDIR		19
#define CONFIG_FILEGEN		20
#define CONFIG_STATISTICS	21
#define CONFIG_PIDFILE		22
#define CONFIG_SETVAR		23
#define CONFIG_DISCARD		24
#define CONFIG_ADJ		25
#define CONFIG_MULTICASTCLIENT	26
#define CONFIG_ENABLE		27
#define CONFIG_DISABLE		28
#define CONFIG_PHONE		29
#define CONFIG_LOGFILE		30
#define CONFIG_LOGCONFIG	31
#define CONFIG_MANYCASTCLIENT	32
#define CONFIG_MANYCASTSERVER	33
#define CONFIG_TOS		34
#define CONFIG_TTL		35
#define CONFIG_INCLUDEFILE      36
#define CONFIG_KEYSDIR		37
#define CONFIG_CDELAY		38
#ifdef OPENSSL
#define CONFIG_CRYPTO		39
#endif /* OPENSSL */

/*
 * "peer", "server", "broadcast" modifier keywords
 */
#define CONF_MOD_VERSION	1
#define CONF_MOD_KEY		2
#define CONF_MOD_MINPOLL	3
#define CONF_MOD_MAXPOLL	4
#define CONF_MOD_PREFER		5
#define CONF_MOD_BURST		6
#define CONF_MOD_IBURST		7
#define CONF_MOD_SKEY		8
#define CONF_MOD_TTL		9
#define CONF_MOD_MODE		10
#define CONF_MOD_NOSELECT 	11

/*
 * "restrict" modifier keywords
 */
#define CONF_RES_MASK		1
#define CONF_RES_IGNORE		2
#define CONF_RES_NOSERVE	3
#define CONF_RES_NOTRUST	4
#define CONF_RES_NOQUERY	5
#define CONF_RES_NOMODIFY	6
#define CONF_RES_NOPEER		7
#define CONF_RES_NOTRAP		8
#define CONF_RES_LPTRAP		9
#define CONF_RES_NTPPORT	10
#define CONF_RES_LIMITED	11
#define CONF_RES_VERSION	12
#define CONF_RES_DEMOBILIZE	13

/*
 * "trap" modifier keywords
 */
#define CONF_TRAP_PORT		1
#define CONF_TRAP_INTERFACE	2

/*
 * "fudge" modifier keywords
 */
#define CONF_FDG_TIME1		1
#define CONF_FDG_TIME2		2
#define CONF_FDG_STRATUM	3
#define CONF_FDG_REFID		4
#define CONF_FDG_FLAG1		5
#define CONF_FDG_FLAG2		6
#define CONF_FDG_FLAG3		7
#define CONF_FDG_FLAG4		8

/*
 * "filegen" modifier keywords
 */
#define CONF_FGEN_FILE		1
#define CONF_FGEN_TYPE		2
#define CONF_FGEN_FLAG_LINK	3
#define CONF_FGEN_FLAG_NOLINK	4
#define CONF_FGEN_FLAG_ENABLE	5
#define CONF_FGEN_FLAG_DISABLE	6

/*
 * "pps" modifier keywords
 */
#define CONF_PPS_ASSERT		1
#define CONF_PPS_CLEAR		2
#define CONF_PPS_HARDPPS	3

/*
 * "discard" modifier keywords
 */
#define CONF_DISCARD_AVERAGE	1
#define CONF_DISCARD_MINIMUM	2
#define	CONF_DISCARD_MONITOR	3

/*
 * "tinker" modifier keywords
 */
#define CONF_CLOCK_MAX		1
#define CONF_CLOCK_PANIC	2
#define CONF_CLOCK_PHI		3
#define CONF_CLOCK_MINSTEP	4
#define CONF_CLOCK_ALLAN	5
#define CONF_CLOCK_HUFFPUFF	6
#define CONF_CLOCK_FREQ		7

/*
 * "tos" modifier keywords
 */
#define CONF_TOS_MINCLOCK	1
#define CONF_TOS_MINSANE	2
#define CONF_TOS_FLOOR		3
#define CONF_TOS_CEILING	4
#define CONF_TOS_COHORT		5

#ifdef OPENSSL
/*
 * "crypto" modifier keywords
 */
#define	CONF_CRYPTO_RSA		1	
#define	CONF_CRYPTO_SIGN	2
#define CONF_CRYPTO_LEAP	3
#define CONF_CRYPTO_CERT	4
#define CONF_CRYPTO_RAND	5
#define CONF_CRYPTO_KEYS	6
#define	CONF_CRYPTO_IFFPAR	7
#define CONF_CRYPTO_GQPAR	8
#define CONF_CRYPTO_MVPAR	9
#define CONF_CRYPTO_PW		10
#endif /* OPENSSL */

/*
 * Address selection, IPv4 or IPv6
 */
#define	CONF_ADDR_IPV4		1
#define	CONF_ADDR_IPV6		2
