/****************************************************************
Copyright 1990, 1993 by AT&T Bell Laboratories and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T Bell Laboratories or
Bellcore or any of their entities not be used in advertising or
publicity pertaining to distribution of the software without
specific, written prior permission.

AT&T and Bellcore disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall AT&T or Bellcore be liable for
any special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
****************************************************************/

#include "stdio.h"
#ifndef KR_headers
#include "stdlib.h"
#include "fcntl.h"	/* for declaration of open, O_RDONLY */
#endif
#ifdef MSDOS
#include "io.h"
#endif
#ifndef O_RDONLY
#define O_RDONLY 0
#endif
#ifndef O_BINARY
#define O_BINARY O_RDONLY
#endif

 char *progname;
 static int ignore_cr;

 void
#ifdef KR_headers
usage(rc)
#else
usage(int rc)
#endif
{
	fprintf(stderr, "usage: %s [-r] [file [file...]]\n\
	option -r ignores carriage return characters\n", progname);
	exit(rc);
	}

typedef unsigned char Uchar;

 long
#ifdef KR_headers
sum32(sum, x, n)
 register long sum;
 register Uchar *x;
 int n;
#else
sum32(register long sum, register Uchar *x, int n)
#endif
{
	register Uchar *xe;
	static long crc_table[256] = {
		0,		151466134,	302932268,	453595578,
		-9583591,	-160762737,	-312236747,	-463170141,
		-19167182,	-136529756,	-321525474,	-439166584,
		28724267,	145849533,	330837255,	448732561,
		-38334364,	-189783822,	-273059512,	-423738914,
		47895677,	199091435,	282375505,	433292743,
		57448534,	174827712,	291699066,	409324012,
		-67019697,	-184128295,	-300991133,	-418902539,
		-76668728,	-227995554,	-379567644,	-530091662,
		67364049,	218420295,	369985021,	520795499,
		95791354,	213031020,	398182870,	515701056,
		-86479645,	-203465611,	-388624945,	-506380967,
		114897068,	266207290,	349655424,	500195606,
		-105581387,	-256654301,	-340093543,	-490887921,
		-134039394,	-251295736,	-368256590,	-485758684,
		124746887,	241716241,	358686123,	476458301,
		-153337456,	-2395898,	-455991108,	-304803798,
		162629001,	11973919,	465560741,	314102835,
		134728098,	16841012,	436840590,	319723544,
		-144044613,	-26395347,	-446403433,	-329032703,
		191582708,	40657250,	426062040,	274858062,
		-200894995,	-50223749,	-435620671,	-284179369,
		-172959290,	-55056048,	-406931222,	-289830788,
		182263263,	64630089,	416513267,	299125861,
		229794136,	78991822,	532414580,	381366498,
		-220224191,	-69691945,	-523123603,	-371788549,
		-211162774,	-93398532,	-513308602,	-396314416,
		201600371,	84090341,	503991391,	386759881,
		-268078788,	-117292630,	-502591472,	-351526778,
		258520357,	107972019,	493278217,	341959839,
		249493774,	131713432,	483432482,	366454964,
		-239911657,	-122417791,	-474129349,	-356881235,
		-306674912,	-457198666,	-4791796,	-156118374,
		315967289,	466778031,	14362133,	165418627,
		325258002,	442776452,	23947838,	141187752,
		-334573813,	-452329571,	-33509849,	-150495567,
		269456196,	419996626,	33682024,	184992510,
		-278767779,	-429561909,	-43239823,	-194312473,
		-288089226,	-405591072,	-52790694,	-170046772,
		297394031,	415166457,	62373443,	179343061,
		383165416,	533828478,	81314500,	232780370,
		-373594127,	-524527769,	-72022307,	-223201717,
		-401789990,	-519431348,	-100447498,	-217810336,
		392228803,	510123861,	91131631,	208256633,
		-345918580,	-496598246,	-110112096,	-261561802,
		336361365,	487278339,	100800185,	251995695,
		364526526,	482151208,	129260178,	246639108,
		-354943065,	-472854735,	-119955829,	-237064675,
		459588272,	308539942,	157983644,	7181066,
		-469170519,	-317835713,	-167286907,	-16754925,
		-440448382,	-323454444,	-139383890,	-21619912,
		450006683,	332774925,	148697015,	31186721,
		-422325548,	-271261118,	-186797064,	-36011154,
		431888077,	280569435,	196114401,	45565815,
		403200742,	286222960,	168180682,	50400092,
		-412770561,	-295522711,	-177471533,	-59977915,
		-536157576,	-384970002,	-234585260,	-83643454,
		526853729,	375396087,	225003341,	74348507,
		517040714,	399923932,	215944038,	98057200,
		-507728301,	-390357307,	-206385281,	-88735767,
		498987548,	347783818,	263426864,	112501670,
		-489671163,	-338229613,	-253864151,	-103192641,
		-479823314,	-362722632,	-244835582,	-126932076,
		470531639,	353144481,	235265819,	117632909
		};

	xe = x + n;
	while(x < xe)
		sum = crc_table[(sum ^ *x++) & 0xff] ^ (sum >> 8 & 0xffffff);
	return sum;
	}

 int
#ifdef KR_headers
cr_purge(buf, n)
 Uchar *buf;
 int n;
#else
cr_purge(Uchar *buf, int n)
#endif
{
	register Uchar *b, *b1, *be;
	b = buf;
	be = b + n;
	while(b < be)
		if (*b++ == '\r') {
			b1 = b - 1;
			while(b < be)
				if ((*b1 = *b++) != '\r')
					b1++;
			return b1 - buf;
			}
	return n;
	}

static Uchar Buf[16*1024];

 void
#ifdef KR_headers
process(s, x)
 char *s;
 int x;
#else
process(char *s, int x)
#endif
{
	register int n;
	long fsize, sum;

	sum = 0;
	fsize = 0;
	while((n = read(x, (char *)Buf, sizeof(Buf))) > 0) {
		if (ignore_cr)
			n = cr_purge(Buf, n);
		fsize += n;
		sum = sum32(sum, Buf, n);
		}
	sum &= 0xffffffff;
        if (n==0)
		printf("%s\t%lx\t%ld\n", s, sum & 0xffffffff, fsize);
        else { perror(s); }
	close(x);
	}

#ifdef KR_headers
main(argc, argv)
 char **argv;
#else
main(int argc, char **argv)
#endif
{
	int x;
	char *s;
	static int rc;

	progname = *argv;
	s = *++argv;
	if (s && *s == '-') {
		switch(s[1]) {
			case '?':
				usage(0);
			case 'r':
				ignore_cr = 1;
			case '-':
				break;
			default:
				fprintf(stderr, "invalid option %s\n", s);
				usage(1);
			}
		s = *++argv;
		}
	if (s) do {
		x = open(s, O_RDONLY|O_BINARY);
		if (x < 0) {
			fprintf(stderr, "%s: can't open %s\n", progname, s);
			rc |= 1;
			}
		else
			process(s, x);
		}
		while(s = *++argv);
	else {
		process("/dev/stdin", fileno(stdin));
		}
	return rc;
	}
