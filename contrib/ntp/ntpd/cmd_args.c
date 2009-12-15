/*
 * cmd_args.c = command-line argument processing
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntpd.h"
#include "ntp_stdlib.h"
#include "ntp_cmdargs.h"

#ifdef SIM
# include "ntpsim.h"
# include "ntpdsim-opts.h"
# define OPTSTRUCT	ntpdsimOptions
#else
# include "ntpd-opts.h"
# define OPTSTRUCT	ntpdOptions
#endif /* SIM */

/*
 * Definitions of things either imported from or exported to outside
 */
extern char const *progname;
extern const char *specific_interface;
extern short default_ai_family;

#ifdef HAVE_NETINFO
extern int	check_netinfo;
#endif


/*
 * getCmdOpts - get command line options
 */
void
getCmdOpts(
	int argc,
	char *argv[]
	)
{
	extern const char *config_file;
	int errflg;
	tOptions *myOptions = &OPTSTRUCT;

	/*
	 * Initialize, initialize
	 */
	errflg = 0;

	switch (WHICH_IDX_IPV4) {
	    case INDEX_OPT_IPV4:
		default_ai_family = AF_INET;
		break;
	    case INDEX_OPT_IPV6:
		default_ai_family = AF_INET6;
		break;
	    default:
		/* ai_fam_templ = ai_fam_default;	*/
		break;
	}

	if (HAVE_OPT( AUTHREQ ))
		proto_config(PROTO_AUTHENTICATE, 1, 0., NULL);

	if (HAVE_OPT( AUTHNOREQ ))
		proto_config(PROTO_AUTHENTICATE, 0, 0., NULL);

	if (HAVE_OPT( BCASTSYNC ))
		proto_config(PROTO_BROADCLIENT, 1, 0., NULL);

	if (HAVE_OPT( CONFIGFILE )) {
		config_file = OPT_ARG( CONFIGFILE );
#ifdef HAVE_NETINFO
		check_netinfo = 0;
#endif
	}

	if (HAVE_OPT( DRIFTFILE ))
		stats_config(STATS_FREQ_FILE, OPT_ARG( DRIFTFILE ));

	if (HAVE_OPT( PANICGATE ))
		allow_panic = TRUE;

	if (HAVE_OPT( JAILDIR )) {
#ifdef HAVE_DROPROOT
			droproot = 1;
			chrootdir = OPT_ARG( JAILDIR );
#else
			fprintf(stderr, 
				"command line -i option (jaildir) is not supported by this binary"
# ifndef SYS_WINNT
				",\n" "can not drop root privileges.  See configure options\n"
				"--enable-clockctl and --enable-linuxcaps.\n");
# else
				".\n");
# endif
			msyslog(LOG_ERR, 
				"command line -i option (jaildir) is not supported by this binary.");
			errflg++;
#endif
	}

	if (HAVE_OPT( KEYFILE ))
		getauthkeys(OPT_ARG( KEYFILE ));

	if (HAVE_OPT( PIDFILE ))
		stats_config(STATS_PID_FILE, OPT_ARG( PIDFILE ));

	if (HAVE_OPT( QUIT ))
		mode_ntpdate = TRUE;

	if (HAVE_OPT( PROPAGATIONDELAY ))
		do {
			double tmp;
			const char *my_ntp_optarg = OPT_ARG( PROPAGATIONDELAY );

			if (sscanf(my_ntp_optarg, "%lf", &tmp) != 1) {
				msyslog(LOG_ERR,
					"command line broadcast delay value %s undecodable",
					my_ntp_optarg);
			} else {
				proto_config(PROTO_BROADDELAY, 0, tmp, NULL);
			}
		} while (0);

	if (HAVE_OPT( STATSDIR ))
		stats_config(STATS_STATSDIR, OPT_ARG( STATSDIR ));

	if (HAVE_OPT( TRUSTEDKEY )) {
		int		ct = STACKCT_OPT(  TRUSTEDKEY );
		const char**	pp = STACKLST_OPT( TRUSTEDKEY );

		do  {
			u_long tkey;
			const char* p = *pp++;

			tkey = (int)atol(p);
			if (tkey == 0 || tkey > NTP_MAXKEY) {
				msyslog(LOG_ERR,
				    "command line trusted key %s is invalid",
				    p);
			} else {
				authtrust(tkey, 1);
			}
		} while (--ct > 0);
	}

	if (HAVE_OPT( USER )) {
#ifdef HAVE_DROPROOT
		char *ntp_optarg = OPT_ARG( USER );

		droproot = 1;
		user = emalloc(strlen(ntp_optarg) + 1);
		(void)strncpy(user, ntp_optarg, strlen(ntp_optarg) + 1);
		group = rindex(user, ':');
		if (group)
			*group++ = '\0'; /* get rid of the ':' */
#else
		fprintf(stderr, 
			"command line -u/--user option is not supported by this binary"
# ifndef SYS_WINNT
			",\n" "can not drop root privileges.  See configure options\n"
			"--enable-clockctl and --enable-linuxcaps.\n");
# else
			".\n");
# endif
		msyslog(LOG_ERR, 
			"command line -u/--user option is not supported by this binary.");
		errflg++;
#endif
	}

	if (HAVE_OPT( VAR )) {
		int		ct = STACKCT_OPT(  VAR );
		const char**	pp = STACKLST_OPT( VAR );

		do  {
			const char* my_ntp_optarg = *pp++;

			set_sys_var(my_ntp_optarg, strlen(my_ntp_optarg)+1,
			    (u_short) (RW));
		} while (--ct > 0);
	}

	if (HAVE_OPT( DVAR )) {
		int		ct = STACKCT_OPT(  DVAR );
		const char**	pp = STACKLST_OPT( DVAR );

		do  {
			const char* my_ntp_optarg = *pp++;

			set_sys_var(my_ntp_optarg, strlen(my_ntp_optarg)+1,
			    (u_short) (RW | DEF));
		} while (--ct > 0);
	}

	if (HAVE_OPT( SLEW ))
		clock_max = 600;

	if (HAVE_OPT( UPDATEINTERVAL )) {
		long val = OPT_VALUE_UPDATEINTERVAL;
			  
		if (val >= 0)
			interface_interval = val;
		else {
			fprintf(stderr, 
				"command line interface update interval %ld must not be negative\n",
				val);
			msyslog(LOG_ERR, 
				"command line interface update interval %ld must not be negative",
				val);
			errflg++;
		}
	}
#ifdef SIM
	if (HAVE_OPT( SIMBROADCASTDELAY ))
		sscanf(OPT_ARG( SIMBROADCASTDELAY ), "%lf", &ntp_node.bdly);

	if (HAVE_OPT( PHASENOISE ))
		sscanf(OPT_ARG( PHASENOISE ), "%lf", &ntp_node.snse);

	if (HAVE_OPT( SIMSLEW ))
		sscanf(OPT_ARG( SIMSLEW ), "%lf", &ntp_node.slew);

	if (HAVE_OPT( SERVERTIME ))
		sscanf(OPT_ARG( SERVERTIME ), "%lf", &ntp_node.clk_time);

	if (HAVE_OPT( ENDSIMTIME ))
		sscanf(OPT_ARG( ENDSIMTIME ), "%lf", &ntp_node.sim_time);

	if (HAVE_OPT( FREQERR ))
		sscanf(OPT_ARG( FREQERR ), "%lf", &ntp_node.ferr);

	if (HAVE_OPT( WALKNOISE ))
		sscanf(OPT_ARG( WALKNOISE ), "%lf", &ntp_node.fnse);

	if (HAVE_OPT( NDELAY ))
		sscanf(OPT_ARG( NDELAY ), "%lf", &ntp_node.ndly);

	if (HAVE_OPT( PDELAY ))
		sscanf(OPT_ARG( PDELAY ), "%lf", &ntp_node.pdly);

#endif /* SIM */

	if (errflg || argc) {
		if (argc)
			fprintf(stderr, "argc after processing is <%d>\n", argc);
		optionUsage(myOptions, 2);
	}
	return;
}
