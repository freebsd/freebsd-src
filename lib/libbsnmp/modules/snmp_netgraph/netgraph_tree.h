/* $FreeBSD$ */
/* generated file, don't edit - use ./genfiles */
int	op_ng_config(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_begemotNgControlNodeName 1
# define LEAF_begemotNgResBufSiz 2
# define LEAF_begemotNgTimeout 3
# define LEAF_begemotNgDebugLevel 4
int	op_ng_stats(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_begemotNgNoMems 1
# define LEAF_begemotNgMsgReadErrs 2
# define LEAF_begemotNgTooLargeMsgs 3
# define LEAF_begemotNgDataReadErrs 4
# define LEAF_begemotNgTooLargeDatas 5
int	op_ng_type(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_begemotNgTypeStatus 2
int	op_ng_node(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_begemotNgNodeStatus 2
# define LEAF_begemotNgNodeName 3
# define LEAF_begemotNgNodeType 4
# define LEAF_begemotNgNodeHooks 5
int	op_ng_hook(struct snmp_context *, struct snmp_value *, u_int, u_int, enum snmp_op);
# define LEAF_begemotNgHookStatus 3
# define LEAF_begemotNgHookPeerNodeId 4
# define LEAF_begemotNgHookPeerHook 5
# define LEAF_begemotNgHookPeerType 6
#define netgraph_CTREE_SIZE 18
extern const struct snmp_node netgraph_ctree[];
