#ifndef _NET_P8022_H
#define _NET_P8022_H

extern struct datalink_proto *register_8022_client(unsigned char type, int (*rcvfunc)(struct sk_buff *, struct net_device *, struct packet_type *));
extern void unregister_8022_client(unsigned char type);

#endif
