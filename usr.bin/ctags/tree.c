/*
 * Copyright (c) 1987 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)tree.c	5.5 (Berkeley) 2/26/91";
#endif /* not lint */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ctags.h"

/*
 * pfnote --
 *	enter a new node in the tree
 */
pfnote(name,ln)
	char	*name;
	int	ln;
{
	extern NODE	*head;		/* head of the sorted binary tree */
	extern char	*curfile;	/* current input file name */
	register NODE	*np;
	register char	*fp;
	char	nbuf[MAXTOKEN];

	/*NOSTRICT*/
	if (!(np = (NODE *)malloc(sizeof(NODE)))) {
		fputs("ctags: too many entries to sort\n",stderr);
		put_entries(head);
		free_tree(head);
		/*NOSTRICT*/
		if (!(head = np = (NODE *)malloc(sizeof(NODE)))) {
			fputs("ctags: out of space.\n",stderr);
			exit(1);
		}
	}
	if (!xflag && !strcmp(name,"main")) {
		if (!(fp = rindex(curfile,'/')))
			fp = curfile;
		else
			++fp;
		(void)sprintf(nbuf,"M%s",fp);
		fp = rindex(nbuf,'.');
		if (fp && !fp[2])
			*fp = EOS;
		name = nbuf;
	}
	if (!(np->entry = strdup(name))) {
		(void)fprintf(stderr, "ctags: %s\n", strerror(errno));
		exit(1);
	}
	np->file = curfile;
	np->lno = ln;
	np->left = np->right = 0;
	if (!(np->pat = strdup(lbuf))) {
		(void)fprintf(stderr, "ctags: %s\n", strerror(errno));
		exit(1);
	}
	if (!head)
		head = np;
	else
		add_node(np,head);
}

add_node(node,cur_node)
	register NODE	*node,
			*cur_node;
{
	extern int	wflag;			/* -w: suppress warnings */
	register int	dif;

	dif = strcmp(node->entry,cur_node->entry);
	if (!dif) {
		if (node->file == cur_node->file) {
			if (!wflag)
				fprintf(stderr,"Duplicate entry in file %s, line %d: %s\nSecond entry ignored\n",node->file,lineno,node->entry);
			return;
		}
		if (!cur_node->been_warned)
			if (!wflag)
				fprintf(stderr,"Duplicate entry in files %s and %s: %s (Warning only)\n",node->file,cur_node->file,node->entry);
		cur_node->been_warned = YES;
	}
	else if (dif < 0)
		if (cur_node->left)
			add_node(node,cur_node->left);
		else
			cur_node->left = node;
	else if (cur_node->right)
		add_node(node,cur_node->right);
	else
		cur_node->right = node;
}

free_tree(node)
	register NODE	*node;
{
	while (node) {
		if (node->right)
			free_tree(node->right);
		cfree(node);
		node = node->left;
	}
}
