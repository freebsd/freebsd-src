/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: pcap.h,v 2.2.2.1 2001/06/26 10:43:20 darrenr Exp $
 */
/*
 * This header file is constructed to match the version described by
 * PCAP_VERSION_MAJ.
 *
 * The structure largely derives from libpcap which wouldn't include
 * nicely without bpf.
 */
typedef	struct	pcap_filehdr	{
	u_int	pc_id;
	u_short	pc_v_maj;
	u_short	pc_v_min;
	u_int	pc_zone;
	u_int	pc_sigfigs;
	u_int	pc_slen;
	u_int	pc_type;
} pcaphdr_t;

#define	TCPDUMP_MAGIC		0xa1b2c3d4

#define	PCAP_VERSION_MAJ	2

typedef	struct	pcap_pkthdr	{
	struct	timeval	ph_ts;
	u_int	ph_clen;
	u_int	ph_len;
} pcappkt_t;

