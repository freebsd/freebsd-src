#define getaddrinfo   musl_getaddrinfo
#define __lookup_serv kboot_lookup_serv_stub

#include "getaddrinfo.c"

/* kboot only needs deterministic TCP service lookups for downloads. */
int
kboot_lookup_serv_stub(struct service *buf, const char *name, int proto,
    int socktype, int flags)
{
	char *end;
	unsigned long port;

	if (socktype != 0 && socktype != SOCK_STREAM)
		return (EAI_SERVICE);

	if (proto != 0 && proto != IPPROTO_TCP)
		return (EAI_SERVICE);

	if (name == NULL) {
		port = 0;
	} else if (*name == '\0') {
		return (EAI_SERVICE);
	} else {
		port = strtoul(name, &end, 10);
		if (*end != '\0') {
			if ((flags & AI_NUMERICSERV) != 0)
				return (EAI_NONAME);
			if (strcmp(name, "http") == 0)
				port = 80;
			else if (strcmp(name, "https") == 0)
				port = 443;
			else
				return (EAI_SERVICE);
		} else if (port > 65535) {
			return (EAI_SERVICE);
		}
	}

	buf[0].port = port;
	buf[0].socktype = SOCK_STREAM;
	buf[0].proto = IPPROTO_TCP;

	return (1);
}
