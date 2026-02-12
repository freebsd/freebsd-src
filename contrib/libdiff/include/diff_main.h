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

struct diff_range {
	int start;
	int end;
};

/* List of all possible return codes of a diff invocation. */
#define DIFF_RC_USE_DIFF_ALGO_FALLBACK	-1
#define DIFF_RC_OK			0
/* Any positive return values are errno values from sys/errno.h */

struct diff_atom {
	struct diff_data *root; /* back pointer to root diff data */

	off_t pos;		/* set whether memory-mapped or not */
	const uint8_t *at;	/* only set if memory-mapped */
	off_t len;

	/* This hash is just a very cheap speed up for finding *mismatching*
	 * atoms. When hashes match, we still need to compare entire atoms to
	 * find out whether they are indeed identical or not.
	 * Calculated over all atom bytes with diff_atom_hash_update(). */
	unsigned int hash;
};

/* Mix another atom_byte into the provided hash value and return the result.
 * The hash value passed in for the first byte of the atom must be zero. */
unsigned int
diff_atom_hash_update(unsigned int hash, unsigned char atom_byte);

/* Compare two atoms for equality. Return 0 on success, or errno on failure.
 * Set cmp to -1, 0, or 1, just like strcmp(). */
int
diff_atom_cmp(int *cmp,
	      const struct diff_atom *left,
	      const struct diff_atom *right);


/* The atom's index in the entire file. For atoms divided by lines of text, this
 * yields the line number (starting with 0). Also works for diff_data that
 * reference only a subsection of a file, always reflecting the global position
 * in the file (and not the relative position within the subsection). */
#define diff_atom_root_idx(DIFF_DATA, ATOM) \
	((ATOM) && ((ATOM) >= (DIFF_DATA)->root->atoms.head) \
	 ? (unsigned int)((ATOM) - ((DIFF_DATA)->root->atoms.head)) \
	 : (DIFF_DATA)->root->atoms.len)

/* The atom's index within DIFF_DATA. For atoms divided by lines of text, this
 * yields the line number (starting with 0). */
#define diff_atom_idx(DIFF_DATA, ATOM) \
	((ATOM) && ((ATOM) >= (DIFF_DATA)->atoms.head) \
	 ? (unsigned int)((ATOM) - ((DIFF_DATA)->atoms.head)) \
	 : (DIFF_DATA)->atoms.len)

#define foreach_diff_atom(ATOM, FIRST_ATOM, COUNT) \
	for ((ATOM) = (FIRST_ATOM); \
	     (ATOM) \
	     && ((ATOM) >= (FIRST_ATOM)) \
	     && ((ATOM) - (FIRST_ATOM) < (COUNT)); \
	     (ATOM)++)

#define diff_data_foreach_atom(ATOM, DIFF_DATA) \
	foreach_diff_atom(ATOM, (DIFF_DATA)->atoms.head, (DIFF_DATA)->atoms.len)

#define diff_data_foreach_atom_from(FROM, ATOM, DIFF_DATA) \
	for ((ATOM) = (FROM); \
	     (ATOM) \
	     && ((ATOM) >= (DIFF_DATA)->atoms.head) \
	     && ((ATOM) - (DIFF_DATA)->atoms.head < (DIFF_DATA)->atoms.len); \
	     (ATOM)++)

#define diff_data_foreach_atom_backwards_from(FROM, ATOM, DIFF_DATA) \
	for ((ATOM) = (FROM); \
	     (ATOM) \
	     && ((ATOM) >= (DIFF_DATA)->atoms.head) \
	     && ((ATOM) - (DIFF_DATA)->atoms.head >= 0); \
	     (ATOM)--)

/* For each file, there is a "root" struct diff_data referencing the entire
 * file, which the atoms are parsed from. In recursion of diff algorithm, there
 * may be "child" struct diff_data only referencing a subsection of the file,
 * re-using the atoms parsing. For "root" structs, atoms_allocated will be
 * nonzero, indicating that the array of atoms is owned by that struct. For
 * "child" structs, atoms_allocated == 0, to indicate that the struct is
 * referencing a subset of atoms. */
struct diff_data {
	FILE *f;		/* if root diff_data and not memory-mapped */
	off_t pos;		/* if not memory-mapped */
	const uint8_t *data;	/* if memory-mapped */
	off_t len;

	int atomizer_flags;
	ARRAYLIST(struct diff_atom) atoms;
	struct diff_data *root;
	struct diff_data *current;
	void *algo_data;

	int diff_flags;

	int err;
};

/* Flags set by file atomizer. */
#define DIFF_ATOMIZER_FOUND_BINARY_DATA	0x00000001

/* Flags set by caller of diff_main(). */
#define DIFF_FLAG_IGNORE_WHITESPACE	0x00000001
#define DIFF_FLAG_SHOW_PROTOTYPES	0x00000002
#define DIFF_FLAG_FORCE_TEXT_DATA	0x00000004

void diff_data_free(struct diff_data *diff_data);

struct diff_chunk;
typedef ARRAYLIST(struct diff_chunk) diff_chunk_arraylist_t;

struct diff_result {
	int rc;

	/*
	 * Pointers to diff data passed in via diff_main.
	 * Do not free these diff_data before freeing the diff_result struct.
	 */
	struct diff_data *left;
	struct diff_data *right;

	diff_chunk_arraylist_t chunks;
};

enum diff_chunk_type {
	CHUNK_EMPTY,
	CHUNK_PLUS,
	CHUNK_MINUS,
	CHUNK_SAME,
	CHUNK_ERROR,
};

enum diff_chunk_type diff_chunk_type(const struct diff_chunk *c);

struct diff_state;

/* Signature of a utility function to divide a file into diff atoms.
 * An example is diff_atomize_text_by_line() in diff_atomize_text.c.
 *
 * func_data: context pointer (free to be used by implementation).
 * d: struct diff_data with d->data and d->len already set up, and
 * d->atoms to be created and d->atomizer_flags to be set up.
 */
typedef int (*diff_atomize_func_t)(void *func_data, struct diff_data *d);

extern int diff_atomize_text_by_line(void *func_data, struct diff_data *d);

struct diff_algo_config;
typedef int (*diff_algo_impl_t)(
	const struct diff_algo_config *algo_config, struct diff_state *state);

/* Form a result with all left-side removed and all right-side added, i.e. no
 * actual diff algorithm involved. */
int diff_algo_none(const struct diff_algo_config *algo_config,
	struct diff_state *state);

/* Myers Diff tracing from the start all the way through to the end, requiring
 * quadratic amounts of memory. This can fail if the required space surpasses
 * algo_config->permitted_state_size. */
extern int diff_algo_myers(const struct diff_algo_config *algo_config,
	struct diff_state *state);

/* Myers "Divide et Impera": tracing forwards from the start and backwards from
 * the end to find a midpoint that divides the problem into smaller chunks.
 * Requires only linear amounts of memory. */
extern int diff_algo_myers_divide(
	const struct diff_algo_config *algo_config, struct diff_state *state);

/* Patience Diff algorithm, which divides a larger diff into smaller chunks. For
 * very specific scenarios, it may lead to a complete diff result by itself, but
 * needs a fallback algo to solve chunks that don't have common-unique atoms. */
extern int diff_algo_patience(
	const struct diff_algo_config *algo_config, struct diff_state *state);

/* Diff algorithms to use, possibly nested. For example:
 *
 * struct diff_algo_config myers, patience, myers_divide;
 *
 * myers = (struct diff_algo_config){
 *         .impl = diff_algo_myers,
 *         .permitted_state_size = 32 * 1024 * 1024,
 *         // When too large, do diff_algo_patience:
 *         .fallback_algo = &patience,
 * };
 *
 * const struct diff_algo_config patience = (struct diff_algo_config){
 * 	.impl = diff_algo_patience,
 * 	// After subdivision, do Patience again:
 * 	.inner_algo = &patience,
 * 	// If subdivision failed, do Myers Divide et Impera:
 * 	.fallback_algo = &myers_then_myers_divide,
 * };
 * 
 * const struct diff_algo_config myers_divide = (struct diff_algo_config){
 * 	.impl = diff_algo_myers_divide,
 * 	// When division succeeded, start from the top:
 * 	.inner_algo = &myers_then_myers_divide,
 * 	// (fallback_algo = NULL implies diff_algo_none).
 * };
 * struct diff_config config = {
 *         .algo = &myers,
 *         ...
 * };
 * diff_main(&config, ...);
 */
struct diff_algo_config {
	diff_algo_impl_t impl;

	/* Fail this algo if it would use more than this amount of memory, and
	 * instead use fallback_algo (diff_algo_myers). permitted_state_size ==
	 * 0 means no limitation. */
	size_t permitted_state_size;

	/* For algorithms that divide into smaller chunks, use this algorithm to
	 * solve the divided chunks. */
	const struct diff_algo_config *inner_algo;

	/* If the algorithm fails (e.g. diff_algo_myers_if_small needs too large
	 * state, or diff_algo_patience can't find any common-unique atoms),
	 * then use this algorithm instead. */
	const struct diff_algo_config *fallback_algo;
};

struct diff_config {
	diff_atomize_func_t atomize_func;
	void *atomize_func_data;

	const struct diff_algo_config *algo;

	/* How deep to step into subdivisions of a source file, a paranoia /
	 * safety measure to guard against infinite loops through diff
	 * algorithms. When the maximum recursion is reached, employ
	 * diff_algo_none (i.e. remove all left atoms and add all right atoms).
	 */
	unsigned int max_recursion_depth;
};

int diff_atomize_file(struct diff_data *d, const struct diff_config *config,
		      FILE *f, const uint8_t *data, off_t len, int diff_flags);
struct diff_result *diff_main(const struct diff_config *config,
			      struct diff_data *left,
			      struct diff_data *right);
void diff_result_free(struct diff_result *result);
int diff_result_contains_printable_chunks(struct diff_result *result);
