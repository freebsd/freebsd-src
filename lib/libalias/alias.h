/*lint -save -library Flexelint comment for external headers */

/*
    Alias.h defines the outside world interfaces for the packet
    aliasing software.

    This software is placed into the public domain with no restrictions
    on its distribution.

    $Id: alias.h,v 1.7 1998/01/16 12:56:07 bde Exp $
*/


#ifndef _ALIAS_H_
#define _ALIAS_H_

#ifndef NULL
#define NULL 0
#endif

/* Alias link representative (incomplete struct) */
struct alias_link;

/* External interfaces (API) to packet aliasing engine */

/* Initialization and Control */
    extern void
    PacketAliasInit(void);

    extern void
    PacketAliasUninit(void);

    extern void
    PacketAliasSetAddress(struct in_addr);

    extern unsigned int
    PacketAliasSetMode(unsigned int, unsigned int);

#ifndef NO_FW_PUNCH
    extern void
    PacketAliasSetFWBase(unsigned int, unsigned int);
#endif

/* Packet Handling */
    extern int
    PacketAliasIn(char *, int maxpacketsize);

    extern int
    PacketAliasOut(char *, int maxpacketsize);

/* Port and Address Redirection */
    extern struct alias_link *
    PacketAliasRedirectPort(struct in_addr, u_short, 
                            struct in_addr, u_short,
                            struct in_addr, u_short,
                            u_char);

    extern struct alias_link *
    PacketAliasRedirectAddr(struct in_addr,
                            struct in_addr);

    extern void
    PacketAliasRedirectDelete(struct alias_link *);

/* Fragment Handling */
    extern int
    PacketAliasSaveFragment(char *);

    extern char *
    PacketAliasGetFragment(char *);

    extern void 
    PacketAliasFragmentIn(char *, char *);

/* Miscellaneous Functions */
    extern void
    PacketAliasSetTarget(struct in_addr addr);

    extern int
    PacketAliasCheckNewLink(void);

    extern u_short
    PacketAliasInternetChecksum(u_short *, int);


/*
   In version 2.2, the function names were rationalized
   to all be of the form PacketAlias...  These are the
   old function names for backwards compatibility
*/
extern int SaveFragmentPtr(char *);
extern char *GetNextFragmentPtr(char *);
extern void FragmentAliasIn(char *, char *);
extern void SetPacketAliasAddress(struct in_addr);
extern void InitPacketAlias(void);
extern unsigned int SetPacketAliasMode(unsigned int, unsigned int);
extern int PacketAliasIn2(char *, struct in_addr, int maxpacketsize);
extern int PacketAliasOut2(char *, struct in_addr, int maxpacketsize);
extern int
PacketAliasPermanentLink(struct in_addr, u_short, 
                         struct in_addr, u_short,
                         u_short, u_char);
extern u_short InternetChecksum(u_short *, int);

/* Obsolete constant */
#define PKT_ALIAS_NEW_LINK 5

/********************** Mode flags ********************/
/* Set these flags using SetPacketAliasMode() */

/* If PKT_ALIAS_LOG is set, a message will be printed to
	/var/log/alias.log every time a link is created or deleted.  This
	is useful for debugging */
#define PKT_ALIAS_LOG 0x01

/* If PKT_ALIAS_DENY_INCOMING is set, then incoming connections (e.g.
	to ftp, telnet or web servers will be prevented by the aliasing
	mechanism.  */
#define PKT_ALIAS_DENY_INCOMING 0x02

/* If PKT_ALIAS_SAME_PORTS is set, packets will be attempted sent from
	the same port as they originated on.  This allows eg rsh to work
	*99% of the time*, but _not_ 100%.  (It will be slightly flakey
	instead of not working at all.)  This mode bit is set by
        PacketAliasInit(), so it is a default mode of operation. */
#define PKT_ALIAS_SAME_PORTS 0x04

/* If PKT_ALIAS_USE_SOCKETS is set, then when partially specified
	links (e.g. destination port and/or address is zero), the packet
	aliasing engine will attempt to allocate a socket for the aliasing
	port it chooses.  This will avoid interference with the host
	machine.  Fully specified links do not require this.  This bit
        is set after a call to PacketAliasInit(), so it is a default
        mode of operation.*/
#define PKT_ALIAS_USE_SOCKETS 0x08

/* If PKT_ALIAS_UNREGISTERED_ONLY is set, then only packets with with
	unregistered source addresses will be aliased (along with those
	of the ppp host maching itself.  Private addresses are those
        in the following ranges:

		10.0.0.0     ->   10.255.255.255
		172.16.0.0   ->   172.31.255.255
		192.168.0.0  ->   192.168.255.255  */
#define PKT_ALIAS_UNREGISTERED_ONLY 0x10

/* If PKT_ALIAS_RESET_ON_ADDR_CHANGE is set, then the table of dynamic
	aliasing links will be reset whenever PacketAliasSetAddress()
        changes the default aliasing address.  If the default aliasing
        address is left unchanged by this functions call, then the
        table of dynamic aliasing links will be left intact.  This
        bit is set after a call to PacketAliasInit(). */
#define PKT_ALIAS_RESET_ON_ADDR_CHANGE 0x20

#ifndef NO_FW_PUNCH
/* If PKT_ALIAS_PUNCH_FW is set, active FTP and IRC DCC connections
   will create a 'hole' in the firewall to allow the transfers to
   work.  Where (IPFW "line-numbers") the hole is created is
   controlled by PacketAliasSetFWBase(base, size). The hole will be
   attached to that particular alias_link, so when the link goes away
   so do the hole.  */
#define PKT_ALIAS_PUNCH_FW 0x40
#endif

/* Return Codes */
#define PKT_ALIAS_ERROR -1
#define PKT_ALIAS_OK 1
#define PKT_ALIAS_IGNORED 2
#define PKT_ALIAS_UNRESOLVED_FRAGMENT 3
#define PKT_ALIAS_FOUND_HEADER_FRAGMENT 4

#endif
/*lint -restore */
