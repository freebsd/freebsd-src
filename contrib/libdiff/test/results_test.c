#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <arraylist.h>
#include <diff_main.h>

#include <diff_internal.h>
#include <diff_debug.h>

void test_minus_after_plus(void)
{
	struct diff_result *result = malloc(sizeof(struct diff_result));
	struct diff_data d_left, d_right;
	char *left_data = "a\nb\nc\nd\ne\nm\nn\n";
	char *right_data = "a\nb\nj\nk\nl\nm\nn\n";
	int i;

	printf("\n--- %s()\n", __func__);

	d_left = (struct diff_data){
		.data = left_data,
		.len = strlen(left_data),
		.root = &d_left,
	};
	d_right = (struct diff_data){
		.data = right_data,
		.len = strlen(right_data),
		.root = &d_right,
	};
	*result = (struct diff_result) {
		.left = &d_left,
		.right = &d_right,
	};

	diff_atomize_text_by_line(NULL, result->left);
	diff_atomize_text_by_line(NULL, result->right);

	struct diff_state state = {
		.result = result,
		.recursion_depth_left = 32,
	};
	diff_data_init_subsection(&state.left, result->left,
				  result->left->atoms.head,
				  result->left->atoms.len);
	diff_data_init_subsection(&state.right, result->right,
				  result->right->atoms.head,
				  result->right->atoms.len);

	/* "same" section */
	diff_state_add_chunk(&state, true,
			     &state.left.atoms.head[0], 2,
			     &state.right.atoms.head[0], 2);

	/* "plus" section */
	diff_state_add_chunk(&state, true,
			     &state.left.atoms.head[2], 0,
			     &state.right.atoms.head[2], 3);

	/* "minus" section */
	diff_state_add_chunk(&state, true,
			     &state.left.atoms.head[2], 3,
			     &state.right.atoms.head[5], 0);

	/* "same" section */
	diff_state_add_chunk(&state, true,
			     &state.left.atoms.head[5], 2,
			     &state.right.atoms.head[5], 2);

	for (i = 0; i < result->chunks.len; i++) {
		struct diff_chunk *c = &result->chunks.head[i];
		enum diff_chunk_type t = diff_chunk_type(c);

		printf("[%d] %s lines L%d R%d @L %lld @R %lld\n",
		      i, (t == CHUNK_MINUS ? "minus" :
			  (t == CHUNK_PLUS ? "plus" :
			   (t == CHUNK_SAME ? "same" : "?"))),
		      c->left_count,
		      c->right_count,
		      (long long)(c->left_start ? diff_atom_root_idx(result->left, c->left_start) : -1LL),
		      (long long)(c->right_start ? diff_atom_root_idx(result->right, c->right_start) : -1LL));
	}

	diff_result_free(result);
	diff_data_free(&d_left);
	diff_data_free(&d_right);
}

void test_plus_after_plus(void)
{
	struct diff_result *result = malloc(sizeof(struct diff_result));
	struct diff_data d_left, d_right;
	char *left_data = "a\nb\nc\nd\ne\nm\nn\n";
	char *right_data = "a\nb\nj\nk\nl\nm\nn\n";
	struct diff_chunk *c;

	printf("\n--- %s()\n", __func__);

	d_left = (struct diff_data){
		.data = left_data,
		.len = strlen(left_data),
		.root = &d_left,
	};
	d_right = (struct diff_data){
		.data = right_data,
		.len = strlen(right_data),
		.root = &d_right,
	};
	*result = (struct diff_result) {
		.left = &d_left,
		.right = &d_right,
	};

	diff_atomize_text_by_line(NULL, result->left);
	diff_atomize_text_by_line(NULL, result->right);

	struct diff_state state = {
		.result = result,
		.recursion_depth_left = 32,
	};
	diff_data_init_subsection(&state.left, result->left,
				  result->left->atoms.head,
				  result->left->atoms.len);
	diff_data_init_subsection(&state.right, result->right,
				  result->right->atoms.head,
				  result->right->atoms.len);

	/* "same" section */
	diff_state_add_chunk(&state, true,
			     &state.left.atoms.head[0], 2,
			     &state.right.atoms.head[0], 2);

	/* "minus" section */
	diff_state_add_chunk(&state, true,
			     &state.left.atoms.head[2], 3,
			     &state.right.atoms.head[2], 0);

	/* "plus" section */
	diff_state_add_chunk(&state, true,
			     &state.left.atoms.head[5], 0,
			     &state.right.atoms.head[2], 1);
	/* "plus" section */
	diff_state_add_chunk(&state, true,
			     &state.left.atoms.head[5], 0,
			     &state.right.atoms.head[3], 2);

	/* "same" section */
	diff_state_add_chunk(&state, true,
			     &state.left.atoms.head[5], 2,
			     &state.right.atoms.head[5], 2);

	ARRAYLIST_FOREACH(c, result->chunks) {
		enum diff_chunk_type t = diff_chunk_type(c);

		printf("[%lu] %s lines L%d R%d @L %lld @R %lld\n",
		      (unsigned long)ARRAYLIST_IDX(c, result->chunks),
		      (t == CHUNK_MINUS ? "minus" :
		       (t == CHUNK_PLUS ? "plus" :
			(t == CHUNK_SAME ? "same" : "?"))),
		      c->left_count,
		      c->right_count,
		      (long long)(c->left_start ? diff_atom_root_idx(result->left, c->left_start) : -1LL),
		      (long long)(c->right_start ? diff_atom_root_idx(result->right, c->right_start) : -1LL));
	}

	diff_result_free(result);
	diff_data_free(&d_left);
	diff_data_free(&d_right);
}

int main(void)
{
	test_minus_after_plus();
	test_plus_after_plus();
	return 0;
}
