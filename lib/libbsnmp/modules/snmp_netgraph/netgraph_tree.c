/* $FreeBSD$ */
/* generated file, don't edit - use ./genfiles */
#include <sys/types.h>
#include <stdio.h>
#include "asn1.h"
#include "snmp.h"
#include "snmpagent.h"
#include "netgraph_tree.h"

const struct snmp_node netgraph_ctree[] = {
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 1, 1, }}, "begemotNgControlNodeName", SNMP_NODE_LEAF, SNMP_SYNTAX_OCTETSTRING, op_ng_config, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 1, 2, }}, "begemotNgResBufSiz", SNMP_NODE_LEAF, SNMP_SYNTAX_INTEGER, op_ng_config, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 1, 3, }}, "begemotNgTimeout", SNMP_NODE_LEAF, SNMP_SYNTAX_INTEGER, op_ng_config, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 1, 4, }}, "begemotNgDebugLevel", SNMP_NODE_LEAF, SNMP_SYNTAX_GAUGE, op_ng_config, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 2, 1, }}, "begemotNgNoMems", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_ng_stats, 0, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 2, 2, }}, "begemotNgMsgReadErrs", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_ng_stats, 0, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 2, 3, }}, "begemotNgTooLargeMsgs", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_ng_stats, 0, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 2, 4, }}, "begemotNgDataReadErrs", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_ng_stats, 0, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 2, 5, }}, "begemotNgTooLargeDatas", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_ng_stats, 0, 0, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 3, 1, 2, }}, "begemotNgTypeStatus", SNMP_NODE_COLUMN, SNMP_SYNTAX_INTEGER, op_ng_type, 0|SNMP_NODE_CANSET, 0x21, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 4, 1, 2, }}, "begemotNgNodeStatus", SNMP_NODE_COLUMN, SNMP_SYNTAX_INTEGER, op_ng_node, 0, 0x11, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 4, 1, 3, }}, "begemotNgNodeName", SNMP_NODE_COLUMN, SNMP_SYNTAX_OCTETSTRING, op_ng_node, 0, 0x11, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 4, 1, 4, }}, "begemotNgNodeType", SNMP_NODE_COLUMN, SNMP_SYNTAX_OCTETSTRING, op_ng_node, 0, 0x11, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 4, 1, 5, }}, "begemotNgNodeHooks", SNMP_NODE_COLUMN, SNMP_SYNTAX_GAUGE, op_ng_node, 0, 0x11, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 5, 1, 3, }}, "begemotNgHookStatus", SNMP_NODE_COLUMN, SNMP_SYNTAX_INTEGER, op_ng_hook, 0, 0x262, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 5, 1, 4, }}, "begemotNgHookPeerNodeId", SNMP_NODE_COLUMN, SNMP_SYNTAX_GAUGE, op_ng_hook, 0, 0x262, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 5, 1, 5, }}, "begemotNgHookPeerHook", SNMP_NODE_COLUMN, SNMP_SYNTAX_OCTETSTRING, op_ng_hook, 0, 0x262, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 2, 1, 5, 1, 6, }}, "begemotNgHookPeerType", SNMP_NODE_COLUMN, SNMP_SYNTAX_OCTETSTRING, op_ng_hook, 0, 0x262, NULL },
};

