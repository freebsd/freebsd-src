/*	$OpenBSD: rb-test.c,v 1.4 2008/04/13 00:22:17 djm Exp $	*/
/*
 * Copyright 2019 Edward Tomasz Napierala <trasz@FreeBSD.org>
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include <sys/types.h>

#include <sys/arb.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

struct node {
	ARB32_ENTRY() next;
	int key;
};

ARB32_HEAD(tree, node) *root;

static int
compare(const struct node *a, const struct node *b)
{
	if (a->key < b->key) return (-1);
	else if (a->key > b->key) return (1);
	return (0);
}

ARB_PROTOTYPE(tree, node, next, compare);

ARB_GENERATE(tree, node, next, compare);

#define ITER 150
#define MIN 5
#define MAX 5000

ATF_TC_WITHOUT_HEAD(arb_test);
ATF_TC_BODY(arb_test, tc)
{
	struct node *tmp, *ins;
	int i, max, min;

	max = min = 42; /* pacify gcc */

	root = (struct tree *)calloc(1, ARB_ALLOCSIZE(root, ITER, tmp));

	ARB_INIT(tmp, next, root, ITER);

	for (i = 0; i < ITER; i++) {
		tmp = ARB_GETFREE(root, next);
		ATF_REQUIRE_MSG(tmp != NULL, "ARB_GETFREE failed");
		do {
			tmp->key = arc4random_uniform(MAX-MIN);
			tmp->key += MIN;
		} while (ARB_FIND(tree, root, tmp) != NULL);
		if (i == 0)
			max = min = tmp->key;
		else {
			if (tmp->key > max)
				max = tmp->key;
			if (tmp->key < min)
				min = tmp->key;
		}
		ATF_REQUIRE_EQ(NULL, ARB_INSERT(tree, root, tmp));
	}

	ins = ARB_MIN(tree, root);
	ATF_REQUIRE_MSG(ins != NULL, "ARB_MIN error");
	ATF_CHECK_EQ(min, ins->key);
	tmp = ins;
	ins = ARB_MAX(tree, root);
	ATF_REQUIRE_MSG(ins != NULL, "ARB_MAX error");
	ATF_CHECK_EQ(max, ins->key);

	ATF_CHECK_EQ(tmp, ARB_REMOVE(tree, root, tmp));

	for (i = 0; i < ITER - 1; i++) {
		tmp = ARB_ROOT(root);
		ATF_REQUIRE_MSG(tmp != NULL, "ARB_ROOT error");
		ATF_CHECK_EQ(tmp, ARB_REMOVE(tree, root, tmp));
	}
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, arb_test);

	return (atf_no_error());
}
