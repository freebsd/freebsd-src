/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for reading the configuration files.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: readconf.c,v 1.49 2000/10/11 20:27:23 markus Exp $");
RCSID("$FreeBSD$");

#include "ssh.h"
#include "readconf.h"
#include "match.h"
#include "xmalloc.h"
#include "compat.h"

/* Format of the configuration file:

   # Configuration data is parsed as follows:
   #  1. command line options
   #  2. user-specific file
   #  3. system-wide file
   # Any configuration value is only changed the first time it is set.
   # Thus, host-specific definitions should be at the beginning of the
   # configuration file, and defaults at the end.

   # Host-specific declarations.  These may override anything above.  A single
   # host may match multiple declarations; these are processed in the order
   # that they are given in.

   Host *.ngs.fi ngs.fi
     FallBackToRsh no

   Host fake.com
     HostName another.host.name.real.org
     User blaah
     Port 34289
     ForwardX11 no
     ForwardAgent no

   Host books.com
     RemoteForward 9999 shadows.cs.hut.fi:9999
     Cipher 3des

   Host fascist.blob.com
     Port 23123
     User tylonen
     RhostsAuthentication no
     PasswordAuthentication no

   Host puukko.hut.fi
     User t35124p
     ProxyCommand ssh-proxy %h %p

   Host *.fr
     UseRsh yes

   Host *.su
     Cipher none
     PasswordAuthentication no

   # Defaults for various options
   Host *
     ForwardAgent no
     ForwardX11 yes
     RhostsAuthentication yes
     PasswordAuthentication yes
     RSAAuthentication yes
     RhostsRSAAuthentication yes
     FallBackToRsh no
     UseRsh no
     StrictHostKeyChecking yes
     KeepAlives no
     IdentityFile ~/.ssh/identity
     Port 22
     EscapeChar ~

*/

/* Keyword tokens. */

typedef enum {
	oBadOption,
	oForwardAgent, oForwardX11, oGatewayPorts, oRhostsAuthentication,
	oPasswordAuthentication, oRSAAuthentication, oFallBackToRsh, oUseRsh,
	oSkeyAuthentication, oXAuthLocation,
#ifdef KRB4
	oKrb4Authentication,
#endif /* KRB4 */
#ifdef KRB5
	oKrb5Authentication, oKrb5TgtPassing,
#endif /* KRB5 */
#ifdef AFS
	oKrb4TgtPassing, oAFSTokenPassing,
#endif
	oIdentityFile, oHostName, oPort, oCipher, oRemoteForward, oLocalForward,
	oUser, oHost, oEscapeChar, oRhostsRSAAuthentication, oProxyCommand,
	oGlobalKnownHostsFile, oUserKnownHostsFile, oConnectionAttempts,
	oBatchMode, oCheckHostIP, oStrictHostKeyChecking, oCompression,
	oCompressionLevel, oKeepAlives, oNumberOfPasswordPrompts, oTISAuthentication,
	oUsePrivilegedPort, oLogLevel, oCiphers, oProtocol, oIdentityFile2,
	oGlobalKnownHostsFile2, oUserKnownHostsFile2, oDSAAuthentication,
	oKbdInteractiveAuthentication, oKbdInteractiveDevices
} OpCodes;

/* Textual representations of the tokens. */

static struct {
	const char *name;
	OpCodes opcode;
} keywords[] = {
	{ "forwardagent", oForwardAgent },
	{ "forwardx11", oForwardX11 },
	{ "xauthlocation", oXAuthLocation },
	{ "gatewayports", oGatewayPorts },
	{ "useprivilegedport", oUsePrivilegedPort },
	{ "rhostsauthentication", oRhostsAuthentication },
	{ "passwordauthentication", oPasswordAuthentication },
	{ "kbdinteractiveauthentication", oKbdInteractiveAuthentication },
	{ "kbdinteractivedevices", oKbdInteractiveDevices },
	{ "rsaauthentication", oRSAAuthentication },
	{ "dsaauthentication", oDSAAuthentication },
	{ "skeyauthentication", oSkeyAuthentication },
#ifdef KRB4
	{ "kerberos4authentication", oKrb4Authentication },
#endif /* KRB4 */
#ifdef KRB5
	{ "kerberos5authentication", oKrb5Authentication },
	{ "kerberos5tgtpassing", oKrb5TgtPassing },
#endif /* KRB5 */
#ifdef AFS
	{ "kerberos4tgtpassing", oKrb4TgtPassing },
	{ "afstokenpassing", oAFSTokenPassing },
#endif
	{ "fallbacktorsh", oFallBackToRsh },
	{ "usersh", oUseRsh },
	{ "identityfile", oIdentityFile },
	{ "identityfile2", oIdentityFile2 },
	{ "hostname", oHostName },
	{ "proxycommand", oProxyCommand },
	{ "port", oPort },
	{ "cipher", oCipher },
	{ "ciphers", oCiphers },
	{ "protocol", oProtocol },
	{ "remoteforward", oRemoteForward },
	{ "localforward", oLocalForward },
	{ "user", oUser },
	{ "host", oHost },
	{ "escapechar", oEscapeChar },
	{ "rhostsrsaauthentication", oRhostsRSAAuthentication },
	{ "globalknownhostsfile", oGlobalKnownHostsFile },
	{ "userknownhostsfile", oUserKnownHostsFile },
	{ "globalknownhostsfile2", oGlobalKnownHostsFile2 },
	{ "userknownhostsfile2", oUserKnownHostsFile2 },
	{ "connectionattempts", oConnectionAttempts },
	{ "batchmode", oBatchMode },
	{ "checkhostip", oCheckHostIP },
	{ "stricthostkeychecking", oStrictHostKeyChecking },
	{ "compression", oCompression },
	{ "compressionlevel", oCompressionLevel },
	{ "keepalive", oKeepAlives },
	{ "numberofpasswordprompts", oNumberOfPasswordPrompts },
	{ "tisauthentication", oTISAuthentication },
	{ "loglevel", oLogLevel },
	{ NULL, 0 }
};

/*
 * Adds a local TCP/IP port forward to options.  Never returns if there is an
 * error.
 */

void
add_local_forward(Options *options, u_short port, const char *host,
		  u_short host_port)
{
	Forward *fwd;
	extern uid_t original_real_uid;
	if (port < IPPORT_RESERVED && original_real_uid != 0)
		fatal("Privileged ports can only be forwarded by root.\n");
	if (options->num_local_forwards >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("Too many local forwards (max %d).", SSH_MAX_FORWARDS_PER_DIRECTION);
	fwd = &options->local_forwards[options->num_local_forwards++];
	fwd->port = port;
	fwd->host = xstrdup(host);
	fwd->host_port = host_port;
}

/*
 * Adds a remote TCP/IP port forward to options.  Never returns if there is
 * an error.
 */

void
add_remote_forward(Options *options, u_short port, const char *host,
		   u_short host_port)
{
	Forward *fwd;
	if (options->num_remote_forwards >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("Too many remote forwards (max %d).",
		      SSH_MAX_FORWARDS_PER_DIRECTION);
	fwd = &options->remote_forwards[options->num_remote_forwards++];
	fwd->port = port;
	fwd->host = xstrdup(host);
	fwd->host_port = host_port;
}

/*
 * Returns the number of the token pointed to by cp of length len. Never
 * returns if the token is not known.
 */

static OpCodes
parse_token(const char *cp, const char *filename, int linenum)
{
	unsigned int i;

	for (i = 0; keywords[i].name; i++)
		if (strcasecmp(cp, keywords[i].name) == 0)
			return keywords[i].opcode;

	fprintf(stderr, "%s: line %d: Bad configuration option: %s\n",
		filename, linenum, cp);
	return oBadOption;
}

/*
 * Processes a single option line as used in the configuration files. This
 * only sets those values that have not already been set.
 */

int
process_config_line(Options *options, const char *host,
		    char *line, const char *filename, int linenum,
		    int *activep)
{
	char buf[256], *s, *string, **charptr, *endofnumber, *keyword, *arg;
	int opcode, *intptr, value;
	u_short fwd_port, fwd_host_port;

	s = line;
	/* Get the keyword. (Each line is supposed to begin with a keyword). */
	keyword = strdelim(&s);
	/* Ignore leading whitespace. */
	if (*keyword == '\0')
		keyword = strdelim(&s);
	if (!*keyword || *keyword == '\n' || *keyword == '#')
		return 0;

	opcode = parse_token(keyword, filename, linenum);

	switch (opcode) {
	case oBadOption:
		/* don't panic, but count bad options */
		return -1;
		/* NOTREACHED */
	case oForwardAgent:
		intptr = &options->forward_agent;
parse_flag:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing yes/no argument.", filename, linenum);
		value = 0;	/* To avoid compiler warning... */
		if (strcmp(arg, "yes") == 0 || strcmp(arg, "true") == 0)
			value = 1;
		else if (strcmp(arg, "no") == 0 || strcmp(arg, "false") == 0)
			value = 0;
		else
			fatal("%.200s line %d: Bad yes/no argument.", filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oForwardX11:
		intptr = &options->forward_x11;
		goto parse_flag;

	case oGatewayPorts:
		intptr = &options->gateway_ports;
		goto parse_flag;

	case oUsePrivilegedPort:
		intptr = &options->use_privileged_port;
		goto parse_flag;

	case oRhostsAuthentication:
		intptr = &options->rhosts_authentication;
		goto parse_flag;

	case oPasswordAuthentication:
		intptr = &options->password_authentication;
		goto parse_flag;

	case oKbdInteractiveAuthentication:
		intptr = &options->kbd_interactive_authentication;
		goto parse_flag;

	case oKbdInteractiveDevices:
		charptr = &options->kbd_interactive_devices;
		goto parse_string;

	case oDSAAuthentication:
		intptr = &options->dsa_authentication;
		goto parse_flag;

	case oRSAAuthentication:
		intptr = &options->rsa_authentication;
		goto parse_flag;

	case oRhostsRSAAuthentication:
		intptr = &options->rhosts_rsa_authentication;
		goto parse_flag;

	case oTISAuthentication:
		/* fallthrough, there is no difference on the client side */
	case oSkeyAuthentication:
		intptr = &options->skey_authentication;
		goto parse_flag;

#ifdef KRB4
	case oKrb4Authentication:
		intptr = &options->krb4_authentication;
		goto parse_flag;
#endif /* KRB4 */

#ifdef KRB5
	case oKrb5Authentication:
		intptr = &options->krb5_authentication;
		goto parse_flag;

	case oKrb5TgtPassing:
		intptr = &options->krb5_tgt_passing;
		goto parse_flag;
#endif /* KRB5 */

#ifdef AFS
	case oKrb4TgtPassing:
		intptr = &options->krb4_tgt_passing;
		goto parse_flag;

	case oAFSTokenPassing:
		intptr = &options->afs_token_passing;
		goto parse_flag;
#endif

	case oFallBackToRsh:
		intptr = &options->fallback_to_rsh;
		goto parse_flag;

	case oUseRsh:
		intptr = &options->use_rsh;
		goto parse_flag;

	case oBatchMode:
		intptr = &options->batch_mode;
		goto parse_flag;

	case oCheckHostIP:
		intptr = &options->check_host_ip;
		goto parse_flag;

	case oStrictHostKeyChecking:
		intptr = &options->strict_host_key_checking;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing yes/no argument.",
			      filename, linenum);
		value = 0;	/* To avoid compiler warning... */
		if (strcmp(arg, "yes") == 0 || strcmp(arg, "true") == 0)
			value = 1;
		else if (strcmp(arg, "no") == 0 || strcmp(arg, "false") == 0)
			value = 0;
		else if (strcmp(arg, "ask") == 0)
			value = 2;
		else
			fatal("%.200s line %d: Bad yes/no/ask argument.", filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oCompression:
		intptr = &options->compression;
		goto parse_flag;

	case oKeepAlives:
		intptr = &options->keepalives;
		goto parse_flag;

	case oNumberOfPasswordPrompts:
		intptr = &options->number_of_password_prompts;
		goto parse_int;

	case oCompressionLevel:
		intptr = &options->compression_level;
		goto parse_int;

	case oIdentityFile:
	case oIdentityFile2:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (*activep) {
			intptr = (opcode == oIdentityFile) ?
			    &options->num_identity_files :
			    &options->num_identity_files2;
			if (*intptr >= SSH_MAX_IDENTITY_FILES)
				fatal("%.200s line %d: Too many identity files specified (max %d).",
				      filename, linenum, SSH_MAX_IDENTITY_FILES);
			charptr = (opcode == oIdentityFile) ?
			    &options->identity_files[*intptr] :
			    &options->identity_files2[*intptr];
			*charptr = xstrdup(arg);
			*intptr = *intptr + 1;
		}
		break;

	case oXAuthLocation:
		charptr=&options->xauth_location;
		goto parse_string;

	case oUser:
		charptr = &options->user;
parse_string:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (*activep && *charptr == NULL)
			*charptr = xstrdup(arg);
		break;

	case oGlobalKnownHostsFile:
		charptr = &options->system_hostfile;
		goto parse_string;

	case oUserKnownHostsFile:
		charptr = &options->user_hostfile;
		goto parse_string;

	case oGlobalKnownHostsFile2:
		charptr = &options->system_hostfile2;
		goto parse_string;

	case oUserKnownHostsFile2:
		charptr = &options->user_hostfile2;
		goto parse_string;

	case oHostName:
		charptr = &options->hostname;
		goto parse_string;

	case oProxyCommand:
		charptr = &options->proxy_command;
		string = xstrdup("");
		while ((arg = strdelim(&s)) != NULL && *arg != '\0') {
			string = xrealloc(string, strlen(string) + strlen(arg) + 2);
			strcat(string, " ");
			strcat(string, arg);
		}
		if (*activep && *charptr == NULL)
			*charptr = string;
		else
			xfree(string);
		return 0;

	case oPort:
		intptr = &options->port;
parse_int:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (arg[0] < '0' || arg[0] > '9')
			fatal("%.200s line %d: Bad number.", filename, linenum);

		/* Octal, decimal, or hex format? */
		value = strtol(arg, &endofnumber, 0);
		if (arg == endofnumber)
			fatal("%.200s line %d: Bad number.", filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oConnectionAttempts:
		intptr = &options->connection_attempts;
		goto parse_int;

	case oCipher:
		intptr = &options->cipher;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		value = cipher_number(arg);
		if (value == -1)
			fatal("%.200s line %d: Bad cipher '%s'.",
			      filename, linenum, arg ? arg : "<NONE>");
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oCiphers:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (!ciphers_valid(arg))
			fatal("%.200s line %d: Bad SSH2 cipher spec '%s'.",
			      filename, linenum, arg ? arg : "<NONE>");
		if (*activep && options->ciphers == NULL)
			options->ciphers = xstrdup(arg);
		break;

	case oProtocol:
		intptr = &options->protocol;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		value = proto_spec(arg);
		if (value == SSH_PROTO_UNKNOWN)
			fatal("%.200s line %d: Bad protocol spec '%s'.",
			      filename, linenum, arg ? arg : "<NONE>");
		if (*activep && *intptr == SSH_PROTO_UNKNOWN)
			*intptr = value;
		break;

	case oLogLevel:
		intptr = (int *) &options->log_level;
		arg = strdelim(&s);
		value = log_level_number(arg);
		if (value == (LogLevel) - 1)
			fatal("%.200s line %d: unsupported log level '%s'\n",
			      filename, linenum, arg ? arg : "<NONE>");
		if (*activep && (LogLevel) * intptr == -1)
			*intptr = (LogLevel) value;
		break;

	case oRemoteForward:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (arg[0] < '0' || arg[0] > '9')
			fatal("%.200s line %d: Badly formatted port number.",
			      filename, linenum);
		fwd_port = atoi(arg);
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing second argument.",
			      filename, linenum);
		if (sscanf(arg, "%255[^:]:%hu", buf, &fwd_host_port) != 2)
			fatal("%.200s line %d: Badly formatted host:port.",
			      filename, linenum);
		if (*activep)
			add_remote_forward(options, fwd_port, buf, fwd_host_port);
		break;

	case oLocalForward:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (arg[0] < '0' || arg[0] > '9')
			fatal("%.200s line %d: Badly formatted port number.",
			      filename, linenum);
		fwd_port = atoi(arg);
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing second argument.",
			      filename, linenum);
		if (sscanf(arg, "%255[^:]:%hu", buf, &fwd_host_port) != 2)
			fatal("%.200s line %d: Badly formatted host:port.",
			      filename, linenum);
		if (*activep)
			add_local_forward(options, fwd_port, buf, fwd_host_port);
		break;

	case oHost:
		*activep = 0;
		while ((arg = strdelim(&s)) != NULL && *arg != '\0')
			if (match_pattern(host, arg)) {
				debug("Applying options for %.100s", arg);
				*activep = 1;
				break;
			}
		/* Avoid garbage check below, as strdelim is done. */
		return 0;

	case oEscapeChar:
		intptr = &options->escape_char;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (arg[0] == '^' && arg[2] == 0 &&
		    (unsigned char) arg[1] >= 64 && (unsigned char) arg[1] < 128)
			value = (unsigned char) arg[1] & 31;
		else if (strlen(arg) == 1)
			value = (unsigned char) arg[0];
		else if (strcmp(arg, "none") == 0)
			value = -2;
		else {
			fatal("%.200s line %d: Bad escape character.",
			      filename, linenum);
			/* NOTREACHED */
			value = 0;	/* Avoid compiler warning. */
		}
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	default:
		fatal("process_config_line: Unimplemented opcode %d", opcode);
	}

	/* Check that there is no garbage at end of line. */
	if ((arg = strdelim(&s)) != NULL && *arg != '\0')
	{
		fatal("%.200s line %d: garbage at end of line; \"%.200s\".",
		      filename, linenum, arg);
	}
	return 0;
}


/*
 * Reads the config file and modifies the options accordingly.  Options
 * should already be initialized before this call.  This never returns if
 * there is an error.  If the file does not exist, this returns immediately.
 */

void
read_config_file(const char *filename, const char *host, Options *options)
{
	FILE *f;
	char line[1024];
	int active, linenum;
	int bad_options = 0;

	/* Open the file. */
	f = fopen(filename, "r");
	if (!f)
		return;

	debug("Reading configuration data %.200s", filename);

	/*
	 * Mark that we are now processing the options.  This flag is turned
	 * on/off by Host specifications.
	 */
	active = 1;
	linenum = 0;
	while (fgets(line, sizeof(line), f)) {
		/* Update line number counter. */
		linenum++;
		if (process_config_line(options, host, line, filename, linenum, &active) != 0)
			bad_options++;
	}
	fclose(f);
	if (bad_options > 0)
		fatal("%s: terminating, %d bad configuration options\n",
		      filename, bad_options);
}

/*
 * Initializes options to special values that indicate that they have not yet
 * been set.  Read_config_file will only set options with this value. Options
 * are processed in the following order: command line, user config file,
 * system config file.  Last, fill_default_options is called.
 */

void
initialize_options(Options * options)
{
	memset(options, 'X', sizeof(*options));
	options->forward_agent = -1;
	options->forward_x11 = -1;
	options->xauth_location = NULL;
	options->gateway_ports = -1;
	options->use_privileged_port = -1;
	options->rhosts_authentication = -1;
	options->rsa_authentication = -1;
	options->dsa_authentication = -1;
	options->skey_authentication = -1;
#ifdef KRB4
	options->krb4_authentication = -1;
#endif
#ifdef KRB5
	options->krb5_authentication = -1;
	options->krb5_tgt_passing = -1;
#endif /* KRB5 */
#ifdef AFS
	options->krb4_tgt_passing = -1;
	options->afs_token_passing = -1;
#endif
	options->password_authentication = -1;
	options->kbd_interactive_authentication = -1;
	options->kbd_interactive_devices = NULL;
	options->rhosts_rsa_authentication = -1;
	options->fallback_to_rsh = -1;
	options->use_rsh = -1;
	options->batch_mode = -1;
	options->check_host_ip = -1;
	options->strict_host_key_checking = -1;
	options->compression = -1;
	options->keepalives = -1;
	options->compression_level = -1;
	options->port = -1;
	options->connection_attempts = -1;
	options->number_of_password_prompts = -1;
	options->cipher = -1;
	options->ciphers = NULL;
	options->protocol = SSH_PROTO_UNKNOWN;
	options->num_identity_files = 0;
	options->num_identity_files2 = 0;
	options->hostname = NULL;
	options->proxy_command = NULL;
	options->user = NULL;
	options->escape_char = -1;
	options->system_hostfile = NULL;
	options->user_hostfile = NULL;
	options->system_hostfile2 = NULL;
	options->user_hostfile2 = NULL;
	options->num_local_forwards = 0;
	options->num_remote_forwards = 0;
	options->log_level = (LogLevel) - 1;
}

/*
 * Called after processing other sources of option data, this fills those
 * options for which no value has been specified with their default values.
 */

void
fill_default_options(Options * options)
{
	if (options->forward_agent == -1)
		options->forward_agent = 0;
	if (options->forward_x11 == -1)
		options->forward_x11 = 0;
#ifdef XAUTH_PATH
	if (options->xauth_location == NULL)
		options->xauth_location = XAUTH_PATH;
#endif /* XAUTH_PATH */
	if (options->gateway_ports == -1)
		options->gateway_ports = 0;
	if (options->use_privileged_port == -1)
		options->use_privileged_port = 1;
	if (options->rhosts_authentication == -1)
		options->rhosts_authentication = 1;
	if (options->rsa_authentication == -1)
		options->rsa_authentication = 1;
	if (options->dsa_authentication == -1)
		options->dsa_authentication = 1;
	if (options->skey_authentication == -1)
		options->skey_authentication = 0;
#ifdef KRB4
	if (options->krb4_authentication == -1)
		options->krb4_authentication = 1;
#endif /* KRB4 */
#ifdef KRB5
	if (options->krb5_authentication == -1)
		options->krb5_authentication = 1;
	if (options->krb5_tgt_passing == -1)
		options->krb5_tgt_passing = 1;
#endif /* KRB5 */
#ifdef AFS
	if (options->krb4_tgt_passing == -1)
		options->krb4_tgt_passing = 1;
	if (options->afs_token_passing == -1)
		options->afs_token_passing = 1;
#endif /* AFS */
	if (options->password_authentication == -1)
		options->password_authentication = 1;
	if (options->kbd_interactive_authentication == -1)
		options->kbd_interactive_authentication = 0;
	if (options->rhosts_rsa_authentication == -1)
		options->rhosts_rsa_authentication = 1;
	if (options->fallback_to_rsh == -1)
		options->fallback_to_rsh = 0;
	if (options->use_rsh == -1)
		options->use_rsh = 0;
	if (options->batch_mode == -1)
		options->batch_mode = 0;
	if (options->check_host_ip == -1)
		options->check_host_ip = 0;
	if (options->strict_host_key_checking == -1)
		options->strict_host_key_checking = 2;	/* 2 is default */
	if (options->compression == -1)
		options->compression = 0;
	if (options->keepalives == -1)
		options->keepalives = 1;
	if (options->compression_level == -1)
		options->compression_level = 6;
	if (options->port == -1)
		options->port = 0;	/* Filled in ssh_connect. */
	if (options->connection_attempts == -1)
		options->connection_attempts = 4;
	if (options->number_of_password_prompts == -1)
		options->number_of_password_prompts = 3;
	/* Selected in ssh_login(). */
	if (options->cipher == -1)
		options->cipher = SSH_CIPHER_NOT_SET;
	/* options->ciphers, default set in myproposals.h */
	if (options->protocol == SSH_PROTO_UNKNOWN)
		options->protocol = SSH_PROTO_1|SSH_PROTO_2|SSH_PROTO_1_PREFERRED;
	if (options->num_identity_files == 0) {
		options->identity_files[0] =
			xmalloc(2 + strlen(SSH_CLIENT_IDENTITY) + 1);
		sprintf(options->identity_files[0], "~/%.100s", SSH_CLIENT_IDENTITY);
		options->num_identity_files = 1;
	}
	if (options->num_identity_files2 == 0) {
		options->identity_files2[0] =
			xmalloc(2 + strlen(SSH_CLIENT_ID_DSA) + 1);
		sprintf(options->identity_files2[0], "~/%.100s", SSH_CLIENT_ID_DSA);
		options->num_identity_files2 = 1;
	}
	if (options->escape_char == -1)
		options->escape_char = '~';
	if (options->system_hostfile == NULL)
		options->system_hostfile = SSH_SYSTEM_HOSTFILE;
	if (options->user_hostfile == NULL)
		options->user_hostfile = SSH_USER_HOSTFILE;
	if (options->system_hostfile2 == NULL)
		options->system_hostfile2 = SSH_SYSTEM_HOSTFILE2;
	if (options->user_hostfile2 == NULL)
		options->user_hostfile2 = SSH_USER_HOSTFILE2;
	if (options->log_level == (LogLevel) - 1)
		options->log_level = SYSLOG_LEVEL_INFO;
	/* options->proxy_command should not be set by default */
	/* options->user will be set in the main program if appropriate */
	/* options->hostname will be set in the main program if appropriate */
}
