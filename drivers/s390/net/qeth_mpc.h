/*
 * linux/drivers/s390/net/qeth_mpc.h
 *
 * Linux on zSeries OSA Express and HiperSockets support
 *
 * Copyright 2000,2003 IBM Corporation
 * Author(s): Utz Bacher <utz.bacher@de.ibm.com>
 *
 */

#ifndef __QETH_MPC_H__
#define __QETH_MPC_H__

#define VERSION_QETH_MPC_H "$Revision: 1.42 $"

#define QETH_IPA_TIMEOUT (card->ipa_timeout)
#define QETH_MPC_TIMEOUT 2000
#define QETH_ADDR_TIMEOUT 1000

#define QETH_SETIP_RETRIES 2

#define IDX_ACTIVATE_SIZE 0x22
#define CM_ENABLE_SIZE 0x63
#define CM_SETUP_SIZE 0x64
#define ULP_ENABLE_SIZE 0x6b
#define ULP_SETUP_SIZE 0x6c
#define DM_ACT_SIZE 0x55

#define QETH_MPC_TOKEN_LENGTH 4
#define QETH_SEQ_NO_LENGTH 4
#define QETH_IPA_SEQ_NO_LENGTH 2

#define QETH_TRANSPORT_HEADER_SEQ_NO(buffer) (buffer+4)
#define QETH_PDU_HEADER_SEQ_NO(buffer) (buffer+0x1c)
#define QETH_PDU_HEADER_ACK_SEQ_NO(buffer) (buffer+0x20)

static unsigned char IDX_ACTIVATE_READ[]={
	0x00,0x00,0x80,0x00, 0x00,0x00,0x00,0x00,

	0x19,0x01,0x01,0x80, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x00,0xc8,0xc1,
	0xd3,0xd3,0xd6,0xd3, 0xc5,0x40,0x00,0x00,
	0x00,0x00
};

static unsigned char IDX_ACTIVATE_WRITE[]={
	0x00,0x00,0x80,0x00, 0x00,0x00,0x00,0x00,

	0x15,0x01,0x01,0x80, 0x00,0x00,0x00,0x00,
	0xff,0xff,0x00,0x00, 0x00,0x00,0xc8,0xc1,
	0xd3,0xd3,0xd6,0xd3, 0xc5,0x40,0x00,0x00,
	0x00,0x00
};

#define QETH_IDX_ACT_ISSUER_RM_TOKEN(buffer) (buffer+0x0c)
#define QETH_IDX_NO_PORTNAME_REQUIRED(buffer) ((buffer)[0x0b]&0x80)
#define QETH_IDX_ACT_FUNC_LEVEL(buffer) (buffer+0x10)
#define QETH_IDX_ACT_DATASET_NAME(buffer) (buffer+0x16)
#define QETH_IDX_ACT_QDIO_DEV_CUA(buffer) (buffer+0x1e)
#define QETH_IDX_ACT_QDIO_DEV_REALADDR(buffer) (buffer+0x20)

#define QETH_IS_IDX_ACT_POS_REPLY(buffer) (((buffer)[0x08]&3)==2)

#define QETH_IDX_REPLY_LEVEL(buffer) (buffer+0x12)
#define QETH_MCL_LENGTH 4

static unsigned char CM_ENABLE[]={
	0x00,0xe0,0x00,0x00, 0x00,0x00,0x00,0x01,
	0x00,0x00,0x00,0x14, 0x00,0x00,0x00,0x63,
	0x10,0x00,0x00,0x01,

		0x00,0x00,0x00,0x00,
	0x81,0x7e,0x00,0x01, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x24,0x00,0x23,
	0x00,0x00,0x23,0x05, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,

	0x01,0x00,0x00,0x23, 0x00,0x00,0x00,0x40,

	0x00,0x0c,0x41,0x02, 0x00,0x17,0x00,0x00,

	0x00,0x00,0x00,0x00,
		0x00,0x0b,0x04,0x01,

	0x7e,0x04,0x05,0x00, 0x01,0x01,0x0f,

		0x00,

	0x0c,0x04,0x02,0xff, 0xff,0xff,0xff,0xff,

	0xff,0xff,0xff
};

#define QETH_CM_ENABLE_ISSUER_RM_TOKEN(buffer) (buffer+0x2c)
#define QETH_CM_ENABLE_FILTER_TOKEN(buffer) (buffer+0x53)
#define QETH_CM_ENABLE_USER_DATA(buffer) (buffer+0x5b)

#define QETH_CM_ENABLE_RESP_FILTER_TOKEN(buffer) (PDU_ENCAPSULATION(buffer)+ \
						  0x13)

static unsigned char CM_SETUP[]={
	0x00,0xe0,0x00,0x00, 0x00,0x00,0x00,0x02,
	0x00,0x00,0x00,0x14, 0x00,0x00,0x00,0x64,
	0x10,0x00,0x00,0x01,

		0x00,0x00,0x00,0x00,
	0x81,0x7e,0x00,0x01, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x24,0x00,0x24,
	0x00,0x00,0x24,0x05, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,

	0x01,0x00,0x00,0x24, 0x00,0x00,0x00,0x40,

	0x00,0x0c,0x41,0x04, 0x00,0x18,0x00,0x00,

	0x00,0x00,0x00,0x00,
		0x00,0x09,0x04,0x04,

	0x05,0x00,0x01,0x01, 0x11,

		0x00,0x09,0x04,

	0x05,0x05,0x00,0x00, 0x00,0x00,

		0x00,0x06,

	0x04,0x06,0xc8,0x00
};

#define QETH_CM_SETUP_DEST_ADDR(buffer) (buffer+0x2c)
#define QETH_CM_SETUP_CONNECTION_TOKEN(buffer) (buffer+0x51)
#define QETH_CM_SETUP_FILTER_TOKEN(buffer) (buffer+0x5a)

#define QETH_CM_SETUP_RESP_DEST_ADDR(buffer) (PDU_ENCAPSULATION(buffer)+ \
					      0x1a)

static unsigned char ULP_ENABLE[]={
	0x00,0xe0,0x00,0x00, 0x00,0x00,0x00,0x03,
	0x00,0x00,0x00,0x14, 0x00,0x00,0x00,0x6b,
	0x10,0x00,0x00,0x01,

		0x00,0x00,0x00,0x00,
	0x41,0x7e,0x00,0x01, 0x00,0x00,0x00,0x01,
	0x00,0x00,0x00,0x00, 0x00,0x24,0x00,0x2b,
	0x00,0x00,0x2b,0x05, 0x20,0x01,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,

	0x01,0x00,0x00,0x2b, 0x00,0x00,0x00,0x40,

	0x00,0x0c,0x41,0x02, 0x00,0x1f,0x00,0x00,

	0x00,0x00,0x00,0x00,
		0x00,0x0b,0x04,0x01,

	0x03,0x04,0x05,0x00, 0x01,0x01,0x12,

		0x00,

	0x14,0x04,0x0a,0x00, 0x20,0x00,0x00,0xff,
	0xff,0x00,0x08,0xc8, 0xe8,0xc4,0xf1,0xc7,

	0xf1,0x00,0x00
};

#define QETH_ULP_ENABLE_LINKNUM(buffer) (buffer+0x61)
#define QETH_ULP_ENABLE_DEST_ADDR(buffer) (buffer+0x2c)
#define QETH_ULP_ENABLE_FILTER_TOKEN(buffer) (buffer+0x53)
#define QETH_ULP_ENABLE_PORTNAME_AND_LL(buffer) (buffer+0x62)

#define QETH_ULP_ENABLE_RESP_FILTER_TOKEN(buffer) (PDU_ENCAPSULATION(buffer)+ \
						   0x13)
#define QETH_ULP_ENABLE_RESP_MAX_MTU(buffer) (PDU_ENCAPSULATION(buffer)+ \
					      0x1f)
#define QETH_ULP_ENABLE_RESP_DIFINFO_LEN(buffer) (PDU_ENCAPSULATION(buffer)+ \
					  	  0x17)
#define QETH_ULP_ENABLE_RESP_LINK_TYPE(buffer) (PDU_ENCAPSULATION(buffer)+ \
						0x2b)

static unsigned char ULP_SETUP[]={
	0x00,0xe0,0x00,0x00, 0x00,0x00,0x00,0x04,
	0x00,0x00,0x00,0x14, 0x00,0x00,0x00,0x6c,
	0x10,0x00,0x00,0x01,

		0x00,0x00,0x00,0x00,
	0x41,0x7e,0x00,0x01, 0x00,0x00,0x00,0x02,
	0x00,0x00,0x00,0x01, 0x00,0x24,0x00,0x2c,
	0x00,0x00,0x2c,0x05, 0x20,0x01,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,

	0x01,0x00,0x00,0x2c, 0x00,0x00,0x00,0x40,

	0x00,0x0c,0x41,0x04, 0x00,0x20,0x00,0x00,

	0x00,0x00,0x00,0x00,
		0x00,0x09,0x04,0x04,

	0x05,0x00,0x01,0x01, 0x14,
		0x00,0x09,0x04,

	0x05,0x05,0x30,0x01, 0x00,0x00,

		0x00,0x06,

	0x04,0x06,0x40,0x00,

		0x00,0x08,0x04,0x0b,

	0x00,0x00,0x00,0x00
};

#define QETH_ULP_SETUP_DEST_ADDR(buffer) (buffer+0x2c)
#define QETH_ULP_SETUP_CONNECTION_TOKEN(buffer) (buffer+0x51)
#define QETH_ULP_SETUP_FILTER_TOKEN(buffer) (buffer+0x5a)
#define QETH_ULP_SETUP_CUA(buffer) (buffer+0x68)
#define QETH_ULP_SETUP_REAL_DEVADDR(buffer) (buffer+0x6a)

#define QETH_ULP_SETUP_RESP_CONNECTION_TOKEN(buffer) (PDU_ENCAPSULATION \
						      (buffer)+0x1a)
						

static unsigned char DM_ACT[]={
	0x00,0xe0,0x00,0x00, 0x00,0x00,0x00,0x05,
	0x00,0x00,0x00,0x14, 0x00,0x00,0x00,0x55,
	0x10,0x00,0x00,0x01,

		0x00,0x00,0x00,0x00,
	0x41,0x7e,0x00,0x01, 0x00,0x00,0x00,0x03,
	0x00,0x00,0x00,0x02, 0x00,0x24,0x00,0x15,
	0x00,0x00,0x2c,0x05, 0x20,0x01,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,

	0x01,0x00,0x00,0x15, 0x00,0x00,0x00,0x40,

	0x00,0x0c,0x43,0x60, 0x00,0x09,0x00,0x00,

	0x00,0x00,0x00,0x00,

		0x00,0x09,0x04,0x04,

	0x05,0x40,0x01,0x01, 0x00
};

#define QETH_DM_ACT_DEST_ADDR(buffer) (buffer+0x2c)
#define QETH_DM_ACT_CONNECTION_TOKEN(buffer) (buffer+0x51)

#define IPA_CMD_STARTLAN 0x01
#define IPA_CMD_STOPLAN 0x02
#define IPA_CMD_SETIP 0xb1
#define IPA_CMD_DELIP 0xb7
#define IPA_CMD_QIPASSIST 0xb2
#define IPA_CMD_SETASSPARMS 0xb3
#define IPA_CMD_SETIPM 0xb4
#define IPA_CMD_DELIPM 0xb5
#define IPA_CMD_SETRTG 0xb6
#define IPA_CMD_SETADAPTERPARMS 0xb8
#define IPA_CMD_ADD_ADDR_ENTRY 0xc1
#define IPA_CMD_DELETE_ADDR_ENTRY 0xc2
#define IPA_CMD_CREATE_ADDR 0xc3
#define IPA_CMD_DESTROY_ADDR 0xc4
#define IPA_CMD_REGISTER_LOCAL_ADDR 0xd1
#define IPA_CMD_UNREGISTER_LOCAL_ADDR 0xd2

#define INITIATOR_HOST 0
#define INITIATOR_HYDRA 1

#define PRIM_VERSION_IPA 1

#define PROT_VERSION_SNA 1
#define PROT_VERSION_IPv4 4
#define PROT_VERSION_IPv6 6

#define OSA_ADDR_LEN 6
#define IPA_SETADAPTERPARMS_IP_VERSION PROT_VERSION_IPv4
#define SR_INFO_LEN 16

#define IPA_ARP_PROCESSING 0x00000001L
#define IPA_INBOUND_CHECKSUM 0x00000002L
#define IPA_OUTBOUND_CHECKSUM 0x00000004L
#define IPA_IP_FRAGMENTATION 0x00000008L
#define IPA_FILTERING 0x00000010L
#define IPA_IPv6 0x00000020L
#define IPA_MULTICASTING 0x00000040L
#define IPA_IP_REASSEMBLY 0x00000080L
#define IPA_QUERY_ARP_COUNTERS 0x00000100L
#define IPA_QUERY_ARP_ADDR_INFO 0x00000200L
#define IPA_SETADAPTERPARMS 0x00000400L
#define IPA_VLAN_PRIO 0x00000800L
#define IPA_PASSTHRU 0x00001000L
#define IPA_FULL_VLAN 0x00004000L
#define IPA_SOURCE_MAC_AVAIL 0x00010000L

#define IPA_SETADP_QUERY_COMMANDS_SUPPORTED 0x01
#define IPA_SETADP_ALTER_MAC_ADDRESS 0x02
#define IPA_SETADP_ADD_DELETE_GROUP_ADDRESS 0x04
#define IPA_SETADP_ADD_DELETE_FUNCTIONAL_ADDR 0x08
#define IPA_SETADP_SET_ADDRESSING_MODE 0x10
#define IPA_SETADP_SET_CONFIG_PARMS 0x20
#define IPA_SETADP_SET_CONFIG_PARMS_EXTENDED 0x40
#define IPA_SETADP_SET_BROADCAST_MODE 0x80
#define IPA_SETADP_SEND_OSA_MESSAGE 0x0100
#define IPA_SETADP_SET_SNMP_CONTROL 0x0200
#define IPA_SETADP_READ_SNMP_PARMS 0x0400
#define IPA_SETADP_WRITE_SNMP_PARMS 0x0800
#define IPA_SETADP_QUERY_CARD_INFO 0x1000

#define CHANGE_ADDR_READ_MAC 0
#define CHANGE_ADDR_REPLACE_MAC 1
#define CHANGE_ADDR_ADD_MAC 2
#define CHANGE_ADDR_DEL_MAC 4
#define CHANGE_ADDR_RESET_MAC 8
#define CHANGE_ADDR_READ_ADDR 0
#define CHANGE_ADDR_ADD_ADDR 1
#define CHANGE_ADDR_DEL_ADDR 2
#define CHANGE_ADDR_FLUSH_ADDR_TABLE 4
 
#define qeth_is_supported(str) (card->ipa_supported&str)
#define qeth_is_supported6(str) (card->ipa6_supported&str)
#define qeth_is_adp_supported(str) (card->adp_supported&str)

#define IPA_CMD_ASS_START 0x0001
#define IPA_CMD_ASS_STOP 0x0002

#define IPA_CMD_ASS_CONFIGURE 0x0003
#define IPA_CMD_ASS_ENABLE 0x0004

#define IPA_CMD_ASS_ARP_SET_NO_ENTRIES 0x0003
#define IPA_CMD_ASS_ARP_QUERY_CACHE 0x0004
#define IPA_CMD_ASS_ARP_ADD_ENTRY 0x0005
#define IPA_CMD_ASS_ARP_REMOVE_ENTRY 0x0006
#define IPA_CMD_ASS_ARP_FLUSH_CACHE 0x0007
#define IPA_CMD_ASS_ARP_QUERY_INFO 0x0104
#define IPA_CMD_ASS_ARP_QUERY_STATS 0x0204

#define IPA_CHECKSUM_ENABLE_MASK 0x001f

#define IPA_CMD_ASS_FILTER_SET_TYPES 0x0003

#define IPA_CMD_ASS_IPv6_SET_FUNCTIONS 0x0003

#define IPA_REPLY_SUCCESS 0
#define IPA_REPLY_FAILED 1
#define IPA_REPLY_OPNOTSUPP 2
#define IPA_REPLY_OPNOTSUPP2 4
#define IPA_REPLY_NOINFO 8

#define IPA_SETIP_FLAGS 0
#define IPA_SETIP_VIPA_FLAGS 1
#define IPA_SETIP_TAKEOVER_FLAGS 2

#define VIPA_2_B_ADDED 0
#define VIPA_ESTABLISHED 1
#define VIPA_2_B_REMOVED 2

#define IPA_DELIP_FLAGS 0

#define IPA_SETADP_CMDSIZE 40

struct ipa_setadp_cmd {
	__u32 supp_hw_cmds;
	__u32 reserved1;
	__u16 cmdlength;
	__u16 reserved2;
	__u32 command_code;
	__u16 return_code;
	__u8 frames_used_total;
	__u8 frame_seq_no;
	__u32 reserved3;
	union {
		struct {
			__u32 no_lantypes_supp;
			__u8 lan_type;
			__u8 reserved1[3];
			__u32 supported_cmds;
			__u8 reserved2[8];
		} query_cmds_supp;
		struct {
			__u32 cmd;
			__u32 addr_size;
			__u32 no_macs;
			__u8 addr[OSA_ADDR_LEN];
		} change_addr;
		__u32 mode;
	} data;
};

typedef struct ipa_cmd_t {
	__u8 command;
	__u8 initiator;
	__u16 seq_no;
	__u16 return_code;
	__u8 adapter_type;
	__u8 rel_adapter_no;
	__u8 prim_version_no;
	__u8 param_count;
	__u16 prot_version;
	__u32 ipa_supported;
	__u32 ipa_enabled;
	union {
		struct {
			__u8 ip[4];
			__u8 netmask[4];
			__u32 flags;
		} setdelip4;
		struct {
			__u8 ip[16];
			__u8 netmask[16];
			__u32 flags;
		} setdelip6;
		struct {
			__u32 assist_no;
			__u16 length;
			__u16 command_code;
			__u16 return_code;
			__u8 number_of_replies;
			__u8 seq_no;
			union {
				__u32 flags_32bit;
				struct {
					__u8 mac[6];
					__u8 reserved[2];
					__u8 ip[16];
					__u8 reserved2[32];
				} add_arp_entry;
				__u8 ip[16];
			} data;
		} setassparms;
		struct {
			__u8 mac[6];
			__u8 padding[2];
			__u8 ip6[12];
			__u8 ip4_6[4];
		} setdelipm;
		struct {
			__u8 type;
		} setrtg;
		struct ipa_setadp_cmd setadapterparms;
		struct {
			__u32 command;
#define ADDR_FRAME_TYPE_DIX 1
#define ADDR_FRAME_TYPE_802_3 2
#define ADDR_FRAME_TYPE_TR_WITHOUT_SR 0x10
#define ADDR_FRAME_TYPE_TR_WITH_SR 0x20
			__u32 frame_type;
			__u32 cmd_flags;
			__u8 ip_addr[16];
			__u32 tag_field;
			__u8 mac_addr[6];
			__u8 reserved[10];
			__u32 sr_len;
			__u8 sr_info[SR_INFO_LEN];
		} add_addr_entry;
		struct {
			__u32 command;
			__u32 cmd_flags;
			__u8 ip_addr[16];
			__u32 tag_field;
		} delete_addr_entry;
		struct {
			__u8 unique_id[8];
		} create_destroy_addr;
	} data;
} ipa_cmd_t __attribute__ ((packed));

#define QETH_IOC_MAGIC 0x22
#define QETH_IOCPROC_OSAEINTERFACES _IOWR(QETH_IOC_MAGIC, 1, arg)
#define QETH_IOCPROC_INTERFACECHANGES _IOWR(QETH_IOC_MAGIC, 2, arg)

#define SNMP_QUERY_CARD_INFO 0x00000002L
#define SNMP_REGISETER_MIB   0x00000004L
#define SNMP_GET_OID         0x00000010L
#define SNMP_SET_OID         0x00000011L
#define SNMP_GET_NEXT_OID    0x00000012L
#define SNMP_QUERY_ALERTS    0x00000020L
#define SNMP_SET_TRAP        0x00000021L


#define ARP_DATA_SIZE 3968
#define ARP_FLUSH -3
#define ARP_RETURNCODE_NOARPDATA -2
#define ARP_RETURNCODE_ERROR -1
#define ARP_RETURNCODE_SUCCESS 0
#define ARP_RETURNCODE_LASTREPLY 1

#define SNMP_BASE_CMDLENGTH 44
#define SNMP_SETADP_CMDLENGTH 16
#define SNMP_REQUEST_DATA_OFFSET 16

typedef struct snmp_ipa_setadp_cmd_t {
	__u32 supp_hw_cmds;
	__u32 reserved1;
	__u16 cmdlength;
	__u16 reserved2;
	__u32 command_code;
	__u16 return_code;
	__u8 frames_used_total;
	__u8 frame_seq_no;
	__u32 reserved3;
	__u8 snmp_token[16];
	union {
		struct {
			__u32 snmp_request;
			__u32 snmp_interface;
			__u32 snmp_returncode;
			__u32 snmp_firmwarelevel;
			__u32 snmp_seqno;
			__u8 snmp_data[ARP_DATA_SIZE];
		} snmp_subcommand;
	} data;
} snmp_ipa_setadp_cmd_t __attribute__ ((packed));

typedef struct arp_cmd_t {
	__u8 command;
	__u8 initiator;
	__u16 seq_no;
	__u16 return_code;
	__u8 adapter_type;
	__u8 rel_adapter_no;
	__u8 prim_version_no;
	__u8 param_count;
	__u16 prot_version;
	__u32 ipa_supported;
	__u32 ipa_enabled;
	union {
		struct {
			__u32 assist_no;
			__u16 length;
			__u16 command_code;
			__u16 return_code;
			__u8 number_of_replies;
			__u8 seq_no;
			union {
				struct {
					__u16 tcpip_requestbitmask;
					__u16 osa_setbitmask;
					__u32 number_of_entries;
					__u8 arp_data[ARP_DATA_SIZE];
				} queryarp_data;
			} data;
		} setassparms;
                snmp_ipa_setadp_cmd_t setadapterparms; 
	} data;
} arp_cmd_t __attribute__ ((packed));



#define IPA_PDU_HEADER_SIZE 0x40
#define QETH_IPA_PDU_LEN_TOTAL(buffer) (buffer+0x0e)
#define QETH_IPA_PDU_LEN_PDU1(buffer) (buffer+0x26)
#define QETH_IPA_PDU_LEN_PDU2(buffer) (buffer+0x2a)
#define QETH_IPA_PDU_LEN_PDU3(buffer) (buffer+0x3a)

static unsigned char IPA_PDU_HEADER[]={
	0x00,0xe0,0x00,0x00, 0x77,0x77,0x77,0x77,
	0x00,0x00,0x00,0x14, 0x00,0x00,
		(IPA_PDU_HEADER_SIZE+sizeof(ipa_cmd_t))/256,
		(IPA_PDU_HEADER_SIZE+sizeof(ipa_cmd_t))%256,
	0x10,0x00,0x00,0x01,

		0x00,0x00,0x00,0x00,
	0xc1,0x03,0x00,0x01, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x24,0x00,sizeof(ipa_cmd_t),
	0x00,0x00,sizeof(ipa_cmd_t),0x05, 0x77,0x77,0x77,0x77,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x01,0x00,sizeof(ipa_cmd_t)/256,sizeof(ipa_cmd_t)%256,
				0x00,0x00,0x00,0x40,
};

#define QETH_IPA_CMD_DEST_ADDR(buffer) (buffer+0x2c)

#define PDU_ENCAPSULATION(buffer) \
	(buffer+ \
	 *(buffer+ (*(buffer+0x0b))+ *(buffer+*(buffer+0x0b)+0x11) +0x07))

#define IS_IPA(buffer) ((buffer) && ( *(buffer+ ((*(buffer+0x0b))+4) )==0xc1) )

#define IS_IPA_REPLY(buffer) ( (buffer) && ( (*(PDU_ENCAPSULATION(buffer)+1)) \
					     ==INITIATOR_HOST ) )

#define IS_ADDR_IPA(buffer) ( (buffer) && ( \
	( ((ipa_cmd_t*)PDU_ENCAPSULATION(buffer))->command== \
	  IPA_CMD_ADD_ADDR_ENTRY ) || \
	( ((ipa_cmd_t*)PDU_ENCAPSULATION(buffer))->command== \
	  IPA_CMD_DELETE_ADDR_ENTRY ) ) )

#define CCW_NOP_CMD 0x03
#define CCW_NOP_COUNT 1

static unsigned char WRITE_CCW[]={
	0x01,CCW_FLAG_SLI,0,0,
	0,0,0,0
};

static unsigned char READ_CCW[]={
	0x02,CCW_FLAG_SLI,0,0,
	0,0,0,0
};

#endif /* __QETH_MPC_H__ */













