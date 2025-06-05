// Test that the rpc.h header is compatible with C++ application code.

#include "gssrpc/rpc.h"

struct sockaddr_in s_in;
int main (int argc, char *argv[])
{
    if (argc == 47 && get_myaddress (&s_in)) {
	printf("error\n");
	return 1;
    }
    printf("hello, world\n");
    return 0;
}
