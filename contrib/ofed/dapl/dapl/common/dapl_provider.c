/*
 * Copyright (c) 2002-2003, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
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

/**********************************************************************
 *
 * MODULE: dapl_provider.c
 *
 * PURPOSE: Provider function table
 * Description: DAT Interfaces to this provider
 *
 * $Id:$
 **********************************************************************/

#include "dapl_provider.h"

extern DAT_RETURN dapl_not_implemented(void);

/*********************************************************************
 *                                                                   *
 * Global Data                                                       *
 *                                                                   *
 *********************************************************************/

DAPL_PROVIDER_LIST g_dapl_provider_list;

/*
 * the function table for this provider
 */

#if defined(__KDAPL__)
DAT_PROVIDER g_dapl_provider_template = {
	NULL,
	0,
	&dapl_ia_open,
	&dapl_ia_query,
	&dapl_ia_close,
	&dapl_ia_memtype_hint,	/* unimplemented */

	&dapl_set_consumer_context,	/* untested */
	&dapl_get_consumer_context,	/* untested */
	&dapl_get_handle_type,	/* untested */

	&dapl_cr_query,
	&dapl_cr_accept,
	&dapl_cr_reject,
	&dapl_cr_handoff,

	&dapl_evd_kcreate,
	&dapl_evd_kquery,	/* untested */
	&dapl_evd_modify_upcall,
	&dapl_evd_resize,	/* unimplemented */
	&dapl_evd_post_se,	/* untested */
	&dapl_evd_dequeue,
	&dapl_evd_free,

	&dapl_ep_create,
	&dapl_ep_query,
	&dapl_ep_modify,	/* untested */
	&dapl_ep_connect,
	&dapl_ep_dup_connect,	/* untested */
	&dapl_ep_disconnect,
	&dapl_ep_post_send,
	&dapl_ep_post_recv,
	&dapl_ep_post_rdma_read,
	&dapl_ep_post_rdma_write,
	&dapl_ep_get_status,
	&dapl_ep_free,

	&dapl_lmr_kcreate,
	&dapl_lmr_query,
	&dapl_lmr_free,

	&dapl_rmr_create,
	&dapl_rmr_query,	/* untested */
	&dapl_rmr_bind,
	&dapl_rmr_free,

	&dapl_psp_create,
	&dapl_psp_query,	/* untested */
	&dapl_psp_free,

	&dapl_rsp_create,
	&dapl_rsp_query,	/* untested */
	&dapl_rsp_free,

	&dapl_pz_create,
	&dapl_pz_query,		/* untested */
	&dapl_pz_free,

	/* dat-1.1 */
	&dapl_psp_create_any,
	&dapl_ep_reset,

	/* dat-1.2 */
	&dapl_lmr_sync_rdma_read,
	&dapl_lmr_sync_rdma_write,

	&dapl_ep_create_with_srq,
	&dapl_ep_recv_query,
	&dapl_ep_set_watermark,
	&dapl_srq_create,
	&dapl_srq_free,
	&dapl_srq_post_recv,
	&dapl_srq_query,
	&dapl_srq_resize,
	&dapl_srq_set_lw
};
#else
/*
 * uDAPL version of the provider jump table
 */
DAT_PROVIDER g_dapl_provider_template = {
	NULL,
	0,
	&dapl_ia_open,
	&dapl_ia_query,
	&dapl_ia_close,

	&dapl_set_consumer_context,
	&dapl_get_consumer_context,
	&dapl_get_handle_type,

	&dapl_cno_create,
	&dapl_cno_modify_agent,
	&dapl_cno_query,
	&dapl_cno_free,
	&dapl_cno_wait,

	&dapl_cr_query,
	&dapl_cr_accept,
	&dapl_cr_reject,
	&dapl_cr_handoff,

	&dapl_evd_create,
	&dapl_evd_query,
	&dapl_evd_modify_cno,
	&dapl_evd_enable,
	&dapl_evd_disable,
	&dapl_evd_wait,
	&dapl_evd_resize,
	&dapl_evd_post_se,
	&dapl_evd_dequeue,
	&dapl_evd_free,

	&dapl_ep_create,
	&dapl_ep_query,
	&dapl_ep_modify,
	&dapl_ep_connect,
	&dapl_ep_dup_connect,
	&dapl_ep_disconnect,
	&dapl_ep_post_send,
	&dapl_ep_post_recv,
	&dapl_ep_post_rdma_read,
	&dapl_ep_post_rdma_write,
	&dapl_ep_get_status,
	&dapl_ep_free,

	&dapl_lmr_create,
	&dapl_lmr_query,
	&dapl_lmr_free,

	&dapl_rmr_create,
	&dapl_rmr_query,
	&dapl_rmr_bind,
	&dapl_rmr_free,

	&dapl_psp_create,
	&dapl_psp_query,
	&dapl_psp_free,

	&dapl_rsp_create,
	&dapl_rsp_query,
	&dapl_rsp_free,

	&dapl_pz_create,
	&dapl_pz_query,
	&dapl_pz_free,

	&dapl_psp_create_any,
	&dapl_ep_reset,
	&dapl_evd_set_unwaitable,
	&dapl_evd_clear_unwaitable,

	/* dat-1.2 */
	&dapl_lmr_sync_rdma_read,
	&dapl_lmr_sync_rdma_write,

	&dapl_ep_create_with_srq,
	&dapl_ep_recv_query,
	&dapl_ep_set_watermark,
	&dapl_srq_create,
	&dapl_srq_free,
	&dapl_srq_post_recv,
	&dapl_srq_query,
	&dapl_srq_resize,
	&dapl_srq_set_lw,

	/* dat-2.0 */
	&dapl_csp_create,
	&dapl_csp_query,
	&dapl_csp_free,
	&dapl_ep_common_connect,
	&dapl_rmr_create_for_ep,
	&dapl_ep_post_send_with_invalidate,
	&dapl_ep_post_rdma_read_to_rmr,
	&dapl_cno_fd_create,
	&dapl_cno_trigger,
	&dapl_ia_ha
#ifdef DAT_EXTENSIONS
	    , &dapl_extensions
#endif
};
#endif				/* __KDAPL__ */

/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

static DAT_BOOLEAN
dapl_provider_list_key_cmp(const char *name_a, const char *name_b);

/*********************************************************************
 *                                                                   *
 * Function Definitions                                              *
 *                                                                   *
 *********************************************************************/

DAT_RETURN dapl_provider_list_create(void)
{
	DAT_RETURN status;

	status = DAT_SUCCESS;

	/* create the head node */
	g_dapl_provider_list.head =
	    dapl_os_alloc(sizeof(DAPL_PROVIDER_LIST_NODE));
	if (NULL == g_dapl_provider_list.head) {
		status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dapl_os_memzero(g_dapl_provider_list.head,
			sizeof(DAPL_PROVIDER_LIST_NODE));

	/* create the tail node */
	g_dapl_provider_list.tail =
	    dapl_os_alloc(sizeof(DAPL_PROVIDER_LIST_NODE));
	if (NULL == g_dapl_provider_list.tail) {
		status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dapl_os_memzero(g_dapl_provider_list.tail,
			sizeof(DAPL_PROVIDER_LIST_NODE));

	g_dapl_provider_list.head->next = g_dapl_provider_list.tail;
	g_dapl_provider_list.tail->prev = g_dapl_provider_list.head;
	g_dapl_provider_list.size = 0;

      bail:
	if (DAT_SUCCESS != status) {
		if (NULL != g_dapl_provider_list.head) {
			dapl_os_free(g_dapl_provider_list.head,
				     sizeof(DAPL_PROVIDER_LIST_NODE));
		}

		if (NULL != g_dapl_provider_list.tail) {
			dapl_os_free(g_dapl_provider_list.tail,
				     sizeof(DAPL_PROVIDER_LIST_NODE));
		}
	}

	return status;
}

DAT_RETURN dapl_provider_list_destroy(void)
{
	DAPL_PROVIDER_LIST_NODE *cur_node;

	while (NULL != g_dapl_provider_list.head) {
		cur_node = g_dapl_provider_list.head;
		g_dapl_provider_list.head = cur_node->next;

		dapl_os_free(cur_node, sizeof(DAPL_PROVIDER_LIST_NODE));
	}

	return DAT_SUCCESS;
}

DAT_COUNT dapl_provider_list_size(void)
{
	return g_dapl_provider_list.size;
}

DAT_RETURN
dapl_provider_list_insert(IN const char *name, IN DAT_PROVIDER ** p_data)
{
	DAPL_PROVIDER_LIST_NODE *cur_node, *prev_node, *next_node;
	DAT_RETURN status;
	unsigned int len;

	status = DAT_SUCCESS;

	cur_node = dapl_os_alloc(sizeof(DAPL_PROVIDER_LIST_NODE));

	if (NULL == cur_node) {
		status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	len = dapl_os_strlen(name);

	if (DAT_NAME_MAX_LENGTH <= len) {
		status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	/* insert node at end of list to preserve registration order */
	prev_node = g_dapl_provider_list.tail->prev;
	next_node = g_dapl_provider_list.tail;

	dapl_os_memcpy(cur_node->name, name, len);
	cur_node->name[len] = '\0';
	cur_node->data = g_dapl_provider_template;
	cur_node->data.device_name = cur_node->name;

	cur_node->next = next_node;
	cur_node->prev = prev_node;

	prev_node->next = cur_node;
	next_node->prev = cur_node;

	g_dapl_provider_list.size++;

	if (NULL != p_data) {
		*p_data = &cur_node->data;
	}

      bail:
	if (DAT_SUCCESS != status) {
		if (NULL != cur_node) {
			dapl_os_free(cur_node, sizeof(DAPL_PROVIDER_LIST_NODE));
		}
	}

	return status;
}

DAT_RETURN
dapl_provider_list_search(IN const char *name, OUT DAT_PROVIDER ** p_data)
{
	DAPL_PROVIDER_LIST_NODE *cur_node;
	DAT_RETURN status;

	status = DAT_ERROR(DAT_PROVIDER_NOT_FOUND, DAT_NAME_NOT_REGISTERED);

	for (cur_node = g_dapl_provider_list.head->next;
	     g_dapl_provider_list.tail != cur_node; cur_node = cur_node->next) {
		if (dapl_provider_list_key_cmp(cur_node->name, name)) {
			if (NULL != p_data) {
				*p_data = &cur_node->data;
			}

			status = DAT_SUCCESS;
			goto bail;
		}
	}

      bail:
	return status;
}

DAT_RETURN dapl_provider_list_remove(IN const char *name)
{
	DAPL_PROVIDER_LIST_NODE *cur_node, *prev_node, *next_node;
	DAT_RETURN status;

	status = DAT_ERROR(DAT_PROVIDER_NOT_FOUND, DAT_NAME_NOT_REGISTERED);

	for (cur_node = g_dapl_provider_list.head->next;
	     g_dapl_provider_list.tail != cur_node; cur_node = cur_node->next) {
		if (dapl_provider_list_key_cmp(cur_node->name, name)) {
			prev_node = cur_node->prev;
			next_node = cur_node->next;

			prev_node->next = next_node;
			next_node->prev = prev_node;

			dapl_os_free(cur_node, sizeof(DAPL_PROVIDER_LIST_NODE));

			g_dapl_provider_list.size--;

			status = DAT_SUCCESS;
			goto bail;
		}
	}

      bail:
	return status;
}

DAT_BOOLEAN dapl_provider_list_key_cmp(const char *name_a, const char *name_b)
{
	unsigned int len;

	len = dapl_os_strlen(name_a);

	if (dapl_os_strlen(name_b) != len) {
		return DAT_FALSE;
	} else if (dapl_os_memcmp(name_a, name_b, len)) {
		return DAT_FALSE;
	} else {
		return DAT_TRUE;
	}
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
