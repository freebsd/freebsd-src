#include <port_before.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>
#include <port_after.h>

char *flags[] = {
	" AI_PASSIVE",
	" AI_CANONNAME",
	" AI_NUMERICHOST",
	" 0x00000008",
	" 0x00000010",
	" 0x00000020",
	" 0x00000040",
	" 0x00000080",
	" 0x00000100",
	" 0x00000200",
	" 0x00000400",
	" 0x00000800",
	" 0x00001000",
	" 0x00002000",
	" 0x00004000",
	" 0x00008000",
	" 0x00010000",
	" 0x00020000",
	" 0x00040000",
	" 0x00080000",
	" 0x00100000",
	" 0x00200000",
	" 0x00400000",
	" 0x00800000",
	" 0x01000000",
	" 0x02000000",
	" 0x04000000",
	" 0x08000000",
	" 0x10000000",
	" 0x20000000",
	" 0x40000000",
	" 0x80000000"
};

void
print_ai(struct addrinfo *answer, int hints) {
	int i;

	if (answer == NULL) {
		fprintf(stdout, "No %s\n", hints ? "Hints" : "Answer");
		return;
	} 

	fprintf(stdout, "%s:\n", hints ? "Hints" : "Answer");

	while (answer) {
		fputs("flags:", stdout);
		for (i = 0; i < 32 ; i++)
			if (answer->ai_flags & (1 << i))
				fputs(flags[i], stdout);
		fputs("\n", stdout);
		fprintf(stdout, "family: %d, socktype: %d, protocol: %d\n",
			answer->ai_family, answer->ai_socktype, answer->ai_protocol);
		if (hints)
			return;

		if (answer->ai_canonname != NULL)
			fprintf(stdout, "canonname: \"%s\"\n",
				answer->ai_canonname);
		else
			fputs("canonname: --none--\n", stdout);

		fprintf(stdout, "addrlen: %d\n", answer->ai_addrlen);

		for (i = 0; i < answer->ai_addrlen; i++)
			fprintf(stdout, "%s%02x", (i == 0) ? "0x" : "", 
				((unsigned char*)(answer->ai_addr))[i]);
		fputs("\n", stdout);

		for (i = 0; i < answer->ai_addrlen; i++)
			fprintf(stdout, "%s%d", (i == 0) ? "" : ".", 
				((unsigned char*)(answer->ai_addr))[i]);
		fputs("\n", stdout);

		for (i = 0; i < answer->ai_addrlen; i++) {
			int c = ((unsigned char*)(answer->ai_addr))[i];
			fprintf(stdout, "%c", (isascii(c) && isprint(c)) ?
					c : '.');
		}
		fputs("\n", stdout);

		answer = answer->ai_next;
	}
}

void
usage() {
	fputs("usage:", stdout);
	fputs("\t-h <hostname>\n", stdout);
	fputs("\t-s <service>\n", stdout);
	fputs("\t-p AI_PASSIVE\n", stdout);
	fputs("\t-c AI_CANONNAME\n", stdout);
	fputs("\t-n AI_NUMERICHOST\n", stdout);
	fputs("\t-4 AF_INET4\n", stdout);
	fputs("\t-6 AF_INET6\n", stdout);
	fputs("\t-l AF_LOCAL\n", stdout);
	fputs("\t-u IPPROTO_UDP\n", stdout);
	fputs("\t-t IPPROTO_TCP\n", stdout);
	fputs("\t-S SOCK_STREAM\n", stdout);
	fputs("\t-D SOCK_DGRAM\n", stdout);
	fputs("\t-R SOCK_RAW\n", stdout);
	fputs("\t-M SOCK_RDM\n", stdout);
	fputs("\t-P SOCK_SEQPACKET\n", stdout);
	exit(1);
}

main(int argc, char **argv) {
	int c;
	char *hostname = NULL;
	char *service = NULL;
	struct addrinfo info;
	int res;
	struct addrinfo *answer;

	memset(&info, 0, sizeof info);

	while ((c = getopt(argc, argv, "h:s:pcn46ltuSDRMP")) != -1) {
		switch (c) {
		case 'h': hostname = optarg; break;
		case 's': service = optarg; break;
		case 'p': info.ai_flags |= AI_PASSIVE; break;
		case 'c': info.ai_flags |= AI_CANONNAME; break;
		case 'n': info.ai_flags |= AI_NUMERICHOST; break;
		case '4': info.ai_family = AF_INET; break;
		case '6': info.ai_family = AF_INET6; break;
#ifdef AF_LOCAL
		case 'l': info.ai_family = AF_LOCAL; break;
#else
		case 'l': fprintf(stdout, "AF_LOCAL not supported\n"); break;
#endif
		case 't': info.ai_protocol = IPPROTO_TCP; break;
		case 'u': info.ai_protocol = IPPROTO_UDP; break;
		case 'S': info.ai_socktype = SOCK_STREAM; break;
		case 'D': info.ai_socktype = SOCK_DGRAM; break;
		case 'R': info.ai_socktype = SOCK_RAW; break;
		case 'M': info.ai_socktype = SOCK_RDM; break;
		case 'P': info.ai_socktype = SOCK_SEQPACKET; break;
		case '?': usage(); break;
		}
		
	}
	res = getaddrinfo(hostname, service, &info, &answer);
	if (res) {
		fprintf(stdout, "%s\n", gai_strerror(res));
	} else {
		print_ai(&info, 1);
		print_ai(answer, 0);
		freeaddrinfo(answer);
	}
	exit (0);
}
