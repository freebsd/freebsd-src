/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file sal_list.c
 *
 * @ingroup SalCtrl
 *
 * List implementations for SAL
 *
 *****************************************************************************/

#include "lac_mem.h"
#include "lac_list.h"

CpaStatus
SalList_add(sal_list_t **list, sal_list_t **tail, void *pObj)
{
	sal_list_t *new_element = NULL;

	if (NULL == *list) {
		/* First element in list */
		*list = malloc(sizeof(sal_list_t), M_QAT, M_WAITOK);
		(*list)->next = NULL;
		(*list)->pObj = pObj;
		*tail = *list;
	} else {
		/* add to tail of the list */
		new_element = malloc(sizeof(sal_list_t), M_QAT, M_WAITOK);
		new_element->pObj = pObj;
		new_element->next = NULL;

		(*tail)->next = new_element;

		*tail = new_element;
	}

	return CPA_STATUS_SUCCESS;
}

void *
SalList_getObject(sal_list_t *list)
{
	if (list == NULL) {
		return NULL;
	}

	return list->pObj;
}

void
SalList_delObject(sal_list_t **list)
{
	if (*list == NULL) {
		return;
	}

	(*list)->pObj = NULL;
	return;
}

void *
SalList_next(sal_list_t *list)
{
	return list->next;
}

void
SalList_free(sal_list_t **list)
{
	sal_list_t *next_element = NULL;
	void *pObj = NULL;
	while (NULL != (*list)) {
		next_element = SalList_next(*list);
		pObj = SalList_getObject((*list));
		LAC_OS_FREE(pObj);
		LAC_OS_FREE(*list);
		*list = next_element;
	}
}

void
SalList_del(sal_list_t **head_list, sal_list_t **pre_list, sal_list_t *list)
{
	void *pObj = NULL;
	if ((NULL == *head_list) || (NULL == *pre_list) || (NULL == list)) {
		return;
	}
	if (*head_list == list) { /* delete the first node in list */
		*head_list = list->next;
	} else {
		(*pre_list)->next = list->next;
	}
	pObj = SalList_getObject(list);
	LAC_OS_FREE(pObj);
	LAC_OS_FREE(list);
	return;
}
