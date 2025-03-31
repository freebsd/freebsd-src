/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Original copyright (c) 2018 Pierre Pronchery <khorben@defora.org> (for the
 * NetBSD Project)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (c) 2022 ADISTA SAS (FreeBSD updates)
 *
 * Updates for FreeBSD by Pierre Pronchery <pierre@defora.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the copyright holder nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: umbctl.c,v 1.4 2020/05/13 21:44:30 khorben Exp $
 */

#include <sys/endian.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "mbim.h"
#include "if_umbreg.h"

/* constants */
static const struct umb_valdescr _umb_actstate[] =
	MBIM_ACTIVATION_STATE_DESCRIPTIONS;

static const struct umb_valdescr _umb_simstate[] =
	MBIM_SIMSTATE_DESCRIPTIONS;

static const struct umb_valdescr _umb_regstate[] =
	MBIM_REGSTATE_DESCRIPTIONS;

static const struct umb_valdescr _umb_pktstate[] =
	MBIM_PKTSRV_STATE_DESCRIPTIONS;

static const struct umb_valdescr _umb_dataclass[] =
	MBIM_DATACLASS_DESCRIPTIONS;

static const struct umb_valdescr _umb_state[] =
	UMB_INTERNAL_STATE_DESCRIPTIONS;

static const struct umb_valdescr _umb_pin_state[] =
{
	{ UMB_PIN_REQUIRED, "PIN required"},
	{ UMB_PIN_UNLOCKED, "PIN unlocked"},
	{ UMB_PUK_REQUIRED, "PUK required"},
	{ 0, NULL }
};

static const struct umb_valdescr _umb_regmode[] =
{
	{ MBIM_REGMODE_UNKNOWN, "unknown" },
	{ MBIM_REGMODE_AUTOMATIC, "automatic" },
	{ MBIM_REGMODE_MANUAL, "manual" },
	{ 0, NULL }
};

static const struct umb_valdescr _umb_ber[] =
{
	{ UMB_BER_EXCELLENT, "excellent" },
	{ UMB_BER_VERYGOOD, "very good" },
	{ UMB_BER_GOOD, "good" },
	{ UMB_BER_OK, "ok" },
	{ UMB_BER_MEDIUM, "medium" },
	{ UMB_BER_BAD, "bad" },
	{ UMB_BER_VERYBAD, "very bad" },
	{ UMB_BER_EXTREMELYBAD, "extremely bad" },
	{ 0, NULL }
};


/* prototypes */
static int _char_to_utf16(const char * in, uint16_t * out, size_t outlen);
static int _error(int ret, char const * format, ...);
static int _umbctl(char const * ifname, int verbose, int argc, char * argv[]);
static int _umbctl_file(char const * ifname, char const * filename,
		int verbose);
static void _umbctl_info(char const * ifname, struct umb_info * umbi);
static int _umbctl_ioctl(char const * ifname, int fd, unsigned long request,
		struct ifreq * ifr);
static int _umbctl_set(char const * ifname, struct umb_parameter * umbp,
		int argc, char * argv[]);
static int _umbctl_socket(void);
static int _usage(void);
static void _utf16_to_char(uint16_t * in, int inlen, char * out, size_t outlen);


/* functions */
/* char_to_utf16 */
/* this function is from OpenBSD's ifconfig(8) */
static int _char_to_utf16(const char * in, uint16_t * out, size_t outlen)
{
	int	n = 0;
	uint16_t c;

	for (;;) {
		c = *in++;

		if (c == '\0') {
			/*
			 * NUL termination is not required, but zero out the
			 * residual buffer
			 */
			memset(out, 0, outlen);
			return n;
		}
		if (outlen < sizeof(*out))
			return -1;

		*out++ = htole16(c);
		n += sizeof(*out);
		outlen -= sizeof(*out);
	}
}


/* error */
static int _error(int ret, char const * format, ...)
{
	va_list ap;

	fputs("umbctl: ", stderr);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fputs("\n", stderr);
	return ret;
}


/* umbctl */
static int _umbctl(char const * ifname, int verbose, int argc, char * argv[])
{
	int fd;
	struct ifreq ifr;
	struct umb_info umbi;
	struct umb_parameter umbp;

	if((fd = _umbctl_socket()) < 0)
		return 2;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if(argc != 0)
	{
		memset(&umbp, 0, sizeof(umbp));
		ifr.ifr_data = (caddr_t)&umbp;
		if(_umbctl_ioctl(ifname, fd, SIOCGUMBPARAM, &ifr) != 0
				|| _umbctl_set(ifname, &umbp, argc, argv) != 0
				|| _umbctl_ioctl(ifname, fd, SIOCSUMBPARAM,
					&ifr) != 0)
		{
			close(fd);
			return 2;
		}
	}
	if(argc == 0 || verbose > 0)
	{
		ifr.ifr_data = (caddr_t)&umbi;
		if(_umbctl_ioctl(ifname, fd, SIOCGUMBINFO, &ifr) != 0)
		{
			close(fd);
			return 3;
		}
		_umbctl_info(ifname, &umbi);
	}
	if(close(fd) != 0)
		return _error(2, "%s: %s", ifname, strerror(errno));
	return 0;
}


/* umbctl_file */
static int _file_parse(char const * ifname, struct umb_parameter * umbp,
		char const * filename);

static int _umbctl_file(char const * ifname, char const * filename, int verbose)
{
	int fd;
	struct ifreq ifr;
	struct umb_parameter umbp;
	struct umb_info umbi;

	if((fd = _umbctl_socket()) < 0)
		return 2;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)&umbp;
	memset(&umbp, 0, sizeof(umbp));
	if(_umbctl_ioctl(ifname, fd, SIOCGUMBPARAM, &ifr) != 0
			|| _file_parse(ifname, &umbp, filename) != 0
			|| _umbctl_ioctl(ifname, fd, SIOCSUMBPARAM, &ifr) != 0)
	{
		close(fd);
		return 2;
	}
	if(verbose > 0)
	{
		ifr.ifr_data = (caddr_t)&umbi;
		if(_umbctl_ioctl(ifname, fd, SIOCGUMBINFO, &ifr) != 0)
		{
			close(fd);
			return 3;
		}
		_umbctl_info(ifname, &umbi);
	}
	if(close(fd) != 0)
		return _error(2, "%s: %s", ifname, strerror(errno));
	return 0;
}

static int _file_parse(char const * ifname, struct umb_parameter * umbp,
		char const * filename)
{
	int ret = 0;
	FILE * fp;
	char buf[512];
	size_t len;
	int i;
	int eof;
	char * tokens[3] = { buf, NULL, NULL };
	char * p;

	if((fp = fopen(filename, "r")) == NULL)
		return _error(2, "%s: %s", filename, strerror(errno));
	while(fgets(buf, sizeof(buf), fp) != NULL)
	{
		if(buf[0] == '#')
			continue;
		buf[sizeof(buf) - 1] = '\0';
		if((len = strlen(buf)) > 0)
		{
			if(buf[len - 1] != '\n')
			{
				ret = _error(2, "%s: %s", filename,
						"Line too long");
				while((i = fgetc(fp)) != EOF && i != '\n');
				continue;
			}
			buf[len - 1] = '\0';
		}
		if((p = strchr(buf, '=')) != NULL)
		{
			tokens[1] = p + 1;
			*p = '\0';
		} else
			tokens[1] = NULL;
		ret |= _umbctl_set(ifname, umbp, (p != NULL) ? 2 : 1, tokens)
			? 2 : 0;
	}
	eof = feof(fp);
	if(fclose(fp) != 0 || !eof)
		return _error(2, "%s: %s", filename, strerror(errno));
	return ret;
}


/* umbctl_info */
static void _umbctl_info(char const * ifname, struct umb_info * umbi)
{
	char provider[UMB_PROVIDERNAME_MAXLEN + 1];
	char pn[UMB_PHONENR_MAXLEN + 1];
	char roaming[UMB_ROAMINGTEXT_MAXLEN + 1];
	char apn[UMB_APN_MAXLEN + 1];
	char fwinfo[UMB_FWINFO_MAXLEN + 1];
	char hwinfo[UMB_HWINFO_MAXLEN + 1];

	_utf16_to_char(umbi->provider, UMB_PROVIDERNAME_MAXLEN,
			provider, sizeof(provider));
	_utf16_to_char(umbi->pn, UMB_PHONENR_MAXLEN, pn, sizeof(pn));
	_utf16_to_char(umbi->roamingtxt, UMB_ROAMINGTEXT_MAXLEN,
			roaming, sizeof(roaming));
	_utf16_to_char(umbi->apn, UMB_APN_MAXLEN, apn, sizeof(apn));
	_utf16_to_char(umbi->fwinfo, UMB_FWINFO_MAXLEN, fwinfo, sizeof(fwinfo));
	_utf16_to_char(umbi->hwinfo, UMB_HWINFO_MAXLEN, hwinfo, sizeof(hwinfo));
	printf("%s: state %s, mode %s, registration %s\n"
			"\tprovider \"%s\", dataclass %s, signal %s\n"
			"\tphone number \"%s\", roaming \"%s\" (%s)\n"
			"\tAPN \"%s\", TX %" PRIu64 ", RX %" PRIu64 "\n"
			"\tfirmware \"%s\", hardware \"%s\"\n",
			ifname, umb_val2descr(_umb_state, umbi->state),
			umb_val2descr(_umb_regmode, umbi->regmode),
			umb_val2descr(_umb_regstate, umbi->regstate), provider,
			umb_val2descr(_umb_dataclass, umbi->cellclass),
			umb_val2descr(_umb_ber, umbi->ber), pn, roaming,
			umbi->enable_roaming ? "allowed" : "denied",
			apn, umbi->uplink_speed, umbi->downlink_speed,
			fwinfo, hwinfo);
}


/* umbctl_ioctl */
static int _umbctl_ioctl(char const * ifname, int fd, unsigned long request,
		struct ifreq * ifr)
{
	if(ioctl(fd, request, ifr) != 0)
		return _error(-1, "%s: %s", ifname, strerror(errno));
	return 0;
}


/* umbctl_set */
/* callbacks */
static int _set_apn(char const *, struct umb_parameter *, char const *);
static int _set_username(char const *, struct umb_parameter *, char const *);
static int _set_password(char const *, struct umb_parameter *, char const *);
static int _set_pin(char const *, struct umb_parameter *, char const *);
static int _set_puk(char const *, struct umb_parameter *, char const *);
static int _set_roaming_allow(char const *, struct umb_parameter *,
		char const *);
static int _set_roaming_deny(char const *, struct umb_parameter *,
		char const *);

static int _umbctl_set(char const * ifname, struct umb_parameter * umbp,
		int argc, char * argv[])
{
	struct
	{
		char const * name;
		int (*callback)(char const *,
				struct umb_parameter *, char const *);
		int parameter;
	} callbacks[] =
	{
		{ "apn", _set_apn, 1 },
		{ "username", _set_username, 1 },
		{ "password", _set_password, 1 },
		{ "pin", _set_pin, 1 },
		{ "puk", _set_puk, 1 },
		{ "roaming", _set_roaming_allow, 0 },
		{ "-roaming", _set_roaming_deny, 0 },
	};
	int i;
	size_t j;

	for(i = 0; i < argc; i++)
	{
		for(j = 0; j < sizeof(callbacks) / sizeof(*callbacks); j++)
			if(strcmp(argv[i], callbacks[j].name) == 0)
			{
				if(callbacks[j].parameter && i + 1 == argc)
					return _error(-1, "%s: Incomplete"
							" parameter", argv[i]);
				if(callbacks[j].callback(ifname, umbp,
							callbacks[j].parameter
							? argv[i + 1] : NULL))
					return -1;
				if(callbacks[j].parameter)
					i++;
				break;
			}
		if(j == sizeof(callbacks) / sizeof(*callbacks))
			return _error(-1, "%s: Unknown parameter", argv[i]);
	}
	return 0;
}

static int _set_apn(char const * ifname, struct umb_parameter * umbp,
		char const * apn)
{
	umbp->apnlen = _char_to_utf16(apn, umbp->apn, sizeof(umbp->apn));
	if(umbp->apnlen < 0 || (size_t)umbp->apnlen > sizeof(umbp->apn))
		return _error(-1, "%s: %s", ifname, "APN too long");
	return 0;
}

static int _set_username(char const * ifname, struct umb_parameter * umbp,
		char const * username)
{
	umbp->usernamelen = _char_to_utf16(username, umbp->username,
			sizeof(umbp->username));
	if(umbp->usernamelen < 0
			|| (size_t)umbp->usernamelen > sizeof(umbp->username))
		return _error(-1, "%s: %s", ifname, "Username too long");
	return 0;
}

static int _set_password(char const * ifname, struct umb_parameter * umbp,
		char const * password)
{
	umbp->passwordlen = _char_to_utf16(password, umbp->password,
			sizeof(umbp->password));
	if(umbp->passwordlen < 0
			|| (size_t)umbp->passwordlen > sizeof(umbp->password))
		return _error(-1, "%s: %s", ifname, "Password too long");
	return 0;
}

static int _set_pin(char const * ifname, struct umb_parameter * umbp,
		char const * pin)
{
	umbp->is_puk = 0;
	umbp->op = MBIM_PIN_OP_ENTER;
	umbp->pinlen = _char_to_utf16(pin, umbp->pin, sizeof(umbp->pin));
	if(umbp->pinlen < 0 || (size_t)umbp->pinlen
			> sizeof(umbp->pin))
		return _error(-1, "%s: %s", ifname, "PIN code too long");
	return 0;
}

static int _set_puk(char const * ifname, struct umb_parameter * umbp,
		char const * puk)
{
	umbp->is_puk = 1;
	umbp->op = MBIM_PIN_OP_ENTER;
	umbp->pinlen = _char_to_utf16(puk, umbp->pin, sizeof(umbp->pin));
	if(umbp->pinlen < 0 || (size_t)umbp->pinlen > sizeof(umbp->pin))
		return _error(-1, "%s: %s", ifname, "PUK code too long");
	return 0;
}

static int _set_roaming_allow(char const * ifname, struct umb_parameter * umbp,
		char const * unused)
{
	(void) ifname;
	(void) unused;

	umbp->roaming = 1;
	return 0;
}

static int _set_roaming_deny(char const * ifname, struct umb_parameter * umbp,
		char const * unused)
{
	(void) ifname;
	(void) unused;

	umbp->roaming = 0;
	return 0;
}


/* umbctl_socket */
static int _umbctl_socket(void)
{
	int fd;

	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return _error(-1, "socket: %s", strerror(errno));
	return fd;
}


/* usage */
static int _usage(void)
{
	fputs("Usage: umbctl [-v] ifname [parameter [value]] [...]\n"
"       umbctl -f config-file ifname\n",
			stderr);
	return 1;
}


/* utf16_to_char */
static void _utf16_to_char(uint16_t * in, int inlen, char * out, size_t outlen)
{
	uint16_t c;

	while (outlen > 0) {
		c = inlen > 0 ? htole16(*in) : 0;
		if (c == 0 || --outlen == 0) {
			/* always NUL terminate result */
			*out = '\0';
			break;
		}
		*out++ = isascii(c) ? (char)c : '?';
		in++;
		inlen--;
	}
}


/* main */
int main(int argc, char * argv[])
{
	int o;
	char const * filename = NULL;
	int verbose = 0;

	while((o = getopt(argc, argv, "f:gv")) != -1)
		switch(o)
		{
			case 'f':
				filename = optarg;
				break;
			case 'v':
				verbose++;
				break;
			default:
				return _usage();
		}
	if(optind == argc)
		return _usage();
	if(filename != NULL)
	{
		if(optind + 1 != argc)
			return _usage();
		return _umbctl_file(argv[optind], filename, verbose);
	}
	return _umbctl(argv[optind], verbose, argc - optind - 1,
			&argv[optind + 1]);
}
