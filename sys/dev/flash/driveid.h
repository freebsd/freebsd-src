
/*
 * driveid.h
 *
 */

#ifndef __DRIVEID_H__
#define __DRIVEID_H__


struct hd_driveid {
        unsigned short  config;         /* lots of obsolete bit flags */
        unsigned short  cyls;           /* Obsolete, "physical" cyls */
        unsigned short  reserved2;      /* reserved (word 2) */
        unsigned short  heads;          /* Obsolete, "physical" heads */
        unsigned short  track_bytes;    /* unformatted bytes per track */
        unsigned short  sector_bytes;   /* unformatted bytes per sector */
        unsigned short  sectors;        /* Obsolete, "physical" sectors per track */
        unsigned short  vendor0;        /* vendor unique */
        unsigned short  vendor1;        /* vendor unique */
        unsigned short  vendor2;        /* Retired vendor unique */
        unsigned char   serial_no[20];  /* 0 = not_specified */
        unsigned short  buf_type;       /* Retired */
        unsigned short  buf_size;       /* Retired, 512 byte increments
                                         * 0 = not_specified
                                         */
        unsigned short  ecc_bytes;      /* for r/w long cmds; 0 = not_specified */
        unsigned char   fw_rev[8];      /* 0 = not_specified */
        unsigned char   model[40];      /* 0 = not_specified */
        unsigned char   max_multsect;   /* 0=not_implemented */
        unsigned char   vendor3;        /* vendor unique */
        unsigned short  dword_io;       /* 0=not_implemented; 1=implemented */
        unsigned char   vendor4;        /* vendor unique */
        unsigned char   capability;     /* (upper byte of word 49)
                                         *  3:  IORDYsup
                                         *  2:  IORDYsw
                                         *  1:  LBA
                                         *  0:  DMA
                                         */
        unsigned short  reserved50;     /* reserved (word 50) */
        unsigned char   vendor5;        /* Obsolete, vendor unique */
        unsigned char   tPIO;           /* Obsolete, 0=slow, 1=medium, 2=fast */
        unsigned char   vendor6;        /* Obsolete, vendor unique */
        unsigned char   tDMA;           /* Obsolete, 0=slow, 1=medium, 2=fast */
        unsigned short  field_valid;    /* (word 53)
                                         *  2:  ultra_ok        word  88
                                         *  1:  eide_ok         words 64-70
                                         *  0:  cur_ok          words 54-58
                                         */
        unsigned short  cur_cyls;       /* Obsolete, logical cylinders */
        unsigned short  cur_heads;      /* Obsolete, l heads */
        unsigned short  cur_sectors;    /* Obsolete, l sectors per track */
        unsigned short  cur_capacity0;  /* Obsolete, l total sectors on drive */
	unsigned short  cur_capacity1;  /* Obsolete, (2 words, misaligned int)     */
        unsigned char   multsect;       /* current multiple sector count */
        unsigned char   multsect_valid; /* when (bit0==1) multsect is ok */
        unsigned int    lba_capacity;   /* Obsolete, total number of sectors */
        unsigned short  dma_1word;      /* Obsolete, single-word dma info */
        unsigned short  dma_mword;      /* multiple-word dma info */
        unsigned short  eide_pio_modes; /* bits 0:mode3 1:mode4 */
        unsigned short  eide_dma_min;   /* min mword dma cycle time (ns) */
        unsigned short  eide_dma_time;  /* recommended mword dma cycle time (ns) */
        unsigned short  eide_pio;       /* min cycle time (ns), no IORDY  */
        unsigned short  eide_pio_iordy; /* min cycle time (ns), with IORDY */
        unsigned short  words69_70[2];  /* reserved words 69-70
                                         * future command overlap and queuing
                                         */
        /* HDIO_GET_IDENTITY currently returns only words 0 through 70 */
        unsigned short  words71_74[4];  /* reserved words 71-74
                                         * for IDENTIFY PACKET DEVICE command
                                         */
        unsigned short  queue_depth;    /* (word 75)
                                         * 15:5 reserved
                                         *  4:0 Maximum queue depth -1
                                         */
        unsigned short  words76_79[4];  /* reserved words 76-79 */
        unsigned short  major_rev_num;  /* (word 80) */
        unsigned short  minor_rev_num;  /* (word 81) */
        unsigned short  command_set_1;  /* (word 82) supported
                                         * 15:  Obsolete
                                         * 14:  NOP command
                                         * 13:  READ_BUFFER
                                         * 12:  WRITE_BUFFER
                                         * 11:  Obsolete
                                         * 10:  Host Protected Area
                                         *  9:  DEVICE Reset
                                         *  8:  SERVICE Interrupt
                                         *  7:  Release Interrupt
                                         *  6:  look-ahead
                                         *  5:  write cache
                                         *  4:  PACKET Command
                                         *  3:  Power Management Feature Set
                                         *  2:  Removable Feature Set
                                         *  1:  Security Feature Set
                                         *  0:  SMART Feature Set
                                         */
	unsigned short  command_set_2;  /* (word 83)
                                         * 15:  Shall be ZERO
                                         * 14:  Shall be ONE
                                         * 13:  FLUSH CACHE EXT
                                         * 12:  FLUSH CACHE
                                         * 11:  Device Configuration Overlay
                                         * 10:  48-bit Address Feature Set
                                         *  9:  Automatic Acoustic Management
                                         *  8:  SET MAX security
                                         *  7:  reserved 1407DT PARTIES
                                         *  6:  SetF sub-command Power-Up
                                         *  5:  Power-Up in Standby Feature Set
                                         *  4:  Removable Media Notification
                                         *  3:  APM Feature Set
                                         *  2:  CFA Feature Set
                                         *  1:  READ/WRITE DMA QUEUED
                                         *  0:  Download MicroCode
                                         */
        unsigned short  cfsse;          /* (word 84)
                                         * cmd set-feature supported extensions
                                         * 15:  Shall be ZERO
                                         * 14:  Shall be ONE
                                         * 13:6 reserved
                                         *  5:  General Purpose Logging
                                         *  4:  Streaming Feature Set
                                         *  3:  Media Card Pass Through
                                         *  2:  Media Serial Number Valid
                                         *  1:  SMART selt-test supported
                                         *  0:  SMART error logging
                                         */
        unsigned short  cfs_enable_1;   /* (word 85)
                                         * command set-feature enabled
                                         * 15:  Obsolete
                                         * 14:  NOP command
                                         * 13:  READ_BUFFER
                                         * 12:  WRITE_BUFFER
                                         * 11:  Obsolete
                                         * 10:  Host Protected Area
                                         *  9:  DEVICE Reset
                                         *  8:  SERVICE Interrupt
                                         *  7:  Release Interrupt
                                         *  6:  look-ahead
                                         *  5:  write cache
                                         *  4:  PACKET Command
					 *  3:  Power Management Feature Set
                                         *  2:  Removable Feature Set
                                         *  1:  Security Feature Set
                                         *  0:  SMART Feature Set
                                         */
        unsigned short  cfs_enable_2;   /* (word 86)
                                         * command set-feature enabled
                                         * 15:  Shall be ZERO
                                         * 14:  Shall be ONE
                                         * 13:  FLUSH CACHE EXT
                                         * 12:  FLUSH CACHE
                                         * 11:  Device Configuration Overlay
                                         * 10:  48-bit Address Feature Set
                                         *  9:  Automatic Acoustic Management
                                         *  8:  SET MAX security
                                         *  7:  reserved 1407DT PARTIES
                                         *  6:  SetF sub-command Power-Up
                                         *  5:  Power-Up in Standby Feature Set
                                         *  4:  Removable Media Notification
                                         *  3:  APM Feature Set
                                         *  2:  CFA Feature Set
                                         *  1:  READ/WRITE DMA QUEUED
                                         *  0:  Download MicroCode
                                         */
        unsigned short  csf_default;    /* (word 87)
                                         * command set-feature default
                                         * 15:  Shall be ZERO
                                         * 14:  Shall be ONE
                                         * 13:6 reserved
                                         *  5:  General Purpose Logging enabled
                                         *  4:  Valid CONFIGURE STREAM executed
                                         *  3:  Media Card Pass Through enabled
                                         *  2:  Media Serial Number Valid
                                         *  1:  SMART selt-test supported
                                         *  0:  SMART error logging
                                         */
        unsigned short  dma_ultra;      /* (word 88) */
        unsigned short  trseuc;         /* time required for security erase */
        unsigned short  trsEuc;         /* time required for enhanced erase */
        unsigned short  CurAPMvalues;   /* current APM values */
        unsigned short  mprc;           /* master password revision code */
        unsigned short  hw_config;      /* hardware config (word 93)
 					 * 15:  Shall be ZERO
                                         * 14:  Shall be ONE
                                         * 13:
                                         * 12:
                                         * 11:
                                         * 10:
                                         *  9:
                                         *  8:
                                         *  7:
                                         *  6:
                                         *  5:
                                         *  4:
                                         *  3:
                                         *  2:
                                         *  1:
                                         *  0:  Shall be ONE
                                         */
        unsigned short  acoustic;       /* (word 94)
                                         * 15:8 Vendor's recommended value
                                         *  7:0 current value
                                         */
        unsigned short  msrqs;          /* min stream request size */
        unsigned short  sxfert;         /* stream transfer time */
        unsigned short  sal;            /* stream access latency */
        unsigned int    spg;            /* stream performance granularity */
        unsigned long long lba_capacity_2;/* 48-bit total number of sectors */
        unsigned short  words104_125[22];/* reserved words 104-125 */
        unsigned short  last_lun;       /* (word 126) */
        unsigned short  word127;        /* (word 127) Feature Set
                                         * Removable Media Notification
                                         * 15:2 reserved
                                         *  1:0 00 = not supported
                                         *      01 = supported
                                         *      10 = reserved
                                         *      11 = reserved
                                         */
        unsigned short  dlf;            /* (word 128)
                                         * device lock function
                                         * 15:9 reserved
                                         *  8   security level 1:max 0:high
                                         *  7:6 reserved
					 *  5   enhanced erase
                                         *  4   expire
                                         *  3   frozen
                                         *  2   locked
                                         *  1   en/disabled
                                         *  0   capability
                                         */
        unsigned short  csfo;           /*  (word 129)
                                         * current set features options
                                         * 15:4 reserved
                                         *  3:  auto reassign
                                         *  2:  reverting
                                         *  1:  read-look-ahead
                                         *  0:  write cache
                                         */
        unsigned short  words130_155[26];/* reserved vendor words 130-155 */
        unsigned short  word156;        /* reserved vendor word 156 */
        unsigned short  words157_159[3];/* reserved vendor words 157-159 */
        unsigned short  cfa_power;      /* (word 160) CFA Power Mode
                                         * 15 word 160 supported
                                         * 14 reserved
                                         * 13
                                         * 12
                                         * 11:0
                                         */
        unsigned short  words161_175[15];/* Reserved for CFA */
        unsigned short  words176_205[30];/* Current Media Serial Number */
        unsigned short  words206_254[49];/* reserved words 206-254 */
        unsigned short  integrity_word; /* (word 255)
                                         * 15:8 Checksum
                                         *  7:0 Signature
                                         */
};

#endif /* __DRIVEID_H__ */

