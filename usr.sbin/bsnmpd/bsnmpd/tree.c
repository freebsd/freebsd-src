/* $FreeBSD$ */
/* generated file, don't edit - use ./genfiles */
#include <sys/types.h>
#include <stdio.h>
#include "asn1.h"
#include "snmp.h"
#include "snmpagent.h"
#include "tree.h"

const struct snmp_node ctree[] = {
    {{ 8, { 1, 3, 6, 1, 2, 1, 1, 1, }}, "sysDescr", SNMP_NODE_LEAF, SNMP_SYNTAX_OCTETSTRING, op_system_group, 0, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 1, 2, }}, "sysObjectId", SNMP_NODE_LEAF, SNMP_SYNTAX_OID, op_system_group, 0, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 1, 3, }}, "sysUpTime", SNMP_NODE_LEAF, SNMP_SYNTAX_TIMETICKS, op_system_group, 0, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 1, 4, }}, "sysContact", SNMP_NODE_LEAF, SNMP_SYNTAX_OCTETSTRING, op_system_group, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 1, 5, }}, "sysName", SNMP_NODE_LEAF, SNMP_SYNTAX_OCTETSTRING, op_system_group, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 1, 6, }}, "sysLocation", SNMP_NODE_LEAF, SNMP_SYNTAX_OCTETSTRING, op_system_group, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 1, 7, }}, "sysServices", SNMP_NODE_LEAF, SNMP_SYNTAX_INTEGER, op_system_group, 0, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 1, 8, }}, "sysORLastChange", SNMP_NODE_LEAF, SNMP_SYNTAX_TIMETICKS, op_system_group, 0, 0, NULL },
    {{ 10, { 1, 3, 6, 1, 2, 1, 1, 9, 1, 2, }}, "sysORID", SNMP_NODE_COLUMN, SNMP_SYNTAX_OID, op_or_table, 0, 0x11, NULL },
    {{ 10, { 1, 3, 6, 1, 2, 1, 1, 9, 1, 3, }}, "sysORDescr", SNMP_NODE_COLUMN, SNMP_SYNTAX_OCTETSTRING, op_or_table, 0, 0x11, NULL },
    {{ 10, { 1, 3, 6, 1, 2, 1, 1, 9, 1, 4, }}, "sysORUpTime", SNMP_NODE_COLUMN, SNMP_SYNTAX_TIMETICKS, op_or_table, 0, 0x11, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 11, 1, }}, "snmpInPkts", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_snmp, 0, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 11, 3, }}, "snmpInBadVersions", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_snmp, 0, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 11, 4, }}, "snmpInBadCommunityNames", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_snmp, 0, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 11, 5, }}, "snmpInBadCommunityUses", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_snmp, 0, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 11, 6, }}, "snmpInASNParseErrs", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_snmp, 0, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 11, 30, }}, "snmpEnableAuthenTraps", SNMP_NODE_LEAF, SNMP_SYNTAX_INTEGER, op_snmp, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 11, 31, }}, "snmpSilentDrops", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_snmp, 0, 0, NULL },
    {{ 8, { 1, 3, 6, 1, 2, 1, 11, 32, }}, "snmpProxyDrops", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_snmp, 0, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 1, 1, }}, "begemotSnmpdTransmitBuffer", SNMP_NODE_LEAF, SNMP_SYNTAX_INTEGER, op_snmpd_config, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 1, 2, }}, "begemotSnmpdReceiveBuffer", SNMP_NODE_LEAF, SNMP_SYNTAX_INTEGER, op_snmpd_config, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 1, 3, }}, "begemotSnmpdCommunityDisable", SNMP_NODE_LEAF, SNMP_SYNTAX_INTEGER, op_snmpd_config, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 1, 4, }}, "begemotSnmpdTrap1Addr", SNMP_NODE_LEAF, SNMP_SYNTAX_IPADDRESS, op_snmpd_config, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 2, 1, 3, }}, "begemotTrapSinkStatus", SNMP_NODE_COLUMN, SNMP_SYNTAX_INTEGER, op_trapsink, 0|SNMP_NODE_CANSET, 0x142, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 2, 1, 4, }}, "begemotTrapSinkComm", SNMP_NODE_COLUMN, SNMP_SYNTAX_OCTETSTRING, op_trapsink, 0|SNMP_NODE_CANSET, 0x142, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 2, 1, 5, }}, "begemotTrapSinkVersion", SNMP_NODE_COLUMN, SNMP_SYNTAX_INTEGER, op_trapsink, 0|SNMP_NODE_CANSET, 0x142, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 4, 1, 3, }}, "begemotSnmpdPortStatus", SNMP_NODE_COLUMN, SNMP_SYNTAX_INTEGER, op_snmp_port, 0|SNMP_NODE_CANSET, 0x142, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 5, 1, 3, }}, "begemotSnmpdCommunityString", SNMP_NODE_COLUMN, SNMP_SYNTAX_OCTETSTRING, op_community, 0|SNMP_NODE_CANSET, 0x622, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 5, 1, 4, }}, "begemotSnmpdCommunityDescr", SNMP_NODE_COLUMN, SNMP_SYNTAX_OCTETSTRING, op_community, 0, 0x622, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 6, 1, 2, }}, "begemotSnmpdModulePath", SNMP_NODE_COLUMN, SNMP_SYNTAX_OCTETSTRING, op_modules, 0|SNMP_NODE_CANSET, 0x21, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 6, 1, 3, }}, "begemotSnmpdModuleComment", SNMP_NODE_COLUMN, SNMP_SYNTAX_OCTETSTRING, op_modules, 0, 0x21, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 7, 1, }}, "begemotSnmpdStatsNoRxBufs", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_snmpd_stats, 0, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 7, 2, }}, "begemotSnmpdStatsNoTxBufs", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_snmpd_stats, 0, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 7, 3, }}, "begemotSnmpdStatsInTooLongPkts", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_snmpd_stats, 0, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 7, 4, }}, "begemotSnmpdStatsInBadPduTypes", SNMP_NODE_LEAF, SNMP_SYNTAX_COUNTER, op_snmpd_stats, 0, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 8, 1, }}, "begemotSnmpdDebugDumpPdus", SNMP_NODE_LEAF, SNMP_SYNTAX_INTEGER, op_debug, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 8, 2, }}, "begemotSnmpdDebugSnmpTrace", SNMP_NODE_LEAF, SNMP_SYNTAX_GAUGE, op_debug, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 12, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 8, 3, }}, "begemotSnmpdDebugSyslogPri", SNMP_NODE_LEAF, SNMP_SYNTAX_INTEGER, op_debug, 0|SNMP_NODE_CANSET, 0, NULL },
    {{ 13, { 1, 3, 6, 1, 4, 1, 12325, 1, 1, 1, 9, 1, 2, }}, "begemotSnmpdLocalPortStatus", SNMP_NODE_COLUMN, SNMP_SYNTAX_INTEGER, op_local_port, 0|SNMP_NODE_CANSET, 0x21, NULL },
    {{ 10, { 1, 3, 6, 1, 6, 3, 1, 1, 6, 1, }}, "snmpSetSerialNo", SNMP_NODE_LEAF, SNMP_SYNTAX_INTEGER, op_snmp_set, 0|SNMP_NODE_CANSET, 0, NULL },
};

