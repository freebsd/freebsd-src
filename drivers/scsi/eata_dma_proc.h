
struct lun_map {
    __u8   id:5,
     chan:3;
    __u8 lun;
};

typedef struct emul_pp {
    __u8 p_code:6,
       null:1,
     p_save:1;
    __u8 p_length;
    __u16 cylinder;
    __u8 heads;
    __u8 sectors;
    __u8 null2;
    __u8 s_lunmap:4,
	  ems:1;
    __u16 drive_type;	/* In Little Endian ! */
    struct lun_map lunmap[4];
}emulpp;


/* Log Sense pages */

typedef struct log_sheader {
    __u8 page_code,
     reserved;
    __u16 length;
}logsh;


/* Log Sense Statistics */

typedef struct read_command_statistics {
    __u16 code;	       /* 0x01 */
    __u8  flags;
    __u8  length;      /* 0x24 */
    __u32 h_commands,
      uncached,
      la_cmds,
      la_blks,
      la_hits,
      missed,
      hits,
      seq_la_blks,
      seq_la_hits;
}r_cmd_stat;

typedef struct write_command_statistics {
    __u16 code;	       /* 0x03 */
    __u8  flags;
    __u8  length;      /* 0x28 */
    __u32 h_commands,
      uncached,
      thru,
      bypass,
      soft_err,
      hits,
      b_idle,
      b_activ,
      b_blks,
      b_blks_clean;
}w_cmd_stat;

typedef struct host_command_statistics {
    __u16 code;		 /* 0x02, 0x04 */
    __u8  flags;
    __u8  length;	 /* 0x30 */
    __u32 sizes[12];
}hst_cmd_stat;

typedef struct physical_command_statistics {
    __u16 code;		 /* 0x06, 0x07 */ 
    __u8  flags;
    __u8  length;	 /* 0x34 */
    __u32 sizes[13]; 
}phy_cmd_stat;

typedef struct misc_device_statistics {
    __u16 code;		  /* 0x05 */
    __u8  flags;
    __u8  length;	  /* 0x10 */
    __u32 disconnect,
      pass_thru,
      sg_commands,
      stripe_boundary_crosses;
}msc_stats;
 
/* Configuration Pages */

typedef struct controller_configuration {
    __u16 code;		  /* 0x01 */
    __u8  flags;
    __u8  length;	  /* 0x02 */
    __u8  intt:1,
       sec:1,
       csh:1,
       key:1,
       tmr:1,
       srs:1,
       nvr:1;
    __u8  interrupt;
}coco;

typedef struct controller_hardware_errors {
    __u16 code;		  /* 0x02 */
    __u8  flags;
    __u8  length;	  /* 0x02 */
    __u8  unused:1,
	 per:1;
    __u8  interrupt;
}coher;

typedef struct memory_map {
    __u16 code;		  /* 0x03, 0x04 */
    __u8  flags;
    __u8  length;	  /* 0x04 */
    __u32 memory_map;
}mema;

typedef struct scsi_transfer {
    __u16 code;		  /* 0x05 */
    __u8  flags;
    __u8  length;	  /* 0x04 */
    __u8  offset,
      period;
    __u16 speed;
}scsitrans;

typedef struct scsi_modes {
    __u16 code;		  /* 0x06 */
    __u8  flags;
    __u8  length;	  /* 0x02 */
    __u8  que:1,
     cdis:1,
     wtru:1,
     dasd:1,
      ncr:1,
     awre:1;
    __u8  reserved;
}scsimod;

typedef struct host_bus {
    __u16 code;		  /* 0x07 */
    __u8  flags;
    __u8  length;	  /* 0x02 */
    __u8  speed:6,
	pci:1,
       eisa:1;
    __u8  reserved;
}hobu;

typedef struct scsi_bus {
    __u16 code;		  /* 0x08 */
    __u8  flags;
    __u8  length;	  /* 0x02 */
    __u8  speed:4,
	res:1,
	ext:1,
       wide:1,
	dif:1;
    __u8 busnum;
}scbu;

typedef struct board_type {
    __u16 code;		  /* 0x09 */
    __u8  flags;
    __u8  length;	  /* 0x04 */
    __u8  unused:1,
	 cmi:1,
	 dmi:1,
	cm4k:1,
	 cm4:1,
	dm4k:1,
	 dm4:1,
	 hba:1;
    __u8  cpu_type,
      cpu_speed;
    __u8    sx1:1,
	sx2:1,
    unused2:4,
       alrm:1,
       srom:1;
}boty;

typedef struct memory_config {
    __u16 code;		  /* 0x0a */
    __u8  flags;
    __u8  length;	  /* 0x04 */
    __u8  banksize[4];
}memco;

typedef struct firmware_info {
    __u16 code;		  /* 0x0b */
    __u8  flags;
    __u8  length;	  /* 0x04 */
    __u8  dnld:1,
     bs528:1,
       fmt:1,
     fw528:1;
    __u8  unused1,
      fw_type,
      unused;
}firm;

typedef struct subsystem_info {
    __u16 code;		  /* 0x0c */
    __u8  flags;
    __u8  length;	  /* 0x02 */
    __u8  shlf:1,
      swap:1,
      noss:1;
    __u8  reserved;
}subinf;

typedef struct per_channel_info {
    __u16 code;		  /* 0x0d */
    __u8  flags;
    __u8  length;	  /* 0x02 */
    __u8  channel;
    __u8  shlf:1,
      swap:1,
      noss:1,
       srs:1,
       que:1,
       ext:1,
      wide:1,
      diff:1;
}pcinf;

typedef struct array_limits {
    __u16 code;		  /* 0x0e */
    __u8  flags;
    __u8  length;	  /* 0x04 */
    __u8  max_groups,
      raid0_drv,
      raid35_drv,
      unused;
}arrlim;

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */

