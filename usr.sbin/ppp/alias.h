/*
    Alias.h defines the outside world interfaces for the packet
    aliasing software.

    This software is placed into the public domain with no restrictions
    on its distribution.

    Initial version:  August, 1996  (cjm)
*/


#ifndef _ALIAS_H_
#define _ALIAS_H_

extern void PacketAliasIn __P((struct ip *));
extern void PacketAliasOut __P((struct ip *));
extern void SetAliasAddress __P((struct in_addr));
extern void InitAlias();
extern void InitAliasLog();

#endif
