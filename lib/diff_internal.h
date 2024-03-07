/* Generic infrastructure to implement various diff algorithms. */
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

#ifndef MAX
#define MAX(A,B) ((A)>(B)?(A):(B))
#endif
#ifndef MIN
#define MIN(A,B) ((A)<(B)?(A):(B))
#endif

static inline bool
diff_range_empty(const struct diff_range *r)
{
	return r->start == r->end;
}

static inline bool
diff_ranges_touch(const struct diff_range *a, const struct diff_range *b)
{
	return (a->end >= b->start) && (a->start <= b->end);
}

static inline void
diff_ranges_merge(struct diff_range *a, const struct diff_range *b)
{
	*a = (struct diff_range){
		.start = MIN(a->start, b->start),
		.end = MAX(a->end, b->end),
	};
}

static inline int
diff_range_len(const struct diff_range *r)
{
	if (!r)
		return 0;
	return r->end - r->start;
}

/* Indicate whether two given diff atoms match. */
int
diff_atom_same(bool *same,
	       const struct diff_atom *left,
	       const struct diff_atom *right);

/* A diff chunk represents a set of atoms on the left and/or a set of atoms on
 * the right.
 *
 * If solved == false:
 * The diff algorithm has divided the source file, and this is a chunk that the
 * inner_algo should run on next.
 * The lines on the left should be diffed against the lines on the right.
 * (If there are no left lines or no right lines, it implies solved == true,
 * because there is nothing to diff.)
 *
 * If solved == true:
 * If there are only left atoms, it is a chunk removing atoms from the left ("a
 * minus chunk").
 * If there are only right atoms, it is a chunk adding atoms from the right ("a
 * plus chunk").
 * If there are both left and right lines, it is a chunk of equal content on
 * both sides, and left_count == right_count:
 *
 * - foo  }
 * - bar  }-- diff_chunk{ left_start = &left.atoms.head[0], left_count = 3,
 * - baz  }               right_start = NULL, right_count = 0 }
 *   moo    }
 *   goo    }-- diff_chunk{ left_start = &left.atoms.head[3], left_count = 3,
 *   zoo    }              right_start = &right.atoms.head[0], right_count = 3 }
 *  +loo      }
 *  +roo      }-- diff_chunk{ left_start = NULL, left_count = 0,
 *  +too      }            right_start = &right.atoms.head[3], right_count = 3 }
 *
 */
struct diff_chunk {
	bool solved;
	struct diff_atom *left_start;
	unsigned int left_count;
	struct diff_atom *right_start;
	unsigned int right_count;
};

#define DIFF_RESULT_ALLOC_BLOCKSIZE 128

struct diff_chunk_context;

bool
diff_chunk_context_empty(const struct diff_chunk_context *cc);

bool
diff_chunk_contexts_touch(const struct diff_chunk_context *cc,
			  const struct diff_chunk_context *other);

void
diff_chunk_contexts_merge(struct diff_chunk_context *cc,
			  const struct diff_chunk_context *other);

struct diff_state {
	/* The final result passed to the original diff caller. */
	struct diff_result *result;

	/* The root diff_data is in result->left,right, these are (possibly)
	 * subsections of the root data. */
	struct diff_data left;
	struct diff_data right;

	unsigned int recursion_depth_left;

	/* Remaining chunks from one diff algorithm pass, if any solved == false
	 * chunks came up. */
	diff_chunk_arraylist_t temp_result;

 	/* State buffer used by Myers algorithm. */
	int *kd_buf;
	size_t kd_buf_size; /* in units of sizeof(int), not bytes */
};

struct diff_chunk *diff_state_add_chunk(struct diff_state *state, bool solved,
					struct diff_atom *left_start,
					unsigned int left_count,
					struct diff_atom *right_start,
					unsigned int right_count);

struct diff_output_info;

int diff_output_lines(struct diff_output_info *output_info, FILE *dest,
		       const char *prefix, struct diff_atom *start_atom,
		       unsigned int count);

int diff_output_trailing_newline_msg(struct diff_output_info *outinfo,
				     FILE *dest,
				     const struct diff_chunk *c);
#define DIFF_FUNCTION_CONTEXT_SIZE	55
int diff_output_match_function_prototype(char *prototype, size_t prototype_size,
					 int *last_prototype_idx,
					 const struct diff_result *result,
					 const struct diff_chunk_context *cc);

struct diff_output_info *diff_output_info_alloc(void);

void
diff_data_init_subsection(struct diff_data *d, struct diff_data *parent,
			  struct diff_atom *from_atom, unsigned int atoms_count);
