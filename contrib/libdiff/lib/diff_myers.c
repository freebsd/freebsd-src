/* Myers diff algorithm implementation, invented by Eugene W. Myers [1].
 * Implementations of both the Myers Divide Et Impera (using linear space)
 * and the canonical Myers algorithm (using quadratic space). */
/*
 * Copyright (c) 2020 Neels Hofmeyr <neels@hofmeyr.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <arraylist.h>
#include <diff_main.h>

#include "diff_internal.h"
#include "diff_debug.h"

/* Myers' diff algorithm [1] is nicely explained in [2].
 * [1] http://www.xmailserver.org/diff2.pdf
 * [2] https://blog.jcoglan.com/2017/02/12/the-myers-diff-algorithm-part-1/ ff.
 *
 * Myers approaches finding the smallest diff as a graph problem.
 * The crux is that the original algorithm requires quadratic amount of memory:
 * both sides' lengths added, and that squared. So if we're diffing lines of
 * text, two files with 1000 lines each would blow up to a matrix of about
 * 2000 * 2000 ints of state, about 16 Mb of RAM to figure out 2 kb of text.
 * The solution is using Myers' "divide and conquer" extension algorithm, which
 * does the original traversal from both ends of the files to reach a middle
 * where these "snakes" touch, hence does not need to backtrace the traversal,
 * and so gets away with only keeping a single column of that huge state matrix
 * in memory.
 */

struct diff_box {
	unsigned int left_start;
	unsigned int left_end;
	unsigned int right_start;
	unsigned int right_end;
};

/* If the two contents of a file are A B C D E and X B C Y,
 * the Myers diff graph looks like:
 *
 *   k0  k1
 *    \   \
 * k-1     0 1 2 3 4 5
 *   \      A B C D E
 *     0   o-o-o-o-o-o
 *      X  | | | | | |
 *     1   o-o-o-o-o-o
 *      B  | |\| | | |
 *     2   o-o-o-o-o-o
 *      C  | | |\| | |
 *     3   o-o-o-o-o-o
 *      Y  | | | | | |\
 *     4   o-o-o-o-o-o c1
 *                  \ \
 *                 c-1 c0
 *
 * Moving right means delete an atom from the left-hand-side,
 * Moving down means add an atom from the right-hand-side.
 * Diagonals indicate identical atoms on both sides, the challenge is to use as
 * many diagonals as possible.
 *
 * The original Myers algorithm walks all the way from the top left to the
 * bottom right, remembers all steps, and then backtraces to find the shortest
 * path. However, that requires keeping the entire graph in memory, which needs
 * quadratic space.
 *
 * Myers adds a variant that uses linear space -- note, not linear time, only
 * linear space: walk forward and backward, find a meeting point in the middle,
 * and recurse on the two separate sections. This is called "divide and
 * conquer".
 *
 * d: the step number, starting with 0, a.k.a. the distance from the starting
 *    point.
 * k: relative index in the state array for the forward scan, indicating on
 *    which diagonal through the diff graph we currently are.
 * c: relative index in the state array for the backward scan, indicating the
 *    diagonal number from the bottom up.
 *
 * The "divide and conquer" traversal through the Myers graph looks like this:
 *
 *      | d=   0   1   2   3      2   1   0
 *  ----+--------------------------------------------
 *  k=  |                                      c=
 *   4  |                                       3
 *      |
 *   3  |                 3,0    5,2            2
 *      |                /          \
 *   2  |             2,0            5,3        1
 *      |            /                 \
 *   1  |         1,0     4,3 >= 4,3    5,4<--  0
 *      |        /       /          \  /
 *   0  |  -->0,0     3,3            4,4       -1
 *      |        \   /              /
 *  -1  |         0,1     1,2    3,4           -2
 *      |            \   /
 *  -2  |             0,2                      -3
 *      |                \
 *      |                 0,3
 *      |  forward->                 <-backward
 *
 * x,y pairs here are the coordinates in the Myers graph:
 * x = atom index in left-side source, y = atom index in the right-side source.
 *
 * Only one forward column and one backward column are kept in mem, each need at
 * most left.len + 1 + right.len items.  Note that each d step occupies either
 * the even or the odd items of a column: if e.g. the previous column is in the
 * odd items, the next column is formed in the even items, without overwriting
 * the previous column's results.
 *
 * Also note that from the diagonal index k and the x coordinate, the y
 * coordinate can be derived:
 *    y = x - k
 * Hence the state array only needs to keep the x coordinate, i.e. the position
 * in the left-hand file, and the y coordinate, i.e. position in the right-hand
 * file, is derived from the index in the state array.
 *
 * The two traces meet at 4,3, the first step (here found in the forward
 * traversal) where a forward position is on or past a backward traced position
 * on the same diagonal.
 *
 * This divides the problem space into:
 *
 *         0 1 2 3 4 5
 *          A B C D E
 *     0   o-o-o-o-o
 *      X  | | | | |
 *     1   o-o-o-o-o
 *      B  | |\| | |
 *     2   o-o-o-o-o
 *      C  | | |\| |
 *     3   o-o-o-o-*-o   *: forward and backward meet here
 *      Y          | |
 *     4           o-o
 *
 * Doing the same on each section lead to:
 *
 *         0 1 2 3 4 5
 *          A B C D E
 *     0   o-o
 *      X  | |
 *     1   o-b    b: backward d=1 first reaches here (sliding up the snake)
 *      B     \   f: then forward d=2 reaches here (sliding down the snake)
 *     2       o     As result, the box from b to f is found to be identical;
 *      C       \    leaving a top box from 0,0 to 1,1 and a bottom trivial
 *     3         f-o tail 3,3 to 4,3.
 *
 *     3           o-*
 *      Y            |
 *     4             o   *: forward and backward meet here
 *
 * and solving the last top left box gives:
 *
 *         0 1 2 3 4 5
 *          A B C D E           -A
 *     0   o-o                  +X
 *      X    |                   B
 *     1     o                   C
 *      B     \                 -D
 *     2       o                -E
 *      C       \               +Y
 *     3         o-o-o
 *      Y            |
 *     4             o
 *
 */

#define xk_to_y(X, K) ((X) - (K))
#define xc_to_y(X, C, DELTA) ((X) - (C) + (DELTA))
#define k_to_c(K, DELTA) ((K) + (DELTA))
#define c_to_k(C, DELTA) ((C) - (DELTA))

/* Do one forwards step in the "divide and conquer" graph traversal.
 * left: the left side to diff.
 * right: the right side to diff against.
 * kd_forward: the traversal state for forwards traversal, modified by this
 *             function.
 *             This is carried over between invocations with increasing d.
 *             kd_forward points at the center of the state array, allowing
 *             negative indexes.
 * kd_backward: the traversal state for backwards traversal, to find a meeting
 *              point.
 *              Since forwards is done first, kd_backward will be valid for d -
 *              1, not d.
 *              kd_backward points at the center of the state array, allowing
 *              negative indexes.
 * d: Step or distance counter, indicating for what value of d the kd_forward
 *    should be populated.
 *    For d == 0, kd_forward[0] is initialized, i.e. the first invocation should
 *    be for d == 0.
 * meeting_snake: resulting meeting point, if any.
 * Return true when a meeting point has been identified.
 */
static int
diff_divide_myers_forward(bool *found_midpoint,
			  struct diff_data *left, struct diff_data *right,
			  int *kd_forward, int *kd_backward, int d,
			  struct diff_box *meeting_snake)
{
	int delta = (int)right->atoms.len - (int)left->atoms.len;
	int k;
	int x;
	int prev_x;
	int prev_y;
	int x_before_slide;
	*found_midpoint = false;

	for (k = d; k >= -d; k -= 2) {
		if (k < -(int)right->atoms.len || k > (int)left->atoms.len) {
			/* This diagonal is completely outside of the Myers
			 * graph, don't calculate it. */
			if (k < 0) {
				/* We are traversing negatively, and already
				 * below the entire graph, nothing will come of
				 * this. */
				debug(" break\n");
				break;
			}
			debug(" continue\n");
			continue;
		}
		if (d == 0) {
			/* This is the initializing step. There is no prev_k
			 * yet, get the initial x from the top left of the Myers
			 * graph. */
			x = 0;
			prev_x = x;
			prev_y = xk_to_y(x, k);
		}
		/* Favoring "-" lines first means favoring moving rightwards in
		 * the Myers graph.
		 * For this, all k should derive from k - 1, only the bottom
		 * most k derive from k + 1:
		 *
		 *      | d=   0   1   2
		 *  ----+----------------
		 *  k=  |
		 *   2  |             2,0 <-- from prev_k = 2 - 1 = 1
		 *      |            /
		 *   1  |         1,0
		 *      |        /
		 *   0  |  -->0,0     3,3
		 *      |       \\   /
		 *  -1  |         0,1 <-- bottom most for d=1 from
		 *      |           \\    prev_k = -1 + 1 = 0
		 *  -2  |             0,2 <-- bottom most for d=2 from
		 *                            prev_k = -2 + 1 = -1
		 *
		 * Except when a k + 1 from a previous run already means a
		 * further advancement in the graph.
		 * If k == d, there is no k + 1 and k - 1 is the only option.
		 * If k < d, use k + 1 in case that yields a larger x. Also use
		 * k + 1 if k - 1 is outside the graph.
		 */
		else if (k > -d
			 && (k == d
			     || (k - 1 >= -(int)right->atoms.len
				 && kd_forward[k - 1] >= kd_forward[k + 1]))) {
			/* Advance from k - 1.
			 * From position prev_k, step to the right in the Myers
			 * graph: x += 1.
			 */
			int prev_k = k - 1;
			prev_x = kd_forward[prev_k];
			prev_y = xk_to_y(prev_x, prev_k);
			x = prev_x + 1;
		} else {
			/* The bottom most one.
			 * From position prev_k, step to the bottom in the Myers
			 * graph: y += 1.
			 * Incrementing y is achieved by decrementing k while
			 * keeping the same x.
			 * (since we're deriving y from y = x - k).
			 */
			int prev_k = k + 1;
			prev_x = kd_forward[prev_k];
			prev_y = xk_to_y(prev_x, prev_k);
			x = prev_x;
		}

		x_before_slide = x;
		/* Slide down any snake that we might find here. */
		while (x < left->atoms.len && xk_to_y(x, k) < right->atoms.len) {
			bool same;
			int r = diff_atom_same(&same,
					       &left->atoms.head[x],
					       &right->atoms.head[
						xk_to_y(x, k)]);
			if (r)
				return r;
			if (!same)
				break;
			x++;
		}
		kd_forward[k] = x;
#if 0
		if (x_before_slide != x) {
			debug("  down %d similar lines\n", x - x_before_slide);
		}

#if DEBUG
		{
			int fi;
			for (fi = d; fi >= k; fi--) {
				debug("kd_forward[%d] = (%d, %d)\n", fi,
				      kd_forward[fi], kd_forward[fi] - fi);
			}
		}
#endif
#endif

		if (x < 0 || x > left->atoms.len
		    || xk_to_y(x, k) < 0 || xk_to_y(x, k) > right->atoms.len)
			continue;

		/* Figured out a new forwards traversal, see if this has gone
		 * onto or even past a preceding backwards traversal.
		 *
		 * If the delta in length is odd, then d and backwards_d hit the
		 * same state indexes:
		 *      | d=   0   1   2      1   0
		 *  ----+----------------    ----------------
		 *  k=  |                              c=
		 *   4  |                               3
		 *      |
		 *   3  |                               2
		 *      |                same
		 *   2  |             2,0====5,3        1
		 *      |            /          \
		 *   1  |         1,0            5,4<-- 0
		 *      |        /              /
		 *   0  |  -->0,0     3,3====4,4       -1
		 *      |        \   /
		 *  -1  |         0,1                  -2
		 *      |            \
		 *  -2  |             0,2              -3
		 *      |
		 *
		 * If the delta is even, they end up off-by-one, i.e. on
		 * different diagonals:
		 *
		 *      | d=   0   1   2    1   0
		 *  ----+----------------  ----------------
		 *      |                            c=
		 *   3  |                             3
		 *      |
		 *   2  |             2,0 off         2
		 *      |            /   \\
		 *   1  |         1,0      4,3        1
		 *      |        /       //   \
		 *   0  |  -->0,0     3,3      4,4<-- 0
		 *      |        \   /        /
		 *  -1  |         0,1      3,4       -1
		 *      |            \   //
		 *  -2  |             0,2            -2
		 *      |
		 *
		 * So in the forward path, we can only match up diagonals when
		 * the delta is odd.
		 */
		if ((delta & 1) == 0)
			continue;
		 /* Forwards is done first, so the backwards one was still at
		  * d - 1. Can't do this for d == 0. */
		int backwards_d = d - 1;
		if (backwards_d < 0)
			continue;

		/* If both sides have the same length, forward and backward
		 * start on the same diagonal, meaning the backwards state index
		 * c == k.
		 * As soon as the lengths are not the same, the backwards
		 * traversal starts on a different diagonal, and c = k shifted
		 * by the difference in length.
		 */
		int c = k_to_c(k, delta);

		/* When the file sizes are very different, the traversal trees
		 * start on far distant diagonals.
		 * They don't necessarily meet straight on. See whether this
		 * forward value is on a diagonal that is also valid in
		 * kd_backward[], and match them if so. */
		if (c >= -backwards_d && c <= backwards_d) {
			/* Current k is on a diagonal that exists in
			 * kd_backward[]. If the two x positions have met or
			 * passed (forward walked onto or past backward), then
			 * we've found a midpoint / a mid-box.
			 *
			 * When forwards and backwards traversals meet, the
			 * endpoints of the mid-snake are not the two points in
			 * kd_forward and kd_backward, but rather the section
			 * that was slid (if any) of the current
			 * forward/backward traversal only.
			 *
			 * For example:
			 *
			 *   o
			 *    \
			 *     o
			 *      \
			 *       o
			 *        \
			 *         o
			 *          \
			 *       X o o
			 *       | | |
			 *     o-o-o o
			 *          \|
			 *           M
			 *            \
			 *             o
			 *              \
			 *               A o
			 *               | |
			 *             o-o-o
			 *
			 * The forward traversal reached M from the top and slid
			 * downwards to A.  The backward traversal already
			 * reached X, which is not a straight line from M
			 * anymore, so picking a mid-snake from M to X would
			 * yield a mistake.
			 *
			 * The correct mid-snake is between M and A. M is where
			 * the forward traversal hit the diagonal that the
			 * backward traversal has already passed, and A is what
			 * it reaches when sliding down identical lines.
			 */
			int backward_x = kd_backward[c];
			if (x >= backward_x) {
				if (x_before_slide != x) {
					/* met after sliding up a mid-snake */
					*meeting_snake = (struct diff_box){
						.left_start = x_before_slide,
						.left_end = x,
						.right_start = xc_to_y(x_before_slide,
								       c, delta),
						.right_end = xk_to_y(x, k),
					};
				} else {
					/* met after a side step, non-identical
					 * line. Mark that as box divider
					 * instead. This makes sure that
					 * myers_divide never returns the same
					 * box that came as input, avoiding
					 * "infinite" looping. */
					*meeting_snake = (struct diff_box){
						.left_start = prev_x,
						.left_end = x,
						.right_start = prev_y,
						.right_end = xk_to_y(x, k),
					};
				}
				debug("HIT x=(%u,%u) - y=(%u,%u)\n",
				      meeting_snake->left_start,
				      meeting_snake->right_start,
				      meeting_snake->left_end,
				      meeting_snake->right_end);
				debug_dump_myers_graph(left, right, NULL,
						       kd_forward, d,
						       kd_backward, d-1);
				*found_midpoint = true;
				return 0;
			}
		}
	}

	return 0;
}

/* Do one backwards step in the "divide and conquer" graph traversal.
 * left: the left side to diff.
 * right: the right side to diff against.
 * kd_forward: the traversal state for forwards traversal, to find a meeting
 *             point.
 *             Since forwards is done first, after this, both kd_forward and
 *             kd_backward will be valid for d.
 *             kd_forward points at the center of the state array, allowing
 *             negative indexes.
 * kd_backward: the traversal state for backwards traversal, to find a meeting
 *              point.
 *              This is carried over between invocations with increasing d.
 *              kd_backward points at the center of the state array, allowing
 *              negative indexes.
 * d: Step or distance counter, indicating for what value of d the kd_backward
 *    should be populated.
 *    Before the first invocation, kd_backward[0] shall point at the bottom
 *    right of the Myers graph (left.len, right.len).
 *    The first invocation will be for d == 1.
 * meeting_snake: resulting meeting point, if any.
 * Return true when a meeting point has been identified.
 */
static int
diff_divide_myers_backward(bool *found_midpoint,
			   struct diff_data *left, struct diff_data *right,
			   int *kd_forward, int *kd_backward, int d,
			   struct diff_box *meeting_snake)
{
	int delta = (int)right->atoms.len - (int)left->atoms.len;
	int c;
	int x;
	int prev_x;
	int prev_y;
	int x_before_slide;

	*found_midpoint = false;

	for (c = d; c >= -d; c -= 2) {
		if (c < -(int)left->atoms.len || c > (int)right->atoms.len) {
			/* This diagonal is completely outside of the Myers
			 * graph, don't calculate it. */
			if (c < 0) {
				/* We are traversing negatively, and already
				 * below the entire graph, nothing will come of
				 * this. */
				break;
			}
			continue;
		}
		if (d == 0) {
			/* This is the initializing step. There is no prev_c
			 * yet, get the initial x from the bottom right of the
			 * Myers graph. */
			x = left->atoms.len;
			prev_x = x;
			prev_y = xc_to_y(x, c, delta);
		}
		/* Favoring "-" lines first means favoring moving rightwards in
		 * the Myers graph.
		 * For this, all c should derive from c - 1, only the bottom
		 * most c derive from c + 1:
		 *
		 *                                  2   1   0
		 *  ---------------------------------------------------
		 *                                               c=
		 *                                                3
		 *
		 *         from prev_c = c - 1 --> 5,2            2
		 *                                    \
		 *                                     5,3        1
		 *                                        \
		 *                                 4,3     5,4<-- 0
		 *                                    \   /
		 *  bottom most for d=1 from c + 1 --> 4,4       -1
		 *                                    /
		 *         bottom most for d=2 --> 3,4           -2
		 *
		 * Except when a c + 1 from a previous run already means a
		 * further advancement in the graph.
		 * If c == d, there is no c + 1 and c - 1 is the only option.
		 * If c < d, use c + 1 in case that yields a larger x.
		 * Also use c + 1 if c - 1 is outside the graph.
		 */
		else if (c > -d && (c == d
				    || (c - 1 >= -(int)right->atoms.len
					&& kd_backward[c - 1] <= kd_backward[c + 1]))) {
			/* A top one.
			 * From position prev_c, step upwards in the Myers
			 * graph: y -= 1.
			 * Decrementing y is achieved by incrementing c while
			 * keeping the same x. (since we're deriving y from
			 * y = x - c + delta).
			 */
			int prev_c = c - 1;
			prev_x = kd_backward[prev_c];
			prev_y = xc_to_y(prev_x, prev_c, delta);
			x = prev_x;
		} else {
			/* The bottom most one.
			 * From position prev_c, step to the left in the Myers
			 * graph: x -= 1.
			 */
			int prev_c = c + 1;
			prev_x = kd_backward[prev_c];
			prev_y = xc_to_y(prev_x, prev_c, delta);
			x = prev_x - 1;
		}

		/* Slide up any snake that we might find here (sections of
		 * identical lines on both sides). */
#if 0
		debug("c=%d x-1=%d Yb-1=%d-1=%d\n", c, x-1, xc_to_y(x, c,
								    delta),
		      xc_to_y(x, c, delta)-1);
		if (x > 0) {
			debug("  l=");
			debug_dump_atom(left, right, &left->atoms.head[x-1]);
		}
		if (xc_to_y(x, c, delta) > 0) {
			debug("  r=");
			debug_dump_atom(right, left,
				&right->atoms.head[xc_to_y(x, c, delta)-1]);
		}
#endif
		x_before_slide = x;
		while (x > 0 && xc_to_y(x, c, delta) > 0) {
			bool same;
			int r = diff_atom_same(&same,
					       &left->atoms.head[x-1],
					       &right->atoms.head[
						xc_to_y(x, c, delta)-1]);
			if (r)
				return r;
			if (!same)
				break;
			x--;
		}
		kd_backward[c] = x;
#if 0
		if (x_before_slide != x) {
			debug("  up %d similar lines\n", x_before_slide - x);
		}

		if (DEBUG) {
			int fi;
			for (fi = d; fi >= c; fi--) {
				debug("kd_backward[%d] = (%d, %d)\n",
				      fi,
				      kd_backward[fi],
				      kd_backward[fi] - fi + delta);
			}
		}
#endif

		if (x < 0 || x > left->atoms.len
		    || xc_to_y(x, c, delta) < 0
		    || xc_to_y(x, c, delta) > right->atoms.len)
			continue;

		/* Figured out a new backwards traversal, see if this has gone
		 * onto or even past a preceding forwards traversal.
		 *
		 * If the delta in length is even, then d and backwards_d hit
		 * the same state indexes -- note how this is different from in
		 * the forwards traversal, because now both d are the same:
		 *
		 *      | d=   0   1   2      2   1   0
		 *  ----+----------------    --------------------
		 *  k=  |                                  c=
		 *   4  |
		 *      |
		 *   3  |                                   3
		 *      |                same
		 *   2  |             2,0====5,2            2
		 *      |            /          \
		 *   1  |         1,0            5,3        1
		 *      |        /              /  \
		 *   0  |  -->0,0     3,3====4,3    5,4<--  0
		 *      |        \   /             /
		 *  -1  |         0,1            4,4       -1
		 *      |            \
		 *  -2  |             0,2                  -2
		 *      |
		 *                                      -3
		 * If the delta is odd, they end up off-by-one, i.e. on
		 * different diagonals.
		 * So in the backward path, we can only match up diagonals when
		 * the delta is even.
		 */
		if ((delta & 1) != 0)
			continue;
		/* Forwards was done first, now both d are the same. */
		int forwards_d = d;

		/* As soon as the lengths are not the same, the
		 * backwards traversal starts on a different diagonal,
		 * and c = k shifted by the difference in length.
		 */
		int k = c_to_k(c, delta);

		/* When the file sizes are very different, the traversal trees
		 * start on far distant diagonals.
		 * They don't necessarily meet straight on. See whether this
		 * backward value is also on a valid diagonal in kd_forward[],
		 * and match them if so. */
		if (k >= -forwards_d && k <= forwards_d) {
			/* Current c is on a diagonal that exists in
			 * kd_forward[]. If the two x positions have met or
			 * passed (backward walked onto or past forward), then
			 * we've found a midpoint / a mid-box.
			 *
			 * When forwards and backwards traversals meet, the
			 * endpoints of the mid-snake are not the two points in
			 * kd_forward and kd_backward, but rather the section
			 * that was slid (if any) of the current
			 * forward/backward traversal only.
			 *
			 * For example:
			 *
			 *   o-o-o
			 *   | |
			 *   o A
			 *   |  \
			 *   o   o
			 *        \
			 *         M
			 *         |\
			 *         o o-o-o
			 *         | | |
			 *         o o X
			 *          \
			 *           o
			 *            \
			 *             o
			 *              \
			 *               o
			 *
			 * The backward traversal reached M from the bottom and
			 * slid upwards.  The forward traversal already reached
			 * X, which is not a straight line from M anymore, so
			 * picking a mid-snake from M to X would yield a
			 * mistake.
			 *
			 * The correct mid-snake is between M and A. M is where
			 * the backward traversal hit the diagonal that the
			 * forwards traversal has already passed, and A is what
			 * it reaches when sliding up identical lines.
			 */

			int forward_x = kd_forward[k];
			if (forward_x >= x) {
				if (x_before_slide != x) {
					/* met after sliding down a mid-snake */
					*meeting_snake = (struct diff_box){
						.left_start = x,
						.left_end = x_before_slide,
						.right_start = xc_to_y(x, c, delta),
						.right_end = xk_to_y(x_before_slide, k),
					};
				} else {
					/* met after a side step, non-identical
					 * line. Mark that as box divider
					 * instead. This makes sure that
					 * myers_divide never returns the same
					 * box that came as input, avoiding
					 * "infinite" looping. */
					*meeting_snake = (struct diff_box){
						.left_start = x,
						.left_end = prev_x,
						.right_start = xc_to_y(x, c, delta),
						.right_end = prev_y,
					};
				}
				debug("HIT x=%u,%u - y=%u,%u\n",
				      meeting_snake->left_start,
				      meeting_snake->right_start,
				      meeting_snake->left_end,
				      meeting_snake->right_end);
				debug_dump_myers_graph(left, right, NULL,
						       kd_forward, d,
						       kd_backward, d);
				*found_midpoint = true;
				return 0;
			}
		}
	}
	return 0;
}

/* Integer square root approximation */
static int
shift_sqrt(int val)
{
	int i;
        for (i = 1; val > 0; val >>= 2)
		i <<= 1;
        return i;
}

#define DIFF_EFFORT_MIN 1024

/* Myers "Divide et Impera": tracing forwards from the start and backwards from
 * the end to find a midpoint that divides the problem into smaller chunks.
 * Requires only linear amounts of memory. */
int
diff_algo_myers_divide(const struct diff_algo_config *algo_config,
		       struct diff_state *state)
{
	int rc = ENOMEM;
	struct diff_data *left = &state->left;
	struct diff_data *right = &state->right;
	int *kd_buf;

	debug("\n** %s\n", __func__);
	debug("left:\n");
	debug_dump(left);
	debug("right:\n");
	debug_dump(right);

	/* Allocate two columns of a Myers graph, one for the forward and one
	 * for the backward traversal. */
	unsigned int max = left->atoms.len + right->atoms.len;
	size_t kd_len = max + 1;
	size_t kd_buf_size = kd_len << 1;

	if (state->kd_buf_size < kd_buf_size) {
		kd_buf = reallocarray(state->kd_buf, kd_buf_size,
		    sizeof(int));
		if (!kd_buf)
			return ENOMEM;
		state->kd_buf = kd_buf;
		state->kd_buf_size = kd_buf_size;
	} else
		kd_buf = state->kd_buf;
	int i;
	for (i = 0; i < kd_buf_size; i++)
		kd_buf[i] = -1;
	int *kd_forward = kd_buf;
	int *kd_backward = kd_buf + kd_len;
	int max_effort = shift_sqrt(max/2);

	if (max_effort < DIFF_EFFORT_MIN)
		max_effort = DIFF_EFFORT_MIN;

	/* The 'k' axis in Myers spans positive and negative indexes, so point
	 * the kd to the middle.
	 * It is then possible to index from -max/2 .. max/2. */
	kd_forward += max/2;
	kd_backward += max/2;

	int d;
	struct diff_box mid_snake = {};
	bool found_midpoint = false;
	for (d = 0; d <= (max/2); d++) {
		int r;
		r = diff_divide_myers_forward(&found_midpoint, left, right,
					      kd_forward, kd_backward, d,
					      &mid_snake);
		if (r)
			return r;
		if (found_midpoint)
			break;
		r = diff_divide_myers_backward(&found_midpoint, left, right,
					       kd_forward, kd_backward, d,
					       &mid_snake);
		if (r)
			return r;
		if (found_midpoint)
			break;

		/* Limit the effort spent looking for a mid snake. If files have
		 * very few lines in common, the effort spent to find nice mid
		 * snakes is just not worth it, the diff result will still be
		 * essentially minus everything on the left, plus everything on
		 * the right, with a few useless matches here and there. */
		if (d > max_effort) {
			/* pick the furthest reaching point from
			 * kd_forward and kd_backward, and use that as a
			 * midpoint, to not step into another diff algo
			 * recursion with unchanged box. */
			int delta = (int)right->atoms.len - (int)left->atoms.len;
			int x = 0;
			int y;
			int i;
			int best_forward_i = 0;
			int best_forward_distance = 0;
			int best_backward_i = 0;
			int best_backward_distance = 0;
			int distance;
			int best_forward_x;
			int best_forward_y;
			int best_backward_x;
			int best_backward_y;

			debug("~~~ HIT d = %d > max_effort = %d\n", d, max_effort);
			debug_dump_myers_graph(left, right, NULL,
					       kd_forward, d,
					       kd_backward, d);

			for (i = d; i >= -d; i -= 2) {
				if (i >= -(int)right->atoms.len && i <= (int)left->atoms.len) {
					x = kd_forward[i];
					y = xk_to_y(x, i);
					distance = x + y;
					if (distance > best_forward_distance) {
						best_forward_distance = distance;
						best_forward_i = i;
					}
				}

				if (i >= -(int)left->atoms.len && i <= (int)right->atoms.len) {
					x = kd_backward[i];
					y = xc_to_y(x, i, delta);
					distance = (right->atoms.len - x)
						+ (left->atoms.len - y);
					if (distance >= best_backward_distance) {
						best_backward_distance = distance;
						best_backward_i = i;
					}
				}
			}

			/* The myers-divide didn't meet in the middle. We just
			 * figured out the places where the forward path
			 * advanced the most, and the backward path advanced the
			 * most. Just divide at whichever one of those two is better.
			 *
			 *   o-o
			 *     |
			 *     o
			 *      \
			 *       o
			 *        \
			 *         F <-- cut here
			 *
			 *
			 *
			 *           or here --> B
			 *                        \
			 *                         o
			 *                          \
			 *                           o
			 *                           |
			 *                           o-o
			 */
			best_forward_x = kd_forward[best_forward_i];
			best_forward_y = xk_to_y(best_forward_x, best_forward_i);
			best_backward_x = kd_backward[best_backward_i];
			best_backward_y = xc_to_y(best_backward_x, best_backward_i, delta);

			if (best_forward_distance >= best_backward_distance) {
				x = best_forward_x;
				y = best_forward_y;
			} else {
				x = best_backward_x;
				y = best_backward_y;
			}

			debug("max_effort cut at x=%d y=%d\n", x, y);
			if (x < 0 || y < 0
			    || x > left->atoms.len || y > right->atoms.len)
				break;

			found_midpoint = true;
			mid_snake = (struct diff_box){
				.left_start = x,
				.left_end = x,
				.right_start = y,
				.right_end = y,
			};
			break;
		}
	}

	if (!found_midpoint) {
		/* Divide and conquer failed to find a meeting point. Use the
		 * fallback_algo defined in the algo_config (leave this to the
		 * caller). This is just paranoia/sanity, we normally should
		 * always find a midpoint.
		 */
		debug(" no midpoint \n");
		rc = DIFF_RC_USE_DIFF_ALGO_FALLBACK;
		goto return_rc;
	} else {
		debug(" mid snake L: %u to %u of %u   R: %u to %u of %u\n",
		      mid_snake.left_start, mid_snake.left_end, left->atoms.len,
		      mid_snake.right_start, mid_snake.right_end,
		      right->atoms.len);

		/* Section before the mid-snake.  */
		debug("Section before the mid-snake\n");

		struct diff_atom *left_atom = &left->atoms.head[0];
		unsigned int left_section_len = mid_snake.left_start;
		struct diff_atom *right_atom = &right->atoms.head[0];
		unsigned int right_section_len = mid_snake.right_start;

		if (left_section_len && right_section_len) {
			/* Record an unsolved chunk, the caller will apply
			 * inner_algo() on this chunk. */
			if (!diff_state_add_chunk(state, false,
						  left_atom, left_section_len,
						  right_atom,
						  right_section_len))
				goto return_rc;
		} else if (left_section_len && !right_section_len) {
			/* Only left atoms and none on the right, they form a
			 * "minus" chunk, then. */
			if (!diff_state_add_chunk(state, true,
						  left_atom, left_section_len,
						  right_atom, 0))
				goto return_rc;
		} else if (!left_section_len && right_section_len) {
			/* No left atoms, only atoms on the right, they form a
			 * "plus" chunk, then. */
			if (!diff_state_add_chunk(state, true,
						  left_atom, 0,
						  right_atom,
						  right_section_len))
				goto return_rc;
		}
		/* else: left_section_len == 0 and right_section_len == 0, i.e.
		 * nothing before the mid-snake. */

		if (mid_snake.left_end > mid_snake.left_start
		    || mid_snake.right_end > mid_snake.right_start) {
			/* The midpoint is a section of identical data on both
			 * sides, or a certain differing line: that section
			 * immediately becomes a solved chunk. */
			debug("the mid-snake\n");
			if (!diff_state_add_chunk(state, true,
				  &left->atoms.head[mid_snake.left_start],
				  mid_snake.left_end - mid_snake.left_start,
				  &right->atoms.head[mid_snake.right_start],
				  mid_snake.right_end - mid_snake.right_start))
				goto return_rc;
		}

		/* Section after the mid-snake. */
		debug("Section after the mid-snake\n");
		debug("  left_end %u  right_end %u\n",
		      mid_snake.left_end, mid_snake.right_end);
		debug("  left_count %u  right_count %u\n",
		      left->atoms.len, right->atoms.len);
		left_atom = &left->atoms.head[mid_snake.left_end];
		left_section_len = left->atoms.len - mid_snake.left_end;
		right_atom = &right->atoms.head[mid_snake.right_end];
		right_section_len = right->atoms.len - mid_snake.right_end;

		if (left_section_len && right_section_len) {
			/* Record an unsolved chunk, the caller will apply
			 * inner_algo() on this chunk. */
			if (!diff_state_add_chunk(state, false,
						  left_atom, left_section_len,
						  right_atom,
						  right_section_len))
				goto return_rc;
		} else if (left_section_len && !right_section_len) {
			/* Only left atoms and none on the right, they form a
			 * "minus" chunk, then. */
			if (!diff_state_add_chunk(state, true,
						  left_atom, left_section_len,
						  right_atom, 0))
				goto return_rc;
		} else if (!left_section_len && right_section_len) {
			/* No left atoms, only atoms on the right, they form a
			 * "plus" chunk, then. */
			if (!diff_state_add_chunk(state, true,
						  left_atom, 0,
						  right_atom,
						  right_section_len))
				goto return_rc;
		}
		/* else: left_section_len == 0 and right_section_len == 0, i.e.
		 * nothing after the mid-snake. */
	}

	rc = DIFF_RC_OK;

return_rc:
	debug("** END %s\n", __func__);
	return rc;
}

/* Myers Diff tracing from the start all the way through to the end, requiring
 * quadratic amounts of memory. This can fail if the required space surpasses
 * algo_config->permitted_state_size. */
int
diff_algo_myers(const struct diff_algo_config *algo_config,
		struct diff_state *state)
{
	/* do a diff_divide_myers_forward() without a _backward(), so that it
	 * walks forward across the entire files to reach the end. Keep each
	 * run's state, and do a final backtrace. */
	int rc = ENOMEM;
	struct diff_data *left = &state->left;
	struct diff_data *right = &state->right;
	int *kd_buf;

	debug("\n** %s\n", __func__);
	debug("left:\n");
	debug_dump(left);
	debug("right:\n");
	debug_dump(right);
	debug_dump_myers_graph(left, right, NULL, NULL, 0, NULL, 0);

	/* Allocate two columns of a Myers graph, one for the forward and one
	 * for the backward traversal. */
	unsigned int max = left->atoms.len + right->atoms.len;
	size_t kd_len = max + 1 + max;
	size_t kd_buf_size = kd_len * kd_len;
	size_t kd_state_size = kd_buf_size * sizeof(int);
	debug("state size: %zu\n", kd_state_size);
	if (kd_buf_size < kd_len /* overflow? */
	    || (SIZE_MAX / kd_len ) < kd_len
	    || kd_state_size > algo_config->permitted_state_size) {
		debug("state size %zu > permitted_state_size %zu, use fallback_algo\n",
		      kd_state_size, algo_config->permitted_state_size);
		return DIFF_RC_USE_DIFF_ALGO_FALLBACK;
	}

	if (state->kd_buf_size < kd_buf_size) {
		kd_buf = reallocarray(state->kd_buf, kd_buf_size,
		    sizeof(int));
		if (!kd_buf)
			return ENOMEM;
		state->kd_buf = kd_buf;
		state->kd_buf_size = kd_buf_size;
	} else
		kd_buf = state->kd_buf;

	int i;
	for (i = 0; i < kd_buf_size; i++)
		kd_buf[i] = -1;

	/* The 'k' axis in Myers spans positive and negative indexes, so point
	 * the kd to the middle.
	 * It is then possible to index from -max .. max. */
	int *kd_origin = kd_buf + max;
	int *kd_column = kd_origin;

	int d;
	int backtrack_d = -1;
	int backtrack_k = 0;
	int k;
	int x, y;
	for (d = 0; d <= max; d++, kd_column += kd_len) {
		debug("-- %s d=%d\n", __func__, d);

		for (k = d; k >= -d; k -= 2) {
			if (k < -(int)right->atoms.len
			    || k > (int)left->atoms.len) {
				/* This diagonal is completely outside of the
				 * Myers graph, don't calculate it. */
				if (k < -(int)right->atoms.len)
					debug(" %d k <"
					      " -(int)right->atoms.len %d\n",
					      k, -(int)right->atoms.len);
				else
					debug(" %d k > left->atoms.len %d\n", k,
					      left->atoms.len);
				if (k < 0) {
					/* We are traversing negatively, and
					 * already below the entire graph,
					 * nothing will come of this. */
					debug(" break\n");
					break;
				}
				debug(" continue\n");
				continue;
			}

			if (d == 0) {
				/* This is the initializing step. There is no
				 * prev_k yet, get the initial x from the top
				 * left of the Myers graph. */
				x = 0;
			} else {
				int *kd_prev_column = kd_column - kd_len;

				/* Favoring "-" lines first means favoring
				 * moving rightwards in the Myers graph.
				 * For this, all k should derive from k - 1,
				 * only the bottom most k derive from k + 1:
				 *
				 *      | d=   0   1   2
				 *  ----+----------------
				 *  k=  |
				 *   2  |             2,0 <-- from
				 *      |            /        prev_k = 2 - 1 = 1
				 *   1  |         1,0
				 *      |        /
				 *   0  |  -->0,0     3,3
				 *      |       \\   /
				 *  -1  |         0,1 <-- bottom most for d=1
				 *      |           \\    from prev_k = -1+1 = 0
				 *  -2  |             0,2 <-- bottom most for
				 *                            d=2 from
				 *                            prev_k = -2+1 = -1
				 *
				 * Except when a k + 1 from a previous run
				 * already means a further advancement in the
				 * graph.
				 * If k == d, there is no k + 1 and k - 1 is the
				 * only option.
				 * If k < d, use k + 1 in case that yields a
				 * larger x. Also use k + 1 if k - 1 is outside
				 * the graph.
				 */
				if (k > -d
				    && (k == d
					|| (k - 1 >= -(int)right->atoms.len
					    && kd_prev_column[k - 1]
					       >= kd_prev_column[k + 1]))) {
					/* Advance from k - 1.
					 * From position prev_k, step to the
					 * right in the Myers graph: x += 1.
					 */
					int prev_k = k - 1;
					int prev_x = kd_prev_column[prev_k];
					x = prev_x + 1;
				} else {
					/* The bottom most one.
					 * From position prev_k, step to the
					 * bottom in the Myers graph: y += 1.
					 * Incrementing y is achieved by
					 * decrementing k while keeping the same
					 * x. (since we're deriving y from y =
					 * x - k).
					 */
					int prev_k = k + 1;
					int prev_x = kd_prev_column[prev_k];
					x = prev_x;
				}
			}

			/* Slide down any snake that we might find here. */
			while (x < left->atoms.len
			       && xk_to_y(x, k) < right->atoms.len) {
				bool same;
				int r = diff_atom_same(&same,
						       &left->atoms.head[x],
						       &right->atoms.head[
							xk_to_y(x, k)]);
				if (r)
					return r;
				if (!same)
					break;
				x++;
			}
			kd_column[k] = x;

			if (x == left->atoms.len
			    && xk_to_y(x, k) == right->atoms.len) {
				/* Found a path */
				backtrack_d = d;
				backtrack_k = k;
				debug("Reached the end at d = %d, k = %d\n",
				      backtrack_d, backtrack_k);
				break;
			}
		}

		if (backtrack_d >= 0)
			break;
	}

	debug_dump_myers_graph(left, right, kd_origin, NULL, 0, NULL, 0);

	/* backtrack. A matrix spanning from start to end of the file is ready:
	 *
	 *      | d=   0   1   2   3   4
	 *  ----+---------------------------------
	 *  k=  |
	 *   3  |
	 *      |
	 *   2  |             2,0
	 *      |            /
	 *   1  |         1,0     4,3
	 *      |        /       /   \
	 *   0  |  -->0,0     3,3     4,4 --> backtrack_d = 4, backtrack_k = 0
	 *      |        \   /   \
	 *  -1  |         0,1     3,4
	 *      |            \
	 *  -2  |             0,2
	 *      |
	 *
	 * From (4,4) backwards, find the previous position that is the largest, and remember it.
	 *
	 */
	for (d = backtrack_d, k = backtrack_k; d >= 0; d--) {
		x = kd_column[k];
		y = xk_to_y(x, k);

		/* When the best position is identified, remember it for that
		 * kd_column.
		 * That kd_column is no longer needed otherwise, so just
		 * re-purpose kd_column[0] = x and kd_column[1] = y,
		 * so that there is no need to allocate more memory.
		 */
		kd_column[0] = x;
		kd_column[1] = y;
		debug("Backtrack d=%d: xy=(%d, %d)\n",
		      d, kd_column[0], kd_column[1]);

		/* Don't access memory before kd_buf */
		if (d == 0)
			break;
		int *kd_prev_column = kd_column - kd_len;

		/* When y == 0, backtracking downwards (k-1) is the only way.
		 * When x == 0, backtracking upwards (k+1) is the only way.
		 *
		 *      | d=   0   1   2   3   4
		 *  ----+---------------------------------
		 *  k=  |
		 *   3  |
		 *      |                ..y == 0
		 *   2  |             2,0
		 *      |            /
		 *   1  |         1,0     4,3
		 *      |        /       /   \
		 *   0  |  -->0,0     3,3     4,4 --> backtrack_d = 4,
		 *      |        \   /   \            backtrack_k = 0
		 *  -1  |         0,1     3,4
		 *      |            \
		 *  -2  |             0,2__
		 *      |                  x == 0
		 */
		if (y == 0
		    || (x > 0
			&& kd_prev_column[k - 1] >= kd_prev_column[k + 1])) {
			k = k - 1;
			debug("prev k=k-1=%d x=%d y=%d\n",
			      k, kd_prev_column[k],
			      xk_to_y(kd_prev_column[k], k));
		} else {
			k = k + 1;
			debug("prev k=k+1=%d x=%d y=%d\n",
			      k, kd_prev_column[k],
			      xk_to_y(kd_prev_column[k], k));
		}
		kd_column = kd_prev_column;
	}

	/* Forwards again, this time recording the diff chunks.
	 * Definitely start from 0,0. kd_column[0] may actually point to the
	 * bottom of a snake starting at 0,0 */
	x = 0;
	y = 0;

	kd_column = kd_origin;
	for (d = 0; d <= backtrack_d; d++, kd_column += kd_len) {
		int next_x = kd_column[0];
		int next_y = kd_column[1];
		debug("Forward track from xy(%d,%d) to xy(%d,%d)\n",
		      x, y, next_x, next_y);

		struct diff_atom *left_atom = &left->atoms.head[x];
		int left_section_len = next_x - x;
		struct diff_atom *right_atom = &right->atoms.head[y];
		int right_section_len = next_y - y;

		rc = ENOMEM;
		if (left_section_len && right_section_len) {
			/* This must be a snake slide.
			 * Snake slides have a straight line leading into them
			 * (except when starting at (0,0)). Find out whether the
			 * lead-in is horizontal or vertical:
			 *
			 *     left
			 *  ---------->
			 *  |
			 * r|   o-o        o
			 * i|      \       |
			 * g|       o      o
			 * h|        \      \
			 * t|         o      o
			 *  v
			 *
			 * If left_section_len > right_section_len, the lead-in
			 * is horizontal, meaning first remove one atom from the
			 * left before sliding down the snake.
			 * If right_section_len > left_section_len, the lead-in
			 * is vetical, so add one atom from the right before
			 * sliding down the snake. */
			if (left_section_len == right_section_len + 1) {
				if (!diff_state_add_chunk(state, true,
							  left_atom, 1,
							  right_atom, 0))
					goto return_rc;
				left_atom++;
				left_section_len--;
			} else if (right_section_len == left_section_len + 1) {
				if (!diff_state_add_chunk(state, true,
							  left_atom, 0,
							  right_atom, 1))
					goto return_rc;
				right_atom++;
				right_section_len--;
			} else if (left_section_len != right_section_len) {
				/* The numbers are making no sense. Should never
				 * happen. */
				rc = DIFF_RC_USE_DIFF_ALGO_FALLBACK;
				goto return_rc;
			}

			if (!diff_state_add_chunk(state, true,
						  left_atom, left_section_len,
						  right_atom,
						  right_section_len))
				goto return_rc;
		} else if (left_section_len && !right_section_len) {
			/* Only left atoms and none on the right, they form a
			 * "minus" chunk, then. */
			if (!diff_state_add_chunk(state, true,
						  left_atom, left_section_len,
						  right_atom, 0))
				goto return_rc;
		} else if (!left_section_len && right_section_len) {
			/* No left atoms, only atoms on the right, they form a
			 * "plus" chunk, then. */
			if (!diff_state_add_chunk(state, true,
						  left_atom, 0,
						  right_atom,
						  right_section_len))
				goto return_rc;
		}

		x = next_x;
		y = next_y;
	}

	rc = DIFF_RC_OK;

return_rc:
	debug("** END %s rc=%d\n", __func__, rc);
	return rc;
}
