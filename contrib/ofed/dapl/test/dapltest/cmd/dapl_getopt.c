/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#include "dapl_proto.h"

#define GETOPT_MAGIC 0x04030201

#define BADCH   (int)'?'
#define BADARG  (int)':'
#define EMSG    ""

/**
 *  Initialize the getopt fields in preparation for parsing
 *  a command line.
 */
void DT_mygetopt_init(mygetopt_t * opts)
{
	opts->magic = GETOPT_MAGIC;
	opts->place = EMSG;
	opts->opterr = 1;
	opts->optind = 1;
	opts->optopt = 0;
	opts->optarg = 0;
}

/**
 *  Parse command line arguments.
 *
 *  Return either the option discovered, or
 *    (int) -1 when there are no more options
 *    (int) '?' when an illegal option is found
 *    (int) ':' when a required option argument is missing.
 */
int
DT_mygetopt_r(int argc, char *const *argv, const char *ostr, mygetopt_t * opts)
{
	char *p;
	char *oli;		/* option letter list index */
	if (GETOPT_MAGIC != opts->magic) {
		DT_Mdep_printf("%s: getopt warning: "
			       "option argument is not properly initialized.\n",
			       argc > 0 ? argv[0] : "unknown command");
		DT_mygetopt_init(opts);
	}
	if (!*(opts->place)) {	/* update scanning pointer */
		if ((opts->optind) >= argc ||
		    *((opts->place) = argv[(opts->optind)]) != '-') {
			(opts->place) = EMSG;
			return (EOF);
		}
		if ((opts->place)[0] != '-') {
			/* Invalid 1st argument */
			return (BADCH);
		}
		if ((opts->place)[1] && *++(opts->place) == '-') {
			/* found "--" which is an invalid option */
			++(opts->optind);
			(opts->place) = EMSG;
			return (BADCH);
		}
	}			/* option letter okay? */
	opts->optopt = (int)*(opts->place)++;
	oli = strchr(ostr, (opts->optopt));
	if (opts->optopt == (int)':' || !oli) {
		/*
		 * if the user didn't specify '-' as an option, assume it means EOF.
		 */
		if ((opts->optopt) == (int)'-') {
			/* return (EOF); */
			return (BADCH);
		}
		if (!*(opts->place)) {
			++(opts->optind);
		}
		if ((opts->opterr) && *ostr != ':') {
			p = strchr(*argv, '/');
			if (!p) {
				p = *argv;
			} else {
				++p;
			}

			if (opts->optopt != '?') {	/* Anything but '?' needs error */
				DT_Mdep_printf("%s: Illegal option -- %c\n",
					       p, (opts->optopt));
			}
		}
		return (BADCH);
	}
	if (*++oli != ':') {	/* don't need argument */
		(opts->optarg) = NULL;
		if (!*(opts->place)) {
			++(opts->optind);
		}
	} else {		/* need an argument */

		if (*(opts->place)) {	/* no white space */
			(opts->optarg) = (opts->place);
		} else {
			if (argc <= ++(opts->optind)) {	/* no arg */
				(opts->place) = EMSG;
				if (*ostr == ':') {
					return (BADARG);
				}
				p = strchr(*argv, '/');
				if (!p) {
					p = *argv;
				} else {
					++p;
				}
				if ((opts->opterr)) {
					DT_Mdep_printf
					    ("%s: option requires an argument -- %c\n",
					     p, (opts->optopt));
				}
				return (BADCH);
			} else {	/* white space */

				(opts->optarg) = argv[(opts->optind)];
			}
		}
		(opts->place) = EMSG;
		++(opts->optind);
	}
	return (opts->optopt);	/* dump back option letter */
}
