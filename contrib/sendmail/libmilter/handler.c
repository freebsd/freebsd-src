/*
 *  Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: handler.c,v 8.19.4.2 2000/07/14 06:16:57 msk Exp $";
#endif /* ! lint */

#if _FFR_MILTER
#include "libmilter.h"


/*
**  HANDLE_SESSION -- Handle a connected session in its own context
**
**	Parameters:
**		ctx -- context structure
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

int
mi_handle_session(ctx)
	SMFICTX_PTR ctx;
{
	int ret;

	if (ctx == NULL)
		return MI_FAILURE;
	ctx->ctx_id = (sthread_t) sthread_get_id();

	/*
	**  detach so resources are free when the thread returns
	**  if we ever "wait" for threads, this call must be removed
	*/
	if (pthread_detach(ctx->ctx_id) != 0)
		return MI_FAILURE;
	ret = mi_engine(ctx);
	if (ValidSocket(ctx->ctx_sd))
		(void) close(ctx->ctx_sd);
	if (ctx->ctx_reply != NULL)
		free(ctx->ctx_reply);
	if (ctx->ctx_privdata != NULL)
	{
		smi_log(SMI_LOG_WARN,
			"%s: private data not NULL",
			ctx->ctx_smfi->xxfi_name);
	}
	mi_clr_macros(ctx, 0);
	free(ctx);
	ctx = NULL;
	return ret;
}
#endif /* _FFR_MILTER */
