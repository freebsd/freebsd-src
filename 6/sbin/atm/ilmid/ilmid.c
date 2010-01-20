/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 */

/*
 * User utilities
 * --------------
 *
 * Implement very minimal ILMI address registration.
 *
 * Implement very crude and basic support for "cracking" and
 * "encoding" SNMP PDU's to support ILMI prefix and NSAP address
 * registration. Code is not robust nor is it meant to provide any
 * "real" SNMP support. Much of the code expects predetermined values
 * and will fail if anything else is found. Much of the "encoding" is
 * done with pre-computed PDU's.
 *
 * See "The Simple Book", Marshall T. Rose, particularly chapter 5,
 * for ASN and BER information.
 *
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>
#include <dev/hfa/fore_stats.h>

#include <err.h>
#include <errno.h>
#include <libatm.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#ifndef	lint
__RCSID("@(#) $FreeBSD$");
#endif


#define	MAX_LEN		9180

#define	MAX_UNITS	8

/*
 * Define some ASN types
 */
#define	ASN_INTEGER	0x02
#define	ASN_OCTET	0x04
#define	ASN_NULL	0x05
#define	ASN_OBJID	0x06
#define	ASN_SEQUENCE	0x30
#define	ASN_IPADDR	0x40
#define	ASN_TIMESTAMP	0x43

static const char *Var_Types[] = {
	"",
	"",
	"ASN_INTEGER",
	"",
	"ASN_OCTET",
	"ASN_NULL",
	"ASN_OBJID"
};

/*
 * Define SNMP PDU types
 */
#define	PDU_TYPE_GET		0xA0
#define	PDU_TYPE_GETNEXT	0xA1
#define	PDU_TYPE_GETRESP	0xA2
#define	PDU_TYPE_SET		0xA3
#define	PDU_TYPE_TRAP		0xA4

static const char *const PDU_Types[] = {
	"GET REQUEST",
	"GETNEXT REQUEST",
	"GET RESPONSE",
	"SET REQUEST",
	"TRAP",
	" ?? ",
	" ??? "
};

/*
 * Define TRAP codes
 */
#define	TRAP_COLDSTART	0
#define	TRAP_WARMSTART	1
#define	TRAP_LINKDOWN	2
#define	TRAP_LINKUP	3
#define	TRAP_AUTHFAIL	4
#define	TRAP_EGPLOSS	5
#define	TRAP_ENTERPRISE	6

/*
 * Define SNMP Version numbers
 */
#define	SNMP_VERSION_1	1
#define	SNMP_VERSION_2	2

/*
 * SNMP Error-status values
 */
#define	SNMP_ERR_NOERROR	0
#define	SNMP_ERR_TOOBIG		1
#define	SNMP_ERR_NOSUCHNAME	2
#define	SNMP_ERR_BADVALUE	3
#define	SNMP_ERR_READONLY	4
#define	SNMP_ERR_GENERR		5

/*
 * Max string length for Variable
 */
#define	STRLEN		128

/*
 * Unknown variable
 */
#define	VAR_UNKNOWN	-1

/*
 * Define our internal representation of an OBJECT IDENTIFIER
 */
struct objid {
	int	oid[128];
};
typedef struct objid Objid;

/*
 * Define a Veriable classso that we can handle multiple GET/SET's
 * per PDU.
 */
typedef struct variable Variable;
struct variable {
	Objid		oid;
	int		type;
	union {
		int		ival;		/* INTEGER/TIMESTAMP */
		Objid		oval;		/* OBJID */
		long		aval;		/* IPADDR */
		char		sval[STRLEN];	/* OCTET */
	} var;
	Variable	*next;
};

/*
 * Every SNMP PDU has the first four fields of this header. The only type
 * which doesn't have the last three fields is the TRAP type.
 */
struct snmp_header {
	int		pdulen;
	int		version;
	char		community[64];
	int		pdutype;

	/* GET/GETNEXT/GETRESP/SET */
	int		reqid;
	int		error;
	int		erridx;

	/* TRAP */
	Objid		enterprise;
	int		ipaddr;
	int		generic_trap;
	int		specific_trap;

	int		varlen;
	Variable	*head,
			*tail;
};
typedef struct snmp_header Snmp_Header;

Snmp_Header	*ColdStart_Header;
Snmp_Header	*PDU_Header;

/*
 * Define some OBJET IDENTIFIERS that we'll try to reply to:
 *
 * sysUpTime: number of time ticks since this deamon came up
 * netpfx_oid:	network prefix table
 * unitype:	is this a PRIVATE or PUBLIC network link
 * univer:	which version of UNI are we running
 * devtype:	is this a USER or NODE ATM device
 * setprefix:	used when the switch wants to tell us its NSAP prefix
 * foresiggrp:	FORE specific Objid we see alot of (being connected to FORE
 *			switches...)
 */
Objid	Objids[] = {
#define	SYS_OBJID	0
	{{  8, 43, 6, 1, 2, 1, 1, 2, 0 }},
#define	UPTIME_OBJID	1
	{{  8, 43, 6, 1, 2, 1,    1, 3, 0 }},
#define	PORT_OBJID	2
	{{ 12, 43, 6, 1, 4, 1,  353, 2, 1, 1, 1, 1, 0 }},
#define	IPNM_OBJID	3
	{{ 10, 43, 6, 1, 4, 1,  353, 2, 1, 2, 0 }},
#define	LAYER_OBJID	4
	{{ 12, 43, 6, 1, 4, 1,  353, 2, 2, 1, 1,  1, 0 }},
#define	MAXVCC_OBJID	5
	{{ 12, 43, 6, 1, 4, 1,  353, 2, 2, 1, 1,  3, 0 }},
#define	UNITYPE_OBJID	6
	{{ 12, 43, 6, 1, 4, 1,  353, 2, 2, 1, 1,  8, 0 }},
#define	UNIVER_OBJID	7
	{{ 12, 43, 6, 1, 4, 1,  353, 2, 2, 1, 1,  9, 0 }},
#define	DEVTYPE_OBJID	8
	{{ 12, 43, 6, 1, 4, 1,  353, 2, 2, 1, 1, 10, 0 }},
#define	ADDRESS_OBJID	9
	{{  8, 43, 6, 1, 4, 1,  353, 2, 6 }},
#define	NETPFX_OBJID	10
	{{  9, 43, 6, 1, 4, 1,  353, 2, 7, 1 }},
#define	MY_OBJID	11
	{{  7, 43, 6, 1, 4, 1, 9999, 1 }},
#define	SETPFX_OBJID	12	/* ATM Forum says 1=valid, 2=invalid, not 0! */
	{{ 12, 43, 6, 1, 4, 1,  353, 2, 7, 1, 1,  3, 0 }},
#define	ENTERPRISE_OBJID 13
	{{  8, 43, 6, 1, 4, 1,    3, 1, 1 }},
#define	ATMF_PORTID	14
	{{ 10, 43, 6, 1, 4, 1,  353, 2, 1, 4, 0 }},
#define	ATMF_SYSID	15
	{{ 12, 43, 6, 1, 4, 1,  353, 2, 1, 1, 1, 8, 0 }},
#define	MADGE_OBJECT1	16	/* I don't have a clue ... -RH */
	{{  9, 43, 6, 1, 4, 1,  353, 2, 7, 99 }},
};

#define	NUM_OIDS	(sizeof(Objids)/sizeof(Objid))

#define	UNIVER_UNI20	1
#define	UNIVER_UNI30	2
#define	UNIVER_UNI31	3
#define	UNIVER_UNI40	4
#define	UNIVER_UNKNOWN	5

#define	UNITYPE_PUBLIC	1
#define	UNITYPE_PRIVATE	2

#define	DEVTYPE_USER	1
#define	DEVTYPE_NODE	2

/* For print_pdu() */
#define	PDU_SEND	1
#define	PDU_RECV	2

/*
 * ILMI protocol states
 */
enum ilmi_states {
	ILMI_UNKNOWN,			/* Uninitialized */
	ILMI_COLDSTART,			/* We need to send a COLD_START trap */
	ILMI_INIT,			/* Ensure that switch has reset */
	ILMI_REG,			/* Looking for SET message */
	ILMI_RUNNING			/* Normal processing */
};

static const char *ILMI_State[] = {
	"UNKNOWN",
	"COLDSTART",
	"INIT",
	"REG",
	"RUNNING"
};

/*
 * Our (incrementing) Request ID
 */
int	Req_ID;

/*
 * Temporary buffer for building response packets. Should help ensure
 * that we aren't accidently overwriting some other memory.
 */
u_char	Resp_Buf[1024];

/*
 * Copy the reponse into a buffer we can modify without
 * changing the original...
 */
#define	COPY_RESP(resp)	\
        bcopy ( (resp), Resp_Buf, (resp)[0] + 1 )

int                     NUnits;

/*
 * fd for units which have seen a coldStart TRAP and are now exchaning SNMP requests
 */
int			ilmi_fd[MAX_UNITS + 1];
/*
 * enum ilmi_states for this unit
 */
int			ilmi_state[MAX_UNITS + 1];
/*
 * Local copy for HARP physical configuration information
 */
struct air_cfg_rsp      Cfg[MAX_UNITS + 1];
/*
 * Local copy for HARP interface configuration information
 */
struct air_int_rsp      Intf[MAX_UNITS + 1];

/*
 * addressEntry table
 */
Objid			addressEntry[MAX_UNITS + 1];

/*
 * When this daemon started
 */
struct timeval	starttime;

int	Debug_Level = 0;
int	foregnd = 0;	/* run in the foreground? */

char	*progname;
char	hostname[80];

				/* File to write debug messages to */
#define	LOG_FILE	"/var/log/ilmid"
FILE	*Log;			/* File descriptor for log messages */

static const char *Months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/*
 * function declarations
 */
static void	write_timestamp	(void);
static void	hexdump		(FILE *, u_int8_t *, int, char *);
static int	asn_get_pdu_len	(u_char **, int *);
static int	asn_get_encoded	(u_char **, int *);
static int	asn_get_int	(u_char **, int *);
static void	asn_set_int	(u_char **, int);
static void	print_objid	(Objid *);
static void	asn_get_objid	(u_char **, Objid *, int *);
static int	asn_put_objid	(u_char **, Objid *);
static void	asn_get_octet	(u_char **, char *, int *);
static void	print_header	(Snmp_Header *);
static void	parse_oids	(Snmp_Header *, u_char **);
static int	oid_cmp		(Objid *, Objid *);
static int	oid_ncmp	(Objid *, Objid *, int);
static int	find_var	(Variable *);
static int	get_ticks	(void);
static void	build_pdu	(Snmp_Header *, int);
static void	free_pdu	(Snmp_Header *);
static void	print_pdu	(int, int, Snmp_Header *, int, u_char *);
static void	send_resp	(int, Snmp_Header *, u_char *);
static void	init_ilmi	(void);
static void	ilmi_open	(void);
static void	get_local_ip	(int, long *);
static void	set_prefix	(Objid *, Snmp_Header *, int);
static void	set_address	(Snmp_Header *, int);
static void	process_get	(Snmp_Header *, int);
static int	lmi_object_find	(Variable *);
static int	lmi_rcvcmd_getnext(Snmp_Header *, int);
static int	lmi_rcvcmd_trap	(Snmp_Header *, int);
static void	ilmi_do_state	(void);
static void	Increment_DL	(int);
static void	Decrement_DL	(int);

static Snmp_Header *	asn_get_header		(u_char **);
static Snmp_Header *	build_cold_start	(void);
static Snmp_Header *	build_generic_header	(void);

/*
 * Write a syslog() style timestamp
 *
 * Write a syslog() style timestamp with month, day, time and hostname
 * to the log file.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
static void
write_timestamp (void)
{
	time_t		clk;
	struct tm 	*tm;

	clk = time ( (time_t)NULL );
	tm = localtime ( &clk );

	if ( Log && Debug_Level > 1 )
	    if ( Log != stderr )
	        fprintf ( Log, "%.3s %2d %.2d:%.2d:%.2d %s: ",
		    Months[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min,
		        tm->tm_sec, hostname );

	return;

}

/*
 * Utility to pretty print buffer as hex dumps
 * 
 * Arguments:
 *	out	- file handle
 *	ptr	- buffer pointer
 *	len	- length to pretty print
 *	desc	- output header
 *
 * Returns:
 *	none
 *
 */
static void
hexdump (out, ptr, len, desc)
	FILE *		out;
	u_int8_t *	ptr;
	int		len;
	char *		desc;
{
	char 			line[17];
	int			i, j;

	if (out == NULL)
		out = stdout;

	if (desc != NULL)
		fprintf(out, "[ %s (%d bytes)]\n", desc, len);

	bzero(line, sizeof(line));

	for (i = 0, j = 0; i < len; i++) {

		if (j == 0) fprintf(out, "%04x: ", i);
		if (j == 8) fprintf(out, "| ");

		fprintf(out, "%02x ", ptr[i]);
		line[j] = isalnum(ptr[i]) ? ptr[i] : '.' ;
		if (j == 15) {
			fprintf(out, "  |%16s|\n", line);
			bzero(line, sizeof(line));
			j = 0;
		} else 
			j++;
	}

	if (j != 0) {
		if (j <= 8) fprintf(out, "  ");
		for (; j < 16; j++) fprintf(out, "   ");
		fprintf(out, "  |%-16s|\n", line);
	}
	fflush(out);

	return;
}

/*
 * Get lengths from PDU encodings
 *
 * Lengths are sometimes encoded as a single byte if the length
 * is less the 127 but are more commonly encoded as one byte with
 * the high bit set and the lower seven bits indicating the nuber
 * of bytes which make up the length value. Trailing data is (to my
 * knowledge) not 7-bit encoded.
 *
 * Arguments:
 * 	bufp	- pointer to buffer pointer
 *	plen	- pointer to PDU length or NULL if not a concern
 *
 * Returns: 
 *	bufp	- updated buffer pointer
 *	plen	- (possibly) adjusted pdu length
 *	<len>	- decoded length
 *
 */
static int
asn_get_pdu_len (u_char **bufp, int *plen)
{
	u_char	*bp = *bufp;
	int	len = 0;
	int	i, b;

	b = *bp++;
	if ( plen )
		(*plen)--;
	 if ( b & 0x80 ) {
		for ( i = 0; i < (b & ~0x80); i++ ) {
			len = len * 256 + *bp++;
			if ( plen )
				(*plen)--;
		}
	} else
		len = b;

	*bufp = bp;
	return ( len );
}

/*
 * Get an 7-bit encoded value.
 *
 * Get a value which is represented using a 7-bit encoding. The last
 * byte in the stream has the high-bit clear.
 *
 * Arguments:
 *	bufp	- pointer to the buffer pointer
 *	len	- pointer to the buffer length
 *
 * Returns:
 *	bufp	- updated buffer pointer
 *	len	- updated buffer length
 *	<val>	- value encoding represented
 *
 */
static int
asn_get_encoded (u_char **bufp, int *len)
{
	u_char	*bp = *bufp;
	int	val = 0;
	int	l = *len;

	/*
	 * Keep going while high bit is set
	 */
	do {
		/*
		 * Each byte can represent 7 bits
	 	 */
		val = ( val << 7 ) + ( *bp & ~0x80 );
		l--;
	} while ( *bp++ & 0x80 );

	*bufp = bp;		/* update buffer pointer */
	*len = l;		/* update buffer length */

	return ( val );
}

/*
 * Get a BER encoded integer
 *
 * Intergers are encoded as one byte length followed by <length> data bytes
 *
 * Arguments:
 *	bufp	- pointer to the buffer pointer
 *	plen	- pointer to PDU length or NULL if not a concern
 *
 * Returns:
 *	bufp	- updated buffer pointer 
 *	plen	- (possibly) updated PDU length
 *	<val>	- value of encoded integer
 *
 */
static int
asn_get_int (u_char **bufp, int *plen)
{
	int	i;
	int	len;
	int	v = 0;
	u_char	*bp = *bufp;

	len = *bp++;
	if ( plen )
		(*plen)--;
	for ( i = 0; i < len; i++ ) {
		v = (v * 256) + *bp++;
		if ( plen )
			(*plen)--;
	}
	*bufp = bp;
	return ( v );
}

/*
 * Set a BER encoded integer
 *
 * Arguments:
 *	bufp	- pointer to buffer pointer where we are to set int in
 *	val	- integer value to set
 *
 * Returns:
 *	none
 *	<bufp>	- updated buffer pointer
 *
 */
static void
asn_set_int (u_char **bufp, int val)
{
	union {
		int	i;
		u_char	c[4];
	} u;
	int	len = sizeof(int);
	size_t	i = 0;
	u_char	*bp = *bufp;

	/* Check for special case where val == 0 */
	if ( val == 0 ) {
		*bp++ = 1;
		*bp++ = 0;
		*bufp = bp;
		return;
	}

	u.i = htonl ( val );

	while ( u.c[i] == 0  && i++ < sizeof(int) )
		len--;

	if ( u.c[i] > 0x7f ) {
		i--;
		len++;
	}

	*bp++ = len;
	bcopy ( (caddr_t)&u.c[sizeof(int)-len], bp, len );
	bp += len;
	*bufp = bp;

	return;
}

/*
 * Utility to print an object identifier.
 *
 * Arguments:
 *	objid	- pointer to objid representation
 *
 * Returns:
 *	none
 *
 */
static void
print_objid (Objid *objid)
{
	int	i;

	/*
	 * First oid coded as 40 * X + Y
	 */
	if ( Log ) {
	    write_timestamp();
	    fprintf ( Log, ".%d.%d", objid->oid[1] / 40,
		objid->oid[1] % 40 );
	}
	for ( i = 2; i <= objid->oid[0]; i++ )
	    if ( Log )
		fprintf ( Log, ".%d", objid->oid[i] );
	if ( Log )
	    fprintf ( Log, "\n" );

	return;
}

/*
 * Get Object Identifier
 *
 * Arguments:
 *	bufp	- pointer to buffer pointer
 *	objid	- pointer to objid buffer
 *	plen	- pointer to PDU length or NULL of not a concern
 *
 * Returns:
 *	bufp	- updated buffer pointer
 *	objid	- internal representation of encoded objid
 *	plen	- (possibly) adjusted PDU length
 *
 */
static void
asn_get_objid (u_char **bufp, Objid *objid, int *plen)
{
	int	len;
	u_char	*bp = *bufp;
	int	*ip = (int *)objid + 1;	/* First byte will contain length */
	int	oidlen = 0;

	len = *bp++;
	if ( plen )
		(*plen)--;
	while ( len ) {
		*ip++ = asn_get_encoded ( &bp, &len );
		if ( plen )
			(*plen)--;
		oidlen++;
	}
	objid->oid[0] = oidlen;
	*bufp = bp;

	return;
}

/*
 * Put OBJID - assumes elements <= 16383 for two byte coding
 *
 */
static int
asn_put_objid (u_char **bufp, Objid *objid)
{
	int	len = 0;
	u_char	*bp = *bufp;
	u_char	*cpp;
	int	i;

	cpp = bp;
	*bp++ = objid->oid[0];
	len++;
	for ( i = 1; i <= objid->oid[0]; i++ ) {
		u_int	c = objid->oid[i];

		while ( c > 127 ) {
			*bp++ = ( ( c >> 7 ) & 0x7f ) | 0x80;
			len++;
			c &= 0x7f;		/* XXX - assumption of two bytes */
			(*cpp)++;
		}
		*bp++ = c;
		len++;
	}

	*bufp = bp;
	return ( len );

}

/*
 * Get OCTET STRING
 *
 * Octet strings are encoded as a 7-bit encoded length followed by <len>
 * data bytes;
 *
 * Arguments:
 *	bufp	- pointer to buffer pointer
 *	octet	- pointer to octet buffer
 *	plen	- pointer to PDU length
 *
 * Returns:
 *	bufp	- updated buffer pointer
 *	octet	- encoded Octet String
 *	plen	- (possibly) adjusted PDU length
 *
 */
static void
asn_get_octet (u_char **bufp, char *octet, int *plen)
{
	u_char	*bp = *bufp;
	int	i = 0;
	int	len = 0;

	/*
	 * &i is really a dummy value here as we don't keep track
	 * of the ongoing buffer length
	 */
	len = asn_get_encoded ( &bp, &i );

	for ( i = 0; i < len; i++ ) {
		*octet++ = *bp++;
		if ( plen )
			(*plen)--;
	}

	*bufp = bp;

	return;

}

/*
 * Utility to print SNMP PDU header information
 *
 * Arguments:
 *	Hdr	- pointer to internal SNMP header structure
 *
 * Returns:
 *	none
 *
 */
static void
print_header (Snmp_Header *Hdr)
{
	Variable	*var;

	if ( Log ) {
		write_timestamp();
		fprintf(Log,
			"     PDU Type: 0x%x (%s)\n"
			"      PDU len: %d\n"
			"      Version: %d\n"
			"    Community: \"%s\"\n",
			Hdr->pdutype, PDU_Types[Hdr->pdutype & 7],
			Hdr->pdulen,
			Hdr->version + 1,
			Hdr->community);

		if (Hdr->pdutype != PDU_TYPE_TRAP) {
			write_timestamp();
			fprintf(Log,
				"       Req Id: 0x%x\n"
				"        Error: %d\n"
				"  Error Index: %d\n",
				Hdr->reqid,
				Hdr->error,
				Hdr->erridx);
		}
	}

	var = Hdr->head;
	while ( var ) {
		if ( Log ) {
			write_timestamp();
			fprintf ( Log, "Variable Type: %d", var->type );
			if ( Var_Types[var->type] )
				fprintf ( Log, " (%s)", Var_Types[var->type] );
			fprintf ( Log, "\n       Object: ");
			print_objid ( &var->oid );
			fprintf ( Log, "        Value: ");
			switch ( var->type ) {
			case ASN_INTEGER:
				fprintf ( Log, "%d (0x%x)\n", var->var.ival, var->var.ival );
				break;
			case ASN_NULL:
				fprintf ( Log, "NULL" );
				break;
			default:
				fprintf ( Log, "[0x%x]", var->type );
				break;
			}
			fprintf ( Log, "\n" );
		}
		var = var->next;
	}

	return;

}

/*
 * Pull OID's from GET/SET message
 *
 * Arguments:
 *	h	- pointer to Snmp_Header
 *	bp	- pointer to input PDU
 *
 * Returns:
 *	none
 *
 */
static void
parse_oids (Snmp_Header *h, u_char **bp)
{
	int		len = h->varlen;
	int		sublen;
	Variable	*var;
	u_char		*bufp = *bp;

	while ( len > 0 ) {
	    if ( *bufp++ == ASN_SEQUENCE ) {
		len--;

		/* Create new Variable instance */
		if ( ( var = (Variable *)malloc(sizeof(Variable)) ) == NULL )
		{
			*bp = bufp;
			return;
		}
		bzero(var, sizeof(Variable));
		/* Link to tail */
		if ( h->tail )
			h->tail->next = var;
		/* Set head iff NULL */
		if ( h->head == NULL ) {
			h->head = var;
		}
		/* Adjust tail */
		h->tail = var;

		/* Get length of variable sequence */
		sublen = asn_get_pdu_len ( &bufp, &len );
		/* Should be OBJID type */
		if ( *bufp++ != ASN_OBJID ) {
			*bp = bufp;
			return;
		}
		asn_get_objid ( (u_char **)&bufp, &var->oid, &len );
		var->type = *bufp++;
		len--;
		switch ( var->type ) {
		case ASN_INTEGER:
			var->var.ival = asn_get_int ( &bufp, &len );
			break;
		case ASN_NULL:
			bufp++;
			len--;
			break;
		case ASN_OBJID:
			asn_get_objid ( &bufp, &var->var.oval, &len );
			break;
		case ASN_OCTET:
			asn_get_octet ( &bufp, var->var.sval, &len );
			break;
		default:
			if ( Log ) {
				write_timestamp();
				fprintf ( Log, "Unknown variable type: %d\n",
					var->type );
			}
			break;
		}
		var->next = NULL;
	    } else
		break;
	}

	*bp = bufp;
	return;
}

/*
 * Crack the SNMP header
 *
 * Pull the PDU length, SNMP version, SNMP community and PDU type.
 * If present, also pull out the Request ID, Error status, and Error
 * index values.
 *
 * Arguments:
 *	bufp	- pointer to buffer pointer
 *
 * Returns:
 *	bufp	- updated buffer pointer
 *		- generated SNMP header
 *
 */
static Snmp_Header *
asn_get_header (u_char **bufp)
{
	Snmp_Header	*h;
	u_char		*bp = *bufp;
	int		len = 0;
	int		dummy = 0;

	/*
	 * Allocate memory to hold the SNMP header
	 */
	if ( ( h = (Snmp_Header *)malloc(sizeof(Snmp_Header)) ) == NULL )
		return ( (Snmp_Header *)NULL );

	/*
	 * Ensure that we wipe the slate clean
	 */
	bzero(h, sizeof(Snmp_Header));

	/*
	 * PDU has to start as SEQUENCE OF
	 */
	if ( *bp++ != ASN_SEQUENCE ) /* Class == Universial, f == 1, tag == SEQUENCE */
		return ( (Snmp_Header *)NULL );

	/*
	 * Get the length of remaining PDU data
	 */
	h->pdulen = asn_get_pdu_len ( &bp, NULL );

	/*
	 * We expect to find an integer encoding Version-1
	 */
	if ( *bp++ != ASN_INTEGER ) {
		return ( (Snmp_Header *)NULL );
	}
	h->version = asn_get_int ( &bp, NULL );

	/*
	 * After the version, we need the community name
	 */
	if ( *bp++ != ASN_OCTET ) {
		return ( (Snmp_Header *)NULL );
	}
	asn_get_octet ( &bp, h->community, NULL );

	/*
	 * Single byte PDU type
	 */
	h->pdutype = *bp++;

	/*
	 * If this isn't a TRAP PDU, then look for the rest of the header
	 */
	if ( h->pdutype != PDU_TYPE_TRAP ) {	/* TRAP uses different format */

		(void) asn_get_pdu_len ( &bp, &dummy );

		/* Request ID */
		if ( *bp++ != ASN_INTEGER ) {
			free( h );
			return ( (Snmp_Header *)NULL );
		}
		h->reqid = asn_get_int ( &bp, NULL );

		/* Error Status */
		if ( *bp++ != ASN_INTEGER ) {
			free ( h );
			return ( (Snmp_Header *)NULL );
		}
		h->error = asn_get_int ( &bp, NULL );

		/* Error Index */
		if ( *bp++ != ASN_INTEGER ) {
			free ( h );
			return ( (Snmp_Header *)NULL );
		}
		h->erridx = asn_get_int ( &bp, NULL );

		/* Sequence of... */
		if ( *bp++ != ASN_SEQUENCE ) {
			free ( h );
			return ( (Snmp_Header *)NULL );
		}
		h->varlen = ( asn_get_pdu_len ( &bp, &len ) - 1 );
		h->varlen += ( len - 1 );

		parse_oids ( h, &bp );
	}

	*bufp = bp;

	return ( h );

}

/*
 * Compare two internal OID representations
 *
 * Arguments:
 *	oid1	- Internal Object Identifier
 *	oid2	- Internal Object Identifier
 *
 * Returns:
 *	0	- Objid's match
 *	1	- Objid's don't match
 *
 */
static int
oid_cmp (Objid *oid1, Objid *oid2)
{
	int	i;
	int	len;

	/*
	 * Compare lengths
	 */
	if ( !(oid1->oid[0] == oid2->oid[0] ) )
		/* Different lengths */
		return ( 1 );

	len = oid1->oid[0];

	/*
	 * value by value compare
	 */
	for ( i = 1; i <= len; i++ ) {
		if ( !(oid1->oid[i] == oid2->oid[i]) )
			/* values don't match */
			return ( 1 );
	}

	/* Objid's are identical */
	return ( 0 );
}

/*
 * Compare two internal OID representations
 *
 * Arguments:
 *	oid1	- Internal Object Identifier
 *	oid2	- Internal Object Identifier
 *	len	- Length of OID to compare
 *
 * Returns:
 *	0	- Objid's match
 *	1	- Objid's don't match
 *
 */
static int
oid_ncmp (Objid *oid1, Objid *oid2, int len)
{
	int	i;

	/*
	 * value by value compare
	 */
	for ( i = 1; i <= len; i++ ) {
		if ( !(oid1->oid[i] == oid2->oid[i]) )
			/* values don't match */
			return ( 1 );
	}

	/* Objid's are identical */
	return ( 0 );
}

/*
 * Find the index of an OBJID which matches this Variable instance.
 *
 * Arguments:
 *	var	- pointer to Variable instance
 *
 * Returns:
 *	idx	- index of matched Variable instance
 *	-1	- no matching Variable found
 *
 */
static int
find_var (Variable *var)
{
	size_t	i;

	for ( i = 0; i < NUM_OIDS; i++ )
		if ( oid_cmp ( &var->oid, &Objids[i] ) == 0 ) {
			return ( i );
		}

	return ( -1 );

}

/*
 * Return the time process has been running as a number of ticks 
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	number of ticks
 *
 */
static int
get_ticks (void)
{
	struct timeval	timenow;
	struct timeval	timediff;

	(void) gettimeofday ( &timenow, NULL );
	/*
	 * Adjust for subtraction
	 */
	timenow.tv_sec--;
	timenow.tv_usec += 1000000;

	/*
	 * Compute time since 'starttime'
	 */
	timediff.tv_sec = timenow.tv_sec - starttime.tv_sec;
	timediff.tv_usec = timenow.tv_usec - starttime.tv_usec;

	/*
	 * Adjust difference timeval
	 */
	if ( timediff.tv_usec >= 1000000 ) {
		timediff.tv_usec -= 1000000;
		timediff.tv_sec++;
	}

	/*
	 * Compute number of ticks
	 */
	return ( ( timediff.tv_sec * 100 ) + ( timediff.tv_usec / 10000 ) );

}

/*
 * Build a response PDU
 *
 * Arguments:
 *	hdr	- pointer to PDU Header with completed Variable list
 *
 * Returns:
 *	none
 *
 */
static void
build_pdu (Snmp_Header *hdr, int type)
{
	u_char		*bp = Resp_Buf;
	u_char		*vpp;
	u_char		*ppp;
	int		erridx = 0;
	int		varidx = 1;
	int		varlen = 0;
	int		pdulen = 0;
	int		traplen = 0;
	Variable	*var;

	/*
	 * Clear out the reply
	 */
	bzero ( Resp_Buf, sizeof(Resp_Buf) );

	/* [0] is reserved for overall length */
	bp++;

	/* Start with SEQUENCE OF */
	*bp++ = ASN_SEQUENCE;
	/* - assume we can code length in two octets */
	*bp++ = 0x82;
	bp++;
	bp++;
	/* Version */
	*bp++ = ASN_INTEGER;
	asn_set_int ( &bp, hdr->version );
	/* Community name */
	*bp++ = ASN_OCTET;
	*bp++ = strlen ( hdr->community );
	bcopy ( hdr->community, bp, strlen ( hdr->community ) );
	bp += strlen ( hdr->community );
	/* PDU Type */
	*bp++ = type;
	ppp = bp;
	/* Length of OID data - assume it'll fit in one octet */
	bp++;

	if ( type != PDU_TYPE_TRAP ) {
	    /* Sequence ID */
	    *bp++ = ASN_INTEGER;
	    asn_set_int ( &bp, hdr->reqid );
	    /*
	     * Check to see if all the vaiables were resolved - we do this
	     * by looking for something which still has an ASN_NULL value.
	     */
	    var = hdr->head;
	    if ( type == PDU_TYPE_GETRESP ) {
	        while ( var && erridx == 0 ) {
		    if ( var->type != ASN_NULL ) {
			    varidx++;
			    var = var->next;
		    } else
			erridx = varidx;
		}
	    }

	    /* Error status */
	    *bp++ = ASN_INTEGER;
	    *bp++ = 0x01;	/* length = 1 */
	    if ( erridx )
		*bp++ = SNMP_ERR_NOSUCHNAME;
	    else
		*bp++ = SNMP_ERR_NOERROR;
	    /* Error Index */
	    *bp++ = ASN_INTEGER;
	    *bp++ = 0x01;	/* length = 1 */
	    *bp++ = erridx;	/* index == 0 if no error */
	} else {
		/* type == PDU_TYPE_TRAP */

		/* Fill in ENTERPRISE OBJID */
		*bp++ = ASN_OBJID;
		(void) asn_put_objid ( &bp, &hdr->enterprise );

		/* Fill in IP address */
		*bp++ = ASN_IPADDR;
		*bp++ = sizeof ( hdr->ipaddr );
		bcopy ( (caddr_t)&hdr->ipaddr, bp, sizeof(hdr->ipaddr) );
		bp += sizeof(hdr->ipaddr);

		/* Fill in generic and specific trap types */
		*bp++ = ASN_INTEGER;
		asn_set_int ( &bp, hdr->generic_trap );
		*bp++ = ASN_INTEGER;
		asn_set_int ( &bp, hdr->specific_trap );

		/* Fill in time-stamp  - assume 0 for now */
		*bp++ = ASN_TIMESTAMP;
		asn_set_int ( &bp, 0 );
		
		/* encoded length */
		traplen = ( bp - ppp - 1 );

		/* Continue with variable processing */
	}

	/* SEQUENCE OF */
	*bp++ = ASN_SEQUENCE;
	*bp++ = 0x82;
	/* - assume we can code length in two octets */
	vpp = bp;
	varlen = 0;
	bp++;
	bp++;

	/* Install Variables */
	var = hdr->head;
	varidx = 1;
	while ( var ) {
		u_char *bpp;
		int	len = 0;

		/* SEQUENCE OF */
		*bp++ = ASN_SEQUENCE;
		*bp++ = 0x82;
		/* - assume we can code length in two octets */
		bpp = bp;
		bp++;
		bp++;
		/* OBJID */
		*bp++ = ASN_OBJID;
		len++;

		len += asn_put_objid ( &bp, &var->oid );

		if ( erridx && varidx >= erridx ) {
			/* Code this variable as NULL */
			*bp++ = ASN_NULL;
			len++;
			bp++;
			len++;
		} else {
			u_char *lpp;
			/* Variable type */
			*bp++ = var->type;
			len++;
			lpp = bp;
			switch ( var->type ) {
			case ASN_INTEGER:
				asn_set_int ( &bp, var->var.ival );
				len += ( *lpp + 1 );
				break;
			case ASN_OCTET:
				*bp++ = var->var.sval[0];
				len++;
				bcopy ( (caddr_t)&var->var.sval[1],
					bp, var->var.sval[0] );
				len += var->var.sval[0];
				bp += var->var.sval[0];
				break;
			case ASN_NULL:
				*bp++ = 0x00;
				len++;
				break;
			case ASN_OBJID:
				len += asn_put_objid ( &bp, &var->var.oval );
				break;
			case ASN_SEQUENCE:
				break;
			case ASN_IPADDR:
				*bp++ = 4;
				len++;
				bcopy ( (caddr_t)&var->var.aval, bp, 4 );
				len += 4;
				bp += 4;
				break;
			case ASN_TIMESTAMP:
				asn_set_int ( &bp, var->var.ival );
				len += ( *lpp + 1 );
				break;
			default:
				break;
			}
		}

		/* Accumulate total Variable sequence length */
		varlen += (len + 4);

		/* Fill in length of this sequence */
		bpp[1] = len & 0xff;
		bpp[0] = len >> 8;

		var = var->next;
	}


	/* Fill in length of Variable sequence */
	vpp[1] = varlen & 0xff;
	vpp[0] = varlen >> 8;

	if ( type != PDU_TYPE_TRAP ) {
		/* Fill in length of data AFTER PDU type */
		*ppp = varlen + 12 + ppp[2];	/* + length of reqid */
	} else {
		/* Fill in length of data AFTER PDU  type */
		*ppp = varlen + traplen + 4;	/* + length of initial sequence of */
	}

	/* Fill in overall sequence length */
	pdulen = *ppp + 7 + strlen ( hdr->community );
	Resp_Buf[4] = pdulen & 0x7f;
	Resp_Buf[3] = pdulen >> 8;

	pdulen = bp - Resp_Buf - 1;

	Resp_Buf[0] = pdulen;

	hdr->pdutype = type;
	hdr->pdulen = pdulen;

	return;
}

static void
free_pdu (Snmp_Header *hdr)
{
	Variable	*var;

	while ( hdr->head ) {
		var = hdr->head->next;		/* Save next link */
		free ( hdr->head );		/* Free current var */
		hdr->head = var;		/* Set head to next link */
	}

	free ( hdr );				/* Free fixed portion */
}

static void
print_pdu (int dir, int intf, Snmp_Header *Hdr, int len, u_char *buf)
{
	const char *	pdu_dir;
	const char *	pdu_type;
	int		pdu_num;

	write_timestamp();

	switch (dir) {
	case PDU_SEND:
		pdu_dir = "SEND";
		break;
	case PDU_RECV:
		pdu_dir = "RECV";
		break;
	default:
		pdu_dir = "undefined";
		break;
	}

	if (Hdr == NULL) {
		pdu_type = "unknown";
		pdu_num = 0;
	} else {
		pdu_type = PDU_Types[Hdr->pdutype & 7];
		pdu_num = Hdr->pdutype;
	}

	fprintf(Log,
		"%s: %s(%d), ILMI %s(%d), PDU Type %s(0x%x) %d/%d bytes.\n",
		pdu_dir,
		Intf[intf].anp_intf, ilmi_fd[intf],
		ILMI_State[intf], ilmi_state[intf],
		pdu_type, pdu_num,
		len, buf[0]);

	if (Hdr == NULL)
		fprintf(Log, "Header seems to be invalid.\n");
	else
		print_header(Hdr);

	hexdump(Log, (u_char *)&buf[1], len, NULL);

	return;
}

/*
 * Send a generic response packet
 *
 * Arguments:
 *	sd	- socket to send the reply on
 *	reqid	- original request ID from GET PDU
 *	resp	- pointer to the response to send
 *
 * Returns:
 *	none	- response sent
 *
 */
static void
send_resp (int intf, Snmp_Header *Hdr, u_char *resp)
{
	int	n;

	if ( ilmi_fd[intf] > 0 ) {
	    n = write ( ilmi_fd[intf], (caddr_t)&resp[1], resp[0] );
	    if ( Log && Debug_Level > 1 ) {
		print_pdu(PDU_SEND, intf, Hdr, n, resp);
	    }
	}

	free_pdu ( Hdr );
	return;
}

/*
 * Build a COLD_START TRAP PDU
 *
 */
static Snmp_Header *
build_cold_start (void)
{
	Snmp_Header	*hdr;
	Variable	*var;

	hdr = (Snmp_Header *)malloc(sizeof(Snmp_Header));
	if (hdr == NULL) {
		fprintf(stderr, "malloc() failed in %s()\n", __func__);
		exit(1);
	}
	bzero(hdr, sizeof(Snmp_Header));

	hdr->pdulen = 0;
	hdr->version = SNMP_VERSION_1 - 1;
	snprintf ( hdr->community, sizeof(hdr->community), "ILMI" );

	hdr->ipaddr = 0x0;	/* 0.0.0.0 */
	hdr->generic_trap = TRAP_COLDSTART;
	hdr->specific_trap = 0;
	bcopy ( (caddr_t)&Objids[ENTERPRISE_OBJID], (caddr_t)&hdr->enterprise,
		sizeof(Objid) );

	hdr->head = (Variable *)malloc(sizeof(Variable));
	if (hdr == NULL) {
		fprintf(stderr, "malloc() failed in %s()\n", __func__);
		exit(1);
	}
	bzero(hdr->head, sizeof(Variable));

	var = hdr->head;
	bcopy ( (caddr_t)&Objids[UPTIME_OBJID], (caddr_t)&var->oid,
		sizeof(Objid) );
	var->type = ASN_NULL;

	return ( hdr );
}

/*
 * Build a Generic PDU Header
 *
 */
static Snmp_Header *
build_generic_header (void)
{
	Snmp_Header	*hdr;

	hdr = (Snmp_Header *)malloc(sizeof(Snmp_Header));
	if (hdr == NULL) {
		fprintf(stderr, "malloc() failed in %s()\n", __func__);
		exit(1);
	}
	bzero(hdr, sizeof(Snmp_Header));

	hdr->pdulen = 0;
	hdr->version = SNMP_VERSION_1 - 1;
	snprintf ( hdr->community, sizeof(hdr->community), "ILMI" );

	return ( hdr );
}

/* 
 * Initialize information on what physical adapters HARP knows about
 *
 * Query the HARP subsystem about configuration and physical interface
 * information for any currently registered ATM adapters. Store the information
 * as arrays for easier indexing by SNMP port/index numbers.
 *      
 * Arguments:
 *      none
 *
 * Returns:
 *      none            Information from HARP available 
 *      
 */
static void
init_ilmi (void)
{
        struct  air_cfg_rsp     *cfg_info = NULL;
        struct  air_int_rsp    *intf_info = NULL;
        int                     buf_len;

	/*
	 * Get configuration info - what's available with 'atm sh config'
	 */
        buf_len = get_cfg_info ( NULL, &cfg_info );
	/*
	 * If error occurred, clear out everything
	 */
	if ( buf_len <= 0 ) {
		bzero ( Cfg, sizeof(Cfg) );
		bzero( Intf, sizeof(Intf) );
		NUnits = 0;
		return;
	}

	/*
	 * Move to local storage
	 */
        bcopy ( cfg_info, (caddr_t)Cfg, buf_len );
	/*
	 * Compute how many units information was returned for
	 */
        NUnits = buf_len / sizeof(struct air_cfg_rsp);
	/* Housecleaning */
        free ( cfg_info );
        cfg_info = NULL;
	/*
	 * Get the per interface information
	 */
        buf_len = get_intf_info ( NULL, &intf_info );
	/*
	 * If error occurred, clear out Intf info
	 */
	if ( buf_len <= 0 ) {
		bzero ( Intf, sizeof(Intf) );
		return;
	}

	/*
	 * Move to local storage
	 */
        bcopy ( intf_info, (caddr_t)Intf, buf_len );
	/* Housecleaning */
        free ( intf_info );
        intf_info = NULL;

	return;

}

/*
 * Open a new SNMP session for ILMI
 *
 * Start by updating interface information, in particular, how many
 * interfaces are in the system. While we'll try to open sessons on
 * all interfaces, this deamon currently can only handle the first
 * interface.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *      none
 *
 */
static void
ilmi_open (void)
{
        struct sockaddr_atm     satm;
        struct t_atm_aal5       aal5;
        struct t_atm_traffic    traffic;
        struct t_atm_bearer     bearer;
        struct t_atm_qos        qos;
	struct t_atm_app_name	appname;
        Atm_addr                subaddr;
        char                    nifname[IFNAMSIZ];
        int                     optlen;
        int                     unit = 0;
	u_char			sig_proto;

        init_ilmi();

	for ( unit = 0; unit < NUnits; unit++ ) {

	    /*
	     * ILMI only makes sense for UNI signalling protocols
	     */
	    sig_proto = Intf[unit].anp_sig_proto;
	    if ( sig_proto != ATM_SIG_UNI30 && sig_proto != ATM_SIG_UNI31 &&
		sig_proto != ATM_SIG_UNI40 )
		    continue;

       	    if ( ilmi_fd[unit] == -1 ) {

       	        ilmi_fd[unit] = socket ( AF_ATM, SOCK_SEQPACKET, ATM_PROTO_AAL5 );

       	        if ( ilmi_fd[unit] < 0 ) {
               	    perror ( "open" );
               	    continue;
       	        }

                /*
                 * Set interface name. For now, we must have a netif to go on...
                 */
                if ( Intf[unit].anp_nif_cnt == 0 ) {
		    if ( Debug_Level > 1 && Log ) {
			write_timestamp();
			fprintf ( Log, "No nif on unit %d\n", unit );
		    }
               	    close ( ilmi_fd[unit] );
               	    ilmi_fd[unit] = -1;
               	    continue;
                }
                sprintf ( nifname, "%s0", Intf[unit].anp_nif_pref );
                optlen = sizeof ( nifname );
                if ( setsockopt ( ilmi_fd[unit], T_ATM_SIGNALING,
		    T_ATM_NET_INTF, (caddr_t)nifname, optlen ) < 0 ) {
                       	perror ( "setsockopt" );
			if ( Log ) {
			    write_timestamp();
                            fprintf ( Log,
				"Couldn't set interface name \"%s\"\n",
				    nifname );
			}
			if ( Debug_Level > 1 && Log ) {
			    write_timestamp();
			    fprintf ( Log, "nifname: closing unit %d\n", unit );
			}
                       	close ( ilmi_fd[unit] );
			ilmi_fd[unit] = -1;
                       	continue;
                }

                /*
                 * Set up destination SAP
                 */
                bzero ( (caddr_t) &satm, sizeof(satm) );
                satm.satm_family = AF_ATM;
#if (defined(BSD) && (BSD >= 199103))
                satm.satm_len = sizeof(satm);
#endif

                satm.satm_addr.t_atm_sap_addr.SVE_tag_addr = T_ATM_PRESENT;
                satm.satm_addr.t_atm_sap_addr.SVE_tag_selector = T_ATM_ABSENT;
                satm.satm_addr.t_atm_sap_addr.address_format = T_ATM_PVC_ADDR;
                satm.satm_addr.t_atm_sap_addr.address_length = sizeof(Atm_addr_pvc);
                ATM_PVC_SET_VPI((Atm_addr_pvc *)satm.satm_addr.t_atm_sap_addr.address,
                    0 );
                ATM_PVC_SET_VCI((Atm_addr_pvc *)satm.satm_addr.t_atm_sap_addr.address,
                    16 );
    
                satm.satm_addr.t_atm_sap_layer2.SVE_tag = T_ATM_PRESENT;
                satm.satm_addr.t_atm_sap_layer2.ID_type = T_ATM_SIMPLE_ID;
                satm.satm_addr.t_atm_sap_layer2.ID.simple_ID = T_ATM_BLLI2_I8802;

                satm.satm_addr.t_atm_sap_layer3.SVE_tag = T_ATM_ABSENT;

                satm.satm_addr.t_atm_sap_appl.SVE_tag = T_ATM_ABSENT;

                /*
                 * Set up connection parameters
                 */
                aal5.forward_max_SDU_size = MAX_LEN;
                aal5.backward_max_SDU_size = MAX_LEN;
                aal5.SSCS_type = T_ATM_NULL;
                optlen = sizeof(aal5);
                if ( setsockopt ( ilmi_fd[unit], T_ATM_SIGNALING, T_ATM_AAL5,
                (caddr_t) &aal5, optlen ) < 0 ) {
                    perror ( "setsockopt(aal5)" );
		    if ( Debug_Level > 1 && Log ) {
			write_timestamp();
			fprintf ( Log, "aal5: closing unit %d\n", unit );
		    }
                    close ( ilmi_fd[unit] );
                    ilmi_fd[unit] = -1;
                    continue;
                }

                traffic.forward.PCR_high_priority = T_ATM_ABSENT;
                traffic.forward.PCR_all_traffic = 100000;
                traffic.forward.SCR_high_priority = T_ATM_ABSENT;
                traffic.forward.SCR_all_traffic = T_ATM_ABSENT;
                traffic.forward.MBS_high_priority = T_ATM_ABSENT;
                traffic.forward.MBS_all_traffic = T_ATM_ABSENT;
                traffic.forward.tagging = T_NO;
                traffic.backward.PCR_high_priority = T_ATM_ABSENT;
                traffic.backward.PCR_all_traffic = 100000;
                traffic.backward.SCR_high_priority = T_ATM_ABSENT;
                traffic.backward.SCR_all_traffic = T_ATM_ABSENT;
                traffic.backward.MBS_high_priority = T_ATM_ABSENT;
                traffic.backward.MBS_all_traffic = T_ATM_ABSENT;
                traffic.backward.tagging = T_NO;
                traffic.best_effort = T_YES;
                optlen = sizeof(traffic);
                if (setsockopt(ilmi_fd[unit], T_ATM_SIGNALING, T_ATM_TRAFFIC,
                        (caddr_t)&traffic, optlen) < 0) {
                    perror("setsockopt(traffic)");
                }
                bearer.bearer_class = T_ATM_CLASS_X;
                bearer.traffic_type = T_ATM_NULL;
                bearer.timing_requirements = T_ATM_NULL;
                bearer.clipping_susceptibility = T_NO;
                bearer.connection_configuration = T_ATM_1_TO_1;
                optlen = sizeof(bearer);
                if (setsockopt(ilmi_fd[unit], T_ATM_SIGNALING, T_ATM_BEARER_CAP,
                        (caddr_t)&bearer, optlen) < 0) {
                    perror("setsockopt(bearer)");
                }

                qos.coding_standard = T_ATM_NETWORK_CODING;
                qos.forward.qos_class = T_ATM_QOS_CLASS_0;
                qos.backward.qos_class = T_ATM_QOS_CLASS_0;
                optlen = sizeof(qos);
                if (setsockopt(ilmi_fd[unit], T_ATM_SIGNALING, T_ATM_QOS, (caddr_t)&qos,
                        optlen) < 0) {
                    perror("setsockopt(qos)");
                }

                subaddr.address_format = T_ATM_ABSENT;
                subaddr.address_length = 0;
                optlen = sizeof(subaddr);
                if (setsockopt(ilmi_fd[unit], T_ATM_SIGNALING, T_ATM_DEST_SUB,
                        (caddr_t)&subaddr, optlen) < 0) {
                    perror("setsockopt(dest_sub)");
                }

	        strncpy(appname.app_name, "ILMI", T_ATM_APP_NAME_LEN);
	        optlen = sizeof(appname);
	        if (setsockopt(ilmi_fd[unit], T_ATM_SIGNALING, T_ATM_APP_NAME,
			(caddr_t)&appname, optlen) < 0) {
		    perror("setsockopt(appname)");
	        }

                /*
                 * Now try to connect to destination
                 */
                if ( connect ( ilmi_fd[unit], (struct sockaddr *) &satm,
                    sizeof(satm)) < 0 ) {
                        perror ( "connect" );
		        if ( Debug_Level > 1 && Log ) {
			    write_timestamp();
			    fprintf ( Log, "connect: closing unit %d\n", unit );
			}
                        close ( ilmi_fd[unit] );
                        ilmi_fd[unit] = -1;
                        continue;
                }

    	        if ( Debug_Level && Log ) {
		    write_timestamp();
		    fprintf ( Log, "***** opened unit %d\n", unit );
		}

		ilmi_state[unit] = ILMI_COLDSTART;

	    }

	}

	return;

}

/*
 * Get our local IP address for this interface
 *
 * Arguments:
 *	s	- socket to find address for
 *	aval	- pointer to variable to store address in
 *
 * Returns:
 *	none
 *
 */
static void
get_local_ip (int s, long *aval)
{
	char			intf_name[IFNAMSIZ];
	socklen_t		namelen = IFNAMSIZ;
	struct air_netif_rsp	*net_info = NULL;
	struct sockaddr_in	*sain;

	/*
	 * Get physical interface name
	 */
	if ( getsockopt ( s, T_ATM_SIGNALING, T_ATM_NET_INTF,
	    (caddr_t) intf_name, &namelen ) )
		return;

	/*
	 * Get network interface information for this physical interface
	 */
	get_netif_info ( intf_name, &net_info );
	if ( net_info == NULL )
		return;

	sain = (struct sockaddr_in *)(void *)&net_info->anp_proto_addr;

	/*
	 * Fill in answer
	 */
	bcopy ( (caddr_t)&sain->sin_addr.s_addr, aval, 4 );

	free ( net_info );

	return;

}

/*
 * Set local NSAP prefix and then reply with our full NSAP address.
 *
 * Switch will send a SET message with the NSAP prefix after a coldStart.
 * We'll set that prefix into HARP and then send a SET message of our own
 * with our full interface NSAP address.
 *
 * Arguments:
 *	oid	- objid from SET message
 *	hdr	- pointer to internal SNMP header
 *	buf	- pointer to SET buffer
 *	s	- socket to send messages on
 *
 * Returns:
 *	none
 *
 */
static void
set_prefix (Objid *oid, Snmp_Header *hdr __unused, int intf)
{
	struct atmsetreq	asr;
	Atm_addr		*aa;
	int			fd;
	int			i;

	/*
	 * Build IOCTL request to set prefix
	 */
	asr.asr_opcode = AIOCS_SET_PRF;
	strncpy ( asr.asr_prf_intf, Intf[intf].anp_intf,
		sizeof(asr.asr_prf_intf ) );
	/*
	 * Pull prefix out of received Objid
	 *	save in set_prefix IOCTL and addressEntry table
	 */
	for ( i = 0; i < oid->oid[13]; i++ ) {
		asr.asr_prf_pref[i] = oid->oid[i + 14];
	}

	/*
	 * Pass new prefix to the HARP kernel
	 */
	fd = socket ( AF_ATM, SOCK_DGRAM, 0 );
	if ( fd < 0 ) 
		return;
	if ( ioctl ( fd, AIOCSET, (caddr_t)&asr ) < 0 ) {
		if ( errno != EALREADY ) {
		    syslog ( LOG_ERR, "ilmid: error setting prefix: %m" );
		    if ( Log ) {
			write_timestamp();
			fprintf ( Log, "errno %d setting prefix\n",
			    errno );
		    }
		    close ( fd );
		    return;
		}
	}
	close ( fd );

	/*
	 * Reload the cfg/intf info with newly set prefix
	 */
	init_ilmi();

	aa = &Intf[intf].anp_addr;

	/*
	 * Copy our NSAP into addressEntry table
	 */

	addressEntry[intf].oid[0] = 0;
	for ( i = 0; i < aa->address_length; i++ ) {
		addressEntry[intf].oid[0]++;	/* Increment length */
		addressEntry[intf].oid[i + 1] = (int)((u_char *)(aa->address))[i];

	}

	return;

}

static void
set_address (Snmp_Header *hdr __unused, int intf)
{
	Variable	*var;
	int		i, j;

	PDU_Header = build_generic_header();

	PDU_Header->head = (Variable *)malloc(sizeof(Variable));
	if (PDU_Header->head == NULL) {
		fprintf(stderr, "malloc() failed in %s()\n", __func__);
		exit(1);
	}
	bzero(PDU_Header->head, sizeof(Variable));

	var = PDU_Header->head;
	/* Copy generic addressEntry OBJID */
	bcopy ( (caddr_t)&Objids[ADDRESS_OBJID], (caddr_t)&var->oid,
		sizeof(Objid) );
	/* Set specific instance */
	i = var->oid.oid[0] + 1;		/* Get length */
	var->oid.oid[i++] = 1;
	var->oid.oid[i++] = 1;
	var->oid.oid[i++] = 3;
	var->oid.oid[i++] = 0;

	/* Copy in address length */
	var->oid.oid[i++] = addressEntry[intf].oid[0];

	/* Copy in address */
	for ( j = 0; j < addressEntry[intf].oid[0]; j++ )
		var->oid.oid[i++] = addressEntry[intf].oid[j + 1];
	var->oid.oid[0] = i - 1;		/* Set new length */

	/* Set == VALID */
	var->type = ASN_INTEGER;
	var->var.ival = 1;

	build_pdu ( PDU_Header, PDU_TYPE_SET );
	send_resp ( intf, PDU_Header, Resp_Buf );
}

/*
 * Increment Debug Level
 *
 * Catches SIGUSR1 signal and increments value of Debug_Level
 *
 * Arguments:
 *	sig	- signal number
 *
 * Returns:
 *	none	- Debug_Level incremented
 *
 */
static void
Increment_DL (int sig __unused)
{
	Debug_Level++;
	if ( Debug_Level && Log == (FILE *)NULL ) {
	    if ( foregnd ) {
		Log = stderr;
	    } else {
	        if ( ( Log = fopen ( LOG_FILE, "a" ) ) == NULL ) 
		    Log = NULL;
	    }
	    if ( Log ) {
		setbuf ( Log, NULL );
		write_timestamp();
		fprintf ( Log, "Raised Debug_Level to %d\n", Debug_Level );
	    }
	}
	signal ( SIGUSR1, Increment_DL );
	return;
}

/*
 * Decrement Debug Level
 *
 * Catches SIGUSR2 signal and decrements value of Debug_Level
 *
 * Arguments:
 *	sig	- signal number
 *
 * Returns:
 *	none	- Debug_Level decremented
 *
 */
static void
Decrement_DL (int sig __unused)
{
	Debug_Level--;
	if ( Debug_Level <= 0 ) {
	    Debug_Level = 0;
	    if ( Log ) {
		write_timestamp();
		fprintf ( Log, "Lowered Debug_Level to %d\n", Debug_Level );
		if ( !foregnd )
		    fclose ( Log );
		Log = NULL;
	    }
	}
	signal ( SIGUSR2, Decrement_DL );
	return;
}

/*
 * Loop through GET variable list looking for matches
 *
 */
static void
process_get (Snmp_Header *hdr, int intf)
{
	Variable	*var;
	int		idx;
	int		x;
	int		oidlen;

	var = hdr->head;
	while ( var ) {

		/* Handle the 'GET PREFIX' request */
		oidlen = Objids[SETPFX_OBJID].oid[0];
		if (oid_ncmp(&var->oid, &Objids[SETPFX_OBJID], oidlen) == 0) {
			var->var.ival = 2;           /* assume not valid */
			for(x = 0; x < 13; x++)
				if (var->oid.oid[oidlen + x + 2] !=
				    addressEntry[intf].oid[x + 1])
					break;

			/* Address Match */
			if (x == 13)
				hdr->head->var.ival = 1;
			var = var->next;
			continue;
		}

		idx = find_var ( var );
		switch ( idx ) {
		case MADGE_OBJECT1:
			/* reply with NO SUCH OBJECT */
			var->type = ASN_NULL;
			break;
		case SYS_OBJID:
			var->type = ASN_OBJID;
			bcopy ( (caddr_t)&Objids[MY_OBJID],
			    (caddr_t)&var->var.oval,
				sizeof(Objid) );
			break;
		case UPTIME_OBJID:
			var->type = ASN_TIMESTAMP;
			var->var.ival = get_ticks();
			break;
		case UNITYPE_OBJID:
			var->type = ASN_INTEGER;
			var->var.ival = UNITYPE_PRIVATE;
			break;
		case UNIVER_OBJID:
			var->type = ASN_INTEGER;
			switch ( Intf[intf].anp_sig_proto ) {
			case ATM_SIG_UNI30:
				var->var.ival = UNIVER_UNI30;
				break;
			case ATM_SIG_UNI31:
				var->var.ival = UNIVER_UNI31;
				break;
			case ATM_SIG_UNI40:
				var->var.ival = UNIVER_UNI40;
				break;
			default:
				var->var.ival = UNIVER_UNKNOWN;
				break;
			}
			break;
		case DEVTYPE_OBJID:
			var->type = ASN_INTEGER;
			var->var.ival = DEVTYPE_USER;
			break;
		case MAXVCC_OBJID:
			var->type = ASN_INTEGER;
			var->var.ival = 1024;
			break;
		case PORT_OBJID:
			var->type = ASN_INTEGER;
			var->var.ival = intf + 1;
			break;
		case IPNM_OBJID:
			var->type = ASN_IPADDR;
			get_local_ip( ilmi_fd[intf], &var->var.aval );
			break;
		case ADDRESS_OBJID:
			break;
		case ATMF_PORTID:
			var->type = ASN_INTEGER;
			var->var.ival = 0x30 + intf;
			break;
		case ATMF_SYSID:
			var->type = ASN_OCTET;
			var->var.sval[0] = 6;
			bcopy ( (caddr_t)&Cfg[intf].acp_macaddr,
			    (caddr_t)&var->var.sval[1], 6 );
			break;
		default:
			/* NO_SUCH */
			break;
		}
		var = var->next;
	}
	build_pdu ( hdr, PDU_TYPE_GETRESP );
	send_resp ( intf, hdr, Resp_Buf );

}

/******************************************************************************
 *
 *  Find an OBJID from known ones
 *
 *  in:  Variable with valid OID
 * out:  OID number (index), -1 = not found
 */
static int
lmi_object_find (Variable *var)
{
	Objid *	obj_var;
	Objid *	obj_cur;
	size_t	x;
	int	y;

	obj_var = &var->oid;

	for (x = 0; x < NUM_OIDS; x++) {
		obj_cur = &Objids[x];
		for (y = 0; y < 128; y++) {
			if (obj_var->oid[y] != obj_cur->oid[y])
				break;
			if (obj_var->oid[y] == 0)    /* object ID endmark */
				return (x);
		}
	}

	return (-1);
}

#if 0
/******************************************************************************
 *
 *  Append instance number to OID
 *
 *  in:  Variable, instance number
 * out:  zero = success
 *
 */
static int
lmi_object_instance (Variable *var, int instnum)
{
	int *	oidptr;
	int	curlen;

	oidptr = var->oid.oid;
	curlen = oidptr[0];	/* current length */
	if (curlen > 126)
		return (1);
	curlen++;
	oidptr[curlen] = instnum;
	oidptr[0] = curlen;
	return (0);
}
#endif

/******************************************************************************
 *
 *  Handle received GETNEXT
 *
 *  in:  Header with valid fields, interface number
 * out:  zero = success
 *
 */
static int
lmi_rcvcmd_getnext (Snmp_Header *header, int intf)
{
	int *	oidptr;
	int	oidlen;
	int	oidnum;
	int	x;

	oidnum = lmi_object_find(header->head);
	oidptr = header->head->oid.oid;
	oidlen = oidptr[0];

	switch(oidnum) {
	/* Should be because the remote side is attempting
	 * to verify that our table is empty
	 */
	case ADDRESS_OBJID:
		if ( addressEntry[intf].oid[0] ) {
			/* XXX - FIXME */
			/* Our table is not empty - return address */
		}
		break;

	/* Madge Collage sends GETNEXT for this */
	case SETPFX_OBJID:
		if(addressEntry[intf].oid[0]) {	/* we have a prefix */
			oidptr[0] += 14;
			oidptr += oidlen;	/* skip to last number */
			oidptr++;
			*oidptr++ = 13;		/* length of prefix */

			/* fill in the prefix */
			for(x = 0; x < 13; x++) {
				*oidptr++ = addressEntry[intf].oid[x+1];
			}
			header->head->type = ASN_INTEGER;
			/* 1=valid, 2=invalid -- only 2 values */
			header->head->var.ival = 1;
		} else {			 /* no prefix available */
			header->head->type = ASN_NULL;
		}
		break;

	default:
		return (1);			/* unknown object ID */
	}
  
	build_pdu(header, PDU_TYPE_GETRESP);
	send_resp(intf, header, Resp_Buf);

	return (0);
}


/******************************************************************************
 *
 *  Handle received TRAP
 *
 *  in:  Header with valid fields, interface number
 * out:  zero = success
 *
 */
static int
lmi_rcvcmd_trap (Snmp_Header *header __unused, int intf)
{

	bzero((caddr_t)&addressEntry[intf], sizeof(Objid));
	return (0);
}

/*
 * ILMI State Processing Loop
 *
 *
 */
static void
ilmi_do_state(void)
{
	struct timeval	tvp;
	fd_set		rfd;
	u_char		buf[1024];
	Variable	*var;
	int		intf;
	int		maxfd = 0;

	/*
	 * Loop forever
	 */
	for ( ; ; ) {
	    int		count;
	    int		n;
	    u_char	*bpp;
	    Snmp_Header	*Hdr;

	    /*
	     * SunOS CC doesn't allow automatic aggregate initialization.
	     * Initialize to zero which effects a poll operation.
	     */
	    tvp.tv_sec = 15;
	    tvp.tv_usec = 0;

	    /*
	     * Clear fd_set and initialize to check this interface
	     */
	    FD_ZERO ( &rfd );
	    for ( intf = 0; intf < MAX_UNITS; intf++ )
	        if ( ilmi_fd[intf] > 0 ) {
		    FD_SET ( ilmi_fd[intf], &rfd );
		    maxfd = MAX ( maxfd, ilmi_fd[intf] );
	        }

	    /*
	     * Check for new interfaces
	     */
	    ilmi_open();

	    for ( intf = 0; intf < MAX_UNITS; intf++ ) {
		/*
		 * Do any pre-message state processing
		 */
	    	switch ( ilmi_state[intf] ) {
	    	case ILMI_COLDSTART:
			/*
	 		 * Clear addressTable
	 		 */
			bzero ( (caddr_t)&addressEntry[intf], sizeof(Objid) );

			/*
			 * Start by sending a COLD_START trap. This should cause the
			 * remote end to clear the associated prefix/address table(s).
	 		 */
			/* Build ColdStart TRAP header */
			ColdStart_Header = build_cold_start();
			build_pdu ( ColdStart_Header, PDU_TYPE_TRAP );
			send_resp ( intf, ColdStart_Header, Resp_Buf );

			/*
	 		 * Start a timeout so that if the next state fails, we re-enter
	 		 * ILMI_COLDSTART.
	 		 */
			/* atm_timeout() */
	
			/* Enter new state */
			ilmi_state[intf] = ILMI_INIT;
			/* fall into ILMI_INIT */

    		case ILMI_INIT:
			/*
	 		 * After a COLD_START, we need to check that the remote end has
	 		 * cleared any tables. Send a GET_NEXT request to check for this.
	 		 * In the event that the table is not empty, or that no reply is
	 		 * received, return to COLD_START state.
	 		 */
			PDU_Header = build_generic_header();

			PDU_Header->head = (Variable *)malloc(sizeof(Variable));
			if (PDU_Header->head == NULL) {
				fprintf(stderr, "malloc() failed in %s()\n", __func__);
				exit(1);
			}
			bzero(PDU_Header->head, sizeof(Variable));

			var = PDU_Header->head;
			bcopy ( (caddr_t)&Objids[ADDRESS_OBJID], (caddr_t)&var->oid,
	    		    sizeof(Objid) );
			var->type = ASN_NULL;
			var->next = NULL;
	
			/*
	 		 * Send GETNEXT request looking for empty ATM Address Table
	 		 */
			PDU_Header->reqid = Req_ID++;
			build_pdu ( PDU_Header, PDU_TYPE_GETNEXT );
			send_resp ( intf, PDU_Header, Resp_Buf );
	
			/*
	 		 * Start a timeout while looking for SET message. If we don't receive
	 		 * a SET, then go back to COLD_START state.
	 		 */
			/* atm_timeout() */
			break;
	
    		case ILMI_RUNNING:
			/* Normal SNMP processing */
			break;
	
    		default:
			break;
    		}
	    }

	    count = select ( maxfd + 1, &rfd, NULL, NULL, &tvp );

	    for ( intf = 0; intf < MAX_UNITS; intf++ ) {
		/*
		 * Check for received messages
		 */
		if ( ilmi_fd[intf] > 0 && FD_ISSET ( ilmi_fd[intf], & rfd ) ) {
		
		    n = read ( ilmi_fd[intf], (caddr_t)&buf[1], sizeof(buf) - 1 );
		    if ( n == -1 && ( errno == ECONNRESET || errno == EBADF ) ) {
			ilmi_state[intf] = ILMI_COLDSTART;
			close ( ilmi_fd[intf] );
			ilmi_fd[intf] = -1;
		    } else {
			bpp = &buf[1];
			Hdr = asn_get_header(&bpp);

		        if ( Log && Debug_Level > 1 )
			    print_pdu(PDU_RECV, intf, Hdr, n, buf);

			if (Hdr == NULL)
			    continue;

		        /* What we do with this messages depends upon the state we're in */
		        switch ( ilmi_state[intf] ) {
		        case ILMI_COLDSTART:
			    /* We should never be in this state here */
			    free_pdu ( Hdr );
			    break;
		        case ILMI_INIT:
			    /* The only messages we care about are GETNEXTs, GETRESPs, and TRAPs */
			    switch ( Hdr->pdutype ) {
			    case PDU_TYPE_GETNEXT:
				lmi_rcvcmd_getnext(Hdr, intf);
				break;
			    case PDU_TYPE_GETRESP:
				/*
				 * This should be in response to our GETNEXT.
				 * Check the OIDs and go onto ILMI_RUNNING if
				 * the address table is empty. We can cheat and
				 * not check sequence numbers because we only send
				 * the one GETNEXT request and ILMI says we shouldn't
				 * have interleaved sessions.
				 */
				/*
				 * First look for empty table. If found, go to next state.
				 */
				if ((Hdr->error == SNMP_ERR_NOSUCHNAME) ||
				    ((Hdr->error == SNMP_ERR_NOERROR) &&
				     ( oid_ncmp ( &Objids[ADDRESS_OBJID], &Hdr->head->oid,
				      Objids[ADDRESS_OBJID].oid[0] ) == 1 ))) {
					ilmi_state[intf] = ILMI_RUNNING; /* ILMI_REG; */
				} else if (Hdr->error == SNMP_ERR_NOERROR) {
					/*
					 * Check to see if this matches our address
					 * and if so, that it's a VALID entry.
					 */
					Atm_addr	*aa;
					int		l;
					int		match = 1;

					aa = &Intf[intf].anp_addr;
					if ( aa->address_length == Hdr->head->oid.oid[13] ) {
					    for ( l = 0; l < aa->address_length; l++ ) {
					        if ( (int)((u_char *)(aa->address))[l] !=
						    Hdr->head->oid.oid[14 + l] ) {
						        match = 0;
						}
					    }
					}
					if ( match ) {
					    if ( Hdr->head->var.ival == 1 ) {
					        ilmi_state[intf] = ILMI_RUNNING;
					    }
					}
				}
				free_pdu ( Hdr );
				break;
			    case PDU_TYPE_SET:
				/* Look for SET_PREFIX Objid */
				if ( oid_ncmp ( &Hdr->head->oid,
					&Objids[SETPFX_OBJID],
					Objids[SETPFX_OBJID].oid[0] ) == 0 ) {
					    set_prefix ( &Hdr->head->oid, Hdr, intf );
					    /* Reply to SET before sending our ADDRESS */
					    build_pdu(Hdr, PDU_TYPE_GETRESP);
					    send_resp( intf, Hdr, Resp_Buf );
					    set_address ( Hdr, intf );
				} else {
					build_pdu(Hdr, PDU_TYPE_GETRESP);
					send_resp( intf, Hdr, Resp_Buf );
				}
				break;
			    case PDU_TYPE_TRAP:
				/* Remote side wants us to start fresh */
				lmi_rcvcmd_trap(Hdr, intf);
				free_pdu ( Hdr );
				break;
			    default:
				/* Ignore */
				free_pdu ( Hdr );
				break;
			    }
			    break;
		        case ILMI_REG:
			    break;
		        case ILMI_RUNNING:
			    /* We'll take anything here */
			    switch ( Hdr->pdutype ) {
			    case PDU_TYPE_GET:
				process_get ( Hdr, intf );
				break;
			    case PDU_TYPE_GETRESP:
				/* Ignore GETRESPs */
				free_pdu ( Hdr );
				break;
			    case PDU_TYPE_GETNEXT:
				lmi_rcvcmd_getnext(Hdr, intf);
				break;
			    case PDU_TYPE_SET:
				/* Look for SET_PREFIX Objid */
				if ( oid_ncmp ( &Hdr->head->oid,
					&Objids[SETPFX_OBJID],
					Objids[SETPFX_OBJID].oid[0] ) == 0 ) {
					    set_prefix ( &Hdr->head->oid, Hdr, intf );
					    /* Reply to SET before sending our ADDRESS */
					    build_pdu(Hdr, PDU_TYPE_GETRESP);
					    send_resp( intf, Hdr, Resp_Buf );
					    set_address ( Hdr, intf );
				} else {
					build_pdu(Hdr, PDU_TYPE_GETRESP);
					send_resp( intf, Hdr, Resp_Buf );
				}
				break;
			    case PDU_TYPE_TRAP:
				lmi_rcvcmd_trap(Hdr, intf);
				free_pdu ( Hdr );
				break;
			    }
			    break;
		        default:
			    /* Unknown state */
			    free_pdu ( Hdr );
			    break;
		        }
		    }			/* if n > 0 */
		}		/* if received message */
	    }		/* for each interface */
	}	/* for ever loop */

}

int
main (int argc, char *argv[])
{
	int	c;
	int	i;
	int	Reset = 0;	/* Should we send a coldStart and exit? */

	/*
	 * What are we running as? (argv[0])
	 */
	progname = strdup ( (char *)basename ( argv[0] ) );
	/*
	 * What host are we
	 */
	gethostname ( hostname, sizeof ( hostname ) );

	/*
	 * Ilmid needs to run as root to set prefix
	 */
	if ( getuid() != 0 ) {
		fprintf ( stderr, "%s: needs to run as root.\n", progname );
		exit ( -1 );
	}

	/*
	 * Parse arguments
	 */
	while ( ( c = getopt ( argc, argv, "d:fr" ) ) != -1 )
	    switch ( c ) {
		case 'd':
			Debug_Level = atoi ( optarg );
			break;
		case 'f':
			foregnd++;
			break;
		case 'r':
			Reset++;
			break;
		case '?':
			fprintf ( stderr, "usage: %s [-d level] [-f] [-r]\n",
				progname );
			exit ( -1 );
/* NOTREACHED */
			break;
	    }

	/*
	 * If we're not doing debugging, run in the background
	 */
	if ( foregnd == 0 ) {
		if ( daemon ( 0, 0 ) )
			err ( 1, "Can't fork" );
	} /* else
		setbuf ( stdout, NULL ); */

	signal ( SIGUSR1, Increment_DL );
	signal ( SIGUSR2, Decrement_DL );

	/*
	 * Open log file
	 */
	if ( Debug_Level ) {
	    if ( foregnd ) {
		Log = stderr;
	    } else {
	        if ( ( Log = fopen ( LOG_FILE, "a" ) ) == NULL )
		    Log = NULL;
	    }
	}
	if ( Log )
	    setbuf ( Log, NULL );

	/*
	 * Get our startup time
	 */
	(void) gettimeofday ( &starttime, NULL );
	starttime.tv_sec--;
	starttime.tv_usec += 1000000;

	/* Randomize starting request ID */
	Req_ID = starttime.tv_sec;

	/*
	 * Reset all the interface descriptors
	 */
	for ( i = 0; i < MAX_UNITS; i++ ) {
		ilmi_fd[i] = -1;
	}
	/*
	 * Try to open all the interfaces
	 */
	ilmi_open ();

	/*
	 * If we're just sending a coldStart end exiting...
	 */
	if ( Reset ) {
		for ( i = 0; i < MAX_UNITS; i++ )
			if ( ilmi_fd[i] >= 0 ) {
			    /* Build ColdStart TRAP header */
			    ColdStart_Header = build_cold_start();
			    build_pdu ( ColdStart_Header, PDU_TYPE_TRAP );
			    send_resp ( i, ColdStart_Header, Resp_Buf );
			    if ( Debug_Level > 1 && Log ) {
				write_timestamp();
				fprintf ( Log, "Close ilmi_fd[%d]: %d\n",
				    i, ilmi_fd[i] );
			    }
			    close ( ilmi_fd[i] );
			}
		exit ( 2 );
	}

	ilmi_do_state();

	exit(0);
}
