/*	$NetBSD: usbd.c,v 1.4 1998/12/09 00:57:19 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* USBD creates 'threads' in the kernel, used for doing discovery when a
 * device has attached or detached. This functionality should be removed
 * once kernel threads have been added to the kernel.
 * It also handles the event queue, and executing commands based on those
 * events.
 *
 * See usbd(8).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <paths.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <regex.h>

#include <dev/usb/usb.h>

/* default name of configuration file
 */

#define CONFIGFILE	"/etc/usbd.conf"

/* the name of the device spitting out usb attach/detach events as well as
 * the prefix for the individual busses (used as a semi kernel thread).
 */
#define USBDEV		"/dev/usb"

/* Maximum number of USB busses expected to be in a system
 * XXX should be replaced by dynamic allocation.
 */
#define MAXUSBDEV	4

/* Sometimes a device does not respond in time for interrupt
 * driven explore to find it.  Therefore we run an exploration
 * at regular intervals to catch those.
 */
#define TIMEOUT		30

/* The wildcard used in actions for strings and integers
 */
#define WILDCARD_STRING	NULL
#define WILDCARD_INT	-1


extern char *__progname;	/* name of program */

char *configfile = CONFIGFILE;	/* name of configuration file */

char *devs[MAXUSBDEV];		/* device names */
int fds[MAXUSBDEV];		/* file descriptors for USBDEV\d+ */
int ndevs = 0;			/* number of entries in fds / devs */
int fd = -1;			/* file descriptor for USBDEV */

int lineno;
int verbose = 0;		/* print message on what it is doing */

typedef struct event_name_s {
	int	type;		/* event number (from usb.h) */
	char	*name;
} event_name_t;

event_name_t event_names[] = {
	{USB_EVENT_CTRLR_ATTACH, "ctrlr-attach"},
	{USB_EVENT_CTRLR_DETACH, "ctrlr-detach"},
	{USB_EVENT_DRIVER_ATTACH, "driver-attach"},
	{USB_EVENT_DRIVER_DETACH, "driver-detach"},
	{USB_EVENT_DEVICE_ATTACH, "device-attach"},
	{USB_EVENT_DEVICE_DETACH, "device-detach"},
	{0, NULL}			/* NULL indicates end of list, not 0 */
};

#define DEVICE_FIELD		0	/* descriptive field */

#define VENDOR_FIELD		1	/* selective fields */
#define PRODUCT_FIELD		2
#define RELEASE_FIELD		3
#define CLASS_FIELD		4
#define SUBCLASS_FIELD		5
#define PROTOCOL_FIELD		6
#define DEVNAME_FIELD		7

#define ATTACH_FIELD		8	/* command fields */
#define DETACH_FIELD		9


typedef struct action_s {
	char 	*name;		/* descriptive string */

	int	vendor;	/* selection criteria */
	int	product;
	int	release;
	int	class;
	int	subclass;
	int	protocol;
	char 	*devname;
	regex_t	devname_regex;

	char	*attach;	/* commands to execute */
	char	*detach;

	STAILQ_ENTRY(action_s) next;
} action_t;

STAILQ_HEAD(action_list, action_s) actions = STAILQ_HEAD_INITIALIZER(actions);

typedef struct action_match_s {
	action_t *action;
	char	*devname;
} action_match_t;


/* the function returns 0 for failure, 1 for all arguments found and 2 for
 * arguments left over in trail.
 */
typedef int (*config_field_fn)	__P((action_t *action, char *args,
					char **trail));

int set_device_field(action_t *action, char *args, char **trail);
int set_vendor_field(action_t *action, char *args, char **trail);
int set_product_field(action_t *action, char *args, char **trail);
int set_release_field(action_t *action, char *args, char **trail);
int set_class_field(action_t *action, char *args, char **trail);
int set_subclass_field(action_t *action, char *args, char **trail);
int set_protocol_field(action_t *action, char *args, char **trail);
int set_devname_field(action_t *action, char *args, char **trail);
int set_attach_field(action_t *action, char *args, char **trail);
int set_detach_field(action_t *action, char *args, char **trail);

/* the list of fields supported in an entry */
typedef struct config_field_s {
	int	event;
	char 	*name;
	config_field_fn	function;
} config_field_t;

config_field_t config_fields[] = {
	{DEVICE_FIELD,		"device",	set_device_field},

	{VENDOR_FIELD,		"vendor",	set_vendor_field},
	{PRODUCT_FIELD,		"product",	set_product_field},
	{RELEASE_FIELD,		"release",	set_release_field},
	{CLASS_FIELD,		"class",	set_class_field},
	{SUBCLASS_FIELD,	"subclass",	set_subclass_field},
	{PROTOCOL_FIELD,	"protocol",	set_protocol_field},
	{DEVNAME_FIELD,		"devname",	set_devname_field},

	{ATTACH_FIELD,		"attach",	set_attach_field},
	{DETACH_FIELD,		"detach",	set_detach_field},

	{0, NULL, NULL}		/* NULL is EOL marker, not the 0 */
};


/* prototypes for some functions */
void print_event	__P((struct usb_event *event));
void print_action	__P((action_t *action, int i));
void print_actions	__P((void));
int  find_action	__P((struct usb_device_info *devinfo,
			action_match_t *action_match));


void
usage(void)
{
	fprintf(stderr, "usage: %s [-d] [-v] [-t timeout] [-e] [-f dev]\n"
			"           [-n] [-c config]\n",
		__progname);
	exit(1);
}


/* generic helper functions for the functions to set the fields of actions */
int
get_string(char *src, char **rdst, char **rsrc)
{
	/* Takes the first string from src, taking quoting into account.
	 * rsrc (if not NULL) is set to the first byte not included in the
	 * string returned in rdst.
	 *
	 * Input is:
	 *   src = 'fir"st \'par"t       second part';
	 * Returned is:
	 *   *dst = 'hello \'world';
	 *   if (rsrc != NULL)
	 *     *rsrc = 'second part';
	 *
	 * Notice the fact that the single quote enclosed in double quotes is
	 * returned. Also notice that before second part there is more than
	 * one space, which is removed in rsrc.
	 *
	 * The string in src is not modified.
	 */

	char *dst;		/* destination string */
	int i;			/* index into src */
	int j;			/* index into dst */
	int quoted = 0;		/* 1 for single, 2 for double quoted */

	dst = malloc(strlen(src)+1);	/* XXX allocation is too big, realloc?*/
	if (dst == NULL) {		/* should not happen, really */
		fprintf(stderr, "%s:%d: Out of memory\n", configfile, lineno);
		exit(2);
	}

	/* find the end of the current string. If quotes are found the search
	 * continues until the corresponding quote is found.
	 * So,
	 *   hel'lo" "wor'ld
	 * represents the string
	 *   hello" "world
	 * and not (hello world).
	 */
	for (i = 0, j = 0; i < strlen(src); i++) {
		if (src[i] == '\'' && (quoted == 0 || quoted == 1)) {
			quoted = (quoted? 0:1);
		} else if (src[i] == '"' && (quoted == 0 || quoted == 2)) {
			quoted = (quoted? 0:2);
		} else if (isspace(src[i]) && !quoted) {
			/* found a space outside quotes -> terminates src */
			break;
		} else {
			dst[j++] = src[i];	/* copy character */
		}
	}

	/* quotes being left open? */
	if (quoted) {
		fprintf(stderr, "%s:%d: Missing %s quote at end of '%s'\n",
			configfile, lineno,
			(quoted == 1? "single":"double"), src);
		exit(2);
	}

	/* skip whitespace for second part */
	for (/*i is set*/; i < strlen(src) && isspace(src[i]); i++)
		;	/* nop */

	dst[j] = '\0';			/* make sure it's NULL terminated */

	*rdst = dst;			/* and return the pointers */
	if (rsrc != NULL)		/* if info wanted */
		*rsrc = &src[i];

	if (*dst == '\0') {		/* empty string */
		return 0;
	} else if (src[i] == '\0') {	/* completely used (1 argument) */
		return 1;
	} else {			/* 2 or more args, *rsrc is rest */
		return 2;
	}
}

int
get_integer(char *src, int *dst, char **rsrc)
{
	char *endptr;

	/* Converts str to a number. If one argument was found in
	 * str, 1 is returned and *dst is set to the value of the integer.
	 * If 2 or more arguments were presented, 2 is returned,
	 * *dst is set to the converted value and rsrc, if not null, points
	 * at the start of the next argument (whitespace skipped).
	 * Else 0 is returned and nothing else is valid.
	 */

	if (src == NULL || *src == '\0')	/* empty src */
		return(0);

	*dst = (int) strtol(src, &endptr, 0);

	/* skip over whitespace of second argument */
	while (isspace(*endptr))
		endptr++;

	if (rsrc)
		*rsrc = endptr;

	if (isspace(*endptr)) {		/* partial match, 2 or more arguments */
		return(2);
	} else if (*endptr == '\0') {	/* full match, 1 argument */
		return(1);
	} else {			/* invalid src, no match */
		return(0);
	}
}

/* functions to set the fields of the actions appropriately */
int
set_device_field(action_t *action, char *args, char **trail)
{
	return(get_string(args, &action->name, trail));
}
int
set_vendor_field(action_t *action, char *args, char **trail)
{
	return(get_integer(args, &action->vendor, trail));
}
int
set_product_field(action_t *action, char *args, char **trail)
{
	return(get_integer(args, &action->product, trail));
}
int
set_release_field(action_t *action, char *args, char **trail)
{
	return(get_integer(args, &action->release, trail));
}
int
set_class_field(action_t *action, char *args, char **trail)
{
	return(get_integer(args, &action->class, trail));
}
int
set_subclass_field(action_t *action, char *args, char **trail)
{
	return(get_integer(args, &action->subclass, trail));
}
int
set_protocol_field(action_t *action, char *args, char **trail)
{
	return(get_integer(args, &action->protocol, trail));
}
int
set_devname_field(action_t *action, char *args, char **trail)
{
	int match = get_string(args, &action->devname, trail);
	int len;
	int error;
	char *string;
#	define ERRSTR_SIZE	100
	char errstr[ERRSTR_SIZE];

	if (match == 0)
		return(0);

	len = strlen(action->devname);
	string = malloc(len + 15);
	if (string == NULL)
		return(0);

	bcopy(action->devname, string+7, len);	/* make some space for */
	bcopy("[[:<:]]", string, 7);		/*   beginning of word */
	bcopy("[[:>:]]", string+7+len, 7);	/*   and end of word   */
	string[len + 14] = '\0';

	error = regcomp(&action->devname_regex, string, REG_NOSUB|REG_EXTENDED);
	if (error) {
		errstr[0] = '\0';
		regerror(error, &action->devname_regex, errstr, ERRSTR_SIZE);
		fprintf(stderr, "%s:%d: %s\n", configfile, lineno, errstr);
		return(0);
	}

	return(match);
}
int
set_attach_field(action_t *action, char *args, char **trail)
{
	return(get_string(args, &action->attach, trail));
}
int
set_detach_field(action_t *action, char *args, char **trail)
{
	return(get_string(args, &action->detach, trail));
}


void
read_configuration(void)
{
	FILE *file;		/* file descriptor */
	char *line;		/* current line */
	char *linez;		/* current line, NULL terminated */
	char *field;		/* first part, the field name */
	char *args;		/* second part, arguments */
	char *trail;		/* remaining part after parsing, should be '' */
	int len;		/* length of current line */
	int i,j;		/* loop counters */
	action_t *action = NULL;	/* current action */

	file = fopen(configfile, "r");
	if (file == NULL) {
		fprintf(stderr, "%s: Could not open for reading, %s\n",
			configfile, strerror(errno));
		exit(2);
	}

	for (lineno = 1; /* nop */;lineno++) {
	
		line = fgetln(file, &len);
		if (line == NULL) {
			if (feof(file))			/* EOF */
				break;
			if (ferror(file)) {
				fprintf(stderr, "%s:%d: Could not read, %s\n",
					configfile, lineno, strerror(errno));
				exit(2);
			}
		}

		/* skip initial spaces */
		while (len > 0 && isspace(*line)) {
			line++;
			len--;
		}

		if (len == 0)		/* empty line */
			continue;
		if (line[0] == '#')	/* comment line */
			continue;

		/* make a NULL terminated copy of the string */
		linez = malloc(len+1);
		if (linez == NULL) {
			fprintf(stderr, "%s:%d: Out of memory\n",
				configfile, lineno);
			exit(2);
		}
		strncpy(linez, line, len);
		linez[len] = '\0';

		/* find the end of the current word (is field), that's the
		 * start of the arguments
		 */
		field = linez;
		args = linez;
		while (*args != '\0' && !isspace(*args))
			args++;

		/* If arguments is not the empty string, NULL terminate the
		 * field and move the argument pointer to the first character
		 * of the arguments.
		 * If arguments is the empty string field and arguments both
		 * are terminated (strlen(field) >= 0, strlen(arguments) == 0).
		 */
		if (*args != '\0') {
			*args = '\0';
			args++;
		}

		/* Skip initial spaces */
		while (*args != '\0' && isspace(*args))
			args++;

		/* Cut off trailing whitespace */
		for (i = 0, j = 0; args[i] != '\0'; i++)
			if (!isspace(args[i]))
				j = i+1;
		args[j] = '\0';

		/* We now have the field and the argument separated into
		 * two strings that are NULL terminated
		 */

		/* If the field is 'device' we have to start a new action. */
		if (strcmp(field, "device") == 0) {
			/* Allocate a new action and set defaults */
			action = malloc(sizeof(*action));
			if (action == NULL) {
				fprintf(stderr, "%s:%d: Out of memory\n",
					configfile, lineno);
				exit(2);
			}
			memset(action, 0, sizeof(*action));
			action->product = WILDCARD_INT;
			action->vendor = WILDCARD_INT;
			action->release = WILDCARD_INT;
			action->class = WILDCARD_INT;
			action->subclass = WILDCARD_INT;
			action->protocol = WILDCARD_INT;
			action->devname = WILDCARD_STRING;

			/* Add it to the end of the list to preserve order */
			STAILQ_INSERT_TAIL(&actions, action, next);
		}

		if (action == NULL) {
			line[len] = '\0';	/* XXX zero terminate */
			fprintf(stderr, "%s:%d: Doesn't start with 'device' "
				"but '%s'\n", configfile, lineno, field);
			exit(2);
		}
		
		for (i = 0; config_fields[i].name  ; i++) {
			/* does the field name match? */
			if (strcmp(config_fields[i].name, field) == 0) {
				/* execute corresponding set-field function */
				if ((config_fields[i].function)(action, args,
								&trail)
				    != 1) {
					fprintf(stderr,"%s:%d: "
						"Syntax error in '%s'\n",
						configfile, lineno, linez);
					exit(2);
				}
				break;
			}
		}
		if (config_fields[i].name == NULL) {	/* Reached end of list*/
			fprintf(stderr, "%s:%d: Unknown field '%s'\n",
				configfile, lineno, field);
			exit(2);
		}
	}

	fclose(file);

	if (verbose >= 2)
		print_actions();
}


void
print_event(struct usb_event *event)
{
	int i;
	struct timespec *timespec = &event->ue_time;
	struct usb_device_info *devinfo = &event->u.ue_device;

	printf("%s: ", __progname);
	for (i = 0; event_names[i].name != NULL; i++) {
		if (event->ue_type == event_names[i].type) {
			printf("%s event", event_names[i].name);
			break;
		}
	}
	if (event_names[i].name == NULL)
		printf("unknown event %d", event->ue_type);

	if (event->ue_type == USB_EVENT_DEVICE_ATTACH ||
	    event->ue_type == USB_EVENT_DEVICE_DETACH) {
		devinfo = &event->u.ue_device;

		printf(" at %ld.%09ld, %s, %s:\n",
			timespec->tv_sec, timespec->tv_nsec,
			devinfo->udi_product, devinfo->udi_vendor);

		printf("  vndr=0x%04x prdct=0x%04x rlse=0x%04x "
		       "clss=0x%04x subclss=0x%04x prtcl=0x%04x\n",
		       devinfo->udi_vendorNo, devinfo->udi_productNo,
		       devinfo->udi_releaseNo,
		       devinfo->udi_class, devinfo->udi_subclass, devinfo->udi_protocol);

		if (devinfo->udi_devnames[0][0] != '\0') {
			char c = ' ';

			printf("  device names:");
			for (i = 0; i < USB_MAX_DEVNAMES; i++) {
				if (devinfo->udi_devnames[i][0] == '\0')
					break;

				printf("%c%s", c, devinfo->udi_devnames[i]);
				c = ',';
			}
		}
	} else if (event->ue_type == USB_EVENT_CTRLR_ATTACH ||
	    event->ue_type == USB_EVENT_CTRLR_DETACH) {
		printf(" bus=%d", &event->u.ue_ctrlr.ue_bus);
	} else if (event->ue_type == USB_EVENT_DRIVER_ATTACH ||
	    event->ue_type == USB_EVENT_DRIVER_DETACH) {
		printf(" cookie=%u devname=%s",
		    &event->u.ue_driver.ue_cookie.cookie,
		    &event->u.ue_driver.ue_devname);
	}
	printf("\n");
}

void
print_action(action_t *action, int i)
{
	if (action == NULL)
		return;

	printf("%s: action %d: %s\n",
		__progname, i,
		(action->name? action->name:""));
	if (action->product != WILDCARD_INT ||
	    action->vendor != WILDCARD_INT ||
	    action->release != WILDCARD_INT ||
	    action->class != WILDCARD_INT ||
	    action->subclass != WILDCARD_INT ||
	    action->protocol != WILDCARD_INT)
		printf(" ");
	if (action->vendor != WILDCARD_INT)
		printf(" vndr=0x%04x", action->vendor);
	if (action->product != WILDCARD_INT)
		printf(" prdct=0x%04x", action->product);
	if (action->release != WILDCARD_INT)
		printf(" rlse=0x%04x", action->release);
	if (action->class != WILDCARD_INT)
		printf(" clss=0x%04x", action->class);
	if (action->subclass != WILDCARD_INT)
		printf(" subclss=0x%04x", action->subclass);
	if (action->protocol != WILDCARD_INT)
		printf(" prtcl=0x%04x", action->protocol);
	if (action->vendor != WILDCARD_INT ||
	    action->product != WILDCARD_INT ||
	    action->release != WILDCARD_INT ||
	    action->class != WILDCARD_INT ||
	    action->subclass != WILDCARD_INT ||
	    action->protocol != WILDCARD_INT)
		printf("\n");
	if (action->devname != WILDCARD_STRING)
		printf("  devname: %s\n", action->devname);

	if (action->attach != NULL)
		printf("  attach='%s'\n",
			action->attach);
	if (action->detach != NULL)
		printf("  detach='%s'\n",
			action->detach);
}

void
print_actions()
{
	int i = 0;
	action_t *action;

	STAILQ_FOREACH(action, &actions, next)
		print_action(action, ++i);

	printf("%s: %d action%s\n", __progname, i, (i == 1? "":"s"));
}


int
match_devname(action_t *action, struct usb_device_info *devinfo)
{
	int i;
	regmatch_t match;
	int error;

	for (i = 0; i < USB_MAX_DEVNAMES; i++) {
		if (devinfo->udi_devnames[i][0] == '\0')
			break;

		error = regexec(&action->devname_regex, devinfo->udi_devnames[i],
				1, &match, 0);
		if (error == 0) {
			if (verbose >= 2)
				printf("%s: %s matches %s\n", __progname,
					devinfo->udi_devnames[i], action->devname);
			return(i);
		}
	}
	
	return(-1);
}


int
find_action(struct usb_device_info *devinfo, action_match_t *action_match)
{
	action_t *action;
	char *devname = NULL;
	int match = -1;

	STAILQ_FOREACH(action, &actions, next) {
		if ((action->vendor == WILDCARD_INT ||
		     action->vendor == devinfo->udi_vendorNo) &&
		    (action->product == WILDCARD_INT ||
		     action->product == devinfo->udi_productNo) &&
		    (action->release == WILDCARD_INT ||
		     action->release == devinfo->udi_releaseNo) &&
		    (action->class == WILDCARD_INT ||
		     action->class == devinfo->udi_class) &&
		    (action->subclass == WILDCARD_INT ||
		     action->subclass == devinfo->udi_subclass) &&
		    (action->protocol == WILDCARD_INT ||
		     action->protocol == devinfo->udi_protocol) &&
		    (action->devname == WILDCARD_STRING ||
		     (match = match_devname(action, devinfo)) != -1)) {
			/* found match !*/

			/* Find a devname for pretty printing. Either
			 * the matched one or otherwise, if there is only
			 * one devname for that device, use that.
			 */
			if (match >= 0)
				devname = devinfo->udi_devnames[match];
			else if (devinfo->udi_devnames[0][0] != '\0' &&
				 devinfo->udi_devnames[1][0] == '\0')
				/* if we have exactly 1 device name */
				devname = devinfo->udi_devnames[0];

			if (verbose) {
				printf("%s: Found action '%s' for %s, %s",
					__progname, action->name,
					devinfo->udi_product, devinfo->udi_vendor);
				if (devname)
					printf(" at %s", devname);
				printf("\n");
			}

			action_match->action = action;
			action_match->devname = devname;

			return(1);
		}
	}

	return(0);
}

void
execute_command(char *cmd)
{
	pid_t pid;
	struct sigaction ign, intact, quitact;
	sigset_t newsigblock, oldsigblock;
	int status;
	int i;

	if (verbose)
		printf("%s: Executing '%s'\n", __progname, cmd);
	if (cmd == NULL)
		return;

	/* The code below is directly taken from the system(3) call.
	 * Added to it is the closing of open file descriptors.
	 */
	/*
	 * Ignore SIGINT and SIGQUIT, block SIGCHLD. Remember to save
	 * existing signal dispositions.
	 */
	ign.sa_handler = SIG_IGN;
	(void) sigemptyset(&ign.sa_mask);
	ign.sa_flags = 0;
	(void) sigaction(SIGINT, &ign, &intact);
	(void) sigaction(SIGQUIT, &ign, &quitact);
	(void) sigemptyset(&newsigblock);
	(void) sigaddset(&newsigblock, SIGCHLD);
	(void) sigprocmask(SIG_BLOCK, &newsigblock, &oldsigblock);
	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "%s: fork failed, %s\n",
			__progname, strerror(errno));
	} else if (pid == 0) {
		/* child here */

		/* close all open file handles for USBDEV\d* devices */
		for (i = 0; i < ndevs; i++)
			close(fds[i]);		/* USBDEV\d+ */
		close(fd);			/* USBDEV */

		/* Restore original signal dispositions and exec the command. */
		(void) sigaction(SIGINT, &intact, NULL);
		(void) sigaction(SIGQUIT,  &quitact, NULL);
		(void) sigprocmask(SIG_SETMASK, &oldsigblock, NULL);

		execl(_PATH_BSHELL, "sh", "-c", cmd, (char *)NULL);

		/* should only be reached in case of error */
		exit(127);
	} else {
		/* parent here */
		do {
			pid = waitpid(pid, &status, 0);
		} while (pid == -1 && errno == EINTR);
	}
	(void) sigaction(SIGINT, &intact, NULL);
	(void) sigaction(SIGQUIT,  &quitact, NULL);
	(void) sigprocmask(SIG_SETMASK, &oldsigblock, NULL);

	if (pid == -1) {
		fprintf(stderr, "%s: waitpid returned: %s\n",
			__progname, strerror(errno));
	} else if (pid == 0) {
		fprintf(stderr, "%s: waitpid returned 0 ?!\n",
			__progname);
	} else {
		if (status == -1) {
			fprintf(stderr, "%s: Could not start '%s'\n",
				__progname, cmd);
		} else if (status == 127) {
			fprintf(stderr, "%s: Shell failed for '%s'\n",
				__progname, cmd);
		} else if (WIFEXITED(status) && WEXITSTATUS(status)) {
			fprintf(stderr, "%s: '%s' returned %d\n",
				__progname, cmd, WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: '%s' caught signal %d\n",
				__progname, cmd, WTERMSIG(status));
		} else if (verbose >= 2) {
			printf("%s: '%s' is ok\n", __progname, cmd);
		}
	}
}

void
process_event_queue(int fd)
{
	struct usb_event event;
	int error;
	int len;
	action_match_t action_match;

	for (;;) {
		len = read(fd, &event, sizeof(event));
		if (len == -1) {
			if (errno == EWOULDBLOCK) {
				/* no more events */
				break;
			} else {
				fprintf(stderr,"%s: Could not read event, %s\n",
					__progname, strerror(errno));
				exit(1);
			}
		}
		if (len == 0)
			break;
		if (len != sizeof(event)) {
			fprintf(stderr, "partial read on %s\n", USBDEV);
			exit(1);
		}

		/* we seem to have gotten a valid event */

		if (verbose)
			print_event(&event);

		/* handle the event appropriately */
		switch (event.ue_type) {
		case USB_EVENT_CTRLR_ATTACH:
			if (verbose)
				printf("USB_EVENT_CTRLR_ATTACH\n");
			break;
		case USB_EVENT_CTRLR_DETACH:
			if (verbose)
				printf("USB_EVENT_CTRLR_DETACH\n");
			break;
		case USB_EVENT_DEVICE_ATTACH:
		case USB_EVENT_DEVICE_DETACH:
			if (find_action(&event.u.ue_device, &action_match) == 0)
				/* nothing found */
				break;

			if (verbose >= 2)
				print_action(action_match.action, 0);

			if (action_match.devname) {
				if (verbose >= 2)
					printf("%s: Setting DEVNAME='%s'\n",
						__progname, action_match.devname);

				error = setenv("DEVNAME", action_match.devname, 1);
				if (error)
					fprintf(stderr, "%s: setenv(\"DEVNAME\", \"%s\",1) failed, %s\n",
						__progname, action_match.devname, strerror(errno));
			}

			if (USB_EVENT_IS_ATTACH(event.ue_type) &&
			    action_match.action->attach) 
				execute_command(action_match.action->attach);
			if (USB_EVENT_IS_DETACH(event.ue_type) &&
			    action_match.action->detach)
				execute_command(action_match.action->detach);
			break;
		case USB_EVENT_DRIVER_ATTACH:
			if (verbose)
				printf("USB_EVENT_DRIVER_DETACH\n");
			break;
		case USB_EVENT_DRIVER_DETACH:
			if (verbose)
				printf("USB_EVENT_DRIVER_DETACH\n");
			break;
		default:
			printf("Unknown USB event %d\n", event.ue_type);
		}
	}	
}


int
main(int argc, char **argv)
{
	int error, i;
	int ch;			/* getopt option */
	int debug = 0;		/* print debugging output */
	int explore_once = 0;	/* don't do only explore */
	int handle_events = 1;	/* do handle the event queue */
	int maxfd;		/* maximum fd in use */
	char buf[50];		/* for creation of the filename */
	fd_set r,w;
	int itimeout = TIMEOUT;	/* timeout for select */
	struct timeval tv;

	if (modfind(USB_UHUB) < 0) {
		if (kldload(USB_KLD) < 0 || modfind(USB_UHUB) < 0) {
			perror(USB_KLD ": Kernel module not available");
			return 1;
		}
	}

	while ((ch = getopt(argc, argv, "c:def:nt:v")) != -1) {
		switch(ch) {
		case 'c':
			configfile = strdup(optarg);
			if (configfile == NULL) {
				fprintf(stderr, "strdup returned NULL\n");
				return 1;
			}
			break;
		case 'd':
			debug++;
			break;
		case 'e':
			explore_once = 1;
			break;
		case 'f':
			if (ndevs < MAXUSBDEV)
				devs[ndevs++] = optarg;
			break;
		case 'n':
			handle_events = 0;
			break;
		case 't':
			itimeout = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	maxfd = 0;
	if (ndevs == 0) {
		/* open all the USBDEVS\d+ devices */
		for (i = 0; i < MAXUSBDEV; i++) {
			sprintf(buf, "%s%d", USBDEV, i);
			fds[ndevs] = open(buf, O_RDWR);
			if (fds[ndevs] >= 0) {
				devs[ndevs] = strdup(buf);
				if (devs[ndevs] == NULL) {
					fprintf(stderr, "strdup returned NULL\n");
					return 1;
				}
				if (verbose)
					printf("%s: opened %s\n", 
					       __progname, devs[ndevs]);
				if (fds[ndevs] > maxfd)
					maxfd = fds[ndevs];
				ndevs++;
			} else if (errno != ENXIO && errno != ENOENT) {
				/* there was an error, on a device that does
				 * exist (device is configured)
				 */
				fprintf(stderr, "%s: Could not open %s, %s\n",
					__progname, buf, strerror(errno));
				exit(1);
			}
		}
	} else {
		/* open all the files specified with -f */
		for (i = 0; i < ndevs; i++) {
			fds[i] = open(devs[i], O_RDWR);
			if (fds[i] < 0) {
				fprintf(stderr, "%s: Could not open %s, %s\n",
					__progname, devs[i], strerror(errno));
				exit(1);
			} else {
				if (verbose)
					printf("%s: opened %s\n", 
					       __progname, devs[i]);
				if (fds[i] > maxfd)
					maxfd = fds[i];
			}
		}
	}

	if (ndevs == 0) {
		fprintf(stderr, "No USB host controllers found\n");
		exit(1);
	}


	/* Do the explore once and exit */
	if (explore_once) {
		for (i = 0; i < ndevs; i++) {
			error = ioctl(fds[i], USB_DISCOVER);
			if (error < 0) {
				fprintf(stderr, "%s: ioctl(%s, USB_DISCOVER) "
					"failed, %s\n",
					__progname, devs[i], strerror(errno));
				exit(1);
			}
		}
		exit(0);
	}

	if (handle_events) {
		if (verbose)
			printf("%s: reading configuration file %s\n",
				__progname, configfile);
		read_configuration();

		fd = open(USBDEV, O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			fprintf(stderr, "%s: Could not open %s, %s\n",
				__progname, USBDEV, strerror(errno));
			exit(1);
		}
		if (verbose)
			printf("%s: opened %s\n", __progname, USBDEV);
		if (fd > maxfd)
			maxfd = fd;

		process_event_queue(fd);	/* dequeue the initial events */
	}

	/* move to the background */
	if (!debug)
		daemon(0, 0);

	/* start select on all the open file descriptors */
	for (;;) {
		FD_ZERO(&r);
		FD_ZERO(&w);
		if (handle_events)
			FD_SET(fd, &r);		/* device USBDEV */
		for (i = 0; i < ndevs; i++)
			FD_SET(fds[i], &w);	/* device USBDEV\d+ */
		tv.tv_usec = 0;
		tv.tv_sec = itimeout;
		error = select(maxfd+1, &r, &w, 0, itimeout ? &tv : 0);
		if (error < 0) {
			fprintf(stderr, "%s: Select failed, %s\n",
				__progname, strerror(errno));
			exit(1);
		}

		/* USBDEV\d+ devices have signaled change, do a usb_discover */
		for (i = 0; i < ndevs; i++) {
			if (error == 0 || FD_ISSET(fds[i], &w)) {
				if (verbose >= 2)
					printf("%s: doing %sdiscovery on %s\n", 
					       __progname,
					       (error? "":"timeout "), devs[i]);
				if (ioctl(fds[i], USB_DISCOVER) < 0) {
					fprintf(stderr, "%s: ioctl(%s, "
						"USB_DISCOVER) failed, %s\n",
						__progname, devs[i],
						strerror(errno));
					exit(1);
				}
			}
		}

		/* check the event queue */
		if (handle_events && (FD_ISSET(fd, &r) || error == 0)) {
			if (verbose >= 2)
				printf("%s: processing event queue %son %s\n",
					__progname,
				       (error? "":"due to timeout "), USBDEV);
			process_event_queue(fd);
		}
	}
}
