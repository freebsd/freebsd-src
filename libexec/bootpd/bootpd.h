#ifndef _BLURB_
#define _BLURB_
/************************************************************************
          Copyright 1988, 1991 by Carnegie Mellon University

                          All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted, provided
that the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation, and that the name of Carnegie Mellon University not be used
in advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
************************************************************************/
#endif /* _BLURB_ */


/*
 * bootpd.h -- common header file for all the modules of the bootpd program.
 */


#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

#ifndef PRIVATE
#define PRIVATE static
#endif

#ifndef SIGUSR1
#define SIGUSR1			 30	/* From 4.3 <signal.h> */
#endif

#define MAXHTYPES		  7	/* Number of htypes defined */
#define MAXHADDRLEN		  6	/* Max hw address length in bytes */
#define MAXSTRINGLEN		 80	/* Max string length */

/*
 * Return the length in bytes of a hardware address of the given type.
 * Return the canonical name of the network of the given type.
 */
#define haddrlength(type)	((hwinfolist[(int) (type)]).hlen)
#define netname(type)		((hwinfolist[(int) (type)]).name)


/*
 * Return pointer to static string which gives full network error message.
 */
#define get_network_errmsg get_errmsg


/*
 * Variables shared among modules.
 */

extern int debug;
extern char *bootptab;

extern struct hwinfo hwinfolist[];

extern hash_tbl *hwhashtable;
extern hash_tbl *iphashtable;
extern hash_tbl *nmhashtable;
extern unsigned char vm_cmu[4];
extern unsigned char vm_rfc1048[4];


/*
 * Functions shared among modules
 */

extern void report();
extern char *get_errmsg();
extern char *haddrtoa();
extern int readtab();



/*
 * Nice typedefs. . .
 */

typedef int boolean;
typedef unsigned char byte;


/*
 * This structure holds information about a specific network type.  The
 * length of the network hardware address is stored in "hlen".
 * The string pointed to by "name" is the cononical name of the network.
 */
struct hwinfo {
    unsigned hlen;
    char *name;
};


/*
 * Data structure used to hold an arbitrary-lengthed list of IP addresses.
 * The list may be shared among multiple hosts by setting the linkcount
 * appropriately.
 */

struct in_addr_list {
    unsigned		linkcount, addrcount;
    struct in_addr	addr[1];		/* Dynamically extended */
};


/*
 * Data structures used to hold shared strings and shared binary data.
 * The linkcount must be set appropriately.
 */

struct shared_string {
    unsigned		linkcount;
    char		string[1];		/* Dynamically extended */
};

struct shared_bindata {
    unsigned		linkcount, length;
    byte		data[1];		/* Dynamically extended */
};


/*
 * Flag structure which indicates which symbols have been defined for a
 * given host.  This information is used to determine which data should or
 * should not be reported in the bootp packet vendor info field.
 */

struct flag {
    unsigned	bootfile	:1,
		bootserver	:1,
		bootsize	:1,
		bootsize_auto	:1,
		cookie_server	:1,
		domain_server	:1,
		gateway		:1,
		generic		:1,
		haddr		:1,
		homedir		:1,
		htype		:1,
		impress_server	:1,
		iaddr		:1,
		log_server	:1,
		lpr_server	:1,
		name_server	:1,
		name_switch	:1,
		rlp_server	:1,
		send_name	:1,
		subnet_mask	:1,
		tftpdir		:1,
		time_offset	:1,
		timeoff_auto	:1,
		time_server	:1,
		vendor_magic	:1,
		dumpfile	:1,
		domainname	:1,
		swap_server	:1,
		rootpath	:1,
		vm_auto		:1;
};



/*
 * The flags structure contains TRUE flags for all the fields which
 * are considered valid, regardless of whether they were explicitly
 * specified or indirectly inferred from another entry.
 *
 * The gateway and the various server fields all point to a shared list of
 * IP addresses.
 *
 * The hostname, home directory, and bootfile are all shared strings.
 *
 * The generic data field is a shared binary data structure.  It is used to
 * hold future RFC1048 vendor data until bootpd is updated to understand it.
 *
 * The vm_cookie field specifies the four-octet vendor magic cookie to use
 * if it is desired to always send the same response to a given host.
 *
 * Hopefully, the rest is self-explanatory.
 */

struct host {
    struct flag		    flags;		/* ALL valid fields */
    struct in_addr_list	    *cookie_server,
			    *domain_server,
			    *gateway,
			    *impress_server,
			    *log_server,
			    *lpr_server,
			    *name_server,
			    *rlp_server,
    			    *time_server;
    struct shared_string    *bootfile,
			    *hostname,
    			    *domainname,
			    *homedir,
			    *tftpdir,
    			    *dumpfile,
    			    *rootpath;
    struct shared_bindata   *generic;
    byte		    vm_cookie[4],
			    htype,  /* RFC826 says this should be 16-bits but
				       RFC951 only allocates 1 byte. . . */
			    haddr[MAXHADDRLEN];
    long		    time_offset;
    unsigned int	    bootsize;
    struct in_addr	    bootserver,
			    iaddr,
			    swapserver,
			    subnet_mask;
};
