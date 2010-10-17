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

/*
 * Increase the size of an array of handles
 */

static bool more_handles(DT_Tdep_Print_Head * phead, DAT_HANDLE ** old_ptrptr,	/* pointer to current pointer   */
			 unsigned int *old_count,	/* number  pointed to           */
			 unsigned int size)
{				/* size of one datum              */
	unsigned int count = *old_count;
	DAT_HANDLE *old_handles = *old_ptrptr;
	DAT_HANDLE *handle_tmp = DT_Mdep_Malloc(count * 2 * size);

	if (!handle_tmp) {
		DT_Tdep_PT_Printf(phead,
				  "Out of memory for more DAT_HANDLEs\n");
		return (false);
	}

	memcpy(handle_tmp, old_handles, count * size);
	DT_Mdep_Free(old_handles);
	*old_ptrptr = handle_tmp;
	*old_count = count * 2;
	return (true);
}

/*
 * Limit test workhorse.
 *
 *  This test creates the sequence of DAT objects needed to move
 *  data back and forth, attempting to find the limits supported
 *  for the DAT object indicated by 'depth'.  For example, if
 *  depth == LIM_LMR, the test will create a set of {IA,PZ,CNO,EVD,EP}
 *  before trying to exhaust LMR creation using the {IA,PZ,CNO,EVD,EP} set.
 *
 *  The 'cmd->width' parameter can be used to control how may of these
 *  parallel DAT object sets we create before stopping to beat upon
 *  the constructor for the object indicated by 'depth', providing for
 *  increased (or at least different) stress on the DAPL.
 */
static bool
limit_test(DT_Tdep_Print_Head * phead, Limit_Cmd_t * cmd, Limit_Index depth)
{
	DAT_EVD_HANDLE conn_handle;
	typedef struct obj_set {
		DAT_IA_HANDLE ia_handle;
		DAT_EVD_HANDLE ia_async_handle;
		DAT_PZ_HANDLE pz_handle;
		DAT_CNO_HANDLE cno_handle;
		DAT_EVD_HANDLE evd_handle;
		DAT_EP_HANDLE ep_handle;
		DAT_LMR_HANDLE lmr_handle;
		char *lmr_buffer;
		DAT_LMR_CONTEXT lmr_context;
		DAT_RMR_HANDLE rmr_handle;
		DAT_RMR_CONTEXT rmr_context;
	} Obj_Set;

	Obj_Set *hdl_sets = (Obj_Set *) NULL;
	bool retval = false;
	char *module = "LimitTest";

#if defined (WIN32)
	/*
	 * The Windows compiler will not deal with complex definitions
	 * in macros, so create a variable here.
	 */
#if defined (DAT_OS_WAIT_PROXY_AGENT_NULL)
#undef DAT_OS_WAIT_PROXY_AGENT_NULL
#endif
	DAT_OS_WAIT_PROXY_AGENT DAT_OS_WAIT_PROXY_AGENT_NULL = { NULL, NULL };
#endif

	DAT_RETURN ret;

#ifdef DFLT_QLEN
#undef DFLT_QLEN
#endif

#   define DFLT_QLEN	  10	/* a small event queue size     */
#   define START_COUNT	1024	/* initial # handles            */
#   define DFLT_BUFFSZ	4096	/* default size for buffer      */
#   define CONN_QUAL0	0xAffab1e

	/* Allocate 'width' Obj_Sets */
	if (depth && !(hdl_sets = DT_Mdep_Malloc(sizeof(Obj_Set) * cmd->width))) {
		DT_Tdep_PT_Printf(phead, "%s: No memory for handle array!\n",
				  module);
		goto clean_up_now;
	}

	/* -----------
	 * IA handling
	 */
	if (depth > LIM_IA) {
		/*
		 * The abuse is not for us this time, just prep Obj_Set.
		 */
		unsigned int w;

		DT_Tdep_PT_Debug(1, (phead,
				     "%s: dat_ia_open X %d\n",
				     module, cmd->width));
		for (w = 0; w < cmd->width; w++) {
			/* Specify that we want to get back an async EVD.  */
			hdl_sets[w].ia_async_handle = DAT_HANDLE_NULL;
			ret = dat_ia_open(cmd->device_name,
					  DFLT_QLEN,
					  &hdl_sets[w].ia_async_handle,
					  &hdl_sets[w].ia_handle);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_ia_open (%s) #%d fails: %s\n",
						  module, cmd->device_name,
						  w + 1, DT_RetToString(ret));
				/* handle contents undefined on failure */
				hdl_sets[w].ia_async_handle = DAT_HANDLE_NULL;
				hdl_sets[w].ia_handle = DAT_HANDLE_NULL;
				goto clean_up_now;
			}
		}
	} else if (depth == LIM_IA) {
		/*
		 * See how many IAs we can create
		 */
		typedef struct _ia {
			DAT_IA_HANDLE ia_handle;
			DAT_EVD_HANDLE ia_async_handle;
		} OneOpen;
		unsigned int count = START_COUNT;
		OneOpen *hdlptr = (OneOpen *)
		    DT_Mdep_Malloc(count * sizeof(*hdlptr));

		/* IA Exhaustion test loop */
		if (hdlptr) {
			unsigned int w = 0;
			unsigned int tmp;

			DT_Tdep_PT_Debug(1,
					 (phead, "%s: Exhausting dat_ia_open\n",
					  module));
			for (w = 0; w < cmd->maximum; w++) {
				DT_Mdep_Schedule();
				if (w == count
				    && !more_handles(phead,
						     (DAT_HANDLE **) & hdlptr,
						     &count, sizeof(*hdlptr))) {
					DT_Tdep_PT_Printf(phead,
							  "%s: IAs opened: %d\n",
							  module, w);
					retval = true;
					break;
				}
				/* Specify that we want to get back an async EVD.  */
				hdlptr[w].ia_async_handle = DAT_HANDLE_NULL;
				ret = dat_ia_open(cmd->device_name,
						  DFLT_QLEN,
						  &hdlptr[w].ia_async_handle,
						  &hdlptr[w].ia_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_ia_open (%s) #%d fails: %s\n",
							  module,
							  cmd->device_name,
							  w + 1,
							  DT_RetToString(ret));
					retval = true;
					break;
				}
			}

			DT_Tdep_PT_Printf(phead, "%s: IAs opened: %d\n", module,
					  w);
			retval = true;

			/* IA Cleanup loop */
			for (tmp = 0; tmp < w; tmp++) {
				DT_Mdep_Schedule();
				ret = dat_ia_close(hdlptr[tmp].ia_handle,
						   DAT_CLOSE_GRACEFUL_FLAG);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_ia_close (graceful) fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
					ret =
					    dat_ia_close(hdlptr[tmp].ia_handle,
							 DAT_CLOSE_ABRUPT_FLAG);
					if (ret != DAT_SUCCESS) {
						DT_Tdep_PT_Printf(phead,
								  "%s: dat_ia_close (abrupt) fails: %s\n",
								  module,
								  DT_RetToString
								  (ret));
					}
				}
			}
			DT_Mdep_Free(hdlptr);
		}
	}

	/* End IA handling */
	/* -----------
	 * PZ handling
	 */
	if (depth > LIM_PZ) {
		/*
		 * The abuse is not for us this time, just prep Obj_Set.
		 */
		unsigned int w;

		DT_Tdep_PT_Debug(1, (phead,
				     "%s: dat_pz_create X %d\n",
				     module, cmd->width));
		for (w = 0; w < cmd->width; w++) {
			ret = dat_pz_create(hdl_sets[w].ia_handle,
					    &hdl_sets[w].pz_handle);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_pz_create #%d fails: %s\n",
						  module, w + 1,
						  DT_RetToString(ret));
				/* handle contents undefined on failure */
				hdl_sets[w].pz_handle = DAT_HANDLE_NULL;
				goto clean_up_now;
			}
		}
	} else if (depth == LIM_PZ) {
		/*
		 * See how many PZs we can create
		 */
		unsigned int count = START_COUNT;
		DAT_PZ_HANDLE *hdlptr = (DAT_PZ_HANDLE *)
		    DT_Mdep_Malloc(count * sizeof(*hdlptr));

		/* PZ Exhaustion test loop */
		if (hdlptr) {
			unsigned int w = 0;
			unsigned int tmp;

			DT_Tdep_PT_Debug(1, (phead,
					     "%s: Exhausting dat_pz_create\n",
					     module));
			for (w = 0; w < cmd->maximum; w++) {
				DT_Mdep_Schedule();
				if (w == count
				    && !more_handles(phead,
						     (DAT_HANDLE **) & hdlptr,
						     &count, sizeof(*hdlptr))) {
					DT_Tdep_PT_Printf(phead,
							  "%s: PZs created: %d\n",
							  module, w);
					retval = true;
					break;
				}
				ret =
				    dat_pz_create(hdl_sets[w % cmd->width].
						  ia_handle, &hdlptr[w]);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_pz_create #%d fails: %s\n",
							  module, w + 1,
							  DT_RetToString(ret));
					retval = true;
					break;
				}
			}

			DT_Tdep_PT_Printf(phead, "%s: PZs created: %d\n",
					  module, w);
			retval = true;

			/* PZ Cleanup loop */
			for (tmp = 0; tmp < w; tmp++) {
				DT_Mdep_Schedule();
				ret = dat_pz_free(hdlptr[tmp]);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_pz_free fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
			}
			DT_Mdep_Free(hdlptr);
		}
	}
	/* End PZ handling */
#ifndef __KDAPLTEST__
	/* -----------
	 * CNO handling
	 */

	if (depth > LIM_CNO) {
		/*
		 * The abuse is not for us this time, just prep Obj_Set.
		 */
		unsigned int w;

		DT_Tdep_PT_Debug(1, (phead,
				     "%s: dat_cno_create X %d\n",
				     module, cmd->width));
		for (w = 0; w < cmd->width; w++) {
			ret = dat_cno_create(hdl_sets[w].ia_handle,
					     DAT_OS_WAIT_PROXY_AGENT_NULL,
					     &hdl_sets[w].cno_handle);
			if (DAT_GET_TYPE(ret) == DAT_NOT_IMPLEMENTED) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_cno_create unimplemented\n",
						  module);
				hdl_sets[w].cno_handle = DAT_HANDLE_NULL;
				/* ignore this error */
				break;
			} else if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_cno_create #%d fails: %s\n",
						  module, w + 1,
						  DT_RetToString(ret));
				/* handle contents undefined on failure */
				hdl_sets[w].cno_handle = DAT_HANDLE_NULL;
				goto clean_up_now;
			}
		}
	} else if (depth == LIM_CNO) {
		/*
		 * See how many CNOs we can create
		 */
		unsigned int count = START_COUNT;
		DAT_CNO_HANDLE *hdlptr = (DAT_CNO_HANDLE *)
		    DT_Mdep_Malloc(count * sizeof(*hdlptr));

		/* CNO Exhaustion test loop */
		if (hdlptr) {
			unsigned int w = 0;
			unsigned int tmp;

			DT_Tdep_PT_Debug(1, (phead,
					     "%s: Exhausting dat_cno_create\n",
					     module));
			for (w = 0; w < cmd->maximum; w++) {
				DT_Mdep_Schedule();
				if (w == count
				    && !more_handles(phead,
						     (DAT_HANDLE **) & hdlptr,
						     &count, sizeof(*hdlptr))) {
					DT_Tdep_PT_Printf(phead,
							  "%s: CNOs created: %d\n",
							  module, w);
					retval = true;
					break;
				}
				ret =
				    dat_cno_create(hdl_sets[w % cmd->width].
						   ia_handle,
						   DAT_OS_WAIT_PROXY_AGENT_NULL,
						   &hdlptr[w]);
				if (DAT_GET_TYPE(ret) == DAT_NOT_IMPLEMENTED) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_cno_create unimplemented\n",
							  module);
					retval = true;
					break;
				} else if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_cno_create #%d fails: %s\n",
							  module, w + 1,
							  DT_RetToString(ret));
					retval = true;
					break;
				}
			}

			DT_Tdep_PT_Printf(phead, "%s: CNOs created: %d\n",
					  module, w);
			retval = true;

			/* CNO Cleanup loop */
			for (tmp = 0; tmp < w; tmp++) {
				DT_Mdep_Schedule();
				ret = dat_cno_free(hdlptr[tmp]);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_cno_free fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
			}
			DT_Mdep_Free(hdlptr);
		}
	}			/* End CNO handling */
#endif				/*  __KDAPLTEST__ */

	/* -----------
	 * EVD handling
	 */
	if (depth > LIM_EVD) {
		/*
		 * The abuse is not for us this time, just prep Obj_Set.
		 */
		unsigned int w = 0;
		DAT_EVD_FLAGS flags = (DAT_EVD_DTO_FLAG
				       /* | DAT_EVD_SOFTWARE_FLAG */
				       | DAT_EVD_CR_FLAG | DAT_EVD_RMR_BIND_FLAG);	/* not ASYNC */

		DT_Tdep_PT_Debug(1, (phead,
				     "%s: dat_evd_create X %d\n",
				     module, cmd->width));
		/*
		 * First create a connection EVD to be used for EP creation
		 */

		ret = DT_Tdep_evd_create(hdl_sets[0].ia_handle,
					 DFLT_QLEN,
					 NULL,
					 DAT_EVD_CONNECTION_FLAG, &conn_handle);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "%s: conn dat_evd_create #%d fails: %s\n",
					  module, w + 1, DT_RetToString(ret));
			/* handle contents undefined on failure */
			conn_handle = DAT_HANDLE_NULL;
			goto clean_up_now;
		}
		for (w = 0; w < cmd->width; w++) {
			ret = DT_Tdep_evd_create(hdl_sets[w].ia_handle,
						 DFLT_QLEN,
						 hdl_sets[w].cno_handle,
						 flags,
						 &hdl_sets[w].evd_handle);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_evd_create #%d fails: %s\n",
						  module, w + 1,
						  DT_RetToString(ret));
				/* handle contents undefined on failure */
				hdl_sets[w].evd_handle = DAT_HANDLE_NULL;
				goto clean_up_now;
			}
		}
	} else if (depth == LIM_EVD) {
		/*
		 * See how many EVDs we can create
		 */
		unsigned int count = START_COUNT;
		DAT_EVD_HANDLE *hdlptr = (DAT_EVD_HANDLE *)
		    DT_Mdep_Malloc(count * sizeof(*hdlptr));
		DAT_EVD_FLAGS flags = (DAT_EVD_DTO_FLAG
				       | DAT_EVD_RMR_BIND_FLAG
				       | DAT_EVD_CR_FLAG);

		/* EVD Exhaustion test loop */
		if (hdlptr) {
			unsigned int w = 0;
			unsigned int tmp;

			DT_Tdep_PT_Debug(1, (phead,
					     "%s: Exhausting dat_evd_create\n",
					     module));
			/*
			 * First create a connection EVD to be used for EP creation
			 */
			ret = DT_Tdep_evd_create(hdl_sets[0].ia_handle,
						 DFLT_QLEN,
						 NULL,
						 DAT_EVD_CONNECTION_FLAG,
						 &conn_handle);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: conn dat_evd_create #%d fails: %s\n",
						  module, w + 1,
						  DT_RetToString(ret));
				/* handle contents undefined on failure */
				conn_handle = DAT_HANDLE_NULL;
			}
			for (w = 0; w < cmd->maximum; w++) {
				DT_Mdep_Schedule();
				if (w == count
				    && !more_handles(phead,
						     (DAT_HANDLE **) & hdlptr,
						     &count, sizeof(*hdlptr))) {
					DT_Tdep_PT_Printf(phead,
							  "%s: EVDs created: %d\n",
							  module, w);
					retval = true;
					break;
				}
				ret =
				    DT_Tdep_evd_create(hdl_sets[w % cmd->width].
						       ia_handle, DFLT_QLEN,
						       hdl_sets[w %
								cmd->width].
						       cno_handle, flags,
						       &hdlptr[w]);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_evd_create #%d fails: %s\n",
							  module, w + 1,
							  DT_RetToString(ret));
					retval = true;
					break;
				}
			}

			DT_Tdep_PT_Printf(phead, "%s: EVDs created: %d\n",
					  module, w);
			retval = true;

			/* EVD Cleanup loop */
			if (conn_handle != DAT_HANDLE_NULL) {
				ret = DT_Tdep_evd_free(conn_handle);
				conn_handle = DAT_HANDLE_NULL;
			}
			for (tmp = 0; tmp < w; tmp++) {
				DT_Mdep_Schedule();
				ret = DT_Tdep_evd_free(hdlptr[tmp]);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_evd_free fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
			}
			DT_Mdep_Free(hdlptr);
		}
	}

	/* End EVD handling */
	/* -----------
	 * EP handling
	 */
	if (depth > LIM_EP) {
		/*
		 * The abuse is not for us this time, just prep Obj_Set.
		 */
		unsigned int w;

		DT_Tdep_PT_Debug(1, (phead,
				     "%s: dat_ep_create X %d\n",
				     module, cmd->width));
		for (w = 0; w < cmd->width; w++) {
			ret = dat_ep_create(hdl_sets[w].ia_handle, hdl_sets[w].pz_handle, hdl_sets[w].evd_handle,	/* recv     */
					    hdl_sets[w].evd_handle,	/* request  */
					    conn_handle,	/* connect  */
					    (DAT_EP_ATTR *) NULL,
					    &hdl_sets[w].ep_handle);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_ep_create #%d fails: %s\n",
						  module, w + 1,
						  DT_RetToString(ret));
				/* handle contents undefined on failure */
				hdl_sets[w].ep_handle = DAT_HANDLE_NULL;
				goto clean_up_now;
			}
		}
	} else if (depth == LIM_EP) {
		/*
		 * See how many EPs we can create
		 */
		unsigned int count = START_COUNT;
		DAT_EP_HANDLE *hdlptr = (DAT_EP_HANDLE *)
		    DT_Mdep_Malloc(count * sizeof(*hdlptr));

		/* EP Exhaustion test loop */
		if (hdlptr) {
			unsigned int w = 0;
			unsigned int tmp;

			DT_Tdep_PT_Debug(1,
					 (phead,
					  "%s: Exhausting dat_ep_create\n",
					  module));
			for (w = 0; w < cmd->maximum; w++) {
				DT_Mdep_Schedule();
				if (w == count
				    && !more_handles(phead,
						     (DAT_HANDLE **) & hdlptr,
						     &count, sizeof(*hdlptr))) {
					DT_Tdep_PT_Printf(phead,
							  "%s: EPs created: %d\n",
							  module, w);
					retval = true;
					break;
				}
				ret = dat_ep_create(hdl_sets[w % cmd->width].ia_handle, hdl_sets[w % cmd->width].pz_handle, hdl_sets[w % cmd->width].evd_handle, hdl_sets[w % cmd->width].evd_handle, conn_handle,	/* connect  */
						    (DAT_EP_ATTR *) NULL,
						    &hdlptr[w]);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_ep_create #%d fails: %s\n",
							  module, w + 1,
							  DT_RetToString(ret));
					retval = true;
					break;
				}
			}

			DT_Tdep_PT_Printf(phead, "%s: EPs created: %d\n",
					  module, w);
			retval = true;

			/* EP Cleanup loop */
			for (tmp = 0; tmp < w; tmp++) {
				DT_Mdep_Schedule();
				ret = dat_ep_free(hdlptr[tmp]);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_ep_free fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
			}
			DT_Mdep_Free(hdlptr);
		}
	}

	/* End EP handling */
	/* -----------
	 * RSP handling
	 *
	 * if (depth > LIM_RSP) {
	 *         Since RSPs are not part of the Obj_Set,
	 *         there's nothing to do.
	 * } else ...
	 */
	if (depth == LIM_RSP) {
		/*
		 * See how many RSPs we can create
		 */
		unsigned int count = START_COUNT;
		DAT_RSP_HANDLE *hdlptr = (DAT_RSP_HANDLE *)
		    DT_Mdep_Malloc(count * sizeof(*hdlptr));
		DAT_EP_HANDLE *epptr = (DAT_EP_HANDLE *)
		    DT_Mdep_Malloc(count * sizeof(*epptr));

		/* RSP Exhaustion test loop */
		if (hdlptr) {
			unsigned int w = 0;
			unsigned int tmp;

			DT_Tdep_PT_Debug(1,
					 (phead,
					  "%s: Exhausting dat_rsp_create\n",
					  module));
			for (w = 0; w < cmd->maximum; w++) {
				DT_Mdep_Schedule();
				if (w == count) {
					unsigned int count1 = count;
					unsigned int count2 = count;

					if (!more_handles
					    (phead, (DAT_HANDLE **) & hdlptr,
					     &count1, sizeof(*hdlptr))) {
						DT_Tdep_PT_Printf(phead,
								  "%s: RSPs created: %d\n",
								  module, w);
						retval = true;
						break;
					}
					if (!more_handles
					    (phead, (DAT_HANDLE **) & epptr,
					     &count2, sizeof(*epptr))) {
						DT_Tdep_PT_Printf(phead,
								  "%s: RSPs created: %d\n",
								  module, w);
						retval = true;
						break;
					}

					if (count1 != count2) {
						DT_Tdep_PT_Printf(phead,
								  "%s: Mismatch in allocation of handle arrays at point %d\n",
								  module, w);
						retval = true;
						break;
					}

					count = count1;
				}

				/*
				 * Each RSP needs a unique EP, so create one first
				 */
				ret =
				    dat_ep_create(hdl_sets[w % cmd->width].
						  ia_handle,
						  hdl_sets[w %
							   cmd->width].
						  pz_handle,
						  hdl_sets[w %
							   cmd->width].
						  evd_handle,
						  hdl_sets[w %
							   cmd->width].
						  evd_handle, conn_handle,
						  (DAT_EP_ATTR *) NULL,
						  &epptr[w]);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_ep_create #%d fails: %s testing RSPs\n",
							  module, w + 1,
							  DT_RetToString(ret));
					retval = true;
					break;
				}

				ret =
				    dat_rsp_create(hdl_sets[w % cmd->width].
						   ia_handle, CONN_QUAL0 + w,
						   epptr[w],
						   hdl_sets[w %
							    cmd->width].
						   evd_handle, &hdlptr[w]);
				if (DAT_GET_TYPE(ret) == DAT_NOT_IMPLEMENTED) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_rsp_create unimplemented\n",
							  module);
					/* ignore this error */
					retval = true;
					break;
				} else if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_rsp_create #%d fails: %s\n",
							  module, w + 1,
							  DT_RetToString(ret));
					/* Cleanup the EP; no-one else will.  */
					ret = dat_ep_free(epptr[w]);
					if (ret != DAT_SUCCESS) {
						DT_Tdep_PT_Printf(phead,
								  "%s: dat_ep_free (internal cleanup @ #%d) fails: %s\n",
								  module, w + 1,
								  DT_RetToString
								  (ret));
					}
					retval = true;
					break;
				}
			}

			DT_Tdep_PT_Printf(phead, "%s: RSPs created: %d\n",
					  module, w);
			retval = true;

			/* RSP Cleanup loop */
			for (tmp = 0; tmp < w; tmp++) {
				DT_Mdep_Schedule();
				ret = dat_rsp_free(hdlptr[tmp]);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_rsp_free fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
				/* Free EPs */
				ret = dat_ep_free(epptr[tmp]);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_ep_free fails: %s for RSPs\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
			}
			DT_Mdep_Free(hdlptr);
		}
	}

	/* End RSP handling */
	/* -----------
	 * PSP handling
	 *
	 * if (depth > LIM_PSP) {
	 *         Since PSPs are not part of the Obj_Set,
	 *         there's nothing to do.
	 * } else ...
	 */
	if (depth == LIM_PSP) {
		/*
		 * See how many PSPs we can create
		 */
		unsigned int count = START_COUNT;
		DAT_PSP_HANDLE *hdlptr = (DAT_PSP_HANDLE *)
		    DT_Mdep_Malloc(count * sizeof(*hdlptr));

		/* PSP Exhaustion test loop */
		if (hdlptr) {
			unsigned int w = 0;
			unsigned int tmp;

			DT_Tdep_PT_Debug(1,
					 (phead,
					  "%s: Exhausting dat_psp_create\n",
					  module));
			for (w = 0; w < cmd->maximum; w++) {
				DT_Mdep_Schedule();
				if (w == count
				    && !more_handles(phead,
						     (DAT_HANDLE **) & hdlptr,
						     &count, sizeof(*hdlptr))) {
					DT_Tdep_PT_Printf(phead,
							  "%s: PSPs created: %d\n",
							  module, w);
					retval = true;
					break;
				}
				ret =
				    dat_psp_create(hdl_sets[w % cmd->width].
						   ia_handle, CONN_QUAL0 + w,
						   hdl_sets[w %
							    cmd->width].
						   evd_handle,
						   DAT_PSP_CONSUMER_FLAG,
						   &hdlptr[w]);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_psp_create #%d fails: %s\n",
							  module, w + 1,
							  DT_RetToString(ret));
					retval = true;
					hdlptr[w] = DAT_HANDLE_NULL;
					break;
				}
			}

			DT_Tdep_PT_Printf(phead, "%s: PSPs created: %d\n",
					  module, w);
			retval = true;

			/* PSP Cleanup loop */
			for (tmp = 0; tmp < w; tmp++) {
				DT_Mdep_Schedule();
				ret = dat_psp_free(hdlptr[tmp]);
				if (DAT_GET_TYPE(ret) == DAT_NOT_IMPLEMENTED) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_psp_free unimplemented\n"
							  "\tNB: Expect EVD+IA cleanup errors!\n",
							  module);
					break;
				} else if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_psp_free fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
			}
			DT_Mdep_Free(hdlptr);
		}
	}

	/* End PSP handling */
	/* -----------
	 * LMR handling
	 */
	if (depth > LIM_LMR) {
		/*
		 * The abuse is not for us this time, just prep Obj_Set.
		 */
		unsigned int w;

		DT_Tdep_PT_Debug(1,
				 (phead, "%s: dat_lmr_create X %d\n", module,
				  cmd->width));
		for (w = 0; w < cmd->width; w++) {
			DAT_REGION_DESCRIPTION region;
			DAT_VLEN reg_size;
			DAT_VADDR reg_addr;

			hdl_sets[w].lmr_buffer = DT_Mdep_Malloc(DFLT_BUFFSZ);
			if (!hdl_sets[w].lmr_buffer) {
				DT_Tdep_PT_Printf(phead,
						  "%s: no memory for LMR buffers\n",
						  module);
				goto clean_up_now;
			}
			memset(&region, 0, sizeof(region));
			region.for_va = hdl_sets[w].lmr_buffer;

			ret = DT_Tdep_lmr_create(hdl_sets[w].ia_handle, DAT_MEM_TYPE_VIRTUAL, region, DFLT_BUFFSZ, hdl_sets[w].pz_handle, DAT_MEM_PRIV_ALL_FLAG, &hdl_sets[w].lmr_handle, &hdl_sets[w].lmr_context, NULL,	/* FIXME */
						 &reg_size, &reg_addr);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_lmr_create #%d fails: %s\n",
						  module, w + 1,
						  DT_RetToString(ret));
				/* handle contents undefined on failure */
				hdl_sets[w].lmr_handle = DAT_HANDLE_NULL;
				goto clean_up_now;
			}
			if ((uintptr_t) reg_addr >
			    (uintptr_t) hdl_sets[w].lmr_buffer
			    || (reg_size <
				DFLT_BUFFSZ + ((uintptr_t) reg_addr -
					       (uintptr_t) hdl_sets[w].
					       lmr_buffer))) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_lmr_create bogus outputs "
						  "in: 0x%p, %x out 0x%llx, %llx\n",
						  module,
						  hdl_sets[w].lmr_buffer,
						  DFLT_BUFFSZ, reg_addr,
						  reg_size);
				goto clean_up_now;
			}
		}
	} else if (depth == LIM_LMR) {
		/*
		 * See how many LMRs we can create
		 */
		unsigned int count = START_COUNT;
		Bpool **hdlptr = (Bpool **)
		    DT_Mdep_Malloc(count * sizeof(*hdlptr));

		/* LMR Exhaustion test loop */
		if (hdlptr) {
			unsigned int w = 0;
			unsigned int tmp;

			DT_Tdep_PT_Debug(1,
					 (phead,
					  "%s: Exhausting dat_lmr_create\n",
					  module));
			for (w = 0; w < cmd->maximum; w++) {
				DT_Mdep_Schedule();
				if (w == count
				    && !more_handles(phead,
						     (DAT_HANDLE **) & hdlptr,
						     &count, sizeof(*hdlptr))) {
					DT_Tdep_PT_Printf(phead,
							  "%s: no memory for LMR handles\n",
							  module);
					DT_Tdep_PT_Printf(phead,
							  "%s: LMRs created: %d\n",
							  module, w);
					retval = true;
					break;
				}
				/*
				 * Let BpoolAlloc do the hard work; this means that
				 * we're testing unique memory registrations rather
				 * than repeatedly binding the same buffer set.
				 */
				hdlptr[w] = DT_BpoolAlloc((Per_Test_Data_t *) 0,
							  phead,
							  hdl_sets[w %
								   cmd->width].
							  ia_handle,
							  hdl_sets[w %
								   cmd->width].
							  pz_handle,
							  hdl_sets[w %
								   cmd->width].
							  ep_handle,
							  hdl_sets[w %
								   cmd->width].
							  evd_handle,
							  DFLT_BUFFSZ, 1,
							  DAT_OPTIMAL_ALIGNMENT,
							  false, false);
				if (!hdlptr[w]) {
					DT_Tdep_PT_Printf(phead,
							  "%s: LMRs created: %d\n",
							  module, w);
					retval = true;
					break;
				}
			}

			DT_Tdep_PT_Printf(phead, "%s: LMRs created: %d\n",
					  module, w);
			retval = true;

			/* LMR Cleanup loop */
			for (tmp = 0; tmp <= w; tmp++) {
				DT_Mdep_Schedule();
				if (hdlptr[tmp]) {
					/* ignore rval - DT_Bpool_Destroy will complain */
					(void)
					    DT_Bpool_Destroy((Per_Test_Data_t *)
							     0, phead,
							     hdlptr[tmp]);
				}
			}
			DT_Mdep_Free(hdlptr);
		}
	}

	/* End LMR handling */
	/* -----------
	 * Posted receive buffer handling
	 */
	if (depth == LIM_RPOST) {
		/*
		 * See how many receive buffers we can post (to each EP).
		 * We are posting the same buffer 'cnt' times, deliberately,
		 * but that should be OK.
		 */
		unsigned int count = START_COUNT;
		DAT_LMR_TRIPLET *hdlptr = (DAT_LMR_TRIPLET *)
		    DT_Mdep_Malloc(count * cmd->width * sizeof(*hdlptr));

		/* Recv-Post Exhaustion test loop */
		if (hdlptr) {
			unsigned int w = 0;
			unsigned int i = 0;
			unsigned int done = 0;

			DT_Tdep_PT_Debug(1,
					 (phead,
					  "%s: Exhausting posting of recv buffers\n",
					  module));
			for (w = 0; w < cmd->maximum && !done; w++) {
				DT_Mdep_Schedule();
				if (w == count
				    && !more_handles(phead,
						     (DAT_HANDLE **) & hdlptr,
						     &count,
						     cmd->width *
						     sizeof(*hdlptr))) {
					DT_Tdep_PT_Printf(phead,
							  "%s: no memory for IOVs \n",
							  module);
					DT_Tdep_PT_Printf(phead,
							  "%s: recv buffers posted per EP: %d\n"
							  "\t\t (total posted: %d)\n",
							  module, w,
							  w * cmd->width);
					done = retval = true;
					break;
				}
				for (i = 0; i < cmd->width; i++) {
					DAT_LMR_TRIPLET *iovp =
					    &hdlptr[w * cmd->width + i];
					DAT_DTO_COOKIE cookie;

					iovp->virtual_address =
					    (DAT_VADDR) (uintptr_t)
					    hdl_sets[i].lmr_buffer;
					iovp->segment_length = DFLT_BUFFSZ;
					iovp->lmr_context =
					    hdl_sets[i].lmr_context;
					cookie.as_64 = (DAT_UINT64) 0UL;
					cookie.as_ptr =
					    (DAT_PVOID) hdl_sets[i].lmr_buffer;

					DT_Tdep_PT_Printf(phead,
							  "%s: dat_ep_post_recv #%d\n",
							  module,
							  w * cmd->width + i +
							  1);
					ret =
					    dat_ep_post_recv(hdl_sets[i].
							     ep_handle, 1, iovp,
							     cookie,
							     DAT_COMPLETION_DEFAULT_FLAG);
					if (ret != DAT_SUCCESS) {
						DT_Tdep_PT_Printf(phead,
								  "%s: dat_ep_post_recv fails: %s\n",
								  module,
								  DT_RetToString
								  (ret));
						DT_Tdep_PT_Printf(phead,
								  "%s: recv buffers posted per EP: %d\n"
								  "\t\t (total posted: %d)\n",
								  module, w,
								  w *
								  cmd->width +
								  i);
						done = retval = true;
						break;
					}
				}	/* end for each EP wide */
			}	/* end forever (!done) loop */

			retval = true;
			DT_Tdep_PT_Printf(phead,
					  "%s: recv buffers posted per EP: %d\n"
					  "\t\t (total posted: %d)\n", module,
					  w, w * cmd->width);

			/* Rpost Cleanup loop */
			for (i = 0; i < cmd->width; i++) {
				DAT_EVENT event;

				/*
				 * Disconnecting an unconnected EP should complete
				 * outstanding recv DTOs in error, and otherwise
				 * be a no-op.
				 */
				ret = dat_ep_reset(hdl_sets[i].ep_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_ep_disconnect (abrupt) fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				} else {
					/*
					 * Remove all DTOs. The disconnect above should have
					 * flushed all posted operations, so this is just a
					 * clean up.
					 */
					do {
						ret =
						    DT_Tdep_evd_dequeue(hdl_sets
									[i].
									evd_handle,
									&event);
					} while (ret == DAT_SUCCESS);
				}

			}
			DT_Mdep_Free(hdlptr);
		}
	}

	/* end depth == LIM_RPOST */
	/* -----------
	 * Test maximum size of LMR allowed
	 */
	if (depth == LIM_SIZE_LMR) {
		DAT_COUNT last_size = 0;
		DAT_COUNT test_size = DFLT_BUFFSZ;
		Bpool *test_bpool;
		for (;;) {
			DT_Mdep_Schedule();
			test_bpool = DT_BpoolAlloc((Per_Test_Data_t *) 0,
						   phead,
						   hdl_sets[0].ia_handle,
						   hdl_sets[0].pz_handle,
						   hdl_sets[0].ep_handle,
						   hdl_sets[0].evd_handle,
						   test_size,
						   1,
						   DAT_OPTIMAL_ALIGNMENT,
						   false, false);

			if (!test_bpool) {
				DT_Tdep_PT_Printf(phead,
						  "%s: Largest LMR was 0x%x bytes\n"
						  "\t (failed attempting 0x%x bytes)\n",
						  module, last_size, test_size);
				retval = true;
				break;
			} else
			    if (!DT_Bpool_Destroy
				((Per_Test_Data_t *) 0, phead, test_bpool)) {
				DT_Tdep_PT_Printf(phead,
						  "%s: Largest LMR was 0x%x bytes\n",
						  module, test_size);
				retval = true;
				break;
			}

			last_size = test_size;
			test_size <<= 1;
			if (test_size < last_size) {
				/* could conceivably wrap on 32-bit architectures */
				DT_Tdep_PT_Printf(phead,
						  "%s: LMR of 0x%x bytes OK - %s\n",
						  module, last_size,
						  "stopping now.");
				retval = true;
				break;
			}
		}		/* end forever loop */
	}
	/* end depth == LIM_SIZE_LMR */
	DT_Tdep_PT_Debug(1, (phead, "%s: Limit Testing Completed - %s\n",
			     module, retval ? "Successfully" : "with errors"));

	/* ----------------------------------------------------------
	 * Clean up and go home
	 */
      clean_up_now:

	DT_Tdep_PT_Debug(1, (phead, "%s: Cleaning up ...\n", module));
	if (depth > LIM_LMR) {
		unsigned int w;

		for (w = 0; w < cmd->width; w++) {
			if (hdl_sets[w].lmr_handle) {
				ret = dat_lmr_free(hdl_sets[w].lmr_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_lmr_free fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
			}
			if ((void *)hdl_sets[w].lmr_buffer) {
				DT_Mdep_Free((void *)hdl_sets[w].lmr_buffer);
			}
		}
	}

	/* end LIM_LMR cleanup */
	/*
	 * if (depth == LIM_PSP) {
	 *         Since PSPs are not part of the Obj_Set,
	 *         there's no cleanup to do.
	 * }
	 *
	 * if (depth == LIM_RSP) {
	 *         Since RSPs are not part of the Obj_Set,
	 *         there'no cleanup nothing to do.
	 * }
	 */
	if (depth > LIM_EP) {
		unsigned int w;

		for (w = 0; w < cmd->width; w++) {
			if (hdl_sets[w].ep_handle) {
				ret = dat_ep_free(hdl_sets[w].ep_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_ep_free fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
			}
		}
	}
	/* end LIM_EP cleanup */
	if (depth > LIM_EVD) {
		unsigned int w;

		if (conn_handle != DAT_HANDLE_NULL) {
			ret = DT_Tdep_evd_free(conn_handle);
			conn_handle = DAT_HANDLE_NULL;
		}
		for (w = 0; w < cmd->width; w++) {
			if (hdl_sets[w].evd_handle) {
				ret = DT_Tdep_evd_free(hdl_sets[w].evd_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_evd_free fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
			}
		}
	}
	/* end LIM_EVD cleanup */
#ifndef __KDAPLTEST__
	if (depth > LIM_CNO) {
		unsigned int w;

		for (w = 0; w < cmd->width; w++) {
			if (hdl_sets[w].cno_handle) {
				ret = dat_cno_free(hdl_sets[w].cno_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_cno_free fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
			}
		}
	}			/* end LIM_CNO cleanup */
#endif

	if (depth > LIM_PZ) {
		unsigned int w;

		for (w = 0; w < cmd->width; w++) {
			if (hdl_sets[w].pz_handle) {
				ret = dat_pz_free(hdl_sets[w].pz_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_pz_free fails: %s\n",
							  module,
							  DT_RetToString(ret));
					retval = false;
				}
			}
		}
	}
	/* end LIM_PZ cleanup */
	if (depth > LIM_IA) {
		unsigned int w;

		for (w = 0; w < cmd->width; w++) {
			if (hdl_sets[w].ia_handle) {
				/* dat_ia_close cleans up async evd handle, too */
				ret = dat_ia_close(hdl_sets[w].ia_handle,
						   DAT_CLOSE_GRACEFUL_FLAG);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_ia_close (graceful) error: %s\n",
							  module,
							  DT_RetToString(ret));
					/*
					 * Since we take some pains to clean up properly,
					 * this really is an error.  But if we get here,
					 * we may as well try the largest hammer we have.
					 */
					retval = false;
					ret =
					    dat_ia_close(hdl_sets[w].ia_handle,
							 DAT_CLOSE_ABRUPT_FLAG);
					if (ret != DAT_SUCCESS) {
						DT_Tdep_PT_Printf(phead,
								  "%s: dat_ia_close (abrupt) error: %s\n",
								  module,
								  DT_RetToString
								  (ret));
					}
				}
			}
		}
	}
	/* end LIM_IA cleanup */
	if (depth && hdl_sets) {
		DT_Mdep_Free(hdl_sets);
	}

	DT_Tdep_PT_Debug(1,
			 (phead, "%s: testing and cleanup complete.\n",
			  module));

	return (retval);
}

/*********************************************************************
 * Framework to run through all of the limit tests
 */
DAT_RETURN DT_cs_Limit(Params_t * params, Limit_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	char *star =
	    "**********************************************************************";

	phead = params->phead;

	if (cmd->Test_List[LIM_IA]) {
		char list[] = {
			"Limitation Test                      limit_ia\n"
			    "Description:  Test max num of opens for the same physical IA"
		}
		;

		DT_Tdep_PT_Printf(phead, "%s\n", star);
		DT_Tdep_PT_Printf(phead, "%s\n", list);
		if (!limit_test(phead, cmd, LIM_IA)) {
			goto error;
		}
		DT_Tdep_PT_Printf(phead, "%s\n", star);
	}

	if (cmd->Test_List[LIM_PZ]) {
		char list[] = {
			"Limitation Test                      limit_pz\n"
			    "Description:  Test max num of PZs that are supported by an IA"
		}
		;

		DT_Tdep_PT_Printf(phead, "%s\n", star);
		DT_Tdep_PT_Printf(phead, "%s\n", list);
		if (!limit_test(phead, cmd, LIM_PZ)) {
			goto error;
		}
		DT_Tdep_PT_Printf(phead, "%s\n", star);
	}
#ifndef __KDAPLTEST__
	if (cmd->Test_List[LIM_CNO]) {
		char list[] = {
			"Limitation Test                      limit_cno\n"
			    "Description:  Test max num of CNOs that are supported by an IA"
		}
		;

		DT_Tdep_PT_Printf(phead, "%s\n", star);
		DT_Tdep_PT_Printf(phead, "%s\n", list);
		if (!limit_test(phead, cmd, LIM_CNO)) {
			goto error;
		}
		DT_Tdep_PT_Printf(phead, "%s\n", star);
	}
#endif

	if (cmd->Test_List[LIM_EVD]) {
		char list[] = {
			"Limitation Test                      limit_evd\n"
			    "Description:  Test max num of EVDs that are supported by an IA"
		}
		;

		DT_Tdep_PT_Printf(phead, "%s\n", star);
		DT_Tdep_PT_Printf(phead, "%s\n", list);
		if (!limit_test(phead, cmd, LIM_EVD)) {
			goto error;
		}
		DT_Tdep_PT_Printf(phead, "%s\n", star);
	}

	if (cmd->Test_List[LIM_EP]) {
		char list[] = {
			"Limitation Test                      limit_ep\n"
			    "Description:  Test max num of EPs that are supported by an IA"
		}
		;

		DT_Tdep_PT_Printf(phead, "%s\n", star);
		DT_Tdep_PT_Printf(phead, "%s\n", list);
		if (!limit_test(phead, cmd, LIM_EP)) {
			goto error;
		}
		DT_Tdep_PT_Printf(phead, "%s\n", star);
	}

	if (cmd->Test_List[LIM_RSP]) {
		char list[] = {
			"Limitation Test                      limit_rsp\n"
			    "Description:  Test max num of RSPs that are supported by an IA"
		}
		;

		DT_Tdep_PT_Printf(phead, "%s\n", star);
		DT_Tdep_PT_Printf(phead, "%s\n", list);
		if (!limit_test(phead, cmd, LIM_RSP)) {
			goto error;
		}
		DT_Tdep_PT_Printf(phead, "%s\n", star);
	}

	if (cmd->Test_List[LIM_PSP]) {
		char list[] = {
			"Limitation Test                      limit_psp\n"
			    "Description:  Test max num of PSPs that are supported by an IA"
		}
		;

		DT_Tdep_PT_Printf(phead, "%s\n", star);
		DT_Tdep_PT_Printf(phead, "%s\n", list);
		if (!limit_test(phead, cmd, LIM_PSP)) {
			goto error;
		}
		DT_Tdep_PT_Printf(phead, "%s\n", star);
	}

	if (cmd->Test_List[LIM_LMR]) {
		char list[] = {
			"Limitation Test                      limit_lmr\n"
			    "Description:  Test max num of LMRs that are supported by an IA"
		}
		;

		DT_Tdep_PT_Printf(phead, "%s\n", star);
		DT_Tdep_PT_Printf(phead, "%s\n", list);
		if (!limit_test(phead, cmd, LIM_LMR)) {
			goto error;
		}
		DT_Tdep_PT_Printf(phead, "%s\n", star);
	}

	if (cmd->Test_List[LIM_RPOST]) {
		char list[] = {
			"Limitation Test                      limit_rpost\n"
			    "Description:  Test max num of receive buffers posted to an EP"
		}
		;

		DT_Tdep_PT_Printf(phead, "%s\n", star);
		DT_Tdep_PT_Printf(phead, "%s\n", list);
		if (!limit_test(phead, cmd, LIM_RPOST)) {
			goto error;
		}
		DT_Tdep_PT_Printf(phead, "%s\n", star);
	}

	if (cmd->Test_List[LIM_SIZE_LMR]) {
		char list[] = {
			"Limitation Test                      limit_size_lmr\n"
			    "Description:  Test max size of LMRs that are supported by an IA"
		}
		;

		DT_Tdep_PT_Printf(phead, "%s\n", star);
		DT_Tdep_PT_Printf(phead, "%s\n", list);
		if (!limit_test(phead, cmd, LIM_SIZE_LMR)) {
			goto error;
		}
		DT_Tdep_PT_Printf(phead, "%s\n", star);
	}

	/* More tests TBS ... */

	return DAT_SUCCESS;

      error:
	DT_Tdep_PT_Printf(phead,
			  "error occurs, can not continue with limit test\n");
	DT_Tdep_PT_Printf(phead, "%s\n", star);
	return DAT_INSUFFICIENT_RESOURCES;
}
