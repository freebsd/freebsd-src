/*
 * $RISS: if_arl/arlconfig/arlconfig.c,v 1.3 2003/01/13 07:23:25 count Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include <dev/arl/if_arlreg.h>

struct freq_list {
	short	fr_no;
	char*	name;
};

struct freq_list freq_list_1[] = {
	 { 0, "908.50" },
	 { 1, "910.06" },
	 { 2, "915.52" },
	 { 3, "915.00" },
	 { 4, "917.83" },
	 { 5, "919.22" },
	 { 6, "922.26" },
	 { 7, "911.45" },
	 { 8, "915.00" },
	 { 9, "918.55" },
	 { 10,"915.00" },
	 { 11,"915.00" }
};

struct freq_list freq_list_6[] = {
	{ 0, "920.31" },
	{ 1, "920.33" },
	{ 2, "921.55" },
	{ 3, "922.17" },
	{ 4, "922.79" },
	{ 5, "921.46" },
	{ 6, "921.55" }
};

struct freq_list freq_list_9[] = {
	{ 0, "Bad"  },
	{ 1, "2412" },
	{ 2, "2427" },
	{ 3, "2442" },
	{ 4, "2457" },
	{ 5, "2465" }
};


struct freq_list freq_list_10[] = {
	{ 0, "Bad"  },
	{ 1, "2412" },
	{ 2, "2427" },
	{ 3, "2442" },
	{ 4, "2457" },
	{ 5, "2472" }
};

struct freq_list freq_list_11[] = {
	{ 0, "Bad"  },
	{ 1, "2484" }
};

struct freq_list freq_list_12[] = {
	{ 0, "Bad" },
	{ 1, "2457" },
	{ 2, "2465" },
	{ 3, "2472" },
};

struct freq_list freq_list_13[] = {
	{ 0, "Bad" },
	{ 1, "2411" },
        { 2, "2425" },
	{ 3, "2439" }
};

struct freq_list freq_list_14[] = {
	{ 0, "Bad" },
	{ 1, "2427" },
	{ 2, "2442" },
	{ 3, "2457" }
};

struct freq_list freq_list_15[] = {
	{ 0, "Bad" },
	{ 1, "2460" }
};

#define MAXFREQ(a) sizeof(a)/sizeof(struct freq_list)

struct rate_list {
	short	rate_no;
	char*	name;
};

struct rate_list rate_list_2400[] = {
	{ 0, "Bad" },
	{ 1, "354" },
	{ 2, "512" },
	{ 3, "1000" },
	{ 4, "2000" }
};

struct radio_type {
	int id;
	char* name;
} radio_type_list []  = {
	{ 0, "No EPROM" },
	{ 1, "092/094"  },
	{ 2, "020"      },
	{ 3, "092A"	},
	{ 4, "020B"	},
	{ 5, "095"	},
	{ 6, "024"	},
	{ 7, "025B"	},
	{ 8, "024B"	},
	{ 9, "024C"	},
	{10, "025C"	},
	{11, "024-1A"	},
	{12, "025-1A"	},
	{13, "Other"	}
};

static struct ch_list {
	short	chan;
	char*	fr;
	char*	country;
	struct	rate_list* rate;
	struct	freq_list* freq;
	int  max_freq;
} CHSET[] = {
	 { 0, 0, 0, 0, 0, 0 },
	 { 1, "900 Mhz",  "Canada, U.S.A., Mexico", 0, freq_list_1, MAXFREQ(freq_list_1) },
	 { 2, 0, 0, 0, 0, 0 },
	 { 3, 0, 0, 0, 0, 0 },
	 { 4, 0, 0, 0, 0, 0 },
	 { 5, 0, 0, 0, 0, 0 },
	 { 6, "900 Mhz",  "Australia", 0, freq_list_6, MAXFREQ(freq_list_6) },
	 { 7, 0, 0, 0, 0, 0 },
	 { 8, 0, 0, 0, 0, 0 },
	 { 9, "2400 Mhz", "North America", rate_list_2400, freq_list_9, MAXFREQ(freq_list_9) },
	{ 10, "2400 Mhz", "E.T.S.I", rate_list_2400, freq_list_10, MAXFREQ(freq_list_10) },
	{ 11, "2400 Mhz", "Japan", rate_list_2400, freq_list_11, MAXFREQ(freq_list_11) },
	{ 12, "2400 Mhz", "France", rate_list_2400, freq_list_12, MAXFREQ(freq_list_12) },
	{ 13, "2400 Mhz", "Australia", rate_list_2400, freq_list_13, MAXFREQ(freq_list_13) },
	{ 14, "2400 Mhz", "Germany", rate_list_2400, freq_list_14, MAXFREQ(freq_list_14) },
	{ 15, "2400 Mhz", "U.K.(MPT1349),Spain", rate_list_2400, freq_list_15, MAXFREQ(freq_list_15) }
};

char* registrationMode[] = {
	"NON-TMA",
	"TMA",
	"PSP"
};

char* priorityList[] = {
	"normal",
	"high",
	"highest"
};

void
usage()
{
	const char *progname = getprogname();

#if 0
	fprintf(stderr, "\nArlan configuration utility.\n\n");
#endif
	fprintf(stderr, "Usage: %s <ifname> [<param> <value> ...]\n", progname);
	fprintf(stderr, "\t<ifname>\tArlan interface name.\n");
	fprintf(stderr, "\t<param>\t\tParameter name (see below).\n");
	fprintf(stderr, "\t<value>\t\tNew value for parameter.\n");
	fprintf(stderr, "Parameter name:\t\tValue:\n");
	fprintf(stderr, "\tname\t\tset Name.\n");
	fprintf(stderr, "\tsid\t\tset System ID.\n");
	fprintf(stderr, "\tfreq\t\tset Frequency Channel (2412, 2427, 2442, 2457, 2465)\n");
	fprintf(stderr, "\tspread\t\tset Bitrate (354, 500, 1000, 2000)\n");
	fprintf(stderr, "\tmode\t\tset Mode (640, 655, PSP).\n");
	fprintf(stderr, "\tcountry\t\tset Country (9-15)\n");
	fprintf(stderr, "\tchannel\t\tset Channel (depended on country)\n");
	fprintf(stderr, "\tbitrate\t\tset Bitrate (0-4)\n");
	fprintf(stderr, "\tpriority\tset Priority (normal, high, highest)\n");
#if 0
	fprintf(stderr, "\tparent\t\tset Arlan parent's MAC.\n");
	fprintf(stderr, "\taddr\t\tset Arlan MAC.\n");
#endif
	fprintf(stderr, "or: %s <ifname> stat\n", progname);
	fprintf(stderr, "\tprint internal arlan statistics block\n");
	fprintf(stderr, "or: %s <ifname> quality <seconds> [count <count>]\n",
		progname);
	fprintf(stderr, "\tprint receive packet level and quality with interval\n");
	exit(0);
}

void
print_al(struct arl_cfg_param *arl_io)
{
	printf("Arlan-655(IC2000) type 0x%x v%d.%d, radio module type %s\n",
	    arl_io->hardwareType,
	    arl_io->majorHardwareVersion,
	    arl_io->minorHardwareVersion,
	    (arl_io->radioModule < 13) ?
	        radio_type_list[arl_io->radioModule].name : "Unknown" );
	printf("\tname %s, sid 0x%06x, mode %s, num tx retry %d\n",
	    arl_io->name,
	    *(int *)arl_io->sid,
	    (arl_io->registrationMode < 3) ?
		registrationMode[arl_io->registrationMode]:"Unknown",
	    arl_io->txRetry );
	printf("\tchannel set %d, %s, %s\n",
	    arl_io->channelSet,
	    CHSET[arl_io->channelSet].fr,
	    CHSET[arl_io->channelSet].country);
	printf("\tfrequency %s Mhz, bitrate %s kb/s, priority %s, receive mode %d\n",
	    (CHSET[arl_io->channelSet].freq &&
	        CHSET[arl_io->channelSet].max_freq > arl_io->channelNumber) ?
		CHSET[arl_io->channelSet].freq[arl_io->channelNumber].name :
		"unknown",
	    (CHSET[arl_io->channelSet].rate) ?
		CHSET[arl_io->channelSet].rate[arl_io->spreadingCode].name :
		"unknown",
	    arl_io->priority <= 2 ?
	        priorityList[arl_io->priority] : "unknown",
	    arl_io->receiveMode);
	printf("\tether %s",
	     (char *)ether_ntoa((struct ether_addr *)arl_io->lanCardNodeId));
	printf(" registered to %s\n",
	     (char *)ether_ntoa((struct ether_addr *)arl_io->specifiedRouter));
}

void
print_stb( struct arl_stats stb )
{
	printf("Arlan internal statistics block\n\n");
	printf("%8u\tdatagrams transmitted\n",
            stb.numDatagramsTransmitted);
	printf("%8u\tre-transmitted\n",
            stb.numReTransmissions);
	printf("%8u\tframes discarded internally in a router\n",
            stb.numFramesDiscarded);
	printf("%8u\tdatagrams received\n",
            stb.numDatagramsReceived);
	printf("%8u\tduplicate received frame\n",
            stb.numDuplicateReceivedFrames);
	printf("%8u\tdatagrams discarded due to unavailable mail box buffer\n",
            stb.numDatagramsDiscarded);
	printf("%8d\tmaximum of re-transmissions datagram\n",
            stb.maxNumReTransmitDatagram);
	printf("%8d\tmaximum of re-transmissions frame\n",
            stb.maxNumReTransmitFrames);
	printf("%8d\tmaximum of consecutive duplicate received frames\n",
            stb.maxNumConsecutiveDuplicateFrames);
	printf("%8u\tbytes transmitted\n",
            stb.numBytesTransmitted);
	printf("%8u\tbytes received\n",
            stb.numBytesReceived);
	printf("%8u\tCRC errors\n",
            stb.numCRCErrors);
	printf("%8u\tlength errors\n",
            stb.numLengthErrors);
	printf("%8u\tabort errors\n",
            stb.numAbortErrors);
	printf("%8u\tTX underuns\n",
            stb.numTXUnderruns);
	printf("%8u\tRX overruns\n",
            stb.numRXOverruns);
	printf("%8u\tHold Offs (channel tested busy, tx delayed)\n",
            stb.numHoldOffs);
	printf("%8u\tframes transmitted\n",
            stb.numFramesTransmitted);
	printf("%8u\tframes received\n",
            stb.numFramesReceived);
	printf("%8u\treceive frames lost due unavailable buffer\n",
            stb.numReceiveFramesLost);
	printf("%8u\tRX buffer overflows \n",
            stb.numRXBufferOverflows);
	printf("%8u\tframes discarded due to Address mismatch\n",
            stb.numFramesDiscardedAddrMismatch);
	printf("%8u\tframes discarded due to SID mismatch\n",
            stb.numFramesDiscardedSIDMismatch);
	printf("%8u\tpolls transmitted\n",
            stb.numPollsTransmistted);
	printf("%8u\tpoll acknowledges received\n",
            stb.numPollAcknowledges);
	printf("%8u\tstatus vector timeout\n",
            stb.numStatusVectorTimeouts);
	printf("%8u\tNACK packets received\n",
            stb.numNACKReceived);
}

#ifdef ARL_QUALITY
void
print_qlt(struct arl_quality qlt, int count)
{
	int i, s;
	int v, v1;
	time_t t;
	int minLevel, maxLevel, avgLevel;
	int minQuality, maxQuality, avgQuality;

	t = time(NULL);
	localtime(&t);
	printf("#%d Current date-time: %s", count, ctime(&t));
	printf("       %-39s %s\n","Receive Level","Receive Quality");
	printf(
" pkts  0.......10 11............100 >   pkts  0.......10 11............100 >\n"\
" -----+----------+-----------------+--- -----+----------+-----------------+---\n");

	minLevel = 16;
	maxLevel = 0;
	avgLevel = 0;
	minQuality = 16;
	maxQuality = 0;
	avgQuality = 0;

	for (i = 0, s = 0; i < ARLAN_MAX_QUALITY; i++) {
		v = qlt.rxLevel[i];
		if (v) {
			if (i < minLevel)
				minLevel = i;
			if (i > maxLevel)
				maxLevel = i;
			avgLevel += v*i;
			printf(" %-4d %x", v, i);
		} else
			printf(" o    %x", i);
		s += v;
		if (v < 10)
			;
		else  if (v < 100)
			v = 10 + ((v-10) * 20) / 90;
		else if (v < 1000)
			v = 31;
		else
			v = 32;
		v1 = v;
		while (v) {
			printf("*");
			v--;
		}

		v = 33 - v1;
		while (v) {
			printf(" ");
			v--;
		}

		v = qlt.rxQuality[i];
		if (v) {
			if (i < minQuality)
				minQuality = i;
			if (i > maxQuality)
				maxQuality = i;
			avgQuality += v*i;
			printf("%-4d %x", v, i);
		} else
			printf("o    %x", i);

		if (v < 10)
			;
		else  if (v < 100)
			v = 10 + ((v-10) * 20) / 90;
		else if (v < 1000)
			v = 31;
		else
			v = 32;
		}
		v1 = v;
		while (v) {
			printf("*");
			v--;
		}

		printf("\n");
	}
	printf(" -----+----------+-----------------+--- -----+----------+-----------------+---\n");
	if (minLevel > 15)
		minLevel = 0;
	if (minQuality > 15)
		minQuality = 0;
	printf("\tPackets count %-6d\n", s);
	if (!s)
		s++;
	printf("\tLevel min %d/avg %d/max %d\n",
	       minLevel, avgLevel/s, maxLevel);
	printf("\tQuality min %d/avg %d/max %d\n",
	       minQuality, avgQuality/s, maxQuality);
}
#endif /* ARL_QUALITY */

int
main(int argc, char *argv[])
{
	struct ifreq		ifr;
	struct arl_req		arl_io;
	struct ether_addr	*ea;
	struct arl_stats	stb;
	/*static arl_quality	qlt;*/
	int			sd, argind, val = -1;
	long			val2;
	char			*param, *value, *value2;
	/*int			end_count, i;*/

	if (argc < 2)
		usage();

	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0)
		err(1,"socket");
	strncpy(ifr.ifr_name, argv[1], sizeof(ifr.ifr_name));
	ifr.ifr_addr.sa_family = AF_INET;
	bzero(&arl_io, sizeof(arl_io));
	ifr.ifr_data = (caddr_t)&arl_io;

	if (argc == 2) {
		if (ioctl(sd, SIOCGARLALL, (caddr_t)&ifr))
			err(1,"Get ALL");
		print_al(&arl_io.cfg);
		exit(0);
	}

	if (argc == 3) {
		if (!strcasecmp(argv[2], "stat")) {
			strncpy(ifr.ifr_name, argv[1], sizeof(ifr.ifr_name));
			ifr.ifr_addr.sa_family = AF_INET;
			ifr.ifr_data = (caddr_t)&stb;
			if (ioctl(sd, SIOCGARLSTB, (caddr_t)&ifr))
				err(1,"Get STB");
			print_stb(stb);
			exit(0);
		}
#ifdef ARL_QUALITY
		if (!strcasecmp(argv[2],"quality") && argc > 3) {
			val = atoi(argv[3]);
			if (val < 0 || val >= 3601)
				err(1,"Bad time range");
			end_count = 0;
			if (argc > 5 && !strcasecmp(argv[4], "count"))
				end_count = atoi(argv[5]);
			printf("\n");
			i = 0;
			while (i < end_count || !end_count) {	/* loop */
				if (i++)
					sleep(val);
				strncpy(ifr.ifr_name,
				    argv[1], sizeof(ifr.ifr_name));
				ifr.ifr_addr.sa_family = AF_INET;
				ifr.ifr_data = (caddr_t)&qlt;
				if (ioctl(sd, SIOCGARLQLT, (caddr_t)&ifr))
					err(1,"Get QLT");
				print_qlt(qlt, i);
			}
			exit(0);
		}
#endif /* ARL_QUALITY */
	}

	arl_io.what_set = 0;

	for (argind = 2; argind < argc; argind += 2) {
		param = argv[argind];
		value = argv[argind+1];
		val = -1;

		if (!strcasecmp(param, "name")) {
			bzero(arl_io.cfg.name, 16);
			strncpy(arl_io.cfg.name, value, 16);
			arl_io.what_set |= ARLAN_SET_name;
		}

		if (!strcasecmp(param, "sid")) {
			val2 = strtol(value, &value2, 0);
			if (val2 < 0 || val2 > 0xffffff || val2 % 2 ||
			    value == value2)
				err(1, "Bad SID - %s", value);
			bcopy(&val2, arl_io.cfg.sid, 4);
			arl_io.what_set |= ARLAN_SET_sid;
		}

		if (!strcasecmp (param, "freq")) {
			if (!strcmp(value, "2412"))
				val = 1;
			else if (!strcmp(value, "2427"))
				val = 2;
			else if (!strcmp(value, "2442"))
				val = 3;
			else if (!strcmp(value, "2457"))
				val = 4;
			else if (!strcmp(value, "2465"))
				val = 5;
			if (val == -1)
				err(1, "Bad Frequency - %s", value);
			arl_io.cfg.channelNumber = val;
			arl_io.what_set |= ARLAN_SET_channelNumber;
		}

		if (!strcasecmp(param, "spread")) {
			if (!strcmp(value, "354"))
				val = 1;
			else if (!strcmp(value, "500"))
				val = 2;
			else if (!strcmp(value, "1000"))
				val = 3;
			else if (!strcmp(value, "2000"))
				val = 4;
			if (val == -1)
				err (1, "Bad Bitrate - %s", value);
			arl_io.cfg.spreadingCode = val;
			arl_io.what_set |= ARLAN_SET_spreadingCode;
		}

		if (!strcasecmp(param, "priority")) {
			if (!strcasecmp(value, "normal"))
				val = 0;
			else if (!strcasecmp(value, "high"))
				val = 1;
			else if (!strcasecmp(value, "highest"))
				val = 2;
			if (val == -1)
				err( 1, "Bad priority - %s", value);
			arl_io.cfg.priority = val;
			arl_io.what_set |= ARLAN_SET_priority;
		}

		if (!strcasecmp(param, "mode")) {
			if (!strcmp(value, "655"))
				val = 0;
			else if (!strcmp(value, "640"))
				val = 1;
			else if (!strcasecmp (value, "PSP"))
				val = 2;
			if (val == -1)
				err (1, "Bad Mode - %s", value);
			arl_io.cfg.registrationMode = val;
			arl_io.what_set |= ARLAN_SET_registrationMode;
		}

		if (!strcasecmp(param, "parent")) {
			if ((ea = (struct ether_addr*) ether_aton(value)) == NULL)
				err (1, "Bad parent's MAC - %s", value);
			for (val = 0; val < 6; val++) {
				arl_io.cfg.specifiedRouter[val] =
				    (int) ea->octet[val];
			}
			arl_io.what_set |= ARLAN_SET_specifiedRouter;
		}

		if (!strcasecmp(param, "addr")) {
			if ((ea = (struct ether_addr*) ether_aton(value)) == NULL)
				err (1, "Bad MAC - %s", value);
			for (val = 0; val < 6; val++) {
				arl_io.cfg.lanCardNodeId[val] =
				    (int) ea->octet[val];
			}
			arl_io.what_set |= ARLAN_SET_lanCardNodeId;
		}

		if (!strcasecmp(param, "country")) {
			arl_io.cfg.channelSet = atoi(value);
			arl_io.what_set |= ARLAN_SET_channelSet;
		}

		if (!strcasecmp(param, "channel")) {
			arl_io.cfg.channelNumber = atoi(value);
			arl_io.what_set |= ARLAN_SET_channelNumber;
		}

		if (!strcasecmp(param, "bitrate")) {
			arl_io.cfg.spreadingCode = atoi(value);
			arl_io.what_set |= ARLAN_SET_spreadingCode;
		}

		if (!strcasecmp(param, "receive")) {
			arl_io.cfg.receiveMode = atoi(value);
			arl_io.what_set |= ARLAN_SET_receiveMode;
		}

		if (!strcasecmp(param, "txretry")) {
			arl_io.cfg.txRetry = atoi(value);
			arl_io.what_set |= ARLAN_SET_txRetry;
		}
	}

	if (arl_io.what_set) {
		if (ioctl(sd, SIOCSARLALL, (caddr_t)&ifr))
			err (1, "Set ALL" );
		if (ioctl(sd, SIOCGARLALL, (caddr_t)&ifr))
			err (1, "Get ALL");
		print_al(&arl_io.cfg);
	}

	return 0;
}
