/* -*- mode: c; tab-width: 3; c-basic-offset: 3; -*-
    Alias_local.h contains the function prototypes for alias.c,
    alias_db.c, alias_util.c and alias_ftp.c, alias_irc.c (as well
    as any future add-ons).  It is intended to be used only within
    the aliasing software.  Outside world interfaces are defined
    in alias.h

    This software is placed into the public domain with no restrictions
    on its distribution.

    Initial version:  August, 1996  (cjm)    
    
     <updated several times by original author and Eivind Eiklund>
*/

extern int packetAliasMode;

struct alias_link;

/* General utilities */
u_short IpChecksum(struct ip *);
u_short TcpChecksum(struct ip *);
void DifferentialChecksum(u_short *, u_short *, u_short *, int);

/* Internal data access */
struct alias_link *
FindIcmpIn(struct in_addr, struct in_addr, u_short);

struct alias_link *
FindIcmpOut(struct in_addr, struct in_addr, u_short);

struct alias_link *
FindFragmentIn1(struct in_addr, struct in_addr, u_short);

struct alias_link *
FindFragmentIn2(struct in_addr, struct in_addr, u_short);

struct alias_link *
AddFragmentPtrLink(struct in_addr, u_short);

struct alias_link *
FindFragmentPtr(struct in_addr, u_short);

struct alias_link *
FindUdpTcpIn (struct in_addr, struct in_addr, u_short, u_short, u_char);

struct alias_link *
FindUdpTcpOut(struct in_addr, struct in_addr, u_short, u_short, u_char);

struct in_addr
FindOriginalAddress(struct in_addr);

struct in_addr
FindAliasAddress(struct in_addr);


/* External data access/modification */
void GetFragmentAddr(struct alias_link *, struct in_addr *);
void SetFragmentAddr(struct alias_link *, struct in_addr);
void GetFragmentPtr(struct alias_link *, char **);
void SetFragmentPtr(struct alias_link *, char *);
void SetStateIn(struct alias_link *, int);
void SetStateOut(struct alias_link *, int);
int GetStateIn(struct alias_link *);
int GetStateOut(struct alias_link *);
struct in_addr GetOriginalAddress(struct alias_link *);
struct in_addr GetDestAddress(struct alias_link *);
struct in_addr GetAliasAddress(struct alias_link *);
struct in_addr GetDefaultAliasAddress(void);
void SetDefaultAliasAddress(struct in_addr);
void SetDefaultTargetAddress(struct in_addr);
void ClearDefaultTargetAddress(void);
u_short GetOriginalPort(struct alias_link *);
u_short GetAliasPort(struct alias_link *);
void SetAckModified(struct alias_link *);
int GetAckModified(struct alias_link *);
int GetDeltaAckIn(struct ip *, struct alias_link *);
int GetDeltaSeqOut(struct ip *, struct alias_link *);
void AddSeq(struct ip *, struct alias_link *, int);
void SetExpire(struct alias_link *, int);
void ClearNewDefaultLink(void);
int CheckNewDefaultLink(void);

/* Housekeeping function */
void HouseKeeping(void);

/* Tcp specfic routines */
/*lint -save -library Suppress flexelint warnings */
void AliasHandleFtpOut(struct ip *, struct alias_link *, int);
void AliasHandleIrcOut(struct ip *pip, struct alias_link *link, int maxsize );
/*lint -restore */
