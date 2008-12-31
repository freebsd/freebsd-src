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
 *
 * $FreeBSD: src/sys/cddl/compat/opensolaris/kern/opensolaris_cmn_err.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

#include <sys/cmn_err.h>

void
vcmn_err(int ce, const char *fmt, va_list adx)
{
	char buf[256];

	switch (ce) {
	case CE_CONT:
		snprintf(buf, sizeof(buf), "Solaris(cont): %s\n", fmt);
		break;
	case CE_NOTE:
		snprintf(buf, sizeof(buf), "Solaris: NOTICE: %s\n", fmt);
		break;
	case CE_WARN:
		snprintf(buf, sizeof(buf), "Solaris: WARNING: %s\n", fmt);
		break;
	case CE_PANIC:
		snprintf(buf, sizeof(buf), "Solaris(panic): %s\n", fmt);
		break;
	case CE_IGNORE:
		break;
	default:
		panic("Solaris: unknown severity level");
	}
	if (ce == CE_PANIC)
		panic(buf);
	if (ce != CE_IGNORE)
		vprintf(buf, adx);
}

void
cmn_err(int type, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vcmn_err(type, fmt, ap);
	va_end(ap);
}
