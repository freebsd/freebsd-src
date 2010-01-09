/*
 * octeon_pip.h		Packet Input Processing Block
 *
 */



#ifndef __OCTEON_PIP_H__
#define __OCTEON_PIP_H__

/** 
 * Enumeration representing the amount of packet processing
 * and validation performed by the input hardware.
 */
typedef enum
{
    OCTEON_PIP_PORT_CFG_MODE_NONE = 0ull,  /**< Packet input doesn't perform any
                                            processing of the input packet. */
    OCTEON_PIP_PORT_CFG_MODE_SKIPL2 = 1ull,/**< Full packet processing is performed
                                            with pointer starting at the L2
                                            (ethernet MAC) header. */
    OCTEON_PIP_PORT_CFG_MODE_SKIPIP = 2ull /**< Input packets are assumed to be IP.
                                            Results from non IP packets is
                                            undefined. Pointers reference the
                                            beginning of the IP header. */
} octeon_pip_port_parse_mode_t;



#define OCTEON_PIP_PRT_CFGX(offset)	(0x80011800A0000200ull+((offset)*8))
#define OCTEON_PIP_PRT_TAGX(offset)	(0x80011800A0000400ull+((offset)*8))
#define OCTEON_PIP_STAT_INB_PKTS(port)	(0x80011800A0001A00ull+((port) * 32))
#define OCTEON_PIP_STAT_INB_ERRS(port)	(0x80011800A0001A10ull+((port) * 32))

/*
 * PIP Global Config
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved2	: 45;	/* Must be zero */
        uint64_t tag_syn	: 1;	/* Not Include src_crc in TCP..*/
        uint64_t ip6_udp	: 1;	/* IPv6/UDP checksum is mandatory */
        uint64_t max_l2		: 1;	/* Largest L2 frame. 0/1 : 1500/1535 */
        uint64_t reserved1	: 5;	/* Must be zero */
        uint64_t raw_shf	: 3;	/* PCI RAW Packet shift/pad amount */
        uint64_t reserved0	: 5;	/* Must be zero */
        uint64_t nip_shf	: 3;	/* Non-IP shift/pad amount */
    } bits;
} octeon_pip_gbl_cfg_t;


typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved4      : 37;      /* Must be zero */
        uint64_t qos            : 3;       /* Default POW QoS queue */
        uint64_t qos_wat        : 4;       /* Bitfield to enable QoS watcher */
                                           /*  look up tables. 4 per port. */
        uint64_t reserved3      : 1;       /* Must be zero */
        uint64_t spare          : 1;       /* Must be zero */
        uint64_t qos_diff       : 1;       /* Use IP diffserv to determine */
                                           /*     the queue in the POW */
        uint64_t qos_vlan       : 1;       /* Use the VLAN tag to determine */
                                           /*     the queue in the POW */
        uint64_t reserved2      : 3;       /* Must be zero */
        uint64_t crc_en         : 1;       /* Enable HW checksum */
        uint64_t reserved1      : 2;       /* Must be zero */
        octeon_pip_port_parse_mode_t mode  : 2;  /* Raw/Parsed/IP/etc */
        uint64_t reserved0      : 1;       /* Must be zero */
        uint64_t skip           : 7;       /* 8 byte words to skip in the */
                                           /*   beginning of a packet buffer */
    } bits;
} octeon_pip_port_cfg_t;



/*
 * Packet input to POW interface. How input packets are tagged for
 * the POW is controlled here.
 */
typedef union {
    uint64_t word64;
    struct {        
        uint64_t reserved                : 24;      /**< Reserved */
        uint64_t grptagbase              : 4;       /**< Offset to use when computing group from tag bits
                                                         when GRPTAG is set. Only applies to IP packets.
                                                         (PASS2 only) */
        uint64_t grptagmask              : 4;       /**< Which bits of the tag to exclude when computing
                                                         group when GRPTAG is set. Only applies to IP packets.
                                                         (PASS2 only) */
        uint64_t grptag                  : 1;       /**< When set, use the lower bit of the tag to compute
                                                         the group in the work queue entry
                                                         GRP = WQE[TAG[3:0]] & ~GRPTAGMASK + GRPTAGBASE.
                                                         Only applies to IP packets. (PASS2 only) */
        uint64_t spare                   : 1;       /**< Spare bit
                                                         (PASS2 only) */
        uint64_t tag_mode     : 2;       /**< Which tag algorithm to use
                                                         0 = always use tuple tag algorithm
                                                         1 = always use mask tag algorithm
                                                         2 = if packet is IP, use tuple else use mask
                                                         3 = tuple XOR mask
                                                         (PASS2 only) */
        uint64_t inc_vs                  : 2;       /**< determines the VLAN ID (VID) to be included in
                                                         tuple tag when VLAN stacking is detected
                                                         0 = do not include VID in tuple tag generation
                                                         1 = include VID (VLAN0) in hash
                                                         2 = include VID (VLAN1) in hash
                                                         3 = include VID ([VLAN0,VLAN1]) in hash
                                                         (PASS2 only) */
        uint64_t inc_vlan                : 1;       /**< when set, the VLAN ID is included in tuple tag
                                                         when VLAN stacking is not detected
                                                         0 = do not include VID in tuple tag generation
                                                         1 = include VID in hash
                                                         (PASS2 only) */
        uint64_t inc_prt_flag            : 1;       /**< sets whether the port is included in tuple tag */
        uint64_t ip6_dprt_flag           : 1;       /**< sets whether the TCP/UDP dst port is
                                                         included in tuple tag for IPv6 packets */
        uint64_t ip4_dprt_flag           : 1;       /**< sets whether the TCP/UDP dst port is
                                                         included in tuple tag for IPv4 */
        uint64_t ip6_sprt_flag           : 1;       /**< sets whether the TCP/UDP src port is
                                                 	included in tuple tag for IPv6 packets */
        uint64_t ip4_sprt_flag           : 1;       /**< sets whether the TCP/UDP src port is
                                                         included in tuple tag for IPv4 */
        uint64_t ip6_nxth_flag           : 1;       /**< sets whether ipv6 includes next header in tuple
                                                         tag hash */
        uint64_t ip4_pctl_flag           : 1;       /**< sets whether ipv4 includes protocol in tuple
                                                         tag hash */
        uint64_t ip6_dst_flag            : 1;       /**< sets whether ipv6 includes dst address in tuple
                                                         tag hash */
        uint64_t ip4_dst_flag            : 1;       /**< sets whether ipv4 includes dst address in tuple
                                                         tag hash */
        uint64_t ip6_src_flag            : 1;       /**< sets whether ipv6 includes src address in tuple
                                                         tag hash */
        uint64_t ip4_src_flag            : 1;       /**< sets whether ipv4 includes src address in tuple
                                                         tag hash */
        uint64_t tcp6_tag_type           : 2;       /**< sets the tag_type of a TCP packet (IPv6)
                                                         0 = ordered tags
                                                         1 = atomic tags
                                                         2 = Null tags */
        uint64_t tcp4_tag_type           : 2;       /**< sets the tag_type of a TCP packet (IPv4)
                                                         0 = ordered tags
                                                         1 = atomic tags
                                                         2 = Null tags */
        uint64_t ip6_tag_type            : 2;       /**< sets whether IPv6 packet tag type
                                                         0 = ordered tags
                                                         1 = atomic tags
                                                         2 = Null tags */
        uint64_t ip4_tag_type            : 2;       /**< sets whether IPv4 packet tag type
                                                         0 = ordered tags
                                                         1 = atomic tags
                                                         2 = Null tags */
        uint64_t non_tag_type            : 2;       /**< sets whether non-IP packet tag type
                                                         0 = ordered tags
                                                         1 = atomic tags
                                                         2 = Null tags */
        uint64_t grp                    : 4;    /* POW group for input pkts */
    } bits;
} octeon_pip_port_tag_cfg_t;


/** 
 * Configure an ethernet input port
 *  
 * @param port_num Port number to configure
 * @param port_cfg Port hardware configuration
 * @param port_tag_cfg
 *                 Port POW tagging configuration
 */
static inline void octeon_pip_config_port(u_int port_num,
                                          octeon_pip_port_cfg_t port_cfg,
                                          octeon_pip_port_tag_cfg_t port_tag_cfg)
{
    oct_write64(OCTEON_PIP_PRT_CFGX(port_num), port_cfg.word64);
    oct_write64(OCTEON_PIP_PRT_TAGX(port_num), port_tag_cfg.word64);
}


#endif /*  __OCTEON_PIP_H__ */
