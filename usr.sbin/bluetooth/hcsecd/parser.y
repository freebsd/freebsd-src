%{
/*
 * parser.y
 *
 * Copyright (c) 2001-2002 Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: parser.y,v 1.1 2002/11/24 20:22:39 max Exp $
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <errno.h>
#include <ng_hci.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include "hcsecd.h"

	int	yyparse  (void);
	int	yylex    (void);

static	void	free_key (link_key_p key);
static	int	hexa2int4(char *a);
static	int	hexa2int8(char *a);

extern	int			 yylineno;
static	LIST_HEAD(, link_key)	 link_keys;
	char			*config_file = "/usr/local/etc/hcsecd.conf";

static	link_key_p		 key = NULL;
%}

%union {
	char	*string;
}

%token <string> T_BDADDRSTRING T_HEXSTRING T_STRING
%token T_DEVICE T_BDADDR T_NAME T_KEY T_PIN T_NOKEY T_NOPIN T_JUNK

%%

config:		line
		| config line
		;

line:		T_DEVICE
			{
			key = (link_key_p) malloc(sizeof(*key));
			if (key == NULL) {
				syslog(LOG_ERR, "Could not allocate new " \
						"config entry");
				exit(1);
			}

			memset(key, 0, sizeof(*key));
			}
		'{' options '}'
			{
			if (get_key(&key->bdaddr, 1) != NULL) {
				syslog(LOG_ERR, "Ignoring duplicated entry " \
						"for bdaddr %x:%x:%x:%x:%x:%x",
						key->bdaddr.b[5],
						key->bdaddr.b[4],
						key->bdaddr.b[3],
						key->bdaddr.b[2],
						key->bdaddr.b[1],
						key->bdaddr.b[0]);
				free_key(key);
			} else 
				LIST_INSERT_HEAD(&link_keys, key, next);

			key = NULL;
			}
		;

options:	option ';'
		| options option ';'
		;

option:		bdaddr
		| name
		| key
		| pin
		;

bdaddr:		T_BDADDR T_BDADDRSTRING
			{
			int a0, a1, a2, a3, a4, a5;

			if (sscanf($2, "%x:%x:%x:%x:%x:%x",
					&a5, &a4, &a3, &a2, &a1, &a0) != 6) {
				syslog(LOG_ERR, "Cound not parse BDADDR " \
						"'%s'", $2);
				exit(1);
			}

			key->bdaddr.b[0] = (a0 & 0xff);
			key->bdaddr.b[1] = (a1 & 0xff);
			key->bdaddr.b[2] = (a2 & 0xff);
			key->bdaddr.b[3] = (a3 & 0xff);
			key->bdaddr.b[4] = (a4 & 0xff);
			key->bdaddr.b[5] = (a5 & 0xff);
			}
		;

name:		T_NAME T_STRING
			{
			if (key->name != NULL)
				free(key->name);

			key->name = strdup($2);
			if (key->name == NULL) {
				syslog(LOG_ERR, "Could not allocate new " \
						"device name");
				exit(1);
			}
			}
		;

key:		T_KEY T_HEXSTRING
			{
			int	i, len;

			if (key->key != NULL)
				free(key->key);

			key->key = (u_int8_t *) malloc(NG_HCI_KEY_SIZE);
			if (key->key == NULL) {
				syslog(LOG_ERR, "Could not allocate new " \
						"link key");
				exit(1);
			}

			memset(key->key, 0, NG_HCI_KEY_SIZE);

			len = strlen($2) / 2;
			if (len > NG_HCI_KEY_SIZE)
				len = NG_HCI_KEY_SIZE;

			for (i = 0; i < len; i ++)
				key->key[i] = hexa2int8((char *)($2) + 2*i);
			}
		| T_KEY T_NOKEY
			{
			if (key->key != NULL)
				free(key->key);

			key->key = NULL;
			}
		;

pin:		T_PIN T_STRING
			{
			if (key->pin != NULL)
				free(key->pin);

			key->pin = strdup($2);
			if (key->pin == NULL) {
				syslog(LOG_ERR, "Could not allocate new " \
						"PIN code");
				exit(1);
			}
			}
		| T_PIN T_NOPIN
			{
			if (key->pin != NULL)
				free(key->pin);

			key->pin = NULL;
			}
		;

%%

/* Display parser error message */
void
yyerror(char const *message)
{
	syslog(LOG_ERR, "%s in line %d", message, yylineno);
}

/* Re-read config file */
void
read_config_file(int s)
{
	extern FILE	*yyin;

	if (config_file == NULL) {
		syslog(LOG_ERR, "Unknown config file name!");
		exit(1);
	}

	if ((yyin = fopen(config_file, "r")) == NULL) {
		syslog(LOG_ERR, "Could not open config file '%s'. %s (%d)",
				config_file, strerror(errno), errno);
		exit(1);
	}

	clean_config();
	if (yyparse() < 0) {
		syslog(LOG_ERR, "Could not parse config file '%s'",config_file);
		exit(1);
	}

	fclose(yyin);
	yyin = NULL;

#if __config_debug__
	dump_config();
#endif
}

/* Clean config */
void
clean_config(void)
{
	link_key_p	key = NULL;

	while ((key = LIST_FIRST(&link_keys)) != NULL) {
		LIST_REMOVE(key, next);
		free_key(key);
	}
}

/* Find link key entry in the list. Return exact or default match */
link_key_p
get_key(bdaddr_p bdaddr, int exact_match)
{
	link_key_p	key = NULL, defkey = NULL;

	LIST_FOREACH(key, &link_keys, next) {
		if (memcmp(bdaddr, &key->bdaddr, sizeof(key->bdaddr)) == 0)
			break;

		if (!exact_match)
			if (memcmp(NG_HCI_BDADDR_ANY, &key->bdaddr,
					sizeof(key->bdaddr)) == 0)
				defkey = key;
	}

	return ((key != NULL)? key : defkey);
}

#if __config_debug__
/* Dump config */
void
dump_config(void)
{
	link_key_p	key = NULL;
	char		buffer[64];

	LIST_FOREACH(key, &link_keys, next) {
		if (key->key != NULL)
			snprintf(buffer, sizeof(buffer),
"0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
				key->key[0], key->key[1], key->key[2],
				key->key[3], key->key[4], key->key[5],
				key->key[6], key->key[7], key->key[8],
				key->key[9], key->key[10], key->key[11],
				key->key[12], key->key[13], key->key[14],
				key->key[15]);

		syslog(LOG_DEBUG, 
"device %s " \
"bdaddr %x:%x:%x:%x:%x:%x " \
"pin %s " \
"key %s",
			(key->name != NULL)? key->name : "noname",
			key->bdaddr.b[5], key->bdaddr.b[4], key->bdaddr.b[3],
			key->bdaddr.b[2], key->bdaddr.b[1], key->bdaddr.b[0],
			(key->pin != NULL)? key->pin : "nopin",
			(key->key != NULL)? buffer : "nokey");
	}
}
#endif

/* Free key entry */
static void
free_key(link_key_p key)
{
	if (key->name != NULL)
		free(key->name);
	if (key->key != NULL)
		free(key->key);
	if (key->pin != NULL)
		free(key->pin);

	memset(key, 0, sizeof(*key));
	free(key);
}

/* Convert hex ASCII to int4 */
static int
hexa2int4(char *a)
{
	if ('0' <= *a && *a <= '9')
		return (*a - '0');

	if ('A' <= *a && *a <= 'F')
		return (*a - 'A' + 0xa);

	if ('a' <= *a && *a <= 'f')
		return (*a - 'a' + 0xa);

	syslog(LOG_ERR, "Invalid hex character: '%c' (%#x)", *a, *a);
	exit(1);
}

/* Convert hex ASCII to int8 */
static int
hexa2int8(char *a)
{
	return ((hexa2int4(a) << 4) | hexa2int4(a + 1));
}

