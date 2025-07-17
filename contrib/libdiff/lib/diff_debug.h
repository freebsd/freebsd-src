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

#define DEBUG 0

#if DEBUG
#include <stdio.h>
#include <unistd.h>
#define print(args...) fprintf(stderr, ##args)
#define debug print
#define debug_dump dump
#define debug_dump_atom dump_atom
#define debug_dump_atoms dump_atoms

static inline void
print_atom_byte(unsigned char c) {
	if (c == '\r')
		print("\\r");
	else if (c == '\n')
		print("\\n");
	else if ((c < 32 || c >= 127) && (c != '\t'))
		print("\\x%02x", c);
	else
		print("%c", c);
}

static inline void
dump_atom(const struct diff_data *left, const struct diff_data *right,
	  const struct diff_atom *atom)
{
	if (!atom) {
		print("NULL atom\n");
		return;
	}
	if (left)
		print(" %3u '", diff_atom_root_idx(left, atom));

	if (atom->at == NULL) {
		off_t remain = atom->len;
		if (fseek(atom->root->f, atom->pos, SEEK_SET) == -1)
			abort(); /* cannot return error */
		while (remain > 0) {
			char buf[16];
			size_t r;
			int i;
			r = fread(buf, 1, MIN(remain, sizeof(buf)),
			    atom->root->f);
			if (r == 0)
				break;
			remain -= r;
			for (i = 0; i < r; i++)
				print_atom_byte(buf[i]);
		}
	} else {
		const char *s;
		for (s = atom->at; s < (const char*)(atom->at + atom->len); s++)
			print_atom_byte(*s);
	}
	print("'\n");
}

static inline void
dump_atoms(const struct diff_data *d, struct diff_atom *atom,
	   unsigned int count)
{
	if (count > 42) {
		dump_atoms(d, atom, 20);
		print("[%u lines skipped]\n", count - 20 - 20);
		dump_atoms(d, atom + count - 20, 20);
		return;
	} else {
		struct diff_atom *i;
		foreach_diff_atom(i, atom, count) {
			dump_atom(d, NULL, i);
		}
	}
}

static inline void
dump(struct diff_data *d)
{
	dump_atoms(d, d->atoms.head, d->atoms.len);
}

/* kd is a quadratic space myers matrix from the original Myers algorithm.
 * kd_forward and kd_backward are linear slices of a myers matrix from the Myers
 * Divide algorithm.
 */
static inline void
dump_myers_graph(const struct diff_data *l, const struct diff_data *r,
		 int *kd, int *kd_forward, int kd_forward_d,
		 int *kd_backward, int kd_backward_d)
{
	#define COLOR_YELLOW "\033[1;33m"
	#define COLOR_GREEN "\033[1;32m"
	#define COLOR_BLUE "\033[1;34m"
	#define COLOR_RED "\033[1;31m"
	#define COLOR_END "\033[0;m"
	int x;
	int y;
	print("   ");
	for (x = 0; x <= l->atoms.len; x++)
		print("%2d", x % 100);
	print("\n");

	for (y = 0; y <= r->atoms.len; y++) {
		print("%3d ", y);
		for (x = 0; x <= l->atoms.len; x++) {

			/* print d advancements from kd, if any. */
			char label = 'o';
			char *color = NULL;
			if (kd) {
				int max = l->atoms.len + r->atoms.len;
				size_t kd_len = max + 1 + max;
				int *kd_pos = kd;
				int di;
#define xk_to_y(X, K) ((X) - (K))
				for (di = 0; di < max; di++) {
					int ki;
					for (ki = di; ki >= -di; ki -= 2) {
						if (x != kd_pos[ki]
						    || y != xk_to_y(x, ki))
							continue;
						label = '0' + (di % 10);
						color = COLOR_YELLOW;
						break;
					}
					if (label != 'o')
						break;
					kd_pos += kd_len;
				}
			}
			if (kd_forward && kd_forward_d >= 0) {
#define xc_to_y(X, C, DELTA) ((X) - (C) + (DELTA))
				int ki;
				for (ki = kd_forward_d;
				     ki >= -kd_forward_d;
				     ki -= 2) {
					if (x != kd_forward[ki])
						continue;
					if (y != xk_to_y(x, ki))
						continue;
					label = 'F';
					color = COLOR_GREEN;
					break;
				}
			}
			if (kd_backward && kd_backward_d >= 0) {
				int delta = (int)r->atoms.len
					    - (int)l->atoms.len;
				int ki;
				for (ki = kd_backward_d;
				     ki >= -kd_backward_d;
				     ki -= 2) {
					if (x != kd_backward[ki])
						continue;
					if (y != xc_to_y(x, ki, delta))
						continue;
					if (label == 'o') {
						label = 'B';
						color = COLOR_BLUE;
					} else {
						label = 'X';
						color = COLOR_RED;
					}
					break;
				}
			}
			if (color)
				print("%s", color);
			print("%c", label);
			if (color)
				print("%s", COLOR_END);
			if (x < l->atoms.len)
				print("-");
		}
		print("\n");
		if (y == r->atoms.len)
			break;

		print("    ");
		for (x = 0; x < l->atoms.len; x++) {
			bool same;
			diff_atom_same(&same, &l->atoms.head[x],
			    &r->atoms.head[y]);
			if (same)
				print("|\\");
			else
				print("| ");
		}
		print("|\n");
	}
}

static inline void
debug_dump_myers_graph(const struct diff_data *l, const struct diff_data *r,
		       int *kd, int *kd_forward, int kd_forward_d,
		       int *kd_backward, int kd_backward_d)
{
	if (l->atoms.len > 99 || r->atoms.len > 99)
		return;
	dump_myers_graph(l, r, kd, kd_forward, kd_forward_d,
			 kd_backward, kd_backward_d);
}

#else
#define debug(args...)
#define debug_dump(args...)
#define debug_dump_atom(args...)
#define debug_dump_atoms(args...)
#define debug_dump_myers_graph(args...)
#endif
