
#include "select-common.c"

static css_error get_libcss_node_data(void *pw, void *n,
		void **libcss_node_data)
{
	node *node = n;
	UNUSED(pw);

	/* Pass any node data back to libcss */
	*libcss_node_data = node->libcss_node_data;

	return CSS_OK;
}
