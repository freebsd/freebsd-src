/*
**  OLDBIND.COMPAT.C
**
**	Very old systems do not have res_query(), res_querydomain() or
**	res_search(), so emulate them here.
**
**	You really ought to be upgrading to a newer version of BIND
**	(4.8.2 or later) rather than be using this.
**
**	J.R. Oldroyd <jr@inset.com>
*/

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

typedef union
{
	HEADER	qb1;
	char	qb2[PACKETSZ];
} querybuf;

res_query(dname, class, type, data, datalen)
	char *		dname;
	int		class;
	int 		type;
	char *		data;
	int		datalen;
{
	int		n;
	querybuf	buf;

	n = res_mkquery(QUERY, dname, class, type, (char *) NULL, 0,
		NULL, (char *) &buf, sizeof buf);
	n = res_send((char *)&buf, n, data, datalen);

	return n;
}

res_querydomain(host, dname, class, type, data, datalen)
	char *		host;
	char *		dname;
	int		class;
	int		type;
	char *		data;
	int		datalen;
{
	int		n;
	querybuf	buf;
	char		dbuf[256];

	strcpy(dbuf, host);
	if (dbuf[strlen(dbuf)-1] != '.')
		strcat(dbuf, ".");
	strcat(dbuf, dname);
	n = res_mkquery(QUERY, dbuf, class, type, (char *) NULL, 0,
		NULL, (char *)&buf, sizeof buf);
	n = res_send((char *) &buf, n, data, datalen);

	return n;
}

res_search(dname, class, type, data, datalen)
	char *		dname;
	int		class;
	int		type;
	char *		data;
	int		datalen;
{
	int		n;
	querybuf	buf;

	n = res_mkquery(QUERY, dname, class, type, (char *)NULL, 0,
		NULL, (char *) &buf, sizeof buf);
	n = res_send((char *) &buf, n, data, datalen);

	return n;
}
