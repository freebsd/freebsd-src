/* ntp_parser.y
 *
 * The parser for the NTP configuration file.
 *
 * Written By:	Sachin Kamboj
 *		University of Delaware
 *		Newark, DE 19711
 * Copyright (c) 2006
 */

%{
  #ifdef HAVE_CONFIG_H
  # include <config.h>
  #endif

  #include "ntp.h"
  #include "ntpd.h"
  #include "ntp_machine.h"
  #include "ntp_stdlib.h"
  #include "ntp_filegen.h"
  #include "ntp_data_structures.h"
  #include "ntp_scanner.h"
  #include "ntp_config.h"
  #include "ntp_crypto.h"

  #include "ntpsim.h"		/* HMS: Do we really want this all the time? */
				/* SK: It might be a good idea to always
				   include the simulator code. That way
				   someone can use the same configuration file
				   for both the simulator and the daemon
				*/


  struct FILE_INFO *ip_file;   /* Pointer to the configuration file stream */

  #define YYMALLOC	emalloc
  #define YYFREE	free
  #define YYERROR_VERBOSE
  #define YYMAXDEPTH	1000   /* stop the madness sooner */
  void yyerror(const char *msg);
  extern int input_from_file;  /* 0=input from ntpq :config */
%}

/* 
 * Enable generation of token names array even without YYDEBUG.
 * We access via token_name() defined below.
 */
%token-table

%union {
    char   *String;
    double  Double;
    int     Integer;
    void   *VoidPtr;
    queue  *Queue;
    struct attr_val *Attr_val;
    struct address_node *Address_node;
    struct setvar_node *Set_var;

    /* Simulation types */
    server_info *Sim_server;
    script_info *Sim_script;
}

/* TERMINALS (do not appear left of colon) */
%token	<Integer>	T_Age
%token	<Integer>	T_All
%token	<Integer>	T_Allan
%token	<Integer>	T_Auth
%token	<Integer>	T_Autokey
%token	<Integer>	T_Automax
%token	<Integer>	T_Average
%token	<Integer>	T_Bclient
%token	<Integer>	T_Beacon
%token	<Integer>	T_Bias
%token	<Integer>	T_Broadcast
%token	<Integer>	T_Broadcastclient
%token	<Integer>	T_Broadcastdelay
%token	<Integer>	T_Burst
%token	<Integer>	T_Calibrate
%token	<Integer>	T_Calldelay
%token	<Integer>	T_Ceiling
%token	<Integer>	T_Clockstats
%token	<Integer>	T_Cohort
%token	<Integer>	T_ControlKey
%token	<Integer>	T_Crypto
%token	<Integer>	T_Cryptostats
%token	<Integer>	T_Day
%token	<Integer>	T_Default
%token	<Integer>	T_Digest
%token	<Integer>	T_Disable
%token	<Integer>	T_Discard
%token	<Integer>	T_Dispersion
%token	<Double>	T_Double
%token	<Integer>	T_Driftfile
%token	<Integer>	T_Drop
%token	<Integer>	T_Ellipsis	/* "..." not "ellipsis" */
%token	<Integer>	T_Enable
%token	<Integer>	T_End
%token	<Integer>	T_False
%token	<Integer>	T_File
%token	<Integer>	T_Filegen
%token	<Integer>	T_Flag1
%token	<Integer>	T_Flag2
%token	<Integer>	T_Flag3
%token	<Integer>	T_Flag4
%token	<Integer>	T_Flake
%token	<Integer>	T_Floor
%token	<Integer>	T_Freq
%token	<Integer>	T_Fudge
%token	<Integer>	T_Host
%token	<Integer>	T_Huffpuff
%token	<Integer>	T_Iburst
%token	<Integer>	T_Ident
%token	<Integer>	T_Ignore
%token	<Integer>	T_Includefile
%token	<Integer>	T_Integer
%token	<Integer>	T_Interface
%token	<Integer>	T_Ipv4
%token	<Integer>	T_Ipv4_flag
%token	<Integer>	T_Ipv6
%token	<Integer>	T_Ipv6_flag
%token	<Integer>	T_Kernel
%token	<Integer>	T_Key
%token	<Integer>	T_Keys
%token	<Integer>	T_Keysdir
%token	<Integer>	T_Kod
%token	<Integer>	T_Mssntp
%token	<Integer>	T_Leapfile
%token	<Integer>	T_Limited
%token	<Integer>	T_Link
%token	<Integer>	T_Listen
%token	<Integer>	T_Logconfig
%token	<Integer>	T_Logfile
%token	<Integer>	T_Loopstats
%token	<Integer>	T_Lowpriotrap
%token	<Integer>	T_Manycastclient
%token	<Integer>	T_Manycastserver
%token	<Integer>	T_Mask
%token	<Integer>	T_Maxclock
%token	<Integer>	T_Maxdist
%token	<Integer>	T_Maxpoll
%token	<Integer>	T_Minclock
%token	<Integer>	T_Mindist
%token	<Integer>	T_Minimum
%token	<Integer>	T_Minpoll
%token	<Integer>	T_Minsane
%token	<Integer>	T_Mode
%token	<Integer>	T_Monitor
%token	<Integer>	T_Month
%token	<Integer>	T_Multicastclient
%token	<Integer>	T_Nic
%token	<Integer>	T_Nolink
%token	<Integer>	T_Nomodify
%token	<Integer>	T_None
%token	<Integer>	T_Nopeer
%token	<Integer>	T_Noquery
%token	<Integer>	T_Noselect
%token	<Integer>	T_Noserve
%token	<Integer>	T_Notrap
%token	<Integer>	T_Notrust
%token	<Integer>	T_Ntp
%token	<Integer>	T_Ntpport
%token	<Integer>	T_NtpSignDsocket
%token	<Integer>	T_Orphan
%token	<Integer>	T_Panic
%token	<Integer>	T_Peer
%token	<Integer>	T_Peerstats
%token	<Integer>	T_Phone
%token	<Integer>	T_Pid
%token	<Integer>	T_Pidfile
%token	<Integer>	T_Pool
%token	<Integer>	T_Port
%token	<Integer>	T_Preempt
%token	<Integer>	T_Prefer
%token	<Integer>	T_Protostats
%token	<Integer>	T_Pw
%token	<Integer>	T_Qos
%token	<Integer>	T_Randfile
%token	<Integer>	T_Rawstats
%token	<Integer>	T_Refid
%token	<Integer>	T_Requestkey
%token	<Integer>	T_Restrict
%token	<Integer>	T_Revoke
%token	<Integer>	T_Saveconfigdir
%token	<Integer>	T_Server
%token	<Integer>	T_Setvar
%token	<Integer>	T_Sign
%token	<Integer>	T_Statistics
%token	<Integer>	T_Stats
%token	<Integer>	T_Statsdir
%token	<Integer>	T_Step
%token	<Integer>	T_Stepout
%token	<Integer>	T_Stratum
%token	<String>	T_String
%token	<Integer>	T_Sysstats
%token	<Integer>	T_Tick
%token	<Integer>	T_Time1
%token	<Integer>	T_Time2
%token	<Integer>	T_Timingstats
%token	<Integer>	T_Tinker
%token	<Integer>	T_Tos
%token	<Integer>	T_Trap
%token	<Integer>	T_True
%token	<Integer>	T_Trustedkey
%token	<Integer>	T_Ttl
%token	<Integer>	T_Type
%token	<Integer>	T_Unconfig
%token	<Integer>	T_Unpeer
%token	<Integer>	T_Version
%token	<Integer>	T_WanderThreshold	/* Not a token */
%token	<Integer>	T_Week
%token	<Integer>	T_Wildcard
%token	<Integer>	T_Xleave
%token	<Integer>	T_Year
%token	<Integer>	T_Flag		/* Not an actual token */
%token	<Integer>	T_Void		/* Not an actual token */
%token	<Integer>	T_EOC


/* NTP Simulator Tokens */
%token	<Integer>	T_Simulate
%token	<Integer>	T_Beep_Delay
%token	<Integer>	T_Sim_Duration
%token	<Integer>	T_Server_Offset
%token	<Integer>	T_Duration
%token	<Integer>	T_Freq_Offset
%token	<Integer>	T_Wander
%token	<Integer>	T_Jitter
%token	<Integer>	T_Prop_Delay
%token	<Integer>	T_Proc_Delay



/*** NON-TERMINALS ***/
%type	<Integer>	access_control_flag
%type	<Queue>		ac_flag_list
%type	<Address_node>	address
%type	<Integer>	address_fam
%type	<Queue>		address_list
%type	<Integer>	boolean
%type	<Integer>	client_type
%type	<Attr_val>	crypto_command
%type	<Queue>		crypto_command_list
%type	<Integer>	crypto_str_keyword
%type	<Attr_val>	discard_option
%type	<Integer>	discard_option_keyword
%type	<Queue>		discard_option_list
%type	<Integer>	enable_disable
%type	<Attr_val>	filegen_option
%type	<Queue>		filegen_option_list
%type	<Integer>	filegen_type
%type	<Attr_val>	fudge_factor
%type	<Integer>	fudge_factor_bool_keyword
%type	<Integer>	fudge_factor_dbl_keyword
%type	<Queue>		fudge_factor_list
%type	<Queue>		integer_list
%type	<Queue>		integer_list_range
%type	<Attr_val>	integer_list_range_elt
%type	<Attr_val>	integer_range
%type	<Integer>	nic_rule_action
%type	<Integer>	interface_command
%type	<Integer>	interface_nic
%type	<Address_node>	ip_address
%type	<Integer>	link_nolink
%type	<Attr_val>	log_config_command
%type	<Queue>		log_config_list
%type	<Integer>	misc_cmd_dbl_keyword
%type	<Integer>	misc_cmd_str_keyword
%type	<Integer>	misc_cmd_str_lcl_keyword
%type	<Integer>	nic_rule_class
%type	<Double>	number
%type	<Attr_val>	option
%type	<Attr_val>	option_flag
%type	<Integer>	option_flag_keyword
%type	<Queue>		option_list
%type	<Attr_val>	option_int
%type	<Integer>	option_int_keyword
%type	<Integer>	stat
%type	<Queue>		stats_list
%type	<Queue>		string_list
%type	<Attr_val>	system_option
%type	<Integer>	system_option_flag_keyword
%type	<Queue>		system_option_list
%type	<Integer>	t_default_or_zero
%type	<Integer>	tinker_option_keyword
%type	<Attr_val>	tinker_option
%type	<Queue>		tinker_option_list
%type	<Attr_val>	tos_option
%type	<Integer>	tos_option_dbl_keyword
%type	<Integer>	tos_option_int_keyword
%type	<Queue>		tos_option_list
%type	<Attr_val>	trap_option
%type	<Queue>		trap_option_list
%type	<Integer>	unpeer_keyword
%type	<Set_var>	variable_assign

/* NTP Simulator non-terminals */
%type	<Queue>		sim_init_statement_list
%type	<Attr_val>	sim_init_statement
%type	<Queue>		sim_server_list
%type	<Sim_server>	sim_server
%type	<Double>	sim_server_offset
%type	<Address_node>	sim_server_name
%type	<Queue>		sim_act_list
%type	<Sim_script>	sim_act
%type	<Queue>		sim_act_stmt_list
%type	<Attr_val>	sim_act_stmt

%%

/* ntp.conf
 * Configuration File Grammar
 * --------------------------
 */

configuration
	:	command_list
	;

command_list
	:	command_list command T_EOC
	|	command T_EOC
	|	error T_EOC
		{
			/* I will need to incorporate much more fine grained
			 * error messages. The following should suffice for
			 * the time being.
			 */
			msyslog(LOG_ERR, 
				"syntax error in %s line %d, column %d",
				ip_file->fname,
				ip_file->err_line_no,
				ip_file->err_col_no);
		}
	;

command :	/* NULL STATEMENT */
	|	server_command
	|	unpeer_command
	|	other_mode_command
	|	authentication_command
	|	monitoring_command
	|	access_control_command
	|	orphan_mode_command
	|	fudge_command
	|	system_option_command
	|	tinker_command
	|	miscellaneous_command
	|	simulate_command
	;

/* Server Commands
 * ---------------
 */

server_command
	:	client_type address option_list
		{
			struct peer_node *my_node =  create_peer_node($1, $2, $3);
			if (my_node)
				enqueue(cfgt.peers, my_node);
		}
	|	client_type address
		{
			struct peer_node *my_node = create_peer_node($1, $2, NULL);
			if (my_node)
				enqueue(cfgt.peers, my_node);
		}
	;

client_type
	:	T_Server
	|	T_Pool
	|	T_Peer
	|	T_Broadcast
	|	T_Manycastclient
	;

address
	:	ip_address
	|	address_fam T_String
			{ $$ = create_address_node($2, $1); }
	;

ip_address
	:	T_String
			{ $$ = create_address_node($1, 0); }
	;

address_fam
	:	T_Ipv4_flag
			{ $$ = AF_INET; }
	|	T_Ipv6_flag
			{ $$ = AF_INET6; }
	;

option_list
	:	option_list option { $$ = enqueue($1, $2); }
	|	option { $$ = enqueue_in_new_queue($1); }
	;

option
	:	option_flag
	|	option_int
	|	T_Bias number
			{ $$ = create_attr_dval($1, $2); }
	;

option_flag
	:	option_flag_keyword
			{ $$ = create_attr_ival(T_Flag, $1); }
	;

option_flag_keyword
	:	T_Autokey
	|	T_Burst
	|	T_Iburst
	|	T_Noselect
	|	T_Preempt
	|	T_Prefer
	|	T_True
	|	T_Xleave
	;

option_int
	:	option_int_keyword T_Integer
			{ $$ = create_attr_ival($1, $2); }
	;

option_int_keyword
	:	T_Key
	|	T_Minpoll
	|	T_Maxpoll
	|	T_Ttl
	|	T_Mode
	|	T_Version
	;



/* unpeer commands
 * ---------------
 */

unpeer_command
	:	unpeer_keyword address
		{
			struct unpeer_node *my_node = create_unpeer_node($2);
			if (my_node)
				enqueue(cfgt.unpeers, my_node);
		}
	;	
unpeer_keyword	
	:	T_Unconfig
	|	T_Unpeer
	;
	
	
/* Other Modes
 * (broadcastclient manycastserver multicastclient)
 * ------------------------------------------------
 */

other_mode_command
	:	T_Broadcastclient
			{ cfgt.broadcastclient = 1; }
	|	T_Manycastserver address_list
			{ append_queue(cfgt.manycastserver, $2); }
	|	T_Multicastclient address_list
			{ append_queue(cfgt.multicastclient, $2); }
	;



/* Authentication Commands
 * -----------------------
 */

authentication_command
	:	T_Automax T_Integer
			{ enqueue(cfgt.vars, create_attr_ival($1, $2)); }
	|	T_ControlKey T_Integer
			{ cfgt.auth.control_key = $2; }
	|	T_Crypto crypto_command_list
		{ 
			cfgt.auth.cryptosw++;
			append_queue(cfgt.auth.crypto_cmd_list, $2);
		}
	|	T_Keys T_String
			{ cfgt.auth.keys = $2; }
	|	T_Keysdir T_String
			{ cfgt.auth.keysdir = $2; }
	|	T_Requestkey T_Integer
			{ cfgt.auth.request_key = $2; }
	|	T_Revoke T_Integer
			{ cfgt.auth.revoke = $2; }
	|	T_Trustedkey integer_list_range
			{ cfgt.auth.trusted_key_list = $2; }
	|	T_NtpSignDsocket T_String
			{ cfgt.auth.ntp_signd_socket = $2; }
	;

crypto_command_list
	:	/* empty list */
			{ $$ = create_queue(); }
	|	crypto_command_list crypto_command
		{ 
			if ($2 != NULL)
				$$ = enqueue($1, $2);
			else
				$$ = $1;
		}
	;

crypto_command
	:	crypto_str_keyword T_String
			{ $$ = create_attr_sval($1, $2); }
	|	T_Revoke T_Integer
		{
			$$ = NULL;
			cfgt.auth.revoke = $2;
			msyslog(LOG_WARNING,
				"'crypto revoke %d' is deprecated, "
				"please use 'revoke %d' instead.",
				cfgt.auth.revoke, cfgt.auth.revoke);
		}
	;

crypto_str_keyword
	:	T_Host
	|	T_Ident
	|	T_Pw
	|	T_Randfile
	|	T_Sign
	|	T_Digest
	;


/* Orphan Mode Commands
 * --------------------
 */

orphan_mode_command
	:	T_Tos tos_option_list
			{ append_queue(cfgt.orphan_cmds,$2); }
	;

tos_option_list
	:	tos_option_list tos_option { $$ = enqueue($1, $2); }
	|	tos_option { $$ = enqueue_in_new_queue($1); }
	;

tos_option
	:	tos_option_int_keyword T_Integer
			{ $$ = create_attr_dval($1, (double)$2); }
	|	tos_option_dbl_keyword number
			{ $$ = create_attr_dval($1, $2); }
	|	T_Cohort boolean
			{ $$ = create_attr_dval($1, (double)$2); }
	;

tos_option_int_keyword
	:	T_Ceiling
	|	T_Floor
	|	T_Orphan
	|	T_Minsane
	|	T_Beacon
	;

tos_option_dbl_keyword
	:	T_Mindist
	|	T_Maxdist
	|	T_Minclock
	|	T_Maxclock
	;


/* Monitoring Commands
 * -------------------
 */

monitoring_command
	:	T_Statistics stats_list
			{ append_queue(cfgt.stats_list, $2); }
	|	T_Statsdir T_String
		{
			if (input_from_file) {
				cfgt.stats_dir = $2;
			} else {
				YYFREE($2);
				yyerror("statsdir remote configuration ignored");
			}
		}
	|	T_Filegen stat filegen_option_list
		{
			enqueue(cfgt.filegen_opts,
				create_filegen_node($2, $3));
		}
	;

stats_list
	:	stats_list stat { $$ = enqueue($1, create_ival($2)); }
	|	stat { $$ = enqueue_in_new_queue(create_ival($1)); }
	;

stat
	:	T_Clockstats
	|	T_Cryptostats
	|	T_Loopstats
	|	T_Peerstats
	|	T_Rawstats
	|	T_Sysstats
	|	T_Timingstats
	|	T_Protostats
	;

filegen_option_list
	:	/* empty list */
			{ $$ = create_queue(); }
	|	filegen_option_list filegen_option
		{
			if ($2 != NULL)
				$$ = enqueue($1, $2);
			else
				$$ = $1;
		}
	;

filegen_option
	:	T_File T_String
		{
			if (input_from_file) {
				$$ = create_attr_sval($1, $2);
			} else {
				$$ = NULL;
				YYFREE($2);
				yyerror("filegen file remote config ignored");
			}
		}
	|	T_Type filegen_type
		{
			if (input_from_file) {
				$$ = create_attr_ival($1, $2);
			} else {
				$$ = NULL;
				yyerror("filegen type remote config ignored");
			}
		}
	|	link_nolink
		{
			const char *err;
			
			if (input_from_file) {
				$$ = create_attr_ival(T_Flag, $1);
			} else {
				$$ = NULL;
				if (T_Link == $1)
					err = "filegen link remote config ignored";
				else
					err = "filegen nolink remote config ignored";
				yyerror(err);
			}
		}
	|	enable_disable
			{ $$ = create_attr_ival(T_Flag, $1); }
	;

link_nolink
	:	T_Link
	|	T_Nolink
	;

enable_disable
	:	T_Enable
	|	T_Disable
	;

filegen_type
	:	T_None
	|	T_Pid
	|	T_Day
	|	T_Week
	|	T_Month
	|	T_Year
	|	T_Age
	;


/* Access Control Commands
 * -----------------------
 */

access_control_command
	:	T_Discard discard_option_list
		{
			append_queue(cfgt.discard_opts, $2);
		}
	|	T_Restrict address ac_flag_list
		{
			enqueue(cfgt.restrict_opts,
				create_restrict_node($2, NULL, $3, ip_file->line_no));
		}
	|	T_Restrict T_Default ac_flag_list
		{
			enqueue(cfgt.restrict_opts,
				create_restrict_node(NULL, NULL, $3, ip_file->line_no));
		}
	|	T_Restrict T_Ipv4_flag T_Default ac_flag_list
		{
			enqueue(cfgt.restrict_opts,
				create_restrict_node(
					create_address_node(
						estrdup("0.0.0.0"), 
						AF_INET),
					create_address_node(
						estrdup("0.0.0.0"), 
						AF_INET),
					$4, 
					ip_file->line_no));
		}
	|	T_Restrict T_Ipv6_flag T_Default ac_flag_list
		{
			enqueue(cfgt.restrict_opts,
				create_restrict_node(
					create_address_node(
						estrdup("::"), 
						AF_INET6),
					create_address_node(
						estrdup("::"), 
						AF_INET6),
					$4, 
					ip_file->line_no));
		}
	|	T_Restrict ip_address T_Mask ip_address ac_flag_list
		{
			enqueue(cfgt.restrict_opts,
				create_restrict_node($2, $4, $5, ip_file->line_no));
		}
	;

ac_flag_list
	:	/* empty list is allowed */
			{ $$ = create_queue(); }
	|	ac_flag_list access_control_flag
			{ $$ = enqueue($1, create_ival($2)); }
	;

access_control_flag
	:	T_Flake
	|	T_Ignore
	|	T_Kod
	|	T_Mssntp
	|	T_Limited
	|	T_Lowpriotrap
	|	T_Nomodify
	|	T_Nopeer
	|	T_Noquery
	|	T_Noserve
	|	T_Notrap
	|	T_Notrust
	|	T_Ntpport
	|	T_Version
	;

discard_option_list
	:	discard_option_list discard_option
			{ $$ = enqueue($1, $2); }
	|	discard_option 
			{ $$ = enqueue_in_new_queue($1); }
	;

discard_option
	:	discard_option_keyword T_Integer
			{ $$ = create_attr_ival($1, $2); }
	;

discard_option_keyword
	:	T_Average
	|	T_Minimum
	|	T_Monitor
	;


/* Fudge Commands
 * --------------
 */

fudge_command
	:	T_Fudge address fudge_factor_list
			{ enqueue(cfgt.fudge, create_addr_opts_node($2, $3)); }
	;

fudge_factor_list
	:	fudge_factor_list fudge_factor
			{ enqueue($1, $2); }
	|	fudge_factor
			{ $$ = enqueue_in_new_queue($1); }
	;
	
fudge_factor
	:	fudge_factor_dbl_keyword number
			{ $$ = create_attr_dval($1, $2); }
	|	fudge_factor_bool_keyword boolean
			{ $$ = create_attr_ival($1, $2); }
	|	T_Stratum T_Integer
			{ $$ = create_attr_ival($1, $2); }
	|	T_Refid T_String
			{ $$ = create_attr_sval($1, $2); }
	;

fudge_factor_dbl_keyword
	:	T_Time1
	|	T_Time2
	;

fudge_factor_bool_keyword
	:	T_Flag1
	|	T_Flag2
	|	T_Flag3
	|	T_Flag4
	;


/* Command for System Options
 * --------------------------
 */

system_option_command
	:	T_Enable system_option_list
			{ append_queue(cfgt.enable_opts, $2);  }
	|	T_Disable system_option_list
			{ append_queue(cfgt.disable_opts, $2);  }
	;

system_option_list
	:	system_option_list system_option
		{
			if ($2 != NULL)
				$$ = enqueue($1, $2);
			else
				$$ = $1;
		}
	|	system_option
		{
			if ($1 != NULL)
				$$ = enqueue_in_new_queue($1);
			else
				$$ = create_queue();
		}
	;

system_option
	:	system_option_flag_keyword
			{ $$ = create_attr_ival(T_Flag, $1); }
	|	T_Stats
		{ 
			if (input_from_file) {
				$$ = create_attr_ival(T_Flag, $1);
			} else {
				$$ = NULL;
				yyerror("enable/disable stats remote config ignored");
			}
		}
	;

system_option_flag_keyword
	:	T_Auth
	|	T_Bclient
	|	T_Calibrate
	|	T_Kernel
	|	T_Monitor
	|	T_Ntp
	;


/* Tinker Commands
 * ---------------
 */

tinker_command
	:	T_Tinker tinker_option_list  { append_queue(cfgt.tinker, $2); }
	;

tinker_option_list
	:	tinker_option_list tinker_option  { $$ = enqueue($1, $2); }
	|	tinker_option { $$ = enqueue_in_new_queue($1); }
	;

tinker_option
	:	tinker_option_keyword number
			{ $$ = create_attr_dval($1, $2); }
	;

tinker_option_keyword
	:	T_Allan
	|	T_Dispersion
	|	T_Freq
	|	T_Huffpuff
	|	T_Panic
	|	T_Step
	|	T_Stepout
	;


/* Miscellaneous Commands
 * ----------------------
 */

miscellaneous_command
	:	interface_command
	|	misc_cmd_dbl_keyword number
		{
			struct attr_val *av;
			
			av = create_attr_dval($1, $2);
			enqueue(cfgt.vars, av);
		}
	|	misc_cmd_str_keyword T_String
		{
			struct attr_val *av;
			
			av = create_attr_sval($1, $2);
			enqueue(cfgt.vars, av);
		}
	|	misc_cmd_str_lcl_keyword T_String
		{
			char error_text[64];
			struct attr_val *av;

			if (input_from_file) {
				av = create_attr_sval($1, $2);
				enqueue(cfgt.vars, av);
			} else {
				YYFREE($2);
				snprintf(error_text, sizeof(error_text),
					 "%s remote config ignored",
					 keyword($1));
				yyerror(error_text);
			}
		}
	|	T_Includefile T_String command
		{
			if (!input_from_file) {
				yyerror("remote includefile ignored");
				break;
			}
			if (curr_include_level >= MAXINCLUDELEVEL) {
				fprintf(stderr, "getconfig: Maximum include file level exceeded.\n");
				msyslog(LOG_ERR, "getconfig: Maximum include file level exceeded.\n");
			} else {
				fp[curr_include_level + 1] = F_OPEN(FindConfig($2), "r");
				if (fp[curr_include_level + 1] == NULL) {
					fprintf(stderr, "getconfig: Couldn't open <%s>\n", FindConfig($2));
					msyslog(LOG_ERR, "getconfig: Couldn't open <%s>\n", FindConfig($2));
				} else {
					ip_file = fp[++curr_include_level];
				}
			}
		}
	|	T_End
		{
			while (curr_include_level != -1)
				FCLOSE(fp[curr_include_level--]);
		}
	|	T_Calldelay T_Integer
			{ enqueue(cfgt.vars, create_attr_ival($1, $2)); }
	|	T_Driftfile drift_parm
			{ /* Null action, possibly all null parms */ }
	|	T_Logconfig log_config_list
			{ append_queue(cfgt.logconfig, $2); }
	|	T_Phone string_list
			{ append_queue(cfgt.phone, $2); }
	|	T_Setvar variable_assign
			{ enqueue(cfgt.setvar, $2); }
	|	T_Trap ip_address
			{ enqueue(cfgt.trap, create_addr_opts_node($2, NULL)); }
	|	T_Trap ip_address trap_option_list
			{ enqueue(cfgt.trap, create_addr_opts_node($2, $3)); }
	|	T_Ttl integer_list
			{ append_queue(cfgt.ttl, $2); }
	;

misc_cmd_dbl_keyword
	:	T_Broadcastdelay
	|	T_Tick
	;

misc_cmd_str_keyword
	:	T_Leapfile
	|	T_Pidfile
	|	T_Qos
	;

misc_cmd_str_lcl_keyword
	:	T_Logfile
	|	T_Saveconfigdir
	;

drift_parm
	:	T_String
			{ enqueue(cfgt.vars, create_attr_sval(T_Driftfile, $1)); }
	|	T_String T_Double
			{ enqueue(cfgt.vars, create_attr_dval(T_WanderThreshold, $2));
			  enqueue(cfgt.vars, create_attr_sval(T_Driftfile, $1)); }
	|	/* Null driftfile,  indicated by null string "\0" */
			{ enqueue(cfgt.vars, create_attr_sval(T_Driftfile, "\0")); }
	;

variable_assign
	:	T_String '=' T_String t_default_or_zero
			{ $$ = create_setvar_node($1, $3, $4); }
	;

t_default_or_zero
	:	T_Default
	|	/* empty, no "default" modifier */
		{ $$ = 0; }
	;

trap_option_list
	:	trap_option_list trap_option
				{ $$ = enqueue($1, $2); }
	|	trap_option	{ $$ = enqueue_in_new_queue($1); }
	;

trap_option
	:	T_Port T_Integer	{ $$ = create_attr_ival($1, $2); }
	|	T_Interface ip_address	{ $$ = create_attr_pval($1, $2); }
	;

log_config_list
	:	log_config_list log_config_command { $$ = enqueue($1, $2); }
	|	log_config_command  { $$ = enqueue_in_new_queue($1); }
	;

log_config_command
	:	T_String
		{
			char prefix = $1[0];
			char *type = $1 + 1;
			
			if (prefix != '+' && prefix != '-' && prefix != '=') {
				yyerror("Logconfig prefix is not '+', '-' or '='\n");
			}
			else
				$$ = create_attr_sval(prefix, estrdup(type));
			YYFREE($1);
		}
	;

interface_command
	:	interface_nic nic_rule_action nic_rule_class
		{
			enqueue(cfgt.nic_rules,
				create_nic_rule_node($3, NULL, $2));
		}
	|	interface_nic nic_rule_action T_String
		{
			enqueue(cfgt.nic_rules,
				create_nic_rule_node(0, $3, $2));
		}
	;

interface_nic
	:	T_Interface
	|	T_Nic
	;

nic_rule_class
	:	T_All
	|	T_Ipv4
	|	T_Ipv6
	|	T_Wildcard
	;

nic_rule_action
	:	T_Listen
	|	T_Ignore
	|	T_Drop
	;



/* Miscellaneous Rules
 * -------------------
 */

integer_list
	:	integer_list T_Integer { $$ = enqueue($1, create_ival($2)); }
	|	T_Integer { $$ = enqueue_in_new_queue(create_ival($1)); }
	;

integer_list_range
	:	integer_list_range integer_list_range_elt
			{ $$ = enqueue($1, $2); }
	|	integer_list_range_elt
			{ $$ = enqueue_in_new_queue($1); }
	;

integer_list_range_elt
	:	T_Integer
			{ $$ = create_attr_ival('i', $1); }
	|	integer_range		/* default of $$ = $1 is good */
	;

integer_range		/* limited to unsigned shorts */
	:	'(' T_Integer T_Ellipsis T_Integer ')'
			{ $$ = create_attr_shorts('-', $2, $4); }
	;

string_list
	:	string_list T_String { $$ = enqueue($1, create_pval($2)); }
	|	T_String { $$ = enqueue_in_new_queue(create_pval($1)); }
	;

address_list
	:	address_list address { $$ = enqueue($1, $2); }
	|	address { $$ = enqueue_in_new_queue($1); }
	;

boolean
	:	T_Integer
		{
			if ($1 != 0 && $1 != 1) {
				yyerror("Integer value is not boolean (0 or 1). Assuming 1");
				$$ = 1;
			} else {
				$$ = $1;
			}
		}
	|	T_True    { $$ = 1; }
	|	T_False   { $$ = 0; }
	;

number
	:	T_Integer { $$ = (double)$1; }
	|	T_Double
	;


/* Simulator Configuration Commands
 * --------------------------------
 */

simulate_command
	:	sim_conf_start '{' sim_init_statement_list sim_server_list '}'
		{
			cfgt.sim_details = create_sim_node($3, $4);

			/* Reset the old_config_style variable */
			old_config_style = 1;
		}
	;

/* The following is a terrible hack to get the configuration file to
 * treat newlines as whitespace characters within the simulation.
 * This is needed because newlines are significant in the rest of the
 * configuration file.
 */
sim_conf_start
	:	T_Simulate { old_config_style = 0; }
	;

sim_init_statement_list
	:	sim_init_statement_list sim_init_statement T_EOC { $$ = enqueue($1, $2); }
	|	sim_init_statement T_EOC			 { $$ = enqueue_in_new_queue($1); }
	;

sim_init_statement
	:	T_Beep_Delay '=' number   { $$ = create_attr_dval($1, $3); }
	|	T_Sim_Duration '=' number { $$ = create_attr_dval($1, $3); }
	;

sim_server_list
	:	sim_server_list sim_server { $$ = enqueue($1, $2); }
	|	sim_server		   { $$ = enqueue_in_new_queue($1); }
	;

sim_server
	:	sim_server_name '{' sim_server_offset sim_act_list '}'
		{ $$ = create_sim_server($1, $3, $4); }
	;

sim_server_offset
	:	T_Server_Offset '=' number T_EOC { $$ = $3; }
	;

sim_server_name
	:	T_Server '=' address { $$ = $3; }
	;

sim_act_list
	:	sim_act_list sim_act { $$ = enqueue($1, $2); }
	|	sim_act		     { $$ = enqueue_in_new_queue($1); }
	;

sim_act
	:	T_Duration '=' number '{' sim_act_stmt_list '}'
			{ $$ = create_sim_script_info($3, $5); }
	;

sim_act_stmt_list
	:	sim_act_stmt_list sim_act_stmt T_EOC { $$ = enqueue($1, $2); }
	|	sim_act_stmt T_EOC		     { $$ = enqueue_in_new_queue($1); }
	;

sim_act_stmt
	:	T_Freq_Offset '=' number
			{ $$ = create_attr_dval($1, $3); }
	|	T_Wander '=' number
			{ $$ = create_attr_dval($1, $3); }
	|	T_Jitter '=' number
			{ $$ = create_attr_dval($1, $3); }
	|	T_Prop_Delay '=' number
			{ $$ = create_attr_dval($1, $3); }
	|	T_Proc_Delay '=' number
			{ $$ = create_attr_dval($1, $3); }
	;


%%

void
yyerror(
	const char *msg
	)
{
	int retval;

	ip_file->err_line_no = ip_file->prev_token_line_no;
	ip_file->err_col_no = ip_file->prev_token_col_no;
	
	msyslog(LOG_ERR, 
		"line %d column %d %s", 
		ip_file->err_line_no,
		ip_file->err_col_no,
		msg);
	if (!input_from_file) {
		/* Save the error message in the correct buffer */
		retval = snprintf(remote_config.err_msg + remote_config.err_pos,
				  MAXLINE - remote_config.err_pos,
				  "column %d %s",
				  ip_file->err_col_no, msg);

		/* Increment the value of err_pos */
		if (retval > 0)
			remote_config.err_pos += retval;

		/* Increment the number of errors */
		++remote_config.no_errors;
	}
}


/*
 * token_name - Convert T_ token integers to text.
 *		Example: token_name(T_Server) returns "T_Server".
 *		see also keyword(T_Server) which returns "server".
 */
const char *
token_name(
	int token
	)
{
	return yytname[YYTRANSLATE(token)];
}


/* Initial Testing function -- ignore
int main(int argc, char *argv[])
{
	ip_file = FOPEN(argv[1], "r");
	if (!ip_file) {
		fprintf(stderr, "ERROR!! Could not open file: %s\n", argv[1]);
	}
	key_scanner = create_keyword_scanner(keyword_list);
	print_keyword_scanner(key_scanner, 0);
	yyparse();
	return 0;
}
*/

