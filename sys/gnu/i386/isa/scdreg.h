/*-
 * Copyright (c) 1995 Mikael Hybsch
 *
 * The Linux driver cdu31a has been used as a reference when writing this
 * code, therefor bringing it under the GNU Public License.  The following
 * conditions of redistribution therefore apply:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef SCD_H
#define	SCD_H

#ifdef __GNUC__
#if __GNUC__ >= 2
#pragma pack(1)
#endif
#endif

typedef unsigned char	bcd_t;
#define	M_msf(msf)	msf[0]
#define	S_msf(msf)	msf[1]
#define	F_msf(msf)	msf[2]

#define	IS_ATTENTION(port)	((inb(port+SCD_STATUS_REG) & SCD_ATTN_BIT) != 0)
#define	IS_BUSY(port)		((inb(port+SCD_STATUS_REG) & SCD_BUSY_BIT) != 0)
#define	IS_DATA_RDY(port)	((inb(port+SCD_STATUS_REG) & SCD_DATA_RDY_BIT) != 0)
#define	STATUS_BIT(port, bit)	((inb(port+SCD_STATUS_REG) & (bit)) != 0)
#define	FIFOST_BIT(port, bit)	((inb(port+SCD_FIFOST_REG) & (bit)) != 0)

#define SCD_CMD_REG     0
#define SCD_PARAM_REG   1
#define SCD_WRITE_REG   2
#define SCD_CONTROL_REG 3
#       define SCD_ATTN_CLR_BIT        0x01
#       define SCD_RES_RDY_CLR_BIT     0x02
#       define SCD_DATA_RDY_CLR_BIT    0x04
#       define SCD_ATTN_INT_EN_BIT     0x08
#       define SCD_RES_RDY_INT_EN_BIT  0x10
#       define SCD_DATA_RDY_INT_EN_BIT 0x20
#       define SCD_PARAM_CLR_BIT       0x40
#       define SCD_DRIVE_RESET_BIT     0x80

#define SCD_STATUS_REG  0
#       define SCD_ATTN_BIT            0x01
#       define SCD_RES_RDY_BIT         0x02
#       define SCD_DATA_RDY_BIT        0x04
#       define SCD_ATTN_INT_ST_BIT     0x08
#       define SCD_RES_RDY_INT_ST_BIT  0x10
#       define SCD_DATA_RDY_INT_ST_BIT 0x20
#       define SCD_DATA_REQUEST_BIT    0x40
#       define SCD_BUSY_BIT            0x80
#define SCD_RESULT_REG  1
#define SCD_READ_REG    2
#define SCD_FIFOST_REG  3
#       define SCD_PARAM_WRITE_RDY_BIT 0x01
#       define SCD_PARAM_REG_EMPTY_BIT 0x02
#       define SCD_RES_REG_NOT_EMP_BIT 0x04
#       define SCD_RES_REG_FULL_BIT    0x08

#define SCD_REQ_DRIVE_CONFIG_CMD       0x00
#define SCD_REQ_DRIVE_MODE_CMD         0x01
#define SCD_REQ_DRIVE_PARAM_CMD        0x02
#define SCD_REQ_MECH_STATUS_CMD        0x03
#define SCD_REQ_AUDIO_STATUS_CMD       0x04
#define SCD_SET_DRIVE_PARAM_CMD        0x10
#define SCD_REQ_TOC_DATA_CMD           0x20
#define SCD_REQ_SUBCODE_ADDRESS_CMD    0x21
#define SCD_REQ_UPC_EAN_CMD            0x22
#define SCD_REQ_ISRC_CMD               0x23
#define SCD_REQ_TOC_DATA_SPEC_CMD      0x24

#define SCD_READ_TOC_CMD               0x30
#define SCD_SEEK_CMD                   0x31
#define SCD_READ_CMD                   0x32
#define SCD_READ_BLKERR_STAT_CMD       0x34
#define SCD_ABORT_CMD                  0x35
#define SCD_READ_TOC_SPEC_CMD          0x36

#define SCD_AUDIO_PLAYBACK_CMD         0x40
#define SCD_AUDIO_STOP_CMD             0x41
#define SCD_AUDIO_SCAN_CMD             0x42

#define SCD_EJECT_CMD                  0x50
#define SCD_SPIN_UP_CMD                0x51
#define SCD_SPIN_DOWN_CMD              0x52

#define SCD_SD_DECODE_PARAM            0x00
#define SCD_SD_INTERFACE_PARAM         0x01
#define SCD_SD_BUFFERING_PARAM         0x02
#define SCD_SD_AUDIO_PARAM             0x03
#define SCD_SD_AUDIO_VOLUME            0x04
#define SCD_SD_MECH_CONTROL            0x05
#define SCD_SD_AUTO_SPIN_DOWN_TIME     0x06

#define SONY_HWC_GET_LOAD_MECH(c)       (c.hw_config[0] & 0x03)
#define SONY_HWC_EJECT(c)               (c.hw_config[0] & 0x04)
#define SONY_HWC_LED_SUPPORT(c)         (c.hw_config[0] & 0x08)
#define SONY_HWC_DOUBLE_SPEED(c)        (c.hw_config[0] & 0x10)
#define SONY_HWC_GET_BUF_MEM_SIZE(c)    ((c.hw_config[0] & 0xc0) >> 6)
#define SONY_HWC_AUDIO_PLAYBACK(c)      (c.hw_config[1] & 0x01)
#define SONY_HWC_ELECTRIC_VOLUME(c)     (c.hw_config[1] & 0x02)
#define SONY_HWC_ELECTRIC_VOLUME_CTL(c) (c.hw_config[1] & 0x04)

#define SONY_HWC_CADDY_LOAD_MECH        0x00
#define SONY_HWC_TRAY_LOAD_MECH         0x01
#define SONY_HWC_POPUP_LOAD_MECH        0x02
#define SONY_HWC_UNKWN_LOAD_MECH        0x03

#define SONY_HWC_8KB_BUFFER             0x00
#define SONY_HWC_32KB_BUFFER            0x01
#define SONY_HWC_64KB_BUFFER            0x02
#define SONY_HWC_UNKWN_BUFFER           0x03

/*
 * The following are errors returned from the drive.
 */

#define SONY_ILL_CMD_ERR                0x10
#define SONY_ILL_PARAM_ERR              0x11

#define SONY_NOT_LOAD_ERR               0x20
#define SONY_NO_DISK_ERR                0x21
#define SONY_NOT_SPIN_ERR               0x22
#define SONY_SPIN_ERR                   0x23
#define SONY_SPINDLE_SERVO_ERR          0x25
#define SONY_FOCUS_SERVO_ERR            0x26
#define SONY_EJECT_MECH_ERR             0x29
#define SONY_AUDIO_PLAYING_ERR          0x2a
#define SONY_EMERGENCY_EJECT_ERR        0x2c

#define SONY_FOCUS_ERR                  0x30
#define SONY_FRAME_SYNC_ERR             0x31
#define SONY_SUBCODE_ADDR_ERR           0x32
#define SONY_BLOCK_SYNC_ERR             0x33
#define SONY_HEADER_ADDR_ERR            0x34

#define SONY_ILL_TRACK_R_ERR            0x40
#define SONY_MODE_0_R_ERR               0x41
#define SONY_ILL_MODE_R_ERR             0x42
#define SONY_ILL_BLOCK_SIZE_R_ERR       0x43
#define SONY_MODE_R_ERR                 0x44
#define SONY_FORM_R_ERR                 0x45
#define SONY_LEAD_OUT_R_ERR             0x46
#define SONY_BUFFER_OVERRUN_R_ERR       0x47

#define SONY_UNREC_CIRC_ERR             0x53
#define SONY_UNREC_LECC_ERR             0x57

#define SONY_NO_TOC_ERR                 0x60
#define SONY_SUBCODE_DATA_NVAL_ERR      0x61
#define SONY_FOCUS_ON_TOC_READ_ERR      0x63
#define SONY_FRAME_SYNC_ON_TOC_READ_ERR 0x64
#define SONY_TOC_DATA_ERR               0x65

#define SONY_HW_FAILURE_ERR             0x70
#define SONY_LEAD_IN_A_ERR              0x91
#define SONY_LEAD_OUT_A_ERR             0x92
#define SONY_DATA_TRACK_A_ERR           0x93

#define SONY_NO_CIRC_ERR_BLK_STAT       0x50
#define SONY_NO_LECC_ERR_BLK_STAT       0x54
#define SONY_RECOV_LECC_ERR_BLK_STAT    0x55
#define SONY_NO_ERR_DETECTION_STAT      0x59

#define SONY_EMER_EJECT_ATTN            0x2c
#define SONY_HW_FAILURE_ATTN            0x70
#define SONY_MECH_LOADED_ATTN           0x80
#define SONY_EJECT_PUSHED_ATTN          0x81

#define SONY_AUDIO_PLAY_DONE_ATTN       0x90
#define SONY_LEAD_IN_ERR_ATTN           0x91
#define SONY_LEAD_OUT_ERR_ATTN          0x92
#define SONY_DATA_TRACK_ERR_ATTN        0x93
#define SONY_AUDIO_PLAYBACK_ERR_ATTN    0x94

#define SONY_SPIN_UP_COMPLETE_ATTN      0x24
#define SONY_SPINDLE_SERVO_ERR_ATTN     0x25
#define SONY_FOCUS_SERVO_ERR_ATTN       0x26
#define SONY_TOC_READ_DONE_ATTN         0x62
#define SONY_FOCUS_ON_TOC_READ_ERR_ATTN 0x63
#define SONY_SYNC_ON_TOC_READ_ERR_ATTN  0x65

#define SONY_SPIN_DOWN_COMPLETE_ATTN    0x27
#define SONY_EJECT_COMPLETE_ATTN        0x28
#define SONY_EJECT_MECH_ERR_ATTN        0x29


struct s_sony_drive_config
{
   unsigned char exec_status[2];
   char vendor_id[8];
   char product_id[16];
   char product_rev_level[8];
   unsigned char hw_config[2];
};

struct s_sony_subcode
{
   unsigned char exec_status[2];
   unsigned char address        :4;
   unsigned char control        :4;
   unsigned char track_num;
   unsigned char index_num;
   unsigned char rel_msf[3];
   unsigned char reserved1;
   unsigned char abs_msf[3];
};

#define MAX_TRACKS 100

struct s_sony_session_toc
{
   unsigned char exec_status[2];
   unsigned char session_number;
   unsigned char address0       :4;
   unsigned char control0       :4;
   unsigned char point0;
   unsigned char first_track_num;
   unsigned char disk_type;
   unsigned char dummy0;
   unsigned char address1       :4;
   unsigned char control1       :4;
   unsigned char point1;
   unsigned char last_track_num;
   unsigned char dummy1;
   unsigned char dummy2;
   unsigned char address2       :4;
   unsigned char control2       :4;
   unsigned char point2;
   unsigned char lead_out_start_msf[3];
   unsigned char addressb0      :4;
   unsigned char controlb0      :4;
   unsigned char pointb0;
   unsigned char next_poss_prog_area_msf[3];
   unsigned char num_mode_5_pointers;
   unsigned char max_start_outer_leadout_msf[3];
   unsigned char addressb1      :4;
   unsigned char controlb1      :4;
   unsigned char pointb1;
   unsigned char dummyb0_1[4];
   unsigned char num_skip_interval_pointers;
   unsigned char num_skip_track_assignments;
   unsigned char dummyb0_2;
   unsigned char addressb2      :4;
   unsigned char controlb2      :4;
   unsigned char pointb2;
   unsigned char tracksb2[7];
   unsigned char addressb3      :4;
   unsigned char controlb3      :4;
   unsigned char pointb3;
   unsigned char tracksb3[7];
   unsigned char addressb4      :4;
   unsigned char controlb4      :4;
   unsigned char pointb4;
   unsigned char tracksb4[7];
   unsigned char addressc0      :4;
   unsigned char controlc0      :4;
   unsigned char pointc0;
   unsigned char dummyc0[7];
   struct s_sony_tracklist
   {
      unsigned char address     :4;
      unsigned char control     :4;
      unsigned char track;
      unsigned char track_start_msf[3];
   } tracks[MAX_TRACKS];
};

#endif /* SCD_H */
