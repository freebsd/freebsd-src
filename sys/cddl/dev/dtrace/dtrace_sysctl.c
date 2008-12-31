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
 * $FreeBSD: src/sys/cddl/dev/dtrace/dtrace_sysctl.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

int	dtrace_debug = 0;
TUNABLE_INT("debug.dtrace.debug", &dtrace_debug);
SYSCTL_INT(_debug_dtrace, OID_AUTO, debug, CTLFLAG_RW, &dtrace_debug, 0, "");

/* Report registered DTrace providers. */
static int
sysctl_dtrace_providers(SYSCTL_HANDLER_ARGS)
{
	char	*p_name	= NULL;
	dtrace_provider_t
		*prov	= dtrace_provider;
	int	error	= 0;
	size_t	len	= 0;

	mutex_enter(&dtrace_provider_lock);
	mutex_enter(&dtrace_lock);

	/* Compute the length of the space-separated provider name string. */
	while (prov != NULL) {
		len += strlen(prov->dtpv_name) + 1;
		prov = prov->dtpv_next;
	}

	if ((p_name = kmem_alloc(len, KM_SLEEP)) == NULL)
		error = ENOMEM;
	else {
		/* Start with an empty string. */
		*p_name = '\0';

		/* Point to the first provider again. */
		prov = dtrace_provider;

		/* Loop through the providers, appending the names. */
		while (prov != NULL) {
			if (prov != dtrace_provider)
				(void) strlcat(p_name, " ", len);

			(void) strlcat(p_name, prov->dtpv_name, len);

			prov = prov->dtpv_next;
		}
	}

	mutex_exit(&dtrace_lock);
	mutex_exit(&dtrace_provider_lock);

	if (p_name != NULL) {
		error = sysctl_handle_string(oidp, p_name, len, req);

		kmem_free(p_name, 0);
	}

	return (error);
}

SYSCTL_PROC(_debug_dtrace, OID_AUTO, providers, CTLTYPE_STRING | CTLFLAG_RD,
    0, 0, sysctl_dtrace_providers, "A", "");

