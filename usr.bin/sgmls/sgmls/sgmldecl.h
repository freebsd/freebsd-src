/* sgmldecl.h: SGML declaration parsing. */

#define QATTCNT 0
#define QATTSPLEN 1
#define QBSEQLEN 2
#define QDTAGLEN 3
#define QDTEMPLEN 4
#define QENTLVL 5
#define QGRPCNT 6
#define QGRPGTCNT 7
#define QGRPLVL 8
#define QLITLEN 9
#define QNAMELEN 10
#define QNORMSEP 11
#define QPILEN 12
#define QTAGLEN 13
#define QTAGLVL 14

#define NQUANTITY (QTAGLVL+1)

#define TOTALCAP 0
#define ENTCAP 1
#define ENTCHCAP 2
#define ELEMCAP 3
#define GRPCAP 4
#define EXGRPCAP 5
#define EXNMCAP 6
#define ATTCAP 7
#define ATTCHCAP 8
#define AVGRPCAP 9
#define NOTCAP 10
#define NOTCHCAP 11
#define IDCAP 12
#define IDREFCAP 13
#define MAPCAP 14
#define LKSETCAP 15
#define LKNMCAP 16

extern char *captab[];

struct sgmldecl {
     long capacity[NCAPACITY];
     long subdoc;
     UNCH formal;
     UNCH omittag;
     UNCH shorttag;
     UNCH shortref;
     UNCH namecase[2];		/* case translation of general/entity names */
     int quantity[NQUANTITY];
};

extern struct sgmldecl sd;

#define OMITTAG (sd.omittag)
#define SUBDOC (sd.subdoc)
#define SHORTTAG (sd.shorttag)
#define FORMAL (sd.formal)

#define ATTCNT (sd.quantity[QATTCNT])
#define ATTSPLEN (sd.quantity[QATTSPLEN])
#define BSEQLEN (sd.quantity[QBSEQLEN])
#define ENTLVL (sd.quantity[QENTLVL])
#define GRPGTCNT (sd.quantity[QGRPGTCNT])
#define GRPCNT (sd.quantity[QGRPCNT])
#define GRPLVL (sd.quantity[QGRPLVL])
#define LITLEN (sd.quantity[QLITLEN])
#define NAMELEN (sd.quantity[QNAMELEN])
#define NORMSEP (sd.quantity[QNORMSEP])
#define PILEN (sd.quantity[QPILEN])
#define TAGLEN (sd.quantity[QTAGLEN])
#define TAGLVL (sd.quantity[QTAGLVL])

#define NAMECASE (sd.namecase[0])
#define ENTCASE (sd.namecase[1])

#define YES 1
#define NO 0

#define UNUSED -1
#define UNKNOWN -2
#define UNDESC -3
#define UNKNOWN_SET -4

extern int asciicharset[];
