#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include "arlib.h"

#ifndef	lint
static	char	sccsid[] = "@(#)sample.c	1.1 12/21/92 (C)1992 Darren Reed. ASYNC DNS";
#endif

char	line[512];

int	lookup = 0, seq = 0;
long	expire = 0;

main()
{
	struct	in_addr adr;
	struct	timeval	tv2;
	fd_set	rd;
	long	now;
	char	*s;
	int	afd, nfd, pid = getpid(), del;

	afd = ar_init(ARES_INITLIST|ARES_CALLINIT|ARES_INITSOCK);

	(void)printf("afd = %d pid = %d\n",afd, pid);

	while (1)
	{
		(void)printf("Host =>");
		(void)fflush(stdout);
		*line = '\0';
		FD_ZERO(&rd);
		FD_SET(0,&rd);
		FD_SET(afd,&rd);
		now = time(NULL);
		if (expire >= now)
		    {
			tv2.tv_usec = 0;
			tv2.tv_sec = expire - now;
			nfd = select(FD_SETSIZE, &rd, NULL, NULL, &tv2);
		    }
		else
			nfd = select(FD_SETSIZE, &rd, NULL, NULL, NULL);

		if (FD_ISSET(0, &rd))
		{
			if (!fgets(line, sizeof(line) - 1, stdin))
				exit(0);
			if (s = index(line, '\n'))
				*s = '\0';
		}

		if (isalpha(*line))
		{
			(void)printf("Asking about [%s] #%d.\n",line, ++seq);
			(void)ar_gethostbyname(line, (char *)&seq,
					       sizeof(seq));
			lookup++;
		}
		else if (isdigit(*line))
		{
			(void)printf("Asking about IP#[%s] #%d.\n",
				line, ++seq);
			adr.s_addr = inet_addr(line);
			(void)ar_gethostbyaddr(&adr, (char *)&seq,
					       sizeof(seq));
			lookup++;
		}
		if (lookup)
			(void)printf("Waiting for answer:\n");
		if (FD_ISSET(afd, &rd))
			(void)waitonlookup(afd);
		del = 0;
		expire = ar_timeout(time(NULL), &del, sizeof(del));
		if (del)
		{
			(void)fprintf(stderr,"#%d failed\n", del);
			lookup--;
		}
	}
}

printhostent(hp)
struct hostent *hp;
{
	struct in_addr ip;
	int i;

	(void)printf("hname = %s\n", hp->h_name);
	for (i = 0; hp->h_aliases[i]; i++)
		(void)printf("alias %d = %s\n", i+1, hp->h_aliases[i]);
	for (i = 0; hp->h_addr_list[i]; i++)
	{
		bcopy(hp->h_addr_list[i], (char *)&ip, sizeof(ip));
		(void)printf("IP# %d = %s\n", i+1, inet_ntoa(ip));
	}
}

int	waitonlookup(afd)
int	afd;
{
	struct	timeval delay;
	struct	hostent	*hp;
	fd_set	rd;
	long	now;
	int	nfd, del;

waitloop:
	FD_ZERO(&rd);
	now = time(NULL);
	if (expire >= now)
		delay.tv_sec = expire - now;
	else
		delay.tv_sec = 1;
	delay.tv_usec = 0;
	FD_SET(afd, &rd);
	FD_SET(0, &rd);

	nfd = select(FD_SETSIZE, &rd, 0, 0, &delay);
	if (nfd == 0)
		return 0;
	else if (FD_ISSET(afd, &rd))
	{
		del = 0;
		hp = ar_answer(&del, sizeof(del));

		(void)printf("hp=%x seq=%d\n",hp,del);
		if (hp)
		    {
			(void)printhostent(hp);
			if (!--lookup)
				return 1;
		    }
	}
	if (FD_ISSET(0, &rd))
		return 2;
	return 0;
}
