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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * ASSERTION:
 * The D inline translation mechanism can be used to facilitate stable
 * translations.
 *
 * SECTION: Translators/ Translator Declarations
 * SECTION: Translators/ Translate Operator
 * SECTION: Translators/Stable Translations
 *
 * NOTES: Uncomment the pragma that explicitly resets the attributes of
 * myinfo identifier to Stable/Stable/Common from Private/Private/Unknown.
 * Run the program with and without the comments as:
 * /usr/sbin/dtrace -vs man.TestTransStability.d
 */

#pragma D option quiet

inline lwpsinfo_t *myinfo = xlate < lwpsinfo_t *> (curthread);

/*
#pragma D attributes Stable/Stable/Common myinfo
*/

BEGIN
{
	trace(myinfo->pr_flag);
	exit(0);
}

ERROR
{
	exit(1);
}
