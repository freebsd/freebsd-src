/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2011, Richard Lowe
 */

#include <alloca.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <sys/systeminfo.h>

int
main(int argc, char **argv)
{
	int i, ac;
	char **av, **p;
	char isaname[16];

	ac = argc + 3;
	av = p = alloca(sizeof (char *) * ac);

	*p++ = "/usr/java/bin/java";
	*p++ = "-jar";
	*p++ = "/opt/SUNWdtrt/lib/java/jdtrace.jar";

	argc--;
	argv++;

	for (i = 0; i < argc; i++) {
		p[i] = argv[i];
	}
	p[i] = NULL;

	if (sysinfo(SI_ARCHITECTURE_64, isaname, sizeof (isaname)) != -1)
		asprintf(av, "/usr/java/bin/%s/java", isaname);

	(void) execv(av[0], av);
	err(1, "exec failed");
}
