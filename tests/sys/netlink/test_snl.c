#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/param.h>
#include <sys/module.h>

#include <net/ethernet.h>

#include <netlink/netlink.h>
#include <netlink/netlink_route.h>
#include "netlink/netlink_snl.h"
#include "netlink/netlink_snl_route.h"
#include "netlink/netlink_snl_route_parsers.h"

#include <atf-c.h>

static const struct snl_hdr_parser *snl_all_core_parsers[] = {
	&snl_errmsg_parser, &snl_donemsg_parser,
	&_nla_bit_parser, &_nla_bitset_parser,
};

static const struct snl_hdr_parser *snl_all_route_parsers[] = {
	&_metrics_mp_nh_parser, &_mpath_nh_parser, &_metrics_parser, &snl_rtm_route_parser,
	&_vf_driver_field_parser, &_vf_driver_parser, &_vf_parser,
	&_vf_status_parser,
	&_link_fbsd_parser, &snl_rtm_link_parser, &snl_rtm_link_parser_simple,
	&_neigh_fbsd_parser, &snl_rtm_neigh_parser,
	&_addr_fbsd_parser, &snl_rtm_addr_parser, &_nh_fbsd_parser, &snl_nhmsg_parser,
};

ATF_TC(snl_verify_core_parsers);
ATF_TC_HEAD(snl_verify_core_parsers, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) core nlmsg parsers are correct");
}

ATF_TC_BODY(snl_verify_core_parsers, tc)
{
	SNL_VERIFY_PARSERS(snl_all_core_parsers);

}

ATF_TC(snl_parse_bitset_array);
ATF_TC_HEAD(snl_parse_bitset_array, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Tests snl(3) parsing a growing nested bit array");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_bitset_array, tc)
{
	struct snl_attr_bit *bit;
	struct snl_parsed_link link = {};
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	uint32_t mask, value;
	char name[16];
	int bits_off, entry_off, fbsd_off, caps_off;

	ATF_REQUIRE(snl_init(&ss, NETLINK_ROUTE));
	snl_init_writer(&ss, &nw);
	hdr = snl_create_msg_request(&nw, RTM_NEWLINK);
	ATF_REQUIRE(hdr != NULL);
	ATF_REQUIRE(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);

	fbsd_off = snl_add_msg_attr_nested(&nw, IFLA_FREEBSD);
	ATF_REQUIRE(fbsd_off != 0);
	caps_off = snl_add_msg_attr_nested(&nw, IFLAF_CAPS);
	ATF_REQUIRE(caps_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, NLA_BITSET_SIZE, 32));
	mask = 0x1ff;
	value = 0x155;
	ATF_REQUIRE(snl_add_msg_attr(&nw, NLA_BITSET_MASK, sizeof(mask),
	    &mask));
	ATF_REQUIRE(snl_add_msg_attr(&nw, NLA_BITSET_VALUE, sizeof(value),
	    &value));
	bits_off = snl_add_msg_attr_nested(&nw, NLA_BITSET_BITS);
	ATF_REQUIRE(bits_off != 0);
	for (uint32_t i = 0; i < 9; i++) {
		entry_off = snl_add_msg_attr_nested(&nw, i + 1);
		ATF_REQUIRE(entry_off != 0);
		ATF_REQUIRE(snl_add_msg_attr_u32(&nw,
		    NLA_BITSET_BIT_INDEX, i));
		snprintf(name, sizeof(name), "bit-%u", i);
		ATF_REQUIRE(snl_add_msg_attr_string(&nw,
		    NLA_BITSET_BIT_NAME, name));
		if ((i & 1) == 0)
			ATF_REQUIRE(snl_add_msg_attr_flag(&nw,
			    NLA_BITSET_BIT_VALUE));
		snl_end_attr_nested(&nw, entry_off);
	}
	snl_end_attr_nested(&nw, bits_off);
	snl_end_attr_nested(&nw, caps_off);
	snl_end_attr_nested(&nw, fbsd_off);
	hdr = snl_finalize_msg(&nw);
	ATF_REQUIRE(hdr != NULL);

	ATF_REQUIRE(snl_parse_nlmsg(&ss, hdr, &snl_rtm_link_parser, &link));
	ATF_REQUIRE_EQ(link.iflaf_caps.bits.count, 9);
	bit = link.iflaf_caps.bits.items[8];
	ATF_CHECK_EQ(bit->bit_index, 8);
	ATF_CHECK_STREQ(bit->bit_name, "bit-8");
	ATF_CHECK_EQ(bit->bit_value, 1);
}

ATF_TC(snl_parse_vf_status);
ATF_TC_HEAD(snl_parse_vf_status, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Tests snl(3) parsing nested VF status");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_vf_status, tc)
{
	static const uint8_t mac[] = { 0x02, 0, 0, 0, 0, 1 };
	static const uint8_t extension_data[] = { 1, 2, 3, 4 };
	struct snl_parsed_vf_driver_field *field;
	struct snl_parsed_vf_driver *driver;
	struct snl_parsed_vf_status *status;
	struct snl_parsed_link link = {};
	struct snl_parsed_vf *vf;
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	uint32_t i;
	int driver_off, entry_off, fbsd_off, field_off, status_off;

	ATF_REQUIRE(snl_init(&ss, NETLINK_ROUTE));
	snl_init_writer(&ss, &nw);
	hdr = snl_create_msg_request(&nw, RTM_NEWLINK);
	ATF_REQUIRE(hdr != NULL);
	ATF_REQUIRE(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLA_NUM_VF, 9));

	fbsd_off = snl_add_msg_attr_nested(&nw, IFLA_FREEBSD);
	ATF_REQUIRE(fbsd_off != 0);
	status_off = snl_add_msg_attr_nested(&nw, IFLAF_VF_STATUS);
	ATF_REQUIRE(status_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_u8(&nw, IFLAF_VFS_PF_LINK_STATE,
	    IFLAF_VF_LINK_UP));
	ATF_REQUIRE(snl_add_msg_attr_u64(&nw, IFLAF_VFS_PF_LINK_SPEED,
	    10000000000ULL));
	snl_end_attr_nested(&nw, status_off);
	snl_end_attr_nested(&nw, fbsd_off);

	entry_off = snl_add_msg_attr_nested(&nw, IFLA_FREEBSD_VF);
	ATF_REQUIRE(entry_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VF_INDEX, 7));
	ATF_REQUIRE(snl_add_msg_attr_bool(&nw, IFLAF_VF_CONFIGURED, true));
	ATF_REQUIRE(snl_add_msg_attr_bool(&nw, IFLAF_VF_TRAFFIC_ALLOWED,
	    true));
	ATF_REQUIRE(snl_add_msg_attr_bool(&nw, IFLAF_VF_FAULT_BLOCKED,
	    false));
	ATF_REQUIRE(snl_add_msg_attr_u16(&nw, IFLAF_VF_NUM_TX_QUEUES, 4));
	ATF_REQUIRE(snl_add_msg_attr_u16(&nw, IFLAF_VF_NUM_RX_QUEUES, 8));
	ATF_REQUIRE(snl_add_msg_attr_u64(&nw, IFLAF_VF_MIN_TX_RATE,
	    25000000000ULL));
	ATF_REQUIRE(snl_add_msg_attr_u64(&nw, IFLAF_VF_MAX_TX_RATE,
	    100000000000ULL));
	ATF_REQUIRE(snl_add_msg_attr_u8(&nw, IFLAF_VF_VLAN_MODE,
	    IFLAF_VF_VLAN_ACCESS));
	ATF_REQUIRE(snl_add_msg_attr_u16(&nw, IFLAF_VF_VLAN, 0));
	ATF_REQUIRE(snl_add_msg_attr_u8(&nw, IFLAF_VF_VLAN_PCP, 5));
	ATF_REQUIRE(snl_add_msg_attr_u16(&nw, IFLAF_VF_VLAN_PROTO,
	    ETHERTYPE_VLAN));
	ATF_REQUIRE(snl_add_msg_attr(&nw, IFLAF_VF_MAC, sizeof(mac), mac));

	driver_off = snl_add_msg_attr_nested(&nw, IFLAF_VF_DRIVER);
	ATF_REQUIRE(driver_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_string(&nw, IFLAF_VFD_NAME,
	    "driver.test"));
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VFD_VERSION, 3));
	field_off = snl_add_msg_attr_nested(&nw, IFLAF_VFD_FIELD);
	ATF_REQUIRE(field_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_string(&nw, IFLAF_VFDF_NAME,
	    "ready"));
	ATF_REQUIRE(snl_add_msg_attr_bool(&nw, IFLAF_VFDF_BOOL, false));
	snl_end_attr_nested(&nw, field_off);
	field_off = snl_add_msg_attr_nested(&nw, IFLAF_VFD_FIELD);
	ATF_REQUIRE(field_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_string(&nw, IFLAF_VFDF_NAME,
	    "opaque"));
	ATF_REQUIRE(snl_add_msg_attr(&nw, IFLAF_VFDF_BINARY,
	    sizeof(extension_data), extension_data));
	snl_end_attr_nested(&nw, field_off);
	field_off = snl_add_msg_attr_nested(&nw, IFLAF_VFD_FIELD);
	ATF_REQUIRE(field_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_string(&nw, IFLAF_VFDF_NAME,
	    "sequence"));
	ATF_REQUIRE(snl_add_msg_attr_u64(&nw, IFLAF_VFDF_NUMBER,
	    0x100000002ULL));
	snl_end_attr_nested(&nw, field_off);
	field_off = snl_add_msg_attr_nested(&nw, IFLAF_VFD_FIELD);
	ATF_REQUIRE(field_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_string(&nw, IFLAF_VFDF_NAME,
	    "mode"));
	ATF_REQUIRE(snl_add_msg_attr_string(&nw, IFLAF_VFDF_STRING,
	    "diagnostic"));
	snl_end_attr_nested(&nw, field_off);
	/* A newer field value type must not hide fields known to this parser. */
	field_off = snl_add_msg_attr_nested(&nw, IFLAF_VFD_FIELD);
	ATF_REQUIRE(field_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_string(&nw, IFLAF_VFDF_NAME,
	    "future-value"));
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VFDF_MAX + 1, 1));
	snl_end_attr_nested(&nw, field_off);
	snl_end_attr_nested(&nw, driver_off);
	snl_end_attr_nested(&nw, entry_off);

	/* A second attribute of the same type is the second array member. */
	entry_off = snl_add_msg_attr_nested(&nw, IFLA_FREEBSD_VF);
	ATF_REQUIRE(entry_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VF_INDEX, 8));
	ATF_REQUIRE(snl_add_msg_attr_bool(&nw, IFLAF_VF_CONFIGURED, false));
	ATF_REQUIRE(snl_add_msg_attr_u64(&nw, IFLAF_VF_MIN_TX_RATE, 0));
	ATF_REQUIRE(snl_add_msg_attr_u64(&nw, IFLAF_VF_MAX_TX_RATE, 0));
	snl_end_attr_nested(&nw, entry_off);
	/* Cross the parser array's initial capacity to exercise growth. */
	for (i = 9; i <= 15; i++) {
		entry_off = snl_add_msg_attr_nested(&nw, IFLA_FREEBSD_VF);
		ATF_REQUIRE(entry_off != 0);
		ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VF_INDEX, i));
		snl_end_attr_nested(&nw, entry_off);
	}
	hdr = snl_finalize_msg(&nw);
	ATF_REQUIRE(hdr != NULL);

	ATF_REQUIRE(snl_parse_nlmsg(&ss, hdr, &snl_rtm_link_parser, &link));
	ATF_CHECK_EQ(link.ifla_num_vf, 9);
	status = &link.iflaf_vf_status;
	ATF_CHECK(status->present);
	ATF_CHECK_EQ(status->pf_link_state, IFLAF_VF_LINK_UP);
	ATF_CHECK_EQ(status->pf_link_speed, 10000000000ULL);
	ATF_REQUIRE_EQ(status->vfs.count, 9);
	vf = status->vfs.items[0];
	ATF_CHECK_EQ(vf->index, 7);
	ATF_CHECK_EQ(vf->configured, 1);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_TRAFFIC_ALLOWED)) != 0);
	ATF_CHECK_EQ(vf->traffic_allowed, true);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_FAULT_BLOCKED)) != 0);
	ATF_CHECK_EQ(vf->fault_blocked, false);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_NUM_TX_QUEUES)) != 0);
	ATF_CHECK_EQ(vf->tx_queue_count, 4);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_NUM_RX_QUEUES)) != 0);
	ATF_CHECK_EQ(vf->rx_queue_count, 8);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_MIN_TX_RATE)) != 0);
	ATF_CHECK_EQ(vf->min_tx_rate_bps, 25000000000ULL);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_MAX_TX_RATE)) != 0);
	ATF_CHECK_EQ(vf->max_tx_rate_bps, 100000000000ULL);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_VLAN_MODE)) != 0);
	ATF_CHECK_EQ(vf->vlan_mode, IFLAF_VF_VLAN_ACCESS);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_VLAN)) != 0);
	ATF_CHECK_EQ(vf->vlan, 0);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_VLAN_PCP)) != 0);
	ATF_CHECK_EQ(vf->vlan_pcp, 5);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_VLAN_PROTO)) != 0);
	ATF_CHECK_EQ(vf->vlan_proto, ETHERTYPE_VLAN);
	ATF_REQUIRE(vf->mac != NULL);
	ATF_CHECK_EQ(NLA_DATA_LEN(vf->mac), sizeof(mac));
	ATF_CHECK(memcmp(NLA_DATA(vf->mac), mac, sizeof(mac)) == 0);
	ATF_REQUIRE_EQ(vf->drivers.count, 1);
	driver = vf->drivers.items[0];
	ATF_CHECK_STREQ(driver->name, "driver.test");
	ATF_CHECK_EQ(driver->version, 3);
	ATF_REQUIRE_EQ(driver->fields.count, 4);
	field = driver->fields.items[0];
	ATF_CHECK_STREQ(field->name, "ready");
	ATF_CHECK_EQ(field->type, SNL_VFDF_BOOL);
	ATF_CHECK_EQ(field->boolean, false);
	field = driver->fields.items[1];
	ATF_CHECK_STREQ(field->name, "opaque");
	ATF_CHECK_EQ(field->type, SNL_VFDF_BINARY);
	ATF_REQUIRE(field->binary != NULL);
	ATF_CHECK_EQ(NLA_DATA_LEN(field->binary), sizeof(extension_data));
	ATF_CHECK(memcmp(NLA_DATA(field->binary), extension_data,
	    sizeof(extension_data)) == 0);
	field = driver->fields.items[2];
	ATF_CHECK_STREQ(field->name, "sequence");
	ATF_CHECK_EQ(field->type, SNL_VFDF_NUMBER);
	ATF_CHECK_EQ(field->number, 0x100000002ULL);
	field = driver->fields.items[3];
	ATF_CHECK_STREQ(field->name, "mode");
	ATF_CHECK_EQ(field->type, SNL_VFDF_STRING);
	ATF_CHECK_STREQ(field->string, "diagnostic");
	vf = status->vfs.items[1];
	ATF_CHECK_EQ(vf->index, 8);
	ATF_CHECK_EQ(vf->configured, false);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_MIN_TX_RATE)) != 0);
	ATF_CHECK_EQ(vf->min_tx_rate_bps, 0);
	ATF_CHECK((vf->attrs & (1ULL << IFLAF_VF_MAX_TX_RATE)) != 0);
	ATF_CHECK_EQ(vf->max_tx_rate_bps, 0);
	vf = status->vfs.items[8];
	ATF_CHECK_EQ(vf->index, 15);
}

ATF_TC(snl_parse_large_vf_status);
ATF_TC_HEAD(snl_parse_large_vf_status, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Tests snl(3) parsing more than 256 full VF records");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_large_vf_status, tc)
{
	static const char *field_names[] = {
		"mirror-configured",
		"mirror-ingress-active",
		"mirror-egress-active",
	};
	static const uint32_t vf_count = 512;
	struct snl_parsed_vf_driver *driver;
	struct snl_parsed_vf_status *status;
	struct snl_parsed_link link = {};
	struct snl_parsed_vf *vf;
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	uint8_t mac[ETHER_ADDR_LEN] = { 0x02, 0, 0, 0, 0, 0 };
	uint32_t i, j;
	int driver_off, entry_off, fbsd_off, field_off, status_off;

	ATF_REQUIRE(snl_init(&ss, NETLINK_ROUTE));
	snl_init_writer(&ss, &nw);
	hdr = snl_create_msg_request(&nw, RTM_NEWLINK);
	ATF_REQUIRE(hdr != NULL);
	ATF_REQUIRE(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLA_NUM_VF, vf_count));
	fbsd_off = snl_add_msg_attr_nested(&nw, IFLA_FREEBSD);
	ATF_REQUIRE(fbsd_off != 0);
	status_off = snl_add_msg_attr_nested(&nw, IFLAF_VF_STATUS);
	ATF_REQUIRE(status_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_u8(&nw, IFLAF_VFS_PF_LINK_STATE,
	    IFLAF_VF_LINK_UP));
	ATF_REQUIRE(snl_add_msg_attr_u64(&nw, IFLAF_VFS_PF_LINK_SPEED,
	    100000000000ULL));
	snl_end_attr_nested(&nw, status_off);
	snl_end_attr_nested(&nw, fbsd_off);

	for (i = 0; i < vf_count; i++) {
		mac[4] = i >> 8;
		mac[5] = i;
		entry_off = snl_add_msg_attr_nested(&nw, IFLA_FREEBSD_VF);
		ATF_REQUIRE(entry_off != 0);
		ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VF_INDEX, i));
		ATF_REQUIRE(snl_add_msg_attr_bool(&nw, IFLAF_VF_CONFIGURED,
		    true));
		ATF_REQUIRE(snl_add_msg_attr_bool(&nw, IFLAF_VF_INITIALIZED,
		    true));
		ATF_REQUIRE(snl_add_msg_attr(&nw, IFLAF_VF_MAC, sizeof(mac),
		    mac));
		ATF_REQUIRE(snl_add_msg_attr_u8(&nw, IFLAF_VF_VLAN_MODE,
		    IFLAF_VF_VLAN_ACCESS));
		ATF_REQUIRE(snl_add_msg_attr_u16(&nw, IFLAF_VF_VLAN,
		    i & EVL_VLID_MASK));
		ATF_REQUIRE(snl_add_msg_attr_u8(&nw, IFLAF_VF_VLAN_PCP,
		    i & 7));
		ATF_REQUIRE(snl_add_msg_attr_u16(&nw, IFLAF_VF_VLAN_PROTO,
		    ETHERTYPE_VLAN));
		ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VF_VLAN_COUNT,
		    1));
		ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VF_VLAN_LIMIT,
		    16));
		ATF_REQUIRE(snl_add_msg_attr_u16(&nw,
		    IFLAF_VF_NUM_TX_QUEUES, 4));
		ATF_REQUIRE(snl_add_msg_attr_u16(&nw,
		    IFLAF_VF_NUM_RX_QUEUES, 4));
		ATF_REQUIRE(snl_add_msg_attr_u64(&nw, IFLAF_VF_MIN_TX_RATE,
		    1000000000ULL));
		ATF_REQUIRE(snl_add_msg_attr_u64(&nw, IFLAF_VF_MAX_TX_RATE,
		    10000000000ULL));
		ATF_REQUIRE(snl_add_msg_attr_bool(&nw, IFLAF_VF_ALLOW_SET_MAC,
		    true));
		ATF_REQUIRE(snl_add_msg_attr_bool(&nw,
		    IFLAF_VF_ALLOW_SET_VLAN, true));
		ATF_REQUIRE(snl_add_msg_attr_bool(&nw,
		    IFLAF_VF_MAC_ANTI_SPOOF, true));
		ATF_REQUIRE(snl_add_msg_attr_bool(&nw, IFLAF_VF_ALLOW_PROMISC,
		    false));
		ATF_REQUIRE(snl_add_msg_attr_bool(&nw,
		    IFLAF_VF_TRAFFIC_ALLOWED, true));
		ATF_REQUIRE(snl_add_msg_attr_bool(&nw,
		    IFLAF_VF_FAULT_BLOCKED, false));
		ATF_REQUIRE(snl_add_msg_attr_bool(&nw, IFLAF_VF_QUARANTINED,
		    false));
		ATF_REQUIRE(snl_add_msg_attr_string(&nw,
		    IFLAF_VF_API_VERSION, "1.1"));
		ATF_REQUIRE(snl_add_msg_attr_u8(&nw,
		    IFLAF_VF_LINK_STATE_POLICY, IFLAF_VF_LINK_AUTO));

		driver_off = snl_add_msg_attr_nested(&nw, IFLAF_VF_DRIVER);
		ATF_REQUIRE(driver_off != 0);
		ATF_REQUIRE(snl_add_msg_attr_string(&nw, IFLAF_VFD_NAME,
		    "driver.ice"));
		ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VFD_VERSION, 1));
		for (j = 0; j < nitems(field_names); j++) {
			field_off = snl_add_msg_attr_nested(&nw,
			    IFLAF_VFD_FIELD);
			ATF_REQUIRE(field_off != 0);
			ATF_REQUIRE(snl_add_msg_attr_string(&nw,
			    IFLAF_VFDF_NAME, field_names[j]));
			ATF_REQUIRE(snl_add_msg_attr_bool(&nw,
			    IFLAF_VFDF_BOOL, false));
			snl_end_attr_nested(&nw, field_off);
		}
		snl_end_attr_nested(&nw, driver_off);
		snl_end_attr_nested(&nw, entry_off);
	}
	hdr = snl_finalize_msg(&nw);
	ATF_REQUIRE(hdr != NULL);
	ATF_REQUIRE_MSG(hdr->nlmsg_len > UINT16_MAX,
	    "test message is only %u bytes", hdr->nlmsg_len);

	ATF_REQUIRE(snl_parse_nlmsg(&ss, hdr, &snl_rtm_link_parser, &link));
	ATF_CHECK_EQ(link.ifla_num_vf, vf_count);
	status = &link.iflaf_vf_status;
	ATF_CHECK(status->present);
	ATF_REQUIRE_EQ(status->vfs.count, vf_count);
	vf = status->vfs.items[0];
	ATF_CHECK_EQ(vf->index, 0);
	vf = status->vfs.items[vf_count - 1];
	ATF_CHECK_EQ(vf->index, vf_count - 1);
	ATF_CHECK_EQ(vf->tx_queue_count, 4);
	ATF_CHECK_EQ(vf->rx_queue_count, 4);
	ATF_REQUIRE_EQ(vf->drivers.count, 1);
	driver = vf->drivers.items[0];
	ATF_CHECK_STREQ(driver->name, "driver.ice");
	ATF_CHECK_EQ(driver->fields.count, nitems(field_names));
}

ATF_TC(snl_parse_empty_vf_status);
ATF_TC_HEAD(snl_parse_empty_vf_status, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Tests snl(3) parsing an empty VF status snapshot");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_empty_vf_status, tc)
{
	struct snl_parsed_link link = {};
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	int fbsd_off, status_off;

	ATF_REQUIRE(snl_init(&ss, NETLINK_ROUTE));
	snl_init_writer(&ss, &nw);
	hdr = snl_create_msg_request(&nw, RTM_NEWLINK);
	ATF_REQUIRE(hdr != NULL);
	ATF_REQUIRE(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLA_NUM_VF, 0));
	fbsd_off = snl_add_msg_attr_nested(&nw, IFLA_FREEBSD);
	ATF_REQUIRE(fbsd_off != 0);
	status_off = snl_add_msg_attr_nested(&nw, IFLAF_VF_STATUS);
	ATF_REQUIRE(status_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_u8(&nw, IFLAF_VFS_PF_LINK_STATE,
	    IFLAF_VF_LINK_DOWN));
	snl_end_attr_nested(&nw, status_off);
	snl_end_attr_nested(&nw, fbsd_off);
	hdr = snl_finalize_msg(&nw);
	ATF_REQUIRE(hdr != NULL);

	ATF_REQUIRE(snl_parse_nlmsg(&ss, hdr, &snl_rtm_link_parser, &link));
	ATF_CHECK_EQ(link.ifla_num_vf, 0);
	ATF_CHECK(link.iflaf_vf_status.present);
	ATF_CHECK_EQ(link.iflaf_vf_status.pf_link_state,
	    IFLAF_VF_LINK_DOWN);
	ATF_CHECK_EQ(link.iflaf_vf_status.vfs.count, 0);
}

ATF_TC(snl_parse_vf_status_error);
ATF_TC_HEAD(snl_parse_vf_status_error, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Tests snl(3) parsing a VF status provider error");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_vf_status_error, tc)
{
	struct snl_parsed_link link = {};
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	int fbsd_off, status_off;

	ATF_REQUIRE(snl_init(&ss, NETLINK_ROUTE));
	snl_init_writer(&ss, &nw);
	hdr = snl_create_msg_request(&nw, RTM_NEWLINK);
	ATF_REQUIRE(hdr != NULL);
	ATF_REQUIRE(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	fbsd_off = snl_add_msg_attr_nested(&nw, IFLA_FREEBSD);
	ATF_REQUIRE(fbsd_off != 0);
	status_off = snl_add_msg_attr_nested(&nw, IFLAF_VF_STATUS);
	ATF_REQUIRE(status_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VFS_ERROR, EMSGSIZE));
	snl_end_attr_nested(&nw, status_off);
	snl_end_attr_nested(&nw, fbsd_off);
	hdr = snl_finalize_msg(&nw);
	ATF_REQUIRE(hdr != NULL);

	ATF_REQUIRE(snl_parse_nlmsg(&ss, hdr, &snl_rtm_link_parser, &link));
	ATF_CHECK(link.iflaf_vf_status.present);
	ATF_CHECK_EQ(link.iflaf_vf_status.error, EMSGSIZE);
	ATF_CHECK_EQ(link.iflaf_vf_status.vfs.count, 0);
}

ATF_TC(snl_verify_route_parsers);
ATF_TC_HEAD(snl_verify_route_parsers, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) route parsers are correct");
}

ATF_TC_BODY(snl_verify_route_parsers, tc)
{
	SNL_VERIFY_PARSERS(snl_all_route_parsers);

}

ATF_TC(snl_parse_errmsg_capped);
ATF_TC_HEAD(snl_parse_errmsg_capped, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) correctly parsing capped errors");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_errmsg_capped, tc)
{
	struct snl_state ss;
	struct snl_writer nw;

	if (!snl_init(&ss, NETLINK_ROUTE))
		atf_tc_fail("snl_init() failed");

	int optval = 1;
	ATF_CHECK(setsockopt(ss.fd, SOL_NETLINK, NETLINK_CAP_ACK, &optval, sizeof(optval)) == 0);

	optval = 0;
	ATF_CHECK(setsockopt(ss.fd, SOL_NETLINK, NETLINK_EXT_ACK, &optval, sizeof(optval)) == 0);
	snl_init_writer(&ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, 255);
	ATF_CHECK(hdr != NULL);
	ATF_CHECK(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	snl_add_msg_attr_string(&nw, 143, "some random string");
	ATF_CHECK(snl_finalize_msg(&nw) != NULL);

	ATF_CHECK(snl_send_message(&ss, hdr));

	struct nlmsghdr *rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq);
	ATF_CHECK(rx_hdr != NULL);
	ATF_CHECK(rx_hdr->nlmsg_type == NLMSG_ERROR);

	struct snl_errmsg_data e = {};
	ATF_CHECK(rx_hdr->nlmsg_len == sizeof(struct nlmsghdr) + sizeof(struct nlmsgerr));
	ATF_CHECK(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_CHECK(e.error != 0);
	ATF_CHECK(!memcmp(hdr, e.orig_hdr, sizeof(struct nlmsghdr)));
}

ATF_TC(snl_parse_errmsg_capped_extack);
ATF_TC_HEAD(snl_parse_errmsg_capped_extack, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) correctly parsing capped errors with extack");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_errmsg_capped_extack, tc)
{
	struct snl_state ss;
	struct snl_writer nw;

	if (!snl_init(&ss, NETLINK_ROUTE))
		atf_tc_fail("snl_init() failed");

	int optval = 1;
	ATF_CHECK(setsockopt(ss.fd, SOL_NETLINK, NETLINK_CAP_ACK, &optval, sizeof(optval)) == 0);
	optval = 1;
	ATF_CHECK(setsockopt(ss.fd, SOL_NETLINK, NETLINK_EXT_ACK, &optval, sizeof(optval)) == 0);

	snl_init_writer(&ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, 255);
	ATF_CHECK(hdr != NULL);
	ATF_CHECK(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	snl_add_msg_attr_string(&nw, 143, "some random string");
	ATF_CHECK(snl_finalize_msg(&nw) != NULL);

	ATF_CHECK(snl_send_message(&ss, hdr));

	struct nlmsghdr *rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq);
	ATF_CHECK(rx_hdr != NULL);
	ATF_CHECK(rx_hdr->nlmsg_type == NLMSG_ERROR);

	struct snl_errmsg_data e = {};
	ATF_CHECK(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_CHECK(e.error != 0);
	ATF_CHECK(!memcmp(hdr, e.orig_hdr, sizeof(struct nlmsghdr)));

	ATF_CHECK(e.error_str != NULL);
}

ATF_TC(snl_parse_errmsg_uncapped_extack);
ATF_TC_HEAD(snl_parse_errmsg_uncapped_extack, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) correctly parsing errors with extack");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_errmsg_uncapped_extack, tc)
{
	struct snl_state ss;
	struct snl_writer nw;

	ATF_CHECK(snl_init(&ss, NETLINK_ROUTE));

	int optval = 1;
	ATF_CHECK(setsockopt(ss.fd, SOL_NETLINK, NETLINK_EXT_ACK, &optval, sizeof(optval)) == 0);

	snl_init_writer(&ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, 255);
	ATF_CHECK(hdr != NULL);
	ATF_CHECK(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	snl_add_msg_attr_string(&nw, 143, "some random string");
	ATF_CHECK(snl_finalize_msg(&nw) != NULL);

	ATF_CHECK(snl_send_message(&ss, hdr));

	struct nlmsghdr *rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq);
	ATF_CHECK(rx_hdr != NULL);
	ATF_CHECK(rx_hdr->nlmsg_type == NLMSG_ERROR);

	struct snl_errmsg_data e = {};
	ATF_CHECK(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_CHECK(e.error != 0);
	ATF_CHECK(!memcmp(hdr, e.orig_hdr, hdr->nlmsg_len));

	ATF_CHECK(e.error_str != NULL);
}

ATF_TC(snl_list_ifaces);
ATF_TC_HEAD(snl_list_ifaces, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) listing interfaces");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

struct nl_parsed_link {
	uint32_t		ifi_index;
	uint32_t		ifla_mtu;
	char			*ifla_ifname;
};

#define	_IN(_field)	offsetof(struct ifinfomsg, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_link, _field)
static struct snl_attr_parser ap_link[] = {
	{ .type = IFLA_IFNAME, .off = _OUT(ifla_ifname), .cb = snl_attr_get_string },
	{ .type = IFLA_MTU, .off = _OUT(ifla_mtu), .cb = snl_attr_get_uint32 },
};
static struct snl_field_parser fp_link[] = {
	{.off_in = _IN(ifi_index), .off_out = _OUT(ifi_index), .cb = snl_field_get_uint32 },
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(link_parser, struct ifinfomsg, fp_link, ap_link);


ATF_TC_BODY(snl_list_ifaces, tc)
{
	struct snl_state ss;
	struct snl_writer nw;

	if (!snl_init(&ss, NETLINK_ROUTE))
		atf_tc_fail("snl_init() failed");

	snl_init_writer(&ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_GETLINK);
	ATF_CHECK(hdr != NULL);
	ATF_CHECK(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	ATF_CHECK(snl_finalize_msg(&nw) != NULL);
	uint32_t seq_id = hdr->nlmsg_seq;

	ATF_CHECK(snl_send_message(&ss, hdr));

	struct snl_errmsg_data e = {};
	int count = 0;

	while ((hdr = snl_read_reply_multi(&ss, seq_id, &e)) != NULL) {
		count++;
	}
	ATF_REQUIRE(e.error == 0);

	ATF_REQUIRE_MSG(count > 0, "Empty interface list");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, snl_verify_core_parsers);
	ATF_TP_ADD_TC(tp, snl_parse_bitset_array);
	ATF_TP_ADD_TC(tp, snl_verify_route_parsers);
	ATF_TP_ADD_TC(tp, snl_parse_vf_status);
	ATF_TP_ADD_TC(tp, snl_parse_large_vf_status);
	ATF_TP_ADD_TC(tp, snl_parse_empty_vf_status);
	ATF_TP_ADD_TC(tp, snl_parse_vf_status_error);
	ATF_TP_ADD_TC(tp, snl_parse_errmsg_capped);
	ATF_TP_ADD_TC(tp, snl_parse_errmsg_capped_extack);
	ATF_TP_ADD_TC(tp, snl_parse_errmsg_uncapped_extack);
	ATF_TP_ADD_TC(tp, snl_list_ifaces);

	return (atf_no_error());
}
