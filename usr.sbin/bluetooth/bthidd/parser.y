%{
/*
 * parser.y
 *
 * Copyright (c) 2004 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * $Id: parser.y,v 1.3 2004/02/13 21:46:21 max Exp $
 * $FreeBSD$
 */

#include <sys/queue.h>
#include <bluetooth.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <usbhid.h>

#ifndef BTHIDCONTROL
#include <stdarg.h>
#include <syslog.h>
#define	SYSLOG		syslog
#define	LOGCRIT		LOG_CRIT
#define	LOGERR		LOG_ERR
#define	LOGWARNING	LOG_WARNING
#define	EOL
#else
#define	SYSLOG		fprintf
#define	LOGCRIT		stderr
#define	LOGERR		stderr
#define	LOGWARNING	stderr
#define	EOL		"\n"
#endif /* ndef BTHIDCONTROL */

#include "bthid_config.h"

	int	yyparse		(void);
	int	yylex		(void);
static	int	check_hid_device(hid_device_p hid_device);
static	void	free_hid_device	(hid_device_p hid_device);

extern	int			 yylineno;
	char			*config_file = BTHIDD_CONFFILE;
	char			*hids_file   = BTHIDD_HIDSFILE;

static	char			 buffer[1024];
static	int			 hid_descriptor_size;
static	hid_device_t		*hid_device = NULL;
static	LIST_HEAD(, hid_device)	 hid_devices;

%}

%union {
	bdaddr_t	bdaddr;
	int		num;
}

%token <bdaddr> T_BDADDRSTRING
%token <num>	T_HEXBYTE
%token T_DEVICE T_BDADDR T_CONTROL_PSM T_INTERRUPT_PSM T_RECONNECT_INITIATE
%token T_BATTERY_POWER T_NORMALLY_CONNECTABLE T_HID_DESCRIPTOR
%token T_TRUE T_FALSE T_ERROR

%%

config:		line
		| config line
		;

line:		T_DEVICE
			{
			hid_device = (hid_device_t *) calloc(1, sizeof(*hid_device));
			if (hid_device == NULL) {
				SYSLOG(LOGCRIT, "Could not allocate new " \
						"config entry" EOL);
				YYABORT;
			}

			hid_device->new_device = 1;
			}
		'{' options '}'
			{
			if (check_hid_device(hid_device))
				LIST_INSERT_HEAD(&hid_devices,hid_device,next);
			else
				free_hid_device(hid_device);

			hid_device = NULL;
			}
		;

options:	option ';'
		| options option ';'
		;

option:		bdaddr
		| control_psm
		| interrupt_psm
		| reconnect_initiate
		| battery_power
		| normally_connectable
		| hid_descriptor
		| parser_error
		;

bdaddr:		T_BDADDR T_BDADDRSTRING
			{
			memcpy(&hid_device->bdaddr, &$2, sizeof(hid_device->bdaddr));
			}
		;

control_psm:	T_CONTROL_PSM T_HEXBYTE
			{
			hid_device->control_psm = $2;
			}
		;

interrupt_psm:	T_INTERRUPT_PSM T_HEXBYTE
			{
			hid_device->interrupt_psm = $2;
			}
		;

reconnect_initiate: T_RECONNECT_INITIATE T_TRUE
			{
			hid_device->reconnect_initiate = 1;
			}
		| T_RECONNECT_INITIATE T_FALSE
			{
			hid_device->reconnect_initiate = 0;
			}
		;

battery_power:	T_BATTERY_POWER T_TRUE
			{
			hid_device->battery_power = 1;
			}
		| T_BATTERY_POWER T_FALSE
			{
			hid_device->battery_power = 0;
			}
		;

normally_connectable: T_NORMALLY_CONNECTABLE T_TRUE
			{
			hid_device->normally_connectable = 1;
			}
		| T_NORMALLY_CONNECTABLE T_FALSE
			{
			hid_device->normally_connectable = 0;
			}
		;

hid_descriptor:	T_HID_DESCRIPTOR	
			{
			hid_descriptor_size = 0;
			}
		'{' hid_descriptor_bytes '}'
			{
			if (hid_device->desc != NULL)
				hid_dispose_report_desc(hid_device->desc);

			hid_device->desc = hid_use_report_desc(buffer, hid_descriptor_size);
			if (hid_device->desc == NULL) {
				SYSLOG(LOGCRIT, "Could not use HID descriptor" EOL);
				YYABORT;
			}
			}
		;

hid_descriptor_bytes: hid_descriptor_byte
		| hid_descriptor_bytes hid_descriptor_byte
		;

hid_descriptor_byte: T_HEXBYTE
			{
			if (hid_descriptor_size >= sizeof(buffer)) {
				SYSLOG(LOGCRIT, "HID descriptor is too big" EOL);
				YYABORT;
			}

			buffer[hid_descriptor_size ++] = $1;
			}
		;

parser_error:	T_ERROR
			{
				YYABORT;
			}

%%

/* Display parser error message */
void
yyerror(char const *message)
{
	SYSLOG(LOGERR, "%s in line %d" EOL, message, yylineno); 
}

/* Re-read config file */
int
read_config_file(void)
{
	extern FILE	*yyin;
	int		 e;

	if (config_file == NULL) {
		SYSLOG(LOGERR, "Unknown config file name!" EOL);
		return (-1);
	}

	if ((yyin = fopen(config_file, "r")) == NULL) {
		SYSLOG(LOGERR, "Could not open config file '%s'. %s (%d)" EOL,
				config_file, strerror(errno), errno);
		return (-1);
	}

	clean_config();
	if (yyparse() < 0) {
		SYSLOG(LOGERR, "Could not parse config file '%s'" EOL,
				config_file);
		e = -1;
	} else
		e = 0;

	fclose(yyin);
	yyin = NULL;

	return (e);
}

/* Clean config */
void
clean_config(void)
{
	while (!LIST_EMPTY(&hid_devices)) {
		hid_device_p	hid_device = LIST_FIRST(&hid_devices);

		LIST_REMOVE(hid_device, next);
		free_hid_device(hid_device);
	}
}

/* Lookup config entry */
hid_device_p
get_hid_device(bdaddr_p bdaddr)
{
	hid_device_p	hid_device;

	LIST_FOREACH(hid_device, &hid_devices, next)
		if (memcmp(&hid_device->bdaddr, bdaddr, sizeof(bdaddr_t)) == 0)
			break;

	return (hid_device);
}

/* Get next config entry */
hid_device_p
get_next_hid_device(hid_device_p d)
{
	return ((d == NULL)? LIST_FIRST(&hid_devices) : LIST_NEXT(d, next));
}

/* Print config entry */
void
print_hid_device(hid_device_p hid_device, FILE *f)
{
	/* XXX FIXME hack! */
	struct report_desc {
		unsigned int	size;
		unsigned char	data[1];
	};
	/* XXX FIXME hack! */

	struct report_desc	*desc = (struct report_desc *) hid_device->desc;
	int			 i;

	fprintf(f,
"device {\n"					\
"	bdaddr			%s;\n"		\
"	control_psm		0x%x;\n"	\
"	interrupt_psm		0x%x;\n"	\
"	reconnect_initiate	%s;\n"		\
"	battery_power		%s;\n"		\
"	normally_connectable	%s;\n"		\
"	hid_descriptor		{",
		bt_ntoa(&hid_device->bdaddr, NULL),
		hid_device->control_psm, hid_device->interrupt_psm,
                hid_device->reconnect_initiate? "true" : "false",
                hid_device->battery_power? "true" : "false",
                hid_device->normally_connectable? "true" : "false");
 
	for (i = 0; i < desc->size; i ++) {
			if ((i % 8) == 0)
				fprintf(f, "\n		");
 
			fprintf(f, "0x%2.2x ", desc->data[i]);
	}
                
	fprintf(f,
"\n"		\
"	};\n"	\
"}\n");
}

/* Check config entry */
static int
check_hid_device(hid_device_p hid_device)
{
	if (get_hid_device(&hid_device->bdaddr) != NULL) {
		SYSLOG(LOGERR, "Ignoring duplicated entry for bdaddr %s" EOL,
				bt_ntoa(&hid_device->bdaddr, NULL));
		return (0);
	}

	if (hid_device->control_psm == 0) {
		SYSLOG(LOGERR, "Ignoring entry with invalid control PSM" EOL);
		return (0);
	}

	if (hid_device->interrupt_psm == 0) {
		SYSLOG(LOGERR, "Ignoring entry with invalid interrupt PSM" EOL);
		return (0);
	}

	if (hid_device->desc == NULL) {
		SYSLOG(LOGERR, "Ignoring entry without HID descriptor" EOL);
		return (0);
	}

	return (1);
}

/* Free config entry */
static void
free_hid_device(hid_device_p hid_device)
{
	if (hid_device->desc != NULL)
		hid_dispose_report_desc(hid_device->desc);

	memset(hid_device, 0, sizeof(*hid_device));
	free(hid_device);
}

/* Re-read hids file */
int
read_hids_file(void)
{
	FILE		*f = NULL;
	hid_device_t	*hid_device = NULL;
	char		*line = NULL;
	bdaddr_t	 bdaddr;
	int		 lineno;

	if (hids_file == NULL) {
		SYSLOG(LOGERR, "Unknown HIDs file name!" EOL);
		return (-1);
	}

	if ((f = fopen(hids_file, "r")) == NULL) {
		if (errno == ENOENT)
			return (0);

		SYSLOG(LOGERR, "Could not open HIDs file '%s'. %s (%d)" EOL,
			hids_file, strerror(errno), errno);
		return (-1);
	}

	for (lineno = 1; fgets(buffer, sizeof(buffer), f) != NULL; lineno ++) {
		if ((line = strtok(buffer, "\r\n\t ")) == NULL)
			continue; /* ignore empty lines */

		if (!bt_aton(line, &bdaddr)) {
			SYSLOG(LOGWARNING, "Ignoring unparseable BD_ADDR in " \
				"%s:%d" EOL, hids_file, lineno);
			continue;
		}

		if ((hid_device = get_hid_device(&bdaddr)) != NULL)
			hid_device->new_device = 0;
	}

	fclose(f);

	return (0);
}

/* Write hids file */
int
write_hids_file(void)
{
	char		 path[PATH_MAX];
	FILE		*f = NULL;
	hid_device_t	*hid_device = NULL;

	if (hids_file == NULL) {
		SYSLOG(LOGERR, "Unknown HIDs file name!" EOL);
		return (-1);
	}

	snprintf(path, sizeof(path), "%s.new", hids_file);

	if ((f = fopen(path, "w")) == NULL) {
		SYSLOG(LOGERR, "Could not open HIDs file '%s'. %s (%d)" EOL,
			path, strerror(errno), errno);
		return (-1);
	}

	LIST_FOREACH(hid_device, &hid_devices, next)
		if (!hid_device->new_device)
			fprintf(f, "%s\n", bt_ntoa(&hid_device->bdaddr, NULL));

	fclose(f);

	if (rename(path, hids_file) < 0) {
		SYSLOG(LOGERR, "Could not rename new HIDs file '%s' to '%s'. " \
			"%s (%d)" EOL, path, hids_file, strerror(errno), errno);
		unlink(path);
		return (-1);
	}

	return (0);
}

