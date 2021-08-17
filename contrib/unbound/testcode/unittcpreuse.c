/*
 * testcode/unittcpreuse.c - unit test for tcp_reuse.
 *
 * Copyright (c) 2021, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/**
 * \file
 * Tests the tcp_reuse functionality.
 */

#include "config.h"
#include "testcode/unitmain.h"
#include "util/log.h"
#include "util/random.h"
#include "services/outside_network.h"

/** add number of new IDs to the reuse tree, randomly chosen */
static void tcpid_addmore(struct reuse_tcp* reuse,
	struct outside_network* outnet, unsigned int addnum)
{
	unsigned int i;
	struct waiting_tcp* w;
	for(i=0; i<addnum; i++) {
		uint16_t id = reuse_tcp_select_id(reuse, outnet);
		unit_assert(!reuse_tcp_by_id_find(reuse, id));
		w = calloc(1, sizeof(*w));
		unit_assert(w);
		w->id = id;
		w->outnet = outnet;
		w->next_waiting = (void*)reuse->pending;
		reuse_tree_by_id_insert(reuse, w);
	}
}

/** fill up the reuse ID tree and test assertions */
static void tcpid_fillup(struct reuse_tcp* reuse,
	struct outside_network* outnet)
{
	int t, numtest=3;
	for(t=0; t<numtest; t++) {
		rbtree_init(&reuse->tree_by_id, reuse_id_cmp);
		tcpid_addmore(reuse, outnet, 65535);
		reuse_del_readwait(&reuse->tree_by_id);
	}
}

/** test TCP ID selection */
static void tcpid_test(void)
{
	struct pending_tcp pend;
	struct outside_network outnet;
	unit_show_func("services/outside_network.c", "reuse_tcp_select_id");
	memset(&pend, 0, sizeof(pend));
	pend.reuse.pending = &pend;
	memset(&outnet, 0, sizeof(outnet));
	outnet.rnd = ub_initstate(NULL);
	rbtree_init(&pend.reuse.tree_by_id, reuse_id_cmp);
	tcpid_fillup(&pend.reuse, &outnet);
	ub_randfree(outnet.rnd);
}

/** check that the tree has present number of nodes and the LRU is linked
 * properly. */
static void check_tree_and_list(struct outside_network* outnet, int present)
{
	int i;
	struct reuse_tcp *reuse, *next_reuse;
	unit_assert(present == (int)outnet->tcp_reuse.count);
	if(present < 1) {
		unit_assert(outnet->tcp_reuse_first == NULL);
		unit_assert(outnet->tcp_reuse_last == NULL);
		return;
	}
	unit_assert(outnet->tcp_reuse_first->item_on_lru_list);
	unit_assert(!outnet->tcp_reuse_first->lru_prev);
	reuse = outnet->tcp_reuse_first;
	for(i=0; i<present-1; i++) {
		unit_assert(reuse->item_on_lru_list);
		unit_assert(reuse->lru_next);
		unit_assert(reuse->lru_next != reuse);
		next_reuse = reuse->lru_next;
		unit_assert(next_reuse->lru_prev == reuse);
		reuse = next_reuse;
	}
	unit_assert(!reuse->lru_next);
	unit_assert(outnet->tcp_reuse_last->item_on_lru_list);
	unit_assert(outnet->tcp_reuse_last == reuse);
}

/** creates pending_tcp. Copy of outside_network.c:create_pending_tcp without
 *  the comm_point creation */
static int create_pending_tcp(struct outside_network* outnet)
{
	size_t i;
	if(outnet->num_tcp == 0)
		return 1; /* no tcp needed, nothing to do */
	if(!(outnet->tcp_conns = (struct pending_tcp **)calloc(
			outnet->num_tcp, sizeof(struct pending_tcp*))))
		return 0;
	for(i=0; i<outnet->num_tcp; i++) {
		if(!(outnet->tcp_conns[i] = (struct pending_tcp*)calloc(1,
			sizeof(struct pending_tcp))))
			return 0;
		outnet->tcp_conns[i]->next_free = outnet->tcp_free;
		outnet->tcp_free = outnet->tcp_conns[i];
	}
	return 1;
}

/** empty the tcp_reuse tree and LRU list */
static void empty_tree(struct outside_network* outnet)
{
	size_t i;
	struct reuse_tcp* reuse;
	reuse = outnet->tcp_reuse_first;
	i = outnet->tcp_reuse.count;
	while(reuse) {
		reuse_tcp_remove_tree_list(outnet, reuse);
		check_tree_and_list(outnet, --i);
		reuse = outnet->tcp_reuse_first;
	}
}

/** check removal of the LRU element on the given position of total elements */
static void check_removal(struct outside_network* outnet, int position, int total)
{
	int i;
	struct reuse_tcp* reuse;
	empty_tree(outnet);
	for(i=0; i<total; i++) {
		reuse_tcp_insert(outnet, outnet->tcp_conns[i]);
	}
	check_tree_and_list(outnet, total);
	reuse = outnet->tcp_reuse_first;
	for(i=0; i<position; i++) reuse = reuse->lru_next;
	reuse_tcp_remove_tree_list(outnet, reuse);
	check_tree_and_list(outnet, total-1);
}

/** check snipping off the last element of the LRU with total elements */
static void check_snip(struct outside_network* outnet, int total)
{
	int i;
	struct reuse_tcp* reuse;
	empty_tree(outnet);
	for(i=0; i<total; i++) {
		reuse_tcp_insert(outnet, outnet->tcp_conns[i]);
	}
	check_tree_and_list(outnet, total);
	reuse = reuse_tcp_lru_snip(outnet);
	while(reuse) {
		reuse_tcp_remove_tree_list(outnet, reuse);
		check_tree_and_list(outnet, --total);
		reuse = reuse_tcp_lru_snip(outnet);
	}
	unit_assert(outnet->tcp_reuse_first == NULL);
	unit_assert(outnet->tcp_reuse_last == NULL);
	unit_assert(outnet->tcp_reuse.count == 0);
}

/** test tcp_reuse tree and LRU list functions */
static void tcp_reuse_tree_list_test(void)
{
	size_t i;
	struct outside_network outnet;
	struct reuse_tcp* reuse;
	memset(&outnet, 0, sizeof(outnet));
	rbtree_init(&outnet.tcp_reuse, reuse_cmp);
	outnet.num_tcp = 5;
	outnet.tcp_reuse_max = outnet.num_tcp;
	if(!create_pending_tcp(&outnet)) fatal_exit("out of memory");
	/* add all to the tree */
	unit_show_func("services/outside_network.c", "reuse_tcp_insert");
	for(i=0; i<outnet.num_tcp; i++) {
		reuse_tcp_insert(&outnet, outnet.tcp_conns[i]);
		check_tree_and_list(&outnet, i+1);
	}
	/* check touching */
	unit_show_func("services/outside_network.c", "reuse_tcp_lru_touch");
	for(i=0; i<outnet.tcp_reuse.count; i++) {
		for(reuse = outnet.tcp_reuse_first; reuse->lru_next; reuse = reuse->lru_next);
		reuse_tcp_lru_touch(&outnet, reuse);
		check_tree_and_list(&outnet, outnet.num_tcp);
	}
	/* check removal */
	unit_show_func("services/outside_network.c", "reuse_tcp_remove_tree_list");
	check_removal(&outnet, 2, 5);
	check_removal(&outnet, 1, 3);
	check_removal(&outnet, 1, 2);
	/* check snip */
	unit_show_func("services/outside_network.c", "reuse_tcp_lru_snip");
	check_snip(&outnet, 4);

	for(i=0; i<outnet.num_tcp; i++)
		if(outnet.tcp_conns[i]) {
			free(outnet.tcp_conns[i]);
		}
	free(outnet.tcp_conns);
}

void tcpreuse_test(void)
{
    unit_show_feature("tcp_reuse");
    tcpid_test();
    tcp_reuse_tree_list_test();
}
