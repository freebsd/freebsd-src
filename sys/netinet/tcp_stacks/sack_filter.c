/*-
 * Copyright (c) 2017-9 Netflix, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 */
#include <sys/cdefs.h>
#ifndef _KERNEL
#define _WANT_TCPCB 1
#endif
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#ifdef _KERNEL
#include <sys/mbuf.h>
#include <sys/sockopt.h>
#endif
#include <netinet/in.h>
#ifdef _KERNEL
#include <netinet/in_pcb.h>
#else
struct inpcb {
	uint32_t stuff;
};
#endif
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_seq.h>
#ifndef _KERNEL
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#endif
#include "sack_filter.h"

/*
 * Sack filter is used to filter out sacks
 * that have already been processed. The idea
 * is pretty simple really, consider two sacks
 *
 * SACK 1
 *   cum-ack A
 *     sack B - C
 * SACK 2
 *   cum-ack A
 *     sack D - E
 *     sack B - C
 *
 * The previous sack information (B-C) is repeated
 * in SACK 2. If the receiver gets SACK 1 and then
 * SACK 2 then any work associated with B-C as already
 * been completed. This only effects where we may have
 * (as in bbr or rack) cases where we walk a linked list.
 *
 * Now the utility trys to keep everything in a single
 * cache line. This means that its not perfect and
 * it could be that so big of sack's come that a
 * "remembered" processed sack falls off the list and
 * so gets re-processed. Thats ok, it just means we
 * did some extra work. We could of course take more
 * cache line hits by expanding the size of this
 * structure, but then that would cost more.
 */

#ifndef _KERNEL
int detailed_dump = 0;
uint64_t cnt_skipped_oldsack = 0;
uint64_t cnt_used_oldsack = 0;
int highest_used=0;
int over_written=0;
int empty_avail=0;
FILE *out = NULL;
FILE *in = NULL;

#endif

#define sack_blk_used(sf, i) ((1 << i) & sf->sf_bits)
#define sack_blk_set(sf, i) ((1 << i) | sf->sf_bits)
#define sack_blk_clr(sf, i) (~(1 << i) & sf->sf_bits)

#ifndef _KERNEL

static u_int tcp_fixed_maxseg(const struct tcpcb *tp)
{
	/* Lets pretend their are timestamps on for user space */
	return (tp->t_maxseg - 12);
}

static
#endif
void
sack_filter_clear(struct sack_filter *sf, tcp_seq seq)
{
	sf->sf_ack = seq;
	sf->sf_bits = 0;
	sf->sf_cur = 0;
	sf->sf_used = 0;
}
/*
 * Given a previous sack filter block, filter out
 * any entries where the cum-ack moves over them
 * fully or partially.
 */
static void
sack_filter_prune(struct sack_filter *sf, tcp_seq th_ack)
{
	int32_t i;
	/* start with the oldest */
	for (i = 0; i < SACK_FILTER_BLOCKS; i++) {
		if (sack_blk_used(sf, i)) {
			if (SEQ_GEQ(th_ack, sf->sf_blks[i].end)) {
				/* This block is consumed */
				sf->sf_bits = sack_blk_clr(sf, i);
				sf->sf_used--;
			} else if (SEQ_GT(th_ack, sf->sf_blks[i].start)) {
				/* Some of it is acked */
				sf->sf_blks[i].start = th_ack;
				/* We could in theory break here, but
				 * there are some broken implementations
				 * that send multiple blocks. We want
				 * to catch them all with similar seq's.
				 */
			}
		}
	}
	sf->sf_ack = th_ack;
}

/*
 * Return true if you find that
 * the sackblock b is on the score
 * board. Update it along the way
 * if part of it is on the board.
 */
static int32_t
is_sack_on_board(struct sack_filter *sf, struct sackblk *b, int32_t segmax, uint32_t snd_max)
{
	int32_t i, cnt;
	int span_cnt = 0;
	uint32_t span_start, span_end;

	if (SEQ_LT(b->start, sf->sf_ack)) {
		/* Behind cum-ack update */
		b->start = sf->sf_ack;
	}
	if (SEQ_LT(b->end, sf->sf_ack)) {
		/* End back behind too */
		b->end = sf->sf_ack;
	}
	if (b->start == b->end) {
		return(1);
	}
	span_start = b->start;
	span_end = b->end;
	for (i = sf->sf_cur, cnt=0; cnt < SACK_FILTER_BLOCKS; cnt++) {
		if (sack_blk_used(sf, i)) {
			/* Jonathans Rule 1 */
			if (SEQ_LEQ(sf->sf_blks[i].start, b->start) &&
			    SEQ_GEQ(sf->sf_blks[i].end, b->end)) {
				/**
				 * Our board has this entirely in
				 * whole or in part:
				 *
				 * board  |-------------|
				 * sack   |-------------|
				 * <or>
				 * board  |-------------|
				 * sack       |----|
				 *
				 */
				return(1);
			}
			/* Jonathans Rule 2 */
			if(SEQ_LT(sf->sf_blks[i].end, b->start)) {
				/**
				 * Not near each other:
				 *
				 * board   |---|
				 * sack           |---|
				 */
				if ((b->end != snd_max) &&
				    (span_cnt < 2) &&
				    ((b->end - b->start) < segmax)) {
					/*
					 * Too small for us to mess with so we
					 * pretend its on the board.
					 */
					return (1);
				}
				goto nxt_blk;
			}
			/* Jonathans Rule 3 */
			if (SEQ_GT(sf->sf_blks[i].start, b->end)) {
				/**
				 * Not near each other:
				 *
				 * board         |---|
				 * sack  |---|
				 */
				if ((b->end != snd_max) &&
				    (sf->sf_blks[i].end != snd_max) &&
				    (span_cnt < 2) &&
				    ((b->end - b->start) < segmax)) {
					/*
					 * Too small for us to mess with so we
					 * pretend its on the board.
					 */
					return (1);
				}
				goto nxt_blk;
			}
			if (SEQ_LEQ(sf->sf_blks[i].start, b->start)) {
				/**
				 * The board block partial meets:
				 *
				 *  board   |--------|
				 *  sack        |----------|
				 *    <or>
				 *  board   |--------|
				 *  sack    |--------------|
				 *
				 * up with this one (we have part of it).
				 *
				 * 1) Update the board block to the new end
				 *      and
				 * 2) Update the start of this block to my end.
				 *
				 * We only do this if the new piece is large enough.
				 */
				if (((b->end != snd_max) || (sf->sf_blks[i].end == snd_max)) &&
				    (span_cnt == 0) &&
				    ((b->end - sf->sf_blks[i].end) < segmax)) {
					/*
					 * Too small for us to mess with so we
					 * pretend its on the board.
					 */
					return (1);
				}
				b->start = sf->sf_blks[i].end;
				sf->sf_blks[i].end = b->end;
				if (span_cnt == 0) {
					span_start = sf->sf_blks[i].start;
					span_end = sf->sf_blks[i].end;
				} else {
					if (SEQ_LT(span_start, sf->sf_blks[i].start)) {
						span_start = sf->sf_blks[i].start;
					}
					if (SEQ_GT(span_end, sf->sf_blks[i].end)) {
						span_end = sf->sf_blks[i].end;
					}
				}
				span_cnt++;
				goto nxt_blk;
			}
			if (SEQ_GEQ(sf->sf_blks[i].end, b->end)) {
				/**
				 * The board block partial meets:
				 *
				 *  board       |--------|
				 *  sack  |----------|
				 *     <or>
				 *  board       |----|
				 *  sack  |----------|
				 *
				 * 1) Update the board block to the new start
				 *      and
				 * 2) Update the start of this block to my end.
				 *
				 * We only do this if the new piece is large enough.
				 */
				if (((b->end != snd_max) || (sf->sf_blks[i].end == snd_max)) &&
				    (span_cnt == 0) &&
				    ((sf->sf_blks[i].start - b->start) < segmax)) {
					/*
					 * Too small for us to mess with so we
					 * pretend its on the board.
					 */
					return (1);
				}
				b->end = sf->sf_blks[i].start;
				sf->sf_blks[i].start = b->start;
				if (span_cnt == 0) {
					span_start = sf->sf_blks[i].start;
					span_end = sf->sf_blks[i].end;
				} else {
					if (SEQ_LT(span_start, sf->sf_blks[i].start)) {
						span_start = sf->sf_blks[i].start;
					}
					if (SEQ_GT(span_end, sf->sf_blks[i].end)) {
						span_end = sf->sf_blks[i].end;
					}
				}
				span_cnt++;
				goto nxt_blk;
			}
		}
	nxt_blk:
		i++;
		i %= SACK_FILTER_BLOCKS;
	}
	/* Did we totally consume it in pieces? */
	if (b->start != b->end) {
		if ((b->end != snd_max) &&
		    ((b->end - b->start) < segmax) &&
		    ((span_end - span_start) < segmax)) {
			/*
			 * Too small for us to mess with so we
			 * pretend its on the board.
			 */
			return (1);
		}
		return(0);
	} else {
		/*
		 * It was all consumed by the board.
		 */
		return(1);
	}
}

/*
 * Given idx its used but there is space available
 * move the entry to the next free slot
 */
static void
sack_move_to_empty(struct sack_filter *sf, uint32_t idx)
{
	int32_t i, cnt;

	i = (idx + 1) % SACK_FILTER_BLOCKS;
	for (cnt=0; cnt <(SACK_FILTER_BLOCKS-1); cnt++) {
		if (sack_blk_used(sf, i) == 0) {
			memcpy(&sf->sf_blks[i], &sf->sf_blks[idx], sizeof(struct sackblk));
			sf->sf_bits = sack_blk_clr(sf, idx);
			sf->sf_bits = sack_blk_set(sf, i);
			return;
		}
		i++;
		i %= SACK_FILTER_BLOCKS;
	}
}

static int32_t
sack_filter_run(struct sack_filter *sf, struct sackblk *in, int numblks, tcp_seq th_ack, int32_t segmax, uint32_t snd_max)
{
	struct sackblk blkboard[TCP_MAX_SACK];
	int32_t num, i, room, at;
	/*
	 * First lets trim the old and possibly
	 * throw any away we have.
	 */
	for(i=0, num=0; i<numblks; i++) {
		if (is_sack_on_board(sf, &in[i], segmax, snd_max))
			continue;
		memcpy(&blkboard[num], &in[i], sizeof(struct sackblk));
		num++;
	}
	if (num == 0) {
		return(num);
	}

	/*
	 * Calculate the space we have in the filter table.
	 */
	room = SACK_FILTER_BLOCKS - sf->sf_used;
	if (room < 1)
		return (0);
	/*
	 * Now lets walk through our filtered blkboard (the previous loop
	 * trimmed off anything on the board we already have so anything
	 * in blkboard is unique and not seen before) if there is room we copy
	 * it back out and place a new entry on our board.
	 */
	for(i=0, at=0; i<num; i++) {
		if (room == 0) {
			/* Can't copy out any more, no more room */
			break;
		}
		/* Copy it out to the outbound */
		memcpy(&in[at], &blkboard[i], sizeof(struct sackblk));
		at++;
		room--;
		/* now lets add it to our sack-board */
		sf->sf_cur++;
		sf->sf_cur %= SACK_FILTER_BLOCKS;
		if ((sack_blk_used(sf, sf->sf_cur)) &&
		    (sf->sf_used < SACK_FILTER_BLOCKS)) {
			sack_move_to_empty(sf, sf->sf_cur);
		}
		memcpy(&sf->sf_blks[sf->sf_cur], &blkboard[i], sizeof(struct sackblk));
		if (sack_blk_used(sf, sf->sf_cur) == 0) {
			sf->sf_used++;
#ifndef _KERNEL
			if (sf->sf_used > highest_used)
				highest_used = sf->sf_used;
#endif
			sf->sf_bits = sack_blk_set(sf, sf->sf_cur);
		}
	}
	return(at);
}

/*
 * Collapse entry src into entry into
 * and free up the src entry afterwards.
 */
static void
sack_collapse(struct sack_filter *sf, int32_t src, int32_t into)
{
	if (SEQ_LT(sf->sf_blks[src].start, sf->sf_blks[into].start)) {
		/* src has a lower starting point */
		sf->sf_blks[into].start = sf->sf_blks[src].start;
	}
	if (SEQ_GT(sf->sf_blks[src].end, sf->sf_blks[into].end)) {
		/* src has a higher ending point */
		sf->sf_blks[into].end = sf->sf_blks[src].end;
	}
	sf->sf_bits = sack_blk_clr(sf, src);
	sf->sf_used--;
}

/*
 * Given a sack block on the board (the skip index) see if
 * any other used entries overlap or meet, if so return the index.
 */
static int32_t
sack_blocks_overlap_or_meet(struct sack_filter *sf, struct sackblk *sb, uint32_t skip)
{
	int32_t i;

	for(i=0; i<SACK_FILTER_BLOCKS; i++) {
		if (sack_blk_used(sf, i) == 0)
			continue;
		if (i == skip)
			continue;
		if (SEQ_GEQ(sf->sf_blks[i].end, sb->start) &&
		    SEQ_LEQ(sf->sf_blks[i].end, sb->end) &&
		    SEQ_LEQ(sf->sf_blks[i].start, sb->start)) {
			/**
			 * The two board blocks meet:
			 *
			 *  board1   |--------|
			 *  board2       |----------|
			 *    <or>
			 *  board1   |--------|
			 *  board2   |--------------|
			 *    <or>
			 *  board1   |--------|
			 *  board2   |--------|
			 */
			return(i);
		}
		if (SEQ_LEQ(sf->sf_blks[i].start, sb->end) &&
		    SEQ_GEQ(sf->sf_blks[i].start, sb->start) &&
		    SEQ_GEQ(sf->sf_blks[i].end, sb->end)) {
			/**
			 * The board block partial meets:
			 *
			 *  board       |--------|
			 *  sack  |----------|
			 *     <or>
			 *  board       |----|
			 *  sack  |----------|
			 * 1) Update the board block to the new start
			 *      and
			 * 2) Update the start of this block to my end.
			 */
			return(i);
		}
	}
	return (-1);
}

static void
sack_board_collapse(struct sack_filter *sf)
{
	int32_t i, j, i_d, j_d;

	for(i=0; i<SACK_FILTER_BLOCKS; i++) {
		if (sack_blk_used(sf, i) == 0)
			continue;
		/*
		 * Look at all other blocks but this guy
		 * to see if they overlap. If so we collapse
		 * the two blocks together.
		 */
		j = sack_blocks_overlap_or_meet(sf, &sf->sf_blks[i], i);
		if (j == -1) {
			/* No overlap */
			continue;
		}
		/*
		 * Ok j and i overlap with each other, collapse the
		 * one out furthest away from the current position.
		 */
		if (sf->sf_cur > i)
			i_d = sf->sf_cur - i;
		else
			i_d = i - sf->sf_cur;
		if (sf->sf_cur > j)
			j_d = sf->sf_cur - j;
		else
			j_d = j - sf->sf_cur;
		if (j_d > i_d) {
			sack_collapse(sf, j, i);
		} else
			sack_collapse(sf, i, j);
	}
}

#ifndef _KERNEL
uint64_t saved=0;
uint64_t tot_sack_blks=0;

static void
sack_filter_dump(FILE *out, struct sack_filter *sf)
{
	int i;
	fprintf(out, "	sf_ack:%u sf_bits:0x%x c:%d used:%d\n",
		sf->sf_ack, sf->sf_bits,
		sf->sf_cur, sf->sf_used);

	for(i=0; i<SACK_FILTER_BLOCKS; i++) {
		if (sack_blk_used(sf, i)) {
			fprintf(out, "Entry:%d start:%u end:%u the block is %s\n",
				i,
				sf->sf_blks[i].start,
				sf->sf_blks[i].end,
				(sack_blk_used(sf, i) ? "USED" : "NOT-USED")
				);
		}
	}
}
#endif

#ifndef _KERNEL
static
#endif
int
sack_filter_blks(struct tcpcb *tp, struct sack_filter *sf, struct sackblk *in, int numblks,
		 tcp_seq th_ack)
{
	int32_t i, ret;
	int32_t segmax;

	if (numblks > TCP_MAX_SACK) {
#ifdef _KERNEL
		panic("sf:%p sb:%p Impossible number of sack blocks %d > 4\n",
		      sf, in,
		      numblks);
#endif
		return(numblks);
	}
	if (sf->sf_used > 1)
		sack_board_collapse(sf);

	segmax = tcp_fixed_maxseg(tp);
	if ((sf->sf_used == 0) && numblks) {
		/*
		 * We are brand new add the blocks in
		 * reverse order. Note we can see more
		 * than one in new, since ack's could be lost.
		 */
		int cnt_added = 0;

		sf->sf_ack = th_ack;
		for(i=0, sf->sf_cur=0; i<numblks; i++) {
			if ((in[i].end != tp->snd_max) &&
			    ((in[i].end - in[i].start) < segmax)) {
				/*
				 * We do not accept blocks less than a MSS minus all
				 * possible options space that is not at max_seg.
				 */
				continue;
			}
			memcpy(&sf->sf_blks[sf->sf_cur], &in[i], sizeof(struct sackblk));
			sf->sf_bits = sack_blk_set(sf, sf->sf_cur);
			sf->sf_cur++;
			sf->sf_cur %= SACK_FILTER_BLOCKS;
			sf->sf_used++;
			cnt_added++;
#ifndef _KERNEL
			if (sf->sf_used > highest_used)
				highest_used = sf->sf_used;
#endif
		}
		if (sf->sf_cur)
			sf->sf_cur--;

		return (cnt_added);
	}
	if (SEQ_GT(th_ack, sf->sf_ack)) {
		sack_filter_prune(sf, th_ack);
	}
	if (numblks) {
		ret = sack_filter_run(sf, in, numblks, th_ack, segmax, tp->snd_max);
		if (sf->sf_used > 1)
			sack_board_collapse(sf);
	} else
		ret = 0;
	return (ret);
}

void
sack_filter_reject(struct sack_filter *sf, struct sackblk *in)
{
	/*
	 * Given a specified block (that had made
	 * it past the sack filter). Reject that
	 * block triming it off any sack-filter block
	 * that has it. Usually because the block was
	 * too small and did not cover a whole send.
	 *
	 * This function will only "undo" sack-blocks
	 * that are fresh and touch the edges of
	 * blocks in our filter.
	 */
	int i;

	for(i=0; i<SACK_FILTER_BLOCKS; i++) {
		if (sack_blk_used(sf, i) == 0)
			continue;
		/*
		 * Now given the sack-filter block does it touch
		 * with one of the ends
		 */
		if (sf->sf_blks[i].end == in->end) {
			/* The end moves back to start */
			if (SEQ_GT(in->start, sf->sf_blks[i].start))
				/* in-blk       |----| */
				/* sf-blk  |---------| */
				sf->sf_blks[i].end = in->start;
			else {
				/* It consumes this block */
				/* in-blk  |---------| */
				/* sf-blk     |------| */
				/* <or> */
				/* sf-blk  |---------| */
				sf->sf_bits = sack_blk_clr(sf, i);
				sf->sf_used--;
			}
			continue;
		}
		if (sf->sf_blks[i].start == in->start) {
			if (SEQ_LT(in->end, sf->sf_blks[i].end)) {
				/* in-blk  |----|      */
				/* sf-blk  |---------| */
				sf->sf_blks[i].start = in->end;
			} else {
				/* It consumes this block */
				/* in-blk  |----------|  */
				/* sf-blk  |-------|     */
				/* <or> */
				/* sf-blk  |----------|  */
				sf->sf_bits = sack_blk_clr(sf, i);
				sf->sf_used--;
			}
			continue;
		}
	}
}

#ifndef _KERNEL

int
main(int argc, char **argv)
{
	char buffer[512];
	struct sackblk blks[TCP_MAX_SACK];
	FILE *err;
	tcp_seq th_ack;
	struct tcpcb tp;
	struct sack_filter sf;
	int32_t numblks,i;
	int snd_una_set=0;
	double a, b, c;
	int invalid_sack_print = 0;
	uint32_t chg_remembered=0;
	uint32_t sack_chg=0;
	char line_buf[10][256];
	int line_buf_at=0;

	in = stdin;
	out = stdout;
	memset(&tp, 0, sizeof(tp));
	tp.t_maxseg = 1460;

	while ((i = getopt(argc, argv, "dIi:o:?hS:")) != -1) {
		switch (i) {
		case 'S':
			tp.t_maxseg = strtol(optarg, NULL, 0);
			break;
		case 'd':
			detailed_dump = 1;
			break;
		case'I':
			invalid_sack_print = 1;
			break;
		case 'i':
			in = fopen(optarg, "r");
			if (in == NULL) {
				fprintf(stderr, "Fatal error can't open %s for input\n", optarg);
				exit(-1);
			}
			break;
		case 'o':
			out = fopen(optarg, "w");
			if (out == NULL) {
				fprintf(stderr, "Fatal error can't open %s for output\n", optarg);
				exit(-1);
			}
			break;
		default:
		case '?':
		case 'h':
			fprintf(stderr, "Use %s [ -i infile -o outfile -I -S maxseg -n -d ]\n", argv[0]);
			return(0);
			break;
		};
	}
	sack_filter_clear(&sf, 0);
	memset(buffer, 0, sizeof(buffer));
	memset(blks, 0, sizeof(blks));
	numblks = 0;
	fprintf(out, "************************************\n");
	while (fgets(buffer, sizeof(buffer), in) != NULL) {
		sprintf(line_buf[line_buf_at], "%s", buffer);
		line_buf_at++;
		if (strncmp(buffer, "quit", 4) == 0) {
			break;
		} else if (strncmp(buffer, "dump", 4) == 0) {
			sack_filter_dump(out, &sf);
		} else if (strncmp(buffer, "max:", 4) == 0) {
			tp.snd_max = strtoul(&buffer[4], NULL, 0);
		} else if (strncmp(buffer, "commit", 6) == 0) {
			int nn, ii;
			if (numblks) {
				uint32_t szof, tot_chg;
				printf("Dumping line buffer (lines:%d)\n", line_buf_at);
				for(ii=0; ii<line_buf_at; ii++) {
					fprintf(out, "%s", line_buf[ii]);
				}
				fprintf(out, "------------------------------------ call sfb() nb:%d\n", numblks);
				nn = sack_filter_blks(&tp, &sf, blks, numblks, th_ack);
				saved += numblks - nn;
				tot_sack_blks += numblks;
				for(ii=0, tot_chg=0; ii<nn; ii++) {
					szof = blks[ii].end - blks[ii].start;
					tot_chg += szof;
					fprintf(out, "sack:%u:%u [%u]\n",
					       blks[ii].start,
						blks[ii].end, szof);
				}
				fprintf(out,"************************************\n");
				chg_remembered = tot_chg;
				if (detailed_dump) {
					sack_filter_dump(out, &sf);
					fprintf(out,"************************************\n");
				}
			}
			memset(blks, 0, sizeof(blks));
			memset(line_buf, 0, sizeof(line_buf));
			line_buf_at=0;
			numblks = 0;
		} else if (strncmp(buffer, "chg:", 4) == 0) {
			sack_chg = strtoul(&buffer[4], NULL, 0);
			if ((sack_chg != chg_remembered) &&
			    (sack_chg > chg_remembered)){
				fprintf(out,"***WARNING WILL RODGERS DANGER!! sack_chg:%u last:%u\n",
					sack_chg, chg_remembered
					);
			}
			sack_chg = chg_remembered = 0;
		} else if (strncmp(buffer, "rxt", 3) == 0) {
			sack_filter_clear(&sf, tp.snd_una);
		} else if (strncmp(buffer, "ack:", 4) == 0) {
			th_ack = strtoul(&buffer[4], NULL, 0);
			if (snd_una_set == 0) {
				tp.snd_una = th_ack;
				snd_una_set = 1;
			} else if (SEQ_GT(th_ack, tp.snd_una)) {
				tp.snd_una = th_ack;
			}
			sack_filter_blks(&tp, &sf, NULL, 0, th_ack);
		} else if (strncmp(buffer, "exit", 4) == 0) {
			sack_filter_clear(&sf, tp.snd_una);
			sack_chg = chg_remembered = 0;
		} else if (strncmp(buffer, "sack:", 5) == 0) {
			char *end=NULL;
			uint32_t start;
			uint32_t endv;

			start = strtoul(&buffer[5], &end, 0);
			if (end) {
				endv = strtoul(&end[1], NULL, 0);
			} else {
				fprintf(out, "--Sack invalid skip 0 start:%u : ??\n", start);
				continue;
			}
			if (SEQ_GT(endv, tp.snd_max))
				tp.snd_max = endv;
			if (SEQ_LT(endv, start)) {
				fprintf(out, "--Sack invalid skip 1 endv:%u < start:%u\n", endv, start);
				continue;
			}
			if (numblks == TCP_MAX_SACK) {
				fprintf(out, "--Exceeded max %d\n", numblks);
				exit(0);
			}
			blks[numblks].start = start;
			blks[numblks].end = endv;
			numblks++;
		} else if (strncmp(buffer, "rej:n:n", 4) == 0) {
			struct sackblk in;
			char *end=NULL;

			in.start = strtoul(&buffer[4], &end, 0);
			if (end) {
				in.end = strtoul(&end[1], NULL, 0);
				sack_filter_reject(&sf, &in);
			} else
				fprintf(out, "Invalid input END:A:B\n");
		} else if (strncmp(buffer, "save", 4) == 0) {
			FILE *io;

			io = fopen("sack_setup.bin", "w+");
			if (io != NULL) {
				if (fwrite(&sf, sizeof(sf), 1, io) != 1) {
					printf("Failed to write out sf data\n");
					unlink("sack_setup.bin");
					goto outwrite;
				}
				if (fwrite(&tp, sizeof(tp), 1, io) != 1) {
					printf("Failed to write out tp data\n");
					unlink("sack_setup.bin");
				} else
					printf("Save completed\n");
			outwrite:
				fclose(io);
			} else {
				printf("failed to open sack_setup.bin for writting .. sorry\n");
			}
		} else if (strncmp(buffer, "restore", 7) == 0) {
			FILE *io;

			io = fopen("sack_setup.bin", "r");
			if (io != NULL) {
				if (fread(&sf, sizeof(sf), 1, io) != 1) {
					printf("Failed to read out sf data\n");
					goto outread;
				}
				if (fread(&tp, sizeof(tp), 1, io) != 1) {
					printf("Failed to read out tp data\n");
				} else {
					printf("Restore completed\n");
					sack_filter_dump(out, &sf);
				}
			outread:
				fclose(io);
			} else {
				printf("can't open sack_setup.bin -- sorry no load\n");
			}

		} else if (strncmp(buffer, "help", 4) == 0) {
help:
			fprintf(out, "You can input:\n");
			fprintf(out, "sack:S:E -- to define a sack block\n");
			fprintf(out, "rxt -- to clear the filter without changing the remembered\n");
			fprintf(out, "save -- save current state to sack_setup.bin\n");
			fprintf(out, "restore -- restore state from sack_setup.bin\n");
			fprintf(out, "exit -- To clear the sack filter and start all fresh\n");
			fprintf(out, "ack:N -- To advance the cum-ack to N\n");
			fprintf(out, "max:N -- To set send-max to N\n");
			fprintf(out, "commit -- To apply the sack you built to the filter and dump the filter\n");
			fprintf(out, "dump -- To display the current contents of the sack filter\n");
			fprintf(out, "quit -- To exit this program\n");
		} else {
			fprintf(out, "Command %s unknown\n", buffer);
			goto help;
		}
		memset(buffer, 0, sizeof(buffer));
	}
	if (in != stdin) {
		fclose(in);
	}
	if (out != stdout) {
		fclose(out);
	}
	a = saved * 100.0;
	b = tot_sack_blks * 1.0;
	if (b > 0.0)
		c = a/b;
	else
		c = 0.0;
	if (out != stdout)
		err = stdout;
	else
		err = stderr;
	fprintf(err, "Saved %lu sack blocks out of %lu (%2.3f%%) old_skip:%lu old_usd:%lu high_cnt:%d ow:%d ea:%d\n",
		saved, tot_sack_blks, c, cnt_skipped_oldsack, cnt_used_oldsack, highest_used, over_written, empty_avail);
	return(0);
}
#endif
