/*
    Alias.p contains the function prototypes for alias.c, alias_db.c,
    alias_util.c and alias_ftp.c (as well as any future add-ons).  It
    is intended to be used only within the aliasing software.  Outside
    world interfaces are defined in alias.h


    This software is placed into the public domain with no restrictions
    on its distribution.

    Initial version:  August, 1996  (cjm)    
*/

#define NULL_PTR 0

/* General utilities */
u_short InternetChecksum(u_short *, int);
u_short IpChecksum(struct ip *);
u_short TcpChecksum(struct ip *);

/* Data access utilities */
int StartPoint(struct in_addr, u_short, int);
u_short GetNewPort();
int SeqDiff(u_long, u_long);
void ShowAliasStats();

/* Internal data access */
void CleanupAliasData();
void IncrementalCleanup();
char * FindLink1(struct in_addr, struct in_addr, u_short, u_short, int);
char * FindLink2(struct in_addr, u_short, u_short, int);
void DeleteLink(char *);
char * AddLink(struct in_addr, struct in_addr, u_short, u_short,
               u_short, int);

/* External data search */
char * FindIcmpIn(struct in_addr, u_short, u_short);
char * FindIcmpOut(struct in_addr, struct in_addr, u_short, u_short);
char * FindFragmentIn1(struct in_addr);
char * FindFragmentIn2(struct in_addr);
char * FindUdpIn(struct in_addr, u_short, u_short);
char * FindUdpOut(struct in_addr, struct in_addr, u_short, u_short);
char * FindTcpIn(struct in_addr, u_short, u_short);
char * FindTcpOut(struct in_addr, struct in_addr, u_short, u_short);

/* External data access/modification */
void GetIcmpData(char *, u_short, u_short, u_long *);
void SetIcmpData(char *, u_short, u_short, u_long);
void GetFragmentAddr(char *, u_short, u_char, struct in_addr *);
void SetFragmentData(char *, u_short, u_char, struct in_addr);
void SetStateIn(char *, int);
void SetStateOut(char *, int);
int GetStateIn(char *);
int GetStateOut(char *);
struct in_addr GetOriginalAddress(char *);
struct in_addr GetDestAddress(char *);
struct in_addr GetAliasAddress();
u_short GetOriginalPort(char *);
u_short GetDestPort(char *);
u_short GetAliasPort(char *);
void SetAckModified(char *);
int GetAckModified(char *);
int GetDeltaAckIn(struct ip *, char *);
int GetDeltaSeqOut(struct ip *, char *);
void AddSeq(struct ip *, char *, int);

/* Tcp specfic routines */
void TcpMonitorIn(struct ip *, char *);
void TcpMonitorOut(struct ip *, char *);
void HandleFtpOut(struct ip *, char *);
void NewFtpPortCommand(struct ip *, char *, struct in_addr, u_short);

/* Protocal specific packet aliasing routines */
void IcmpAliasIn1(struct ip *);
void IcmpAliasIn2(struct ip *);
void IcmpAliasIn(struct ip *);
void IcmpAliasOut(struct ip *);
void IcmpAliasOut1(struct ip *);
void UdpAliasIn(struct ip *);
void UdpAliasOut(struct ip *);
void TcpAliasIn(struct ip *);
void TcpAliasOut(struct ip *);

/* Fragment handling */
void FragmentIn(struct ip *);
void FragmentOut(struct ip *);

/* Outside world interfaces */
void PacketAliasIn(struct ip *);
void PacketAliasOut(struct ip *);
void SetAliasAddress(struct in_addr);
void InitAlias();
void InitAliasLog();

