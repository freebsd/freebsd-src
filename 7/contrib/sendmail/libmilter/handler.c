/*
 *  Copyright (c) 1999-2003, 2006 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: handler.c,v 8.39 2008/11/25 01:14:16 ca Exp $")

#include "libmilter.h"

#if !_FFR_WORKERS_POOL
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
	**  Detach so resources are free when the thread returns.
	**  If we ever "wait" for threads, this call must be removed.
	*/

	if (pthread_detach(ctx->ctx_id) != 0)
		ret = MI_FAILURE;
	else
		ret = mi_engine(ctx);
	mi_clr_ctx(ctx);
	ctx = NULL;
	return ret;
}
#endif /* !_FFR_WORKERS_POOL */
