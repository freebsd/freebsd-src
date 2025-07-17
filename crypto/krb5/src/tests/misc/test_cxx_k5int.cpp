// Test that the krb5 internal headers are compatible with C++ code.
// (Some Windows-specific code is in C++ in this source tree.)

#include <stdio.h>
#include "k5-int.h"
#include "k5-ipc_stream.h"
#include "k5-utf8.h"

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
