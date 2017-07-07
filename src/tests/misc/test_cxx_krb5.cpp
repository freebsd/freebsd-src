// Test that the krb5.h header is compatible with C++ application code.

#include <stdio.h>
#include "krb5.h"
#include "krb5/locate_plugin.h"
#include "profile.h"

int main (int argc, char *argv[])
{
    krb5_context ctx;

    if (krb5_init_context(&ctx) != 0) {
	printf("krb5_init_context returned an error\n");
	return 1;
    }
    printf("hello, world\n");
    krb5_free_context(ctx);
    return 0;
}
