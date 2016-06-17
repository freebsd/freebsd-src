/**********************************************************************
 * iph5526.c: Structures for the Interphase 5526 PCI Fibre Channel 
 *			  IP/SCSI driver.
 * Copyright (C) 1999 Vineet M Abraham <vmabraham@hotmail.com>
 **********************************************************************/

#ifndef _TACH_STRUCT_H
#define _TACH_STRUCT_H

typedef struct {
	u_short cmnd_code;
	u_short payload_length;
	u_short type_code;
	u_short est_image_pair;
	u_int originator_pa;
	u_int responder_pa;
	u_int service_params;
} PRLI;

typedef struct {
	u_int flags_and_byte_offset;
	u_int byte_count;
	u_short no_of_recvd_frames;
	u_short no_of_expected_frames;
	u_int last_fctl;
	u_int sdb_address;
	u_int scratch_pad;
	u_int expected_ro;
	u_short buffer_index;
	u_short buffer_offset;
	} INB_SEST_ENTRY;

typedef struct {
	u_int flags_and_did;
	u_short max_frame_len;
	u_short cntl;
	u_int total_seq_length;
	u_short link;
	u_short rx_id;
	u_int transaction_id;
	u_int header_address;
	u_char seq_id;
	u_char reserved;
	u_short header_length;
	u_int edb_address;
	} OUTB_SEST_ENTRY;

typedef struct {
	u_short d_naa;
	u_short dest_high;
	u_int dest_low;
	u_short s_naa;
	u_short source_high;
	u_int source_low;
	} NW_HEADER;

typedef struct {
	u_int resv;
	u_char sof_and_eof;
	u_char dest_alpa;
	u_short lcr_and_time_stamp;
	u_int r_ctl_and_d_id;
	u_int vc_id_and_s_id;
	u_int type_and_f_cntl;
	u_char seq_id;
	u_char df_cntl;
	u_short seq_cnt;
	u_short ox_id;
	u_short rx_id;
	u_int ro;
	NW_HEADER nw_header;
	} TACHYON_HEADER;

typedef struct {
	u_short service_options;
	u_short initiator_ctl;
	u_short recipient_ctl;
	u_short recv_data_field_size;
	u_short concurrent_sequences;
	u_short n_port_end_to_end_credit;
	u_short open_seq_per_exchange;
	u_short resv;
	}CLASS_OF_SERVICE;

typedef struct {
	u_int logo_cmnd;
	u_char reserved;
	u_char n_port_id_2;
	u_char n_port_id_1;
	u_char n_port_id_0;
	u_int port_name_up;
	u_int port_name_low;
	} LOGO;

typedef struct {
	u_int ls_cmnd_code;
	u_int hard_address;
	u_int port_name_high;
	u_int port_name_low;
	u_int node_name_high;
	u_int node_name_low;
	u_int n_port_id;
	} ADISC;
	
typedef struct {
	u_int cmnd_code;
	u_int reason_code;
	} LS_RJT;

typedef struct {
	u_int cmnd_code;
	} ACC;

typedef struct  {
	u_int seq_d_id;
	u_int tot_len;
	u_short cntl;
	u_short rx_id;
	u_short cs_enable;
	u_short cs_seed;
	u_int trans_id;
	u_int hdr_addr;
	u_short frame_len;
	u_short hdr_len;
	u_int edb_addr;
	}ODB;

typedef struct {
	u_int cmnd_code;
	u_int reg_function; /* in the last byte */
	} SCR;

typedef struct {
	u_int rev_in_id;
	u_char fs_type;
	u_char fs_subtype;
	u_char options;
	u_char resv1;
	u_short cmnd_resp_code;
	u_short max_res_size;
	u_char resv2;
	u_char reason_code;
	u_char expln_code;
	u_char vendor_unique;
	} CT_HDR;

typedef struct {
	CT_HDR	 ct_hdr;
	u_int s_id;
	u_char bit_map[32]; /* 32 byte bit map */
	} RFC_4;

typedef struct {
	u_int ls_cmnd_code;
	u_short fc_ph_version;
	u_short buff_to_buff_credit;
	u_short common_features;
	u_short recv_data_field_size;
	u_short n_port_total_conc_seq;
	u_short rel_off_by_info_cat;
	u_int ED_TOV;
	u_int n_port_name_high;
	u_int n_port_name_low;
	u_int node_name_high;
	u_int node_name_low;
	CLASS_OF_SERVICE c_of_s[3];
	u_int resv[4];
	u_int vendor_version_level[4];
	}LOGIN;

typedef struct {
	CT_HDR	 ct_hdr;
	u_int port_type; /* in the first byte */
	} GP_ID4;

typedef struct  {
	u_int   buf_addr;
	u_short ehf;
	u_short buf_len;
	}EDB;

/* (i)chip Registers */
struct i_chip_regs {
	u_int	ptr_ichip_hw_control_reg;
	u_int	ptr_ichip_hw_status_reg;
	u_int	ptr_ichip_hw_addr_mask_reg;
};

struct iph5526_novram {
	u_int	ptr_novram_hw_control_reg;
	u_int	ptr_novram_hw_status_reg;
	u_short data[IPH5526_NOVRAM_SIZE];
};

/* Tachyon Registers */
struct tachyon_regs {
	u_int    ptr_ocq_base_reg;
	u_int    ptr_ocq_len_reg;
	u_int    ptr_ocq_prod_indx_reg;
	u_int    ptr_ocq_cons_indx_reg;

	u_int    ptr_imq_base_reg;
	u_int    ptr_imq_len_reg;
	u_int    ptr_imq_cons_indx_reg;
	u_int    ptr_imq_prod_indx_reg;

	u_int    ptr_mfsbq_base_reg;
	u_int    ptr_mfsbq_len_reg;
	u_int    ptr_mfsbq_prod_reg;
	u_int    ptr_mfsbq_cons_reg;
	u_int    ptr_mfsbuff_len_reg;

	u_int    ptr_sfsbq_base_reg;
	u_int    ptr_sfsbq_len_reg;
	u_int    ptr_sfsbq_prod_reg;
	u_int    ptr_sfsbq_cons_reg;
	u_int    ptr_sfsbuff_len_reg;

	u_int    ptr_sest_base_reg;
	u_int    ptr_sest_len_reg;
	u_int    ptr_scsibuff_len_reg;

	u_int    ptr_tach_config_reg;
	u_int    ptr_tach_control_reg;
	u_int    ptr_tach_status_reg;
	u_int    ptr_tach_flush_oxid_reg;

	u_int    ptr_fm_config_reg;
	u_int    ptr_fm_control_reg;
	u_int    ptr_fm_status_reg;
	u_int    ptr_fm_tov_reg;
	u_int    ptr_fm_wwn_hi_reg;
	u_int    ptr_fm_wwn_low_reg;
	u_int    ptr_fm_rx_al_pa_reg;
};

struct globals {
	u_long tachyon_base;
	u_int *mem_base;
	u_short ox_id; /* OX_ID used for IP and ELS frames */
	u_short scsi_oxid; /* OX_ID for SEST entry */
	u_char seq_id;
	u_int my_id;
	u_int my_ddaa; /* my domain and area in a fabric */
	volatile u_char loop_up;
	volatile u_char ptp_up; /* we have a point-to-point link */
	volatile u_char link_up;
	volatile u_char n_port_try;
	volatile u_char nport_timer_set;
	volatile u_char lport_timer_set;
	/* Hmmm... We dont want to Initialize while closing */
	u_char dont_init; 
	u_int my_node_name_high;
	u_int my_node_name_low;
	u_int my_port_name_high;
	u_int my_port_name_low;
	u_char fabric_present;
	u_char explore_fabric;
	u_char name_server;  
	u_int my_mtu;
	u_int *els_buffer[MAX_PENDING_FRAMES]; /* temp space for ELS frames */
	char *arp_buffer; /* temp space for ARP frames */
	u_int mfs_buffer_count; /* keep track of MFS buffers used*/
	u_char scsi_registered;
	/* variables for port discovery */
	volatile u_char port_discovery;
	volatile u_char perform_adisc;
	u_short alpa_list_index;
	u_short type_of_frame; /* Could be IP/SCSI Read/SCSI Write*/	
	u_char no_of_targets; /* used to assign target_ids */
	u_long sem; /* to synchronize between IP and SCSI */
	u_char e_i;

	/* the frames */
	TACHYON_HEADER tach_header;
	LOGIN login;
	PRLI prli;
	LOGO logo;
	ADISC adisc;
	LS_RJT ls_rjt;
	ODB	odb;
	INB_SEST_ENTRY inb_sest_entry;
	OUTB_SEST_ENTRY outb_sest_entry;
	ACC	acc;
	SCR	scr;
	EDB	edb;
	RFC_4 rfc_4;
	GP_ID4 gp_id4;
};

struct queue_variables {
	/* Indices maintained in host memory.
	 */
	u_int *host_ocq_cons_indx, *host_hpcq_cons_indx, *host_imq_prod_indx;
	u_int *ptr_host_ocq_cons_indx, *ptr_host_hpcq_cons_indx, *ptr_host_imq_prod_indx;

	/* Variables for Outbound Command Queue (OCQ).
	 */
	u_int *ptr_ocq_base;
	u_int ocq_len, ocq_end;
	u_int ocq_prod_indx;
	u_int *ptr_odb[OCQ_LENGTH];

	/* Variables for Inbound Message Queue (IMQ).
	 */
	u_int *ptr_imq_base;
	u_int imq_len, imq_end;
	u_int imq_cons_indx;
	u_int imq_prod_indx;
	u_int *ptr_imqe[IMQ_LENGTH];

	u_int *ptr_mfsbq_base;
	u_int mfsbq_len, mfsbq_end;
	u_int mfsbq_prod_indx;
	u_int mfsbq_cons_indx;
	u_int mfsbuff_len, mfsbuff_end;

	u_int *ptr_sfsbq_base;
	u_int sfsbq_len, sfsbq_end;
	u_int sfsbq_prod_indx;
	u_int sfsbq_cons_indx;
	u_int sfsbuff_len, sfsbuff_end;
	u_int *ptr_sfs_buffers[SFSBQ_LENGTH * NO_OF_ENTRIES];

	/* Tables for SCSI Transactions */
	u_int *ptr_sest_base;
	u_int *ptr_sest[SEST_LENGTH];
	u_char free_scsi_oxid[SEST_LENGTH];
	u_int *ptr_sdb_base;
	u_int *ptr_sdb_slot[NO_OF_SDB_ENTRIES];
	u_char sdb_slot_status[NO_OF_SDB_ENTRIES];
	u_int sdb_indx;
	u_int *ptr_fcp_cmnd_base;
	u_int *ptr_fcp_cmnd[NO_OF_FCP_CMNDS];
	u_int fcp_cmnd_indx;

	/* Table for data to be transmitted.
	 */
	u_int *ptr_edb_base;
	u_int *ptr_edb[EDB_LEN];
	u_int edb_buffer_indx;
	volatile u_char free_edb_list[EDB_LEN];

	/* Table of Tachyon Headers.
	 */
	u_int *ptr_tachyon_header[NO_OF_TACH_HEADERS];
	u_int *ptr_tachyon_header_base;
	u_int tachyon_header_indx;
};

/* Used to match incoming ACCs to ELS requests sent out */
struct ox_id_els_map {
	u_short ox_id;
	u_int els;
	struct ox_id_els_map *next;
};


/* Carries info about individual nodes... stores the info got at login 
 * time. Also maintains mapping between MAC->FC addresses 
 */
struct fc_node_info {
	/* Itz the WWN (8 bytes), the last 6 bytes is the MAC address */
	u_char hw_addr[PORT_NAME_LEN]; 
	u_char node_name[NODE_NAME_LEN]; 
	u_int d_id;  /*real FC address, 3 bytes */
	int mtu;
	/* login = 1 if login attempted
	 * login = 2 if login completed 
	 */
	int login;    
	u_char scsi; /*  = 1 if device is a SCSI Target */
	u_char target_id;
	CLASS_OF_SERVICE c_of_s[3];
	struct fc_node_info *next;
};

struct fc_info {
	char name[8];
	u_long base_addr;
	int irq;
	struct net_device_stats fc_stats;
	struct fc_node_info *node_info_list;
	int num_nodes;
	struct ox_id_els_map *ox_id_list;
	struct i_chip_regs i_r;
	struct tachyon_regs t_r;
	struct queue_variables q;
	struct globals g;
	struct iph5526_novram n_r;
	u_short clone_id;
	struct timer_list nport_timer;
	struct timer_list lport_timer;
	struct timer_list explore_timer;
	struct timer_list display_cache_timer;
	struct net_device *dev;
	struct Scsi_Host *host;
	spinlock_t fc_lock;
};

struct iph5526_hostdata {
	struct fc_info *fi;
	fcp_cmd cmnd;
	Scsi_Cmnd *cmnd_handler[SEST_LENGTH];
	u_int tag_ages[MAX_SCSI_TARGETS];
};

/* List of valid AL_PAs */
u_char alpa_list[127] = { 
	0x00, 0x01, 0x02, 0x04, 0x08, 0x0F, 0x10, 0x17, 
	0x18, 0x1B, 0x1D, 0x1E, 0x1F, 0x23, 0x25, 0x26, 
	0x27, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x31, 
	0x32, 0x33, 0x34, 0x35, 0x36, 0x39, 0x3A, 0x3C, 
	0x43, 0x45, 0x46, 0x47, 0x49, 0x4A, 0x4B, 0x4C, 
	0x4D, 0x4E, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 
	0x59, 0x5A, 0x5C, 0x63, 0x65, 0x66, 0x67, 0x69, 
	0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x71, 0x72, 0x73, 
	0x74, 0x75, 0x76, 0x79, 0x7A, 0x7C, 0x80, 0x81, 
	0x82, 0x84, 0x88, 0x8F, 0x90, 0x97, 0x98, 0x9B, 
	0x9D, 0x9E, 0x9F, 0xA3, 0xA5, 0xA6, 0xA7, 0xA9, 
	0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xB1, 0xB2, 0xB3, 
	0xB4, 0xB5, 0xB6, 0xB9, 0xBA, 0xBC, 0xC3, 0xC5, 
	0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 
	0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD9, 0xDA, 
	0xDC, 0xE0, 0xE1, 0xE2, 0xE4, 0xE8, 0xEF
};

#endif /* _TACH_STRUCT_H */
