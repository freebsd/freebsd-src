/*
 * 
 * servconf.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Mon Aug 21 15:48:58 1995 ylo
 * 
 */

#include "includes.h"
RCSID("$Id: servconf.c,v 1.29 2000/01/04 00:07:59 markus Exp $");

#include "ssh.h"
#include "servconf.h"
#include "xmalloc.h"

/* add listen address */
void add_listen_addr(ServerOptions *options, char *addr);

/* Initializes the server options to their default values. */

void 
initialize_server_options(ServerOptions *options)
{
	memset(options, 0, sizeof(*options));
	options->num_ports = 0;
	options->ports_from_cmdline = 0;
	options->listen_addrs = NULL;
	options->host_key_file = NULL;
	options->server_key_bits = -1;
	options->login_grace_time = -1;
	options->key_regeneration_time = -1;
	options->permit_root_login = -1;
	options->ignore_rhosts = -1;
	options->ignore_user_known_hosts = -1;
	options->print_motd = -1;
	options->check_mail = -1;
	options->x11_forwarding = -1;
	options->x11_display_offset = -1;
	options->strict_modes = -1;
	options->keepalives = -1;
	options->log_facility = (SyslogFacility) - 1;
	options->log_level = (LogLevel) - 1;
	options->rhosts_authentication = -1;
	options->rhosts_rsa_authentication = -1;
	options->rsa_authentication = -1;
#ifdef KRB4
	options->kerberos_authentication = -1;
	options->kerberos_or_local_passwd = -1;
	options->kerberos_ticket_cleanup = -1;
#endif
#ifdef AFS
	options->kerberos_tgt_passing = -1;
	options->afs_token_passing = -1;
#endif
	options->password_authentication = -1;
#ifdef SKEY
	options->skey_authentication = -1;
#endif
	options->permit_empty_passwd = -1;
	options->use_login = -1;
	options->num_allow_users = 0;
	options->num_deny_users = 0;
	options->num_allow_groups = 0;
	options->num_deny_groups = 0;
}

void 
fill_default_server_options(ServerOptions *options)
{
	if (options->num_ports == 0)
		options->ports[options->num_ports++] = SSH_DEFAULT_PORT;
	if (options->listen_addrs == NULL)
		add_listen_addr(options, NULL);
	if (options->host_key_file == NULL)
		options->host_key_file = HOST_KEY_FILE;
	if (options->server_key_bits == -1)
		options->server_key_bits = 768;
	if (options->login_grace_time == -1)
		options->login_grace_time = 600;
	if (options->key_regeneration_time == -1)
		options->key_regeneration_time = 3600;
	if (options->permit_root_login == -1)
		options->permit_root_login = 1;			/* yes */
	if (options->ignore_rhosts == -1)
		options->ignore_rhosts = 0;
	if (options->ignore_user_known_hosts == -1)
		options->ignore_user_known_hosts = 0;
	if (options->check_mail == -1)
		options->check_mail = 0;
	if (options->print_motd == -1)
		options->print_motd = 1;
	if (options->x11_forwarding == -1)
		options->x11_forwarding = 1;
	if (options->x11_display_offset == -1)
		options->x11_display_offset = 1;
	if (options->strict_modes == -1)
		options->strict_modes = 1;
	if (options->keepalives == -1)
		options->keepalives = 1;
	if (options->log_facility == (SyslogFacility) (-1))
		options->log_facility = SYSLOG_FACILITY_AUTH;
	if (options->log_level == (LogLevel) (-1))
		options->log_level = SYSLOG_LEVEL_INFO;
	if (options->rhosts_authentication == -1)
		options->rhosts_authentication = 0;
	if (options->rhosts_rsa_authentication == -1)
		options->rhosts_rsa_authentication = 1;
	if (options->rsa_authentication == -1)
		options->rsa_authentication = 1;
#ifdef KRB4
	if (options->kerberos_authentication == -1)
		options->kerberos_authentication = (access(KEYFILE, R_OK) == 0);
	if (options->kerberos_or_local_passwd == -1)
		options->kerberos_or_local_passwd = 1;
	if (options->kerberos_ticket_cleanup == -1)
		options->kerberos_ticket_cleanup = 1;
#endif /* KRB4 */
#ifdef AFS
	if (options->kerberos_tgt_passing == -1)
		options->kerberos_tgt_passing = 0;
	if (options->afs_token_passing == -1)
		options->afs_token_passing = k_hasafs();
#endif /* AFS */
	if (options->password_authentication == -1)
		options->password_authentication = 1;
#ifdef SKEY
	if (options->skey_authentication == -1)
		options->skey_authentication = 1;
#endif
	if (options->permit_empty_passwd == -1)
		options->permit_empty_passwd = 1;
	if (options->use_login == -1)
		options->use_login = 0;
}

#define WHITESPACE " \t\r\n"

/* Keyword tokens. */
typedef enum {
	sBadOption,		/* == unknown option */
	sPort, sHostKeyFile, sServerKeyBits, sLoginGraceTime, sKeyRegenerationTime,
	sPermitRootLogin, sLogFacility, sLogLevel,
	sRhostsAuthentication, sRhostsRSAAuthentication, sRSAAuthentication,
#ifdef KRB4
	sKerberosAuthentication, sKerberosOrLocalPasswd, sKerberosTicketCleanup,
#endif
#ifdef AFS
	sKerberosTgtPassing, sAFSTokenPassing,
#endif
#ifdef SKEY
	sSkeyAuthentication,
#endif
	sPasswordAuthentication, sListenAddress,
	sPrintMotd, sIgnoreRhosts, sX11Forwarding, sX11DisplayOffset,
	sStrictModes, sEmptyPasswd, sRandomSeedFile, sKeepAlives, sCheckMail,
	sUseLogin, sAllowUsers, sDenyUsers, sAllowGroups, sDenyGroups,
	sIgnoreUserKnownHosts
} ServerOpCodes;

/* Textual representation of the tokens. */
static struct {
	const char *name;
	ServerOpCodes opcode;
} keywords[] = {
	{ "port", sPort },
	{ "hostkey", sHostKeyFile },
	{ "serverkeybits", sServerKeyBits },
	{ "logingracetime", sLoginGraceTime },
	{ "keyregenerationinterval", sKeyRegenerationTime },
	{ "permitrootlogin", sPermitRootLogin },
	{ "syslogfacility", sLogFacility },
	{ "loglevel", sLogLevel },
	{ "rhostsauthentication", sRhostsAuthentication },
	{ "rhostsrsaauthentication", sRhostsRSAAuthentication },
	{ "rsaauthentication", sRSAAuthentication },
#ifdef KRB4
	{ "kerberosauthentication", sKerberosAuthentication },
	{ "kerberosorlocalpasswd", sKerberosOrLocalPasswd },
	{ "kerberosticketcleanup", sKerberosTicketCleanup },
#endif
#ifdef AFS
	{ "kerberostgtpassing", sKerberosTgtPassing },
	{ "afstokenpassing", sAFSTokenPassing },
#endif
	{ "passwordauthentication", sPasswordAuthentication },
#ifdef SKEY
	{ "skeyauthentication", sSkeyAuthentication },
#endif
	{ "checkmail", sCheckMail },
	{ "listenaddress", sListenAddress },
	{ "printmotd", sPrintMotd },
	{ "ignorerhosts", sIgnoreRhosts },
	{ "ignoreuserknownhosts", sIgnoreUserKnownHosts },
	{ "x11forwarding", sX11Forwarding },
	{ "x11displayoffset", sX11DisplayOffset },
	{ "strictmodes", sStrictModes },
	{ "permitemptypasswords", sEmptyPasswd },
	{ "uselogin", sUseLogin },
	{ "randomseed", sRandomSeedFile },
	{ "keepalive", sKeepAlives },
	{ "allowusers", sAllowUsers },
	{ "denyusers", sDenyUsers },
	{ "allowgroups", sAllowGroups },
	{ "denygroups", sDenyGroups },
	{ NULL, 0 }
};

/*
 * Returns the number of the token pointed to by cp of length len. Never
 * returns if the token is not known.
 */

static ServerOpCodes 
parse_token(const char *cp, const char *filename,
	    int linenum)
{
	unsigned int i;

	for (i = 0; keywords[i].name; i++)
		if (strcasecmp(cp, keywords[i].name) == 0)
			return keywords[i].opcode;

	fprintf(stderr, "%s: line %d: Bad configuration option: %s\n",
		filename, linenum, cp);
	return sBadOption;
}

/*
 * add listen address
 */
void 
add_listen_addr(ServerOptions *options, char *addr)
{
	extern int IPv4or6;
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr;
	int i;

	if (options->num_ports == 0)
		options->ports[options->num_ports++] = SSH_DEFAULT_PORT;
	for (i = 0; i < options->num_ports; i++) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = IPv4or6;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = (addr == NULL) ? AI_PASSIVE : 0;
		snprintf(strport, sizeof strport, "%d", options->ports[i]);
		if ((gaierr = getaddrinfo(addr, strport, &hints, &aitop)) != 0)
			fatal("bad addr or host: %s (%s)\n",
			    addr ? addr : "<NULL>",
			    gai_strerror(gaierr));
		for (ai = aitop; ai->ai_next; ai = ai->ai_next)
			;
		ai->ai_next = options->listen_addrs;
		options->listen_addrs = aitop;
	}
}

/* Reads the server configuration file. */

void 
read_server_config(ServerOptions *options, const char *filename)
{
	FILE *f;
	char line[1024];
	char *cp, **charptr;
	int linenum, *intptr, value;
	int bad_options = 0;
	ServerOpCodes opcode;

	f = fopen(filename, "r");
	if (!f) {
		perror(filename);
		exit(1);
	}
	linenum = 0;
	while (fgets(line, sizeof(line), f)) {
		linenum++;
		cp = line + strspn(line, WHITESPACE);
		if (!*cp || *cp == '#')
			continue;
		cp = strtok(cp, WHITESPACE);
		opcode = parse_token(cp, filename, linenum);
		switch (opcode) {
		case sBadOption:
			bad_options++;
			continue;
		case sPort:
			/* ignore ports from configfile if cmdline specifies ports */
			if (options->ports_from_cmdline)
				continue;
			if (options->listen_addrs != NULL)
				fatal("%s line %d: ports must be specified before "
				    "ListenAdress.\n", filename, linenum);
			if (options->num_ports >= MAX_PORTS)
				fatal("%s line %d: too many ports.\n",
			            filename, linenum);
			cp = strtok(NULL, WHITESPACE);
			if (!cp)
				fatal("%s line %d: missing port number.\n",
				    filename, linenum);
			options->ports[options->num_ports++] = atoi(cp);
			break;

		case sServerKeyBits:
			intptr = &options->server_key_bits;
parse_int:
			cp = strtok(NULL, WHITESPACE);
			if (!cp) {
				fprintf(stderr, "%s line %d: missing integer value.\n",
					filename, linenum);
				exit(1);
			}
			value = atoi(cp);
			if (*intptr == -1)
				*intptr = value;
			break;

		case sLoginGraceTime:
			intptr = &options->login_grace_time;
			goto parse_int;

		case sKeyRegenerationTime:
			intptr = &options->key_regeneration_time;
			goto parse_int;

		case sListenAddress:
			cp = strtok(NULL, WHITESPACE);
			if (!cp)
				fatal("%s line %d: missing inet addr.\n",
				    filename, linenum);
			add_listen_addr(options, cp);
			break;

		case sHostKeyFile:
			charptr = &options->host_key_file;
			cp = strtok(NULL, WHITESPACE);
			if (!cp) {
				fprintf(stderr, "%s line %d: missing file name.\n",
					filename, linenum);
				exit(1);
			}
			if (*charptr == NULL)
				*charptr = tilde_expand_filename(cp, getuid());
			break;

		case sRandomSeedFile:
			fprintf(stderr, "%s line %d: \"randomseed\" option is obsolete.\n",
				filename, linenum);
			cp = strtok(NULL, WHITESPACE);
			break;

		case sPermitRootLogin:
			intptr = &options->permit_root_login;
			cp = strtok(NULL, WHITESPACE);
			if (!cp) {
				fprintf(stderr, "%s line %d: missing yes/without-password/no argument.\n",
					filename, linenum);
				exit(1);
			}
			if (strcmp(cp, "without-password") == 0)
				value = 2;
			else if (strcmp(cp, "yes") == 0)
				value = 1;
			else if (strcmp(cp, "no") == 0)
				value = 0;
			else {
				fprintf(stderr, "%s line %d: Bad yes/without-password/no argument: %s\n",
					filename, linenum, cp);
				exit(1);
			}
			if (*intptr == -1)
				*intptr = value;
			break;

		case sIgnoreRhosts:
			intptr = &options->ignore_rhosts;
parse_flag:
			cp = strtok(NULL, WHITESPACE);
			if (!cp) {
				fprintf(stderr, "%s line %d: missing yes/no argument.\n",
					filename, linenum);
				exit(1);
			}
			if (strcmp(cp, "yes") == 0)
				value = 1;
			else if (strcmp(cp, "no") == 0)
				value = 0;
			else {
				fprintf(stderr, "%s line %d: Bad yes/no argument: %s\n",
					filename, linenum, cp);
				exit(1);
			}
			if (*intptr == -1)
				*intptr = value;
			break;

		case sIgnoreUserKnownHosts:
			intptr = &options->ignore_user_known_hosts;
			goto parse_int;

		case sRhostsAuthentication:
			intptr = &options->rhosts_authentication;
			goto parse_flag;

		case sRhostsRSAAuthentication:
			intptr = &options->rhosts_rsa_authentication;
			goto parse_flag;

		case sRSAAuthentication:
			intptr = &options->rsa_authentication;
			goto parse_flag;

#ifdef KRB4
		case sKerberosAuthentication:
			intptr = &options->kerberos_authentication;
			goto parse_flag;

		case sKerberosOrLocalPasswd:
			intptr = &options->kerberos_or_local_passwd;
			goto parse_flag;

		case sKerberosTicketCleanup:
			intptr = &options->kerberos_ticket_cleanup;
			goto parse_flag;
#endif

#ifdef AFS
		case sKerberosTgtPassing:
			intptr = &options->kerberos_tgt_passing;
			goto parse_flag;

		case sAFSTokenPassing:
			intptr = &options->afs_token_passing;
			goto parse_flag;
#endif

		case sPasswordAuthentication:
			intptr = &options->password_authentication;
			goto parse_flag;

		case sCheckMail:
			intptr = &options->check_mail;
			goto parse_flag;

#ifdef SKEY
		case sSkeyAuthentication:
			intptr = &options->skey_authentication;
			goto parse_flag;
#endif

		case sPrintMotd:
			intptr = &options->print_motd;
			goto parse_flag;

		case sX11Forwarding:
			intptr = &options->x11_forwarding;
			goto parse_flag;

		case sX11DisplayOffset:
			intptr = &options->x11_display_offset;
			goto parse_int;

		case sStrictModes:
			intptr = &options->strict_modes;
			goto parse_flag;

		case sKeepAlives:
			intptr = &options->keepalives;
			goto parse_flag;

		case sEmptyPasswd:
			intptr = &options->permit_empty_passwd;
			goto parse_flag;

		case sUseLogin:
			intptr = &options->use_login;
			goto parse_flag;

		case sLogFacility:
			intptr = (int *) &options->log_facility;
			cp = strtok(NULL, WHITESPACE);
			value = log_facility_number(cp);
			if (value == (SyslogFacility) - 1)
				fatal("%.200s line %d: unsupported log facility '%s'\n",
				  filename, linenum, cp ? cp : "<NONE>");
			if (*intptr == -1)
				*intptr = (SyslogFacility) value;
			break;

		case sLogLevel:
			intptr = (int *) &options->log_level;
			cp = strtok(NULL, WHITESPACE);
			value = log_level_number(cp);
			if (value == (LogLevel) - 1)
				fatal("%.200s line %d: unsupported log level '%s'\n",
				  filename, linenum, cp ? cp : "<NONE>");
			if (*intptr == -1)
				*intptr = (LogLevel) value;
			break;

		case sAllowUsers:
			while ((cp = strtok(NULL, WHITESPACE))) {
				if (options->num_allow_users >= MAX_ALLOW_USERS) {
					fprintf(stderr, "%s line %d: too many allow users.\n",
						filename, linenum);
					exit(1);
				}
				options->allow_users[options->num_allow_users++] = xstrdup(cp);
			}
			break;

		case sDenyUsers:
			while ((cp = strtok(NULL, WHITESPACE))) {
				if (options->num_deny_users >= MAX_DENY_USERS) {
					fprintf(stderr, "%s line %d: too many deny users.\n",
						filename, linenum);
					exit(1);
				}
				options->deny_users[options->num_deny_users++] = xstrdup(cp);
			}
			break;

		case sAllowGroups:
			while ((cp = strtok(NULL, WHITESPACE))) {
				if (options->num_allow_groups >= MAX_ALLOW_GROUPS) {
					fprintf(stderr, "%s line %d: too many allow groups.\n",
						filename, linenum);
					exit(1);
				}
				options->allow_groups[options->num_allow_groups++] = xstrdup(cp);
			}
			break;

		case sDenyGroups:
			while ((cp = strtok(NULL, WHITESPACE))) {
				if (options->num_deny_groups >= MAX_DENY_GROUPS) {
					fprintf(stderr, "%s line %d: too many deny groups.\n",
						filename, linenum);
					exit(1);
				}
				options->deny_groups[options->num_deny_groups++] = xstrdup(cp);
			}
			break;

		default:
			fprintf(stderr, "%s line %d: Missing handler for opcode %s (%d)\n",
				filename, linenum, cp, opcode);
			exit(1);
		}
		if (strtok(NULL, WHITESPACE) != NULL) {
			fprintf(stderr, "%s line %d: garbage at end of line.\n",
				filename, linenum);
			exit(1);
		}
	}
	fclose(f);
	if (bad_options > 0) {
		fprintf(stderr, "%s: terminating, %d bad configuration options\n",
			filename, bad_options);
		exit(1);
	}
}
