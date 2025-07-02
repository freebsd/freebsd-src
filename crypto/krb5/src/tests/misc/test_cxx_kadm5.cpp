// Test that the kadm5 header is compatible with C++ application code.

#include "kadm5/admin.h"

krb5_context ctx;
kadm5_config_params p_in, p_out;
int main (int argc, char *argv[])
{
    if (argc == 47 && kadm5_get_config_params(ctx, 1, &p_in, &p_out)) {
	printf("error\n");
	return 1;
    }
    printf("hello, world\n");
    return 0;
}
