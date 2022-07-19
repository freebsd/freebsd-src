/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 ***************************************************************************
 * @file lac_list.h
 *
 * @defgroup SalList
 *
 * @ingroup SalCtrl
 *
 * List structure and list functions.
 *
 ***************************************************************************/

#ifndef LAC_LIST_H
#define LAC_LIST_H

/**
 *****************************************************************************
 * @ingroup SalList
 *
 * @description
 *      List structure
 *
 *****************************************************************************/
typedef struct sal_list_s {

	struct sal_list_s *next;
	void *pObj;

} sal_list_t;

/**
*******************************************************************************
 * @ingroup SalList
 *      Add a structure to tail of a list.
 *
 * @description
 *      Adds pObj to the tail of  list (if it exists). Allocates and sets a
 *      new sal_list_t structure.
 *
 * @param[in] list                      Pointer to the head pointer of the list.
 *                                      Can be NULL if no elements yet in list.
 * @param[in/out] tail                  Pointer to tail pointer of the list.
 *                                      Can be NULL if no elements yet in list.
 *                                      Is updated by the function to point to
*tail
 *                                      of list if pObj has been successfully
*added.
 * @param[in] pObj                      Pointer to structure to add to tail of
 *                                      the list.
 * @retval status
 *
 *****************************************************************************/
CpaStatus SalList_add(sal_list_t **list, sal_list_t **tail, void *pObj);

/**
*******************************************************************************
 * @ingroup SalList
 *      Delete an element from the list.
 *
 * @description
 *      Delete an element from the list.
 *
 * @param[in/out] head_list             Pointer to the head pointer of the list.
 *                                      Can be NULL if no elements yet in list.
 *                                      Is updated by the function
 *                                      to point to list->next if head_list is
*list.
 * @param[in/out] pre_list              Pointer to the previous pointer of the
*list.
 *                                      Can be NULL if no elements yet in list.
 *                                      (*pre_list)->next is updated
 *                                      by the function to point to list->next
 * @param[in] list                      Pointer to list.
 *
 *****************************************************************************/
void
SalList_del(sal_list_t **head_list, sal_list_t **pre_list, sal_list_t *list);

/**
*******************************************************************************
 * @ingroup SalList
 *      Returns pObj element in list structure.
 *
 * @description
 *      Returns pObj associated with sal_list_t structure.
 *
 * @param[in] list                      Pointer to list element.
 * @retval void*                        pObj member of list structure.
 *
 *****************************************************************************/
void *SalList_getObject(sal_list_t *list);

/**
*******************************************************************************
 * @ingroup SalList
 *      Set pObj to be NULL in the list.
 *
 * @description
 *      Set pObj of a element in the list to be NULL.
 *
 * @param[in] list                      Pointer to list element.
 *
 *****************************************************************************/
void SalList_delObject(sal_list_t **list);

/**
*******************************************************************************
 * @ingroup SalList
 *      Returns next element in list structure.
 *
 * @description
 *      Returns next associated with sal_list_t structure.
 *
 * @param[in] list                      Pointer to list element.
 * @retval void*                        next member of list structure.
 *
 *****************************************************************************/
void *SalList_next(sal_list_t *);

/**
*******************************************************************************
 * @ingroup SalList
 *      Frees memory associated with list structure.
 *
 * @description
 *      Frees memory associated with list structure and the Obj pointed to by
 *      the list.
 *
 * @param[in] list                      Pointer to list.
 *
 *****************************************************************************/
void SalList_free(sal_list_t **);

#endif
