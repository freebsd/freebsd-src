/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>

#include "mthca.h"

#define MTHCA_FREE_MAP_SIZE (MTHCA_DB_REC_PER_PAGE / (SIZEOF_LONG * 8))

struct mthca_db_page {
	unsigned long		free[MTHCA_FREE_MAP_SIZE];
	struct mthca_buf	db_rec;
};

struct mthca_db_table {
	int 	       	     npages;
	int 	       	     max_group1;
	int 	       	     min_group2;
	pthread_mutex_t      mutex;
	struct mthca_db_page page[];
};

int mthca_alloc_db(struct mthca_db_table *db_tab, enum mthca_db_type type,
		   uint32_t **db)
{
	int i, j, k;
	int group, start, end, dir;
	int ret = 0;

	pthread_mutex_lock(&db_tab->mutex);

	switch (type) {
	case MTHCA_DB_TYPE_CQ_ARM:
	case MTHCA_DB_TYPE_SQ:
		group = 0;
		start = 0;
		end   = db_tab->max_group1;
		dir   = 1;
		break;

	case MTHCA_DB_TYPE_CQ_SET_CI:
	case MTHCA_DB_TYPE_RQ:
	case MTHCA_DB_TYPE_SRQ:
		group = 1;
		start = db_tab->npages - 1;
		end   = db_tab->min_group2;
		dir   = -1;
		break;

	default:
		ret = -1;
		goto out;
	}

	for (i = start; i != end; i += dir)
		if (db_tab->page[i].db_rec.buf)
			for (j = 0; j < MTHCA_FREE_MAP_SIZE; ++j)
				if (db_tab->page[i].free[j])
					goto found;

	if (db_tab->max_group1 >= db_tab->min_group2 - 1) {
		ret = -1;
		goto out;
	}

	if (mthca_alloc_buf(&db_tab->page[i].db_rec,
			    MTHCA_DB_REC_PAGE_SIZE,
			    MTHCA_DB_REC_PAGE_SIZE)) {
		ret = -1;
		goto out;
	}

	memset(db_tab->page[i].db_rec.buf, 0, MTHCA_DB_REC_PAGE_SIZE);
	memset(db_tab->page[i].free, 0xff, sizeof db_tab->page[i].free);

	if (group == 0)
		++db_tab->max_group1;
	else
		--db_tab->min_group2;

found:
	for (j = 0; j < MTHCA_FREE_MAP_SIZE; ++j) {
		k = ffsl(db_tab->page[i].free[j]);
		if (k)
			break;
	}

	if (!k) {
		ret = -1;
		goto out;
	}

	--k;
	db_tab->page[i].free[j] &= ~(1UL << k);

	j = j * SIZEOF_LONG * 8 + k;
	if (group == 1)
		j = MTHCA_DB_REC_PER_PAGE - 1 - j;

	ret = i * MTHCA_DB_REC_PER_PAGE + j;
	*db = db_tab->page[i].db_rec.buf + j * 8;

out:
	pthread_mutex_unlock(&db_tab->mutex);
	return ret;
}

void mthca_set_db_qn(uint32_t *db, enum mthca_db_type type, uint32_t qn)
{
	db[1] = htonl((qn << 8) | (type << 5));
}

void mthca_free_db(struct mthca_db_table *db_tab, enum mthca_db_type type, int db_index)
{
	int i, j;
	struct mthca_db_page *page;

	i = db_index / MTHCA_DB_REC_PER_PAGE;
	j = db_index % MTHCA_DB_REC_PER_PAGE;

	page = db_tab->page + i;

	pthread_mutex_lock(&db_tab->mutex);
	*(uint64_t *) (page->db_rec.buf + j * 8) = 0;

	if (i >= db_tab->min_group2)
		j = MTHCA_DB_REC_PER_PAGE - 1 - j;

	page->free[j / (SIZEOF_LONG * 8)] |= 1UL << (j % (SIZEOF_LONG * 8));

	pthread_mutex_unlock(&db_tab->mutex);
}

struct mthca_db_table *mthca_alloc_db_tab(int uarc_size)
{
	struct mthca_db_table *db_tab;
	int npages;
	int i;

	npages = uarc_size / MTHCA_DB_REC_PAGE_SIZE;
	db_tab = malloc(sizeof (struct mthca_db_table) +
			npages * sizeof (struct mthca_db_page));

	pthread_mutex_init(&db_tab->mutex, NULL);

	db_tab->npages     = npages;
	db_tab->max_group1 = 0;
	db_tab->min_group2 = npages - 1;

	for (i = 0; i < npages; ++i)
		db_tab->page[i].db_rec.buf = NULL;

	return db_tab;
}

void mthca_free_db_tab(struct mthca_db_table *db_tab)
{
	int i;

	if (!db_tab)
		return;

	for (i = 0; i < db_tab->npages; ++i)
		if (db_tab->page[i].db_rec.buf)
			mthca_free_buf(&db_tab->page[i].db_rec);

	free(db_tab);
}
