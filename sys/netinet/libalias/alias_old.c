/*
    This file can be considered a junk pile of old functions that
    are either obsolete or have had their names changed.  In the
    transition from alias2.1 to alias2.2, all the function names
    were rationalized so that they began with "PacketAlias..."

    These functions are included for backwards compatibility.  
*/

#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "alias.h"
#include "alias_local.h"

void
InitPacketAlias(void)
{
    PacketAliasInit();
}

void
SetPacketAliasAddress(struct in_addr addr)
{
    PacketAliasSetAddress(addr);
}

unsigned int
SetPacketAliasMode(unsigned int flags, unsigned int mask)
{
    return PacketAliasSetMode(flags, mask);
}

int
PacketAliasPermanentLink(struct in_addr src_addr,   u_short src_port,
                         struct in_addr dst_addr,   u_short dst_port,
                         u_short alias_port, u_char proto)
{
    struct alias_link *link;
    struct in_addr null_address;

    null_address.s_addr = 0;
    link = PacketAliasRedirectPort(src_addr, src_port,
                                       dst_addr, dst_port,
                                       null_address, alias_port,
                                       proto);

    if (link == NULL)
        return -1;
    else
        return 0;
}

int
SaveFragmentPtr(char *ptr)
{
    return PacketAliasSaveFragment(ptr);
}

char *
GetNextFragmentPtr(char *ptr)
{
    return PacketAliasGetFragment(ptr);
}

void
FragmentAliasIn(char *header, char *fragment)
{
    PacketAliasFragmentIn(header, fragment);
}

u_short
InternetChecksum(u_short *ptr, int len)
{
    return PacketAliasInternetChecksum(ptr, len);
}
