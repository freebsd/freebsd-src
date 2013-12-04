#include <config.h>
#include "utilities.h"
#include <assert.h>

/* Display a NTP packet in hex with leading address offset 
 * e.g. offset: value, 0: ff 1: fe ... 255: 00
 */ 
void 
pkt_output (
		struct pkt *dpkg,
		int pkt_length, 
		FILE *output
	   )
{
	register int a;
	u_char *pkt;

	pkt = (u_char *)dpkg;

	fprintf(output, HLINE);

	for (a = 0; a < pkt_length; a++) {
		if (a > 0 && a % 8 == 0)
			fprintf(output, "\n");

		fprintf(output, "%d: %x \t", a, pkt[a]);
	}

	fprintf(output, "\n");
	fprintf(output, HLINE);
}

/* Output a long floating point value in hex in the style described above 
 */
void
l_fp_output (
		l_fp *ts,
		FILE *output
	    )
{
	register int a;

	fprintf(output, HLINE);

	for(a=0; a<8; a++) 
		fprintf(output, "%i: %x \t", a, ((unsigned char *) ts)[a]);

	fprintf(output, "\n");
	fprintf(output, HLINE);

}

/* Output a long floating point value in binary in the style described above
 */
void 
l_fp_output_bin (
		l_fp *ts,
		FILE *output
		)
{
	register int a, b;

	fprintf(output, HLINE);

	for(a=0; a<8; a++) {
		short tmp = ((unsigned char *) ts)[a];
		tmp++;

		fprintf(output, "%i: ", a);

		for(b=7; b>=0; b--) {
			int texp = (int) pow(2, b);

			if(tmp - texp > 0) {
				fprintf(output, "1");
				tmp -= texp;
			}
			else {
				fprintf(output, "0");
			}
		}

		fprintf(output, " ");
	}

	fprintf(output, "\n");
	fprintf(output, HLINE);
}

/* Output a long floating point value in decimal in the style described above
 */
void
l_fp_output_dec (
		l_fp *ts,
		FILE *output
	    )
{
	register int a;

	fprintf(output, HLINE);

	for(a=0; a<8; a++) 
		fprintf(output, "%i: %i \t", a, ((unsigned char *) ts)[a]);

	fprintf(output, "\n");
	fprintf(output, HLINE);

}

/* Convert a struct addrinfo to a string containing the address in style
 * of inet_ntoa
 */
char *
addrinfo_to_str (
	struct addrinfo *addr
	)
{
	sockaddr_u	s;
	
	memset(&s, 0, sizeof(s));
	memcpy(&s, addr->ai_addr, min(sizeof(s), addr->ai_addrlen));

	return ss_to_str(&s);
}

/* Convert a sockaddr_u to a string containing the address in
 * style of inet_ntoa
 * Why not switch callers to use stoa from libntp?  No free() needed
 * in that case.
 */
char *
ss_to_str (
	sockaddr_u *saddr
	)
{
	char *	buf;
	
	buf = emalloc(INET6_ADDRSTRLEN);
	strncpy(buf, stoa(saddr), INET6_ADDRSTRLEN);

	return buf;
}
/*
 * Converts a struct tv to a date string
 */
char *
tv_to_str(
	const struct timeval *tv
	)
{
	const size_t bufsize = 48;
	char *buf;
	time_t gmt_time, local_time;
	struct tm *p_tm_local;
	int hh, mm, lto;

	/*
	 * convert to struct tm in UTC, then intentionally feed
	 * that tm to mktime() which expects local time input, to
	 * derive the offset from UTC to local time.
	 */
	gmt_time = tv->tv_sec;
	local_time = mktime(gmtime(&gmt_time));
	p_tm_local = localtime(&gmt_time);

	/* Local timezone offsets should never cause an overflow.  Yeah. */
	lto = difftime(local_time, gmt_time);
	lto /= 60;
	hh = lto / 60;
	mm = abs(lto % 60);

	buf = emalloc(bufsize);
	snprintf(buf, bufsize,
		 "%d-%.2d-%.2d %.2d:%.2d:%.2d.%.6d (%+03d%02d)",
		 p_tm_local->tm_year + 1900,
		 p_tm_local->tm_mon + 1,
		 p_tm_local->tm_mday,
		 p_tm_local->tm_hour,
		 p_tm_local->tm_min,
		 p_tm_local->tm_sec,
		 (int)tv->tv_usec,
		 hh,
		 mm);

	return buf;
}



		
