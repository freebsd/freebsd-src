/* From net/if.h */

#define IFF_MULTICAST   0x800           /* supports multicast */

/* From netinet/in.h */

#define INADDR_ALLHOSTS_GROUP   (u_long)0xe0000001      /* 224.0.0.1   */
#define IP_MULTICAST_IF         0x10    /* set/get IP multicast interface  */
#define IP_ADD_MEMBERSHIP       0x13    /* add  an IP group membership     */
#define IP_MULTICAST_TTL        0x11    /* set/get IP multicast timetolive */

/*
 * Argument structure for IP_ADD_MEMBERSHIP and IP_DROP_MEMBERSHIP.
 */
struct ip_mreq {
        struct in_addr  imr_multiaddr;  /* IP multicast address of group */
        struct in_addr  imr_interface;  /* local IP address of interface */
};
 
