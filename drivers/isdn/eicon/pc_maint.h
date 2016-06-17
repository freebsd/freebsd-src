/*
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.0  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef PC_MAINT_H
#define PC_MAINT_H

#if !defined(MIPS_SCOM)
#define BUFFER_SZ  48
#define MAINT_OFFS 0x380
#else
#define BUFFER_SZ  128
#define MAINT_OFFS 0xff00
#endif

#define MIPS_BUFFER_SZ  128
#define MIPS_MAINT_OFFS 0xff00

#define DO_LOG                     1
#define MEMR                    2
#define MEMW                    3
#define IOR                     4
#define IOW                     5
#define B1TEST                  6
#define B2TEST                  7
#define BTESTOFF                8
#define DSIG_STATS              9
#define B_CH_STATS              10
#define D_CH_STATS              11
#define BL1_STATS               12
#define BL1_STATS_C             13
#define GET_VERSION             14
#define OS_STATS                15
#define XLOG_SET_MASK           16
#define XLOG_GET_MASK           17
#define DSP_READ                20
#define DSP_WRITE               21

#define OK 0xff
#define MORE_EVENTS 0xfe
#define NO_EVENT 1

struct DSigStruc
{
  byte Id;
  byte uX;
  byte listen;
  byte active;
  byte sin[3];
  byte bc[6];
  byte llc[6];
  byte hlc[6];
  byte oad[20];
};

struct BL1Struc {
  dword cx_b1;
  dword cx_b2;
  dword cr_b1;
  dword cr_b2;
  dword px_b1;
  dword px_b2;
  dword pr_b1;
  dword pr_b2;
  word er_b1;
  word er_b2;
};

struct L2Struc {
  dword XTotal;
  dword RTotal;
  word XError;
  word RError;
};

struct OSStruc {
  word free_n;
};

typedef union
{
  struct DSigStruc DSigStats;
  struct BL1Struc BL1Stats;
  struct L2Struc L2Stats;
  struct OSStruc OSStats;
  byte   b[BUFFER_SZ];
  word   w[BUFFER_SZ>>1];
  word   l[BUFFER_SZ>>2]; /* word is wrong, do not use! Use 'd' instead. */
  dword  d[BUFFER_SZ>>2];
} BUFFER;

typedef union
{
  struct DSigStruc DSigStats;
  struct BL1Struc BL1Stats;
  struct L2Struc L2Stats;
  struct OSStruc OSStats;
  byte   b[MIPS_BUFFER_SZ];
  word   w[MIPS_BUFFER_SZ>>1];
  word   l[BUFFER_SZ>>2]; /* word is wrong, do not use! Use 'd' instead. */
  dword  d[MIPS_BUFFER_SZ>>2];
} MIPS_BUFFER;


#if !defined(MIPS_SCOM)
struct pc_maint
{
  byte req;
  byte rc;
  byte *mem;  /*far*/
  short length;
  word port;
  byte fill[6];
  BUFFER data;
};
#else
struct pc_maint
{
  byte req;
  byte rc;
  byte reserved[2];     /* R3000 alignment ... */
  byte far *mem;
  short length;
  word port;
  byte fill[4];         /* data at offset 16   */
  BUFFER data;
};
#endif

struct mi_pc_maint
{
  byte req;
  byte rc;
  byte reserved[2];     /* R3000 alignment ... */
  byte *mem; /*far*/
  short length;
  word port;
  byte fill[4];         /* data at offset 16   */
  MIPS_BUFFER data;
};

#endif /* PC_MAINT_H */
