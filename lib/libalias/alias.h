/*lint -save -library Flexelint comment for external headers */

/*
    Alias.h defines the outside world interfaces for the packet
    aliasing software.

    This software is placed into the public domain with no restrictions
    on its distribution.
*/


#ifndef _ALIAS_H_
#define _ALIAS_H_

/* Alias link representativei (incomplete struct) */
struct alias_link;

/* External interfaces (API) to packet aliasing engine */
extern int SaveFragmentPtr(char *);
extern char *GetNextFragmentPtr(char *);
extern void FragmentAliasIn(char *, char *);
extern int PacketAliasIn(char *, int maxpacketsize);
extern int PacketAliasOut(char *, int maxpacketsize);
extern int PacketAliasIn2(char *, struct in_addr, int maxpacketsize);
extern int PacketAliasOut2(char *, struct in_addr, int maxpacketsize);
extern void SetPacketAliasAddress(struct in_addr);
extern void InitPacketAlias(void);
extern void InitPacketAliasLog(void);
extern void UninitPacketAliasLog(void);
extern unsigned int SetPacketAliasMode(unsigned int, unsigned int);
extern struct alias_link *
PacketAliasRedirectPort(struct in_addr, u_short, 
                        struct in_addr, u_short,
                        struct in_addr, u_short,
                        u_char);
extern int
PacketAliasPermanentLink(struct in_addr, u_short, 
                         struct in_addr, u_short,
                         u_short, u_char);
extern struct alias_link *
PacketAliasRedirectAddr(struct in_addr,
                        struct in_addr);
void PacketAliasRedirectDelete(struct alias_link *);


/* InternetChecksum() is not specifically part of the
   packet aliasing API, but is sometimes needed outside
   the module. (~for instance, natd uses it to create
   an ICMP error message when interface size is
   exceeded.) */
   
extern u_short InternetChecksum(u_short *, int);


/********************** Mode flags ********************/
/* Set these flags using SetPacketAliasMode() */

/* If PKT_ALIAS_LOG is set, a message will be printed to
	/var/log/alias.log every time a link is created or deleted.  This
	is useful for debugging */
#define PKT_ALIAS_LOG 1

/* If PKT_ALIAS_DENY_INCOMING is set, then incoming connections (e.g.
	to ftp, telnet or web servers will be prevented by the aliasing
	mechanism.  */
#define PKT_ALIAS_DENY_INCOMING 2

/* If PKT_ALIAS_SAME_PORTS is set, packets will be attempted sent from
	the same port as they originated on.  This allow eg rsh to work
	*99% of the time*, but _not_ 100%.  (It will be slightly flakey
	instead of not working at all.) */
#define PKT_ALIAS_SAME_PORTS 4

/* If PKT_ALIAS_USE_SOCKETS is set, then when partially specified
	links (e.g. destination port and/or address is zero), the packet
	aliasing engine will attempt to allocate a socket for the aliasing
	port it chooses.  This will avoid interference with the host
	machine.  Fully specified links do not require this.  */
#define PKT_ALIAS_USE_SOCKETS 8

/* If PKT_ALIAS_UNREGISTERED_ONLY is set, then only packets with with
	unregistered source addresses will be aliased (along with those
	of the ppp host maching itself.  Private addresses are those
        in the following ranges:

		10.0.0.0     ->   10.255.255.255
		172.16.0.0   ->   172.31.255.255
		192.168.0.0  ->   192.168.255.255  */
#define PKT_ALIAS_UNREGISTERED_ONLY 16



/* Return Codes */
#define PKT_ALIAS_ERROR -1
#define PKT_ALIAS_OK 1
#define PKT_ALIAS_IGNORED 2
#define PKT_ALIAS_UNRESOLVED_FRAGMENT 3
#define PKT_ALIAS_FOUND_HEADER_FRAGMENT 4
#define PKT_ALIAS_NEW_LINK 5

#endif
/*lint -restore */
