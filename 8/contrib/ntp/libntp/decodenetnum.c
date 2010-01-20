/*
 * decodenetnum - return a net number (this is crude, but careful)
 */
#include <sys/types.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "ntp_stdlib.h"

int
decodenetnum(
	const char *num,
	struct sockaddr_storage *netnum
	)
{
	struct addrinfo hints, *ai = NULL;
	register int err, i;
	register const char *cp;
	char name[80];

	cp = num;

	if (*cp == '[') {
		cp++;
		for (i = 0; *cp != ']'; cp++, i++)
			name[i] = *cp;
	name[i] = '\0';
	num = name; 
	}
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_NUMERICHOST;
	err = getaddrinfo(num, NULL, &hints, &ai);
	if (err != 0)
		return 0;
	memcpy(netnum, (struct sockaddr_storage *)ai->ai_addr, ai->ai_addrlen); 
	freeaddrinfo(ai);
	return 1;
}
