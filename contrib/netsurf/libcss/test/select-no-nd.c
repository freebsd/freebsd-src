
#include "select-common.c"

static css_error get_libcss_node_data(void *pw, void *n,
		void **libcss_node_data)
{
	UNUSED(pw);
	UNUSED(n);

	/* Test case were node data is deleted, by not passing any node data */
	*libcss_node_data = NULL;

	return CSS_OK;
}
