#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

static void test(const char *);

int
main() {
	test("192.5.4.0/23");
	test("192.5.4.0");
	test("192.5.5.1");
	test("192.5.5.1/23");
	test("192.5.5.1/24");
	test("192.5.5.1/28");
	test("192.5.5.1/32");
	return (0);
}

static void
test(const char *input) {
	int bits;
	u_char temp[sizeof (struct in_addr)];
	char output[sizeof "255.255.255.255/32"];

	memset(temp, 0x5e, sizeof temp);
	if (inet_cidr_pton(AF_INET, input, temp, &bits) < 0) {
		perror(input);
		exit(1);
	}
	if (inet_cidr_ntop(AF_INET, temp, bits, output, sizeof output)==NULL){
		perror("inet_cidr_ntop");
		exit(1);
	}
	printf("input '%s', temp '%x %x %x %x', bits %d, output '%s'\n",
	       input, temp[0], temp[1], temp[2], temp[3], bits, output);
}
