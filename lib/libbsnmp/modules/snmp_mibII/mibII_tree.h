/* $FreeBSD$ */
/* generated file, don't edit - use ./genfiles */
int	op_interfaces(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ifNumber 1
int	op_ifentry(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ifIndex 1
# define LEAF_ifDescr 2
# define LEAF_ifType 3
# define LEAF_ifMtu 4
# define LEAF_ifSpeed 5
# define LEAF_ifPhysAddress 6
# define LEAF_ifAdminStatus 7
# define LEAF_ifOperStatus 8
# define LEAF_ifLastChange 9
# define LEAF_ifInOctets 10
# define LEAF_ifInUcastPkts 11
# define LEAF_ifInNUcastPkts 12
# define LEAF_ifInDiscards 13
# define LEAF_ifInErrors 14
# define LEAF_ifInUnknownProtos 15
# define LEAF_ifOutOctets 16
# define LEAF_ifOutUcastPkts 17
# define LEAF_ifOutNUcastPkts 18
# define LEAF_ifOutDiscards 19
# define LEAF_ifOutErrors 20
# define LEAF_ifOutQLen 21
# define LEAF_ifSpecific 22
int	op_ip(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ipForwarding 1
# define LEAF_ipDefaultTTL 2
int	op_ipstat(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ipInReceives 3
# define LEAF_ipInHdrErrors 4
# define LEAF_ipInAddrErrors 5
# define LEAF_ipForwDatagrams 6
# define LEAF_ipInUnknownProtos 7
# define LEAF_ipInDiscards 8
# define LEAF_ipInDelivers 9
# define LEAF_ipOutRequests 10
# define LEAF_ipOutDiscards 11
# define LEAF_ipOutNoRoutes 12
# define LEAF_ipReasmTimeout 13
# define LEAF_ipReasmReqds 14
# define LEAF_ipReasmOKs 15
# define LEAF_ipReasmFails 16
# define LEAF_ipFragOKs 17
# define LEAF_ipFragFails 18
# define LEAF_ipFragCreates 19
int	op_ipaddr(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ipAdEntAddr 1
# define LEAF_ipAdEntIfIndex 2
# define LEAF_ipAdEntNetMask 3
# define LEAF_ipAdEntBcastAddr 4
# define LEAF_ipAdEntReasmMaxSize 5
int	op_nettomedia(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ipNetToMediaIfIndex 1
# define LEAF_ipNetToMediaPhysAddress 2
# define LEAF_ipNetToMediaNetAddress 3
# define LEAF_ipNetToMediaType 4
int	op_route(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ipCidrRouteNumber 3
int	op_route_table(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ipCidrRouteDest 1
# define LEAF_ipCidrRouteMask 2
# define LEAF_ipCidrRouteTos 3
# define LEAF_ipCidrRouteNextHop 4
# define LEAF_ipCidrRouteIfIndex 5
# define LEAF_ipCidrRouteType 6
# define LEAF_ipCidrRouteProto 7
# define LEAF_ipCidrRouteAge 8
# define LEAF_ipCidrRouteInfo 9
# define LEAF_ipCidrRouteNextHopAS 10
# define LEAF_ipCidrRouteMetric1 11
# define LEAF_ipCidrRouteMetric2 12
# define LEAF_ipCidrRouteMetric3 13
# define LEAF_ipCidrRouteMetric4 14
# define LEAF_ipCidrRouteMetric5 15
# define LEAF_ipCidrRouteStatus 16
int	op_icmpstat(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_icmpInMsgs 1
# define LEAF_icmpInErrors 2
# define LEAF_icmpInDestUnreachs 3
# define LEAF_icmpInTimeExcds 4
# define LEAF_icmpInParmProbs 5
# define LEAF_icmpInSrcQuenchs 6
# define LEAF_icmpInRedirects 7
# define LEAF_icmpInEchos 8
# define LEAF_icmpInEchoReps 9
# define LEAF_icmpInTimestamps 10
# define LEAF_icmpInTimestampReps 11
# define LEAF_icmpInAddrMasks 12
# define LEAF_icmpInAddrMaskReps 13
# define LEAF_icmpOutMsgs 14
# define LEAF_icmpOutErrors 15
# define LEAF_icmpOutDestUnreachs 16
# define LEAF_icmpOutTimeExcds 17
# define LEAF_icmpOutParmProbs 18
# define LEAF_icmpOutSrcQuenchs 19
# define LEAF_icmpOutRedirects 20
# define LEAF_icmpOutEchos 21
# define LEAF_icmpOutEchoReps 22
# define LEAF_icmpOutTimestamps 23
# define LEAF_icmpOutTimestampReps 24
# define LEAF_icmpOutAddrMasks 25
# define LEAF_icmpOutAddrMaskReps 26
int	op_tcp(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_tcpRtoAlgorithm 1
# define LEAF_tcpRtoMin 2
# define LEAF_tcpRtoMax 3
# define LEAF_tcpMaxConn 4
# define LEAF_tcpActiveOpens 5
# define LEAF_tcpPassiveOpens 6
# define LEAF_tcpAttemptFails 7
# define LEAF_tcpEstabResets 8
# define LEAF_tcpCurrEstab 9
# define LEAF_tcpInSegs 10
# define LEAF_tcpOutSegs 11
# define LEAF_tcpRetransSegs 12
int	op_tcpconn(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_tcpConnState 1
# define LEAF_tcpConnLocalAddress 2
# define LEAF_tcpConnLocalPort 3
# define LEAF_tcpConnRemAddress 4
# define LEAF_tcpConnRemPort 5
# define LEAF_tcpInErrs 14
int	op_udp(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_udpInDatagrams 1
# define LEAF_udpNoPorts 2
# define LEAF_udpInErrors 3
# define LEAF_udpOutDatagrams 4
int	op_udptable(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_udpLocalAddress 1
# define LEAF_udpLocalPort 2
int	op_ifxtable(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ifName 1
# define LEAF_ifInMulticastPkts 2
# define LEAF_ifInBroadcastPkts 3
# define LEAF_ifOutMulticastPkts 4
# define LEAF_ifOutBroadcastPkts 5
# define LEAF_ifHCInOctets 6
# define LEAF_ifHCInUcastPkts 7
# define LEAF_ifHCInMulticastPkts 8
# define LEAF_ifHCInBroadcastPkts 9
# define LEAF_ifHCOutOctets 10
# define LEAF_ifHCOutUcastPkts 11
# define LEAF_ifHCOutMulticastPkts 12
# define LEAF_ifHCOutBroadcastPkts 13
# define LEAF_ifLinkUpDownTrapEnable 14
# define LEAF_ifHighSpeed 15
# define LEAF_ifPromiscuousMode 16
# define LEAF_ifConnectorPresent 17
# define LEAF_ifAlias 18
# define LEAF_ifCounterDiscontinuityTime 19
int	op_ifstack(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ifStackStatus 3
int	op_rcvaddr(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ifRcvAddressStatus 2
# define LEAF_ifRcvAddressType 3
int	op_ifmib(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_ifTableLastChange 5
# define LEAF_ifStackLastChange 6
#define mibII_CTREE_SIZE 142
extern const struct snmp_node mibII_ctree[];
