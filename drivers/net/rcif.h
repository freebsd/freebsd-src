/*
** *************************************************************************
**
**
**     R C I F . H
**
**
**  RedCreek InterFace include file.
**
**  ---------------------------------------------------------------------
**  ---     Copyright (c) 1998-1999, RedCreek Communications Inc.     ---
**  ---                   All rights reserved.                        ---
**  ---------------------------------------------------------------------
**
** File Description:
**
** Header file private ioctl commands.
**
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.

**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.

**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
** *************************************************************************
*/

#ifndef RCIF_H
#define RCIF_H

/* The following protocol revision # should be incremented every time
   a new protocol or new structures are used in this file. */
int USER_PROTOCOL_REV = 2;	/* used to track different protocol revisions */

/* define a single TCB & buffer */
typedef struct {		/* a single buffer */
	U32 context;		/* context */
	U32 scount;		/* segment count */
	U32 size;		/* segment size */
	U32 addr;		/* segment physical address */
} __attribute__ ((packed))
    singleB, *psingleB;
typedef struct {		/* a single TCB */
	/*
	   **  +-----------------------+
	   **  |         1             |  one buffer in the TCB
	   **  +-----------------------+
	   **  |  <user's Context>     |  user's buffer reference
	   **  +-----------------------+
	   **  |         1             |  one segment buffer
	   **  +-----------------------+                            _
	   **  |    <buffer size>      |  size                       \ 
	   **  +-----------------------+                              \ segment descriptor
	   **  |  <physical address>   |  physical address of buffer  /
	   **  +-----------------------+                            _/
	 */
	U32 bcount;		/* buffer count */
	singleB b;		/* buffer */

} __attribute__ ((packed))
    singleTCB, *psingleTCB;

/*
   When adding new entries, please add all 5 related changes, since 
   it helps keep everything consistent:
      1) User structure entry
      2) User data entry
      3) Structure short-cut entry
      4) Data short-cut entry
      5) Command identifier entry

   For Example ("GETSPEED"):
      1) struct  RCgetspeed_tag { U32 LinkSpeedCode; } RCgetspeed;
      2) struct  RCgetspeed_tag *getspeed;
      3) #define RCUS_GETSPEED  data.RCgetspeed;
      4) #define RCUD_GETSPEED  _RC_user_data.getspeed
      5) #define RCUC_GETSPEED  0x02
  
   Notes for the "GETSPEED" entry, above:
      1) RCgetspeed      - RC{name}
         RCgetspeed_tag  - RC{name}_tag
         LinkSpeedCode   - create any structure format desired (not too large,
                           since memory will be unioned with all other entries)
      2) RCgetspeed_tag  - RC{name}_tag chosen in #1
         getspeed        - arbitrary name (ptr to structure in #1)
      3) RCUS_GETSPEED   - RCUS_{NAME}   ("NAME" & "name" do not have to the same)
         data.RCgetspeed - data.RC{name}  ("RC{name}" from #1)
      4) RCUD_GETSPEED   - _RC_user_data.getspeed  ("getspeed" from #2)
      5) RCUC_GETSPEED   - unique hex identifier entry.
*/

typedef struct RC_user_tag RCuser_struct;

/* 1) User structure entry */
struct RC_user_tag {
	int cmd;
	union {
		/* GETINFO structure */
		struct RCgetinfo_tag {
			unsigned long int mem_start;
			unsigned long int mem_end;
			unsigned long int base_addr;
			unsigned char irq;
			unsigned char dma;
			unsigned char port;
		} RCgetinfo;	/* <---- RCgetinfo */

		/* GETSPEED structure */
		struct RCgetspeed_tag {
			U32 LinkSpeedCode;
		} RCgetspeed;	/* <---- RCgetspeed */

		/* SETSPEED structure */
		struct RCsetspeed_tag {
			U16 LinkSpeedCode;
		} RCsetspeed;	/* <---- RCsetspeed */

		/* GETPROM structure */
		struct RCgetprom_tag {
			U32 PromMode;
		} RCgetprom;	/* <---- RCgetprom */

		/* SETPROM structure */
		struct RCsetprom_tag {
			U16 PromMode;
		} RCsetprom;	/* <---- RCsetprom */

		/* GETBROADCAST structure */
		struct RCgetbroadcast_tag {
			U32 BroadcastMode;
		} RCgetbroadcast;	/* <---- RCgetbroadcast */

		/* SETBROADCAST structure */
		struct RCsetbroadcast_tag {
			U16 BroadcastMode;
		} RCsetbroadcast;	/* <---- RCsetbroadcast */

		/* GETFIRMWAREVER structure */
#define FirmStringLen 80
		struct RCgetfwver_tag {
			U8 FirmString[FirmStringLen];
		} RCgetfwver;	/* <---- RCgetfwver */

		/* GETIPANDMASK structure */
		struct RCgetipnmask_tag {
			U32 IpAddr;
			U32 NetMask;
		} RCgetipandmask;	/* <---- RCgetipandmask */

		/* SETIPANDMASK structure */
		struct RCsetipnmask_tag {
			U32 IpAddr;
			U32 NetMask;
		} RCsetipandmask;	/* <---- RCsetipandmask */

		/* GETMAC structure */
#define MAC_SIZE 10
		struct RCgetmac_tag {
			U8 mac[MAC_SIZE];
		} RCgetmac;	/* <---- RCgetmac */

		/* SETMAC structure */
		struct RCsetmac_tag {
			U8 mac[MAC_SIZE];
		} RCsetmac;	/* <---- RCsetmac */

		/* GETLINKSTATUS structure */
		struct RCgetlnkstatus_tag {
			U32 ReturnStatus;
		} RCgetlnkstatus;	/* <---- RCgetlnkstatus */

		/* GETLINKSTATISTICS structure */
		struct RCgetlinkstats_tag {
			RCLINKSTATS StatsReturn;
		} RCgetlinkstats;	/* <---- RCgetlinkstats */

		/* DEFAULT structure (when no command was recognized) */
		struct RCdefault_tag {
			int rc;
		} RCdefault;	/* <---- RCdefault */

	} data;

};				/* struct RC_user_tag { ... } */

/* 2) User data entry */
/* RCUD = RedCreek User Data */
union RC_user_data_tag {	/* structure tags used are taken from RC_user_tag structure above */
	struct RCgetinfo_tag *getinfo;
	struct RCgetspeed_tag *getspeed;
	struct RCgetprom_tag *getprom;
	struct RCgetbroadcast_tag *getbroadcast;
	struct RCgetfwver_tag *getfwver;
	struct RCgetipnmask_tag *getipandmask;
	struct RCgetmac_tag *getmac;
	struct RCgetlnkstatus_tag *getlinkstatus;
	struct RCgetlinkstats_tag *getlinkstatistics;
	struct RCdefault_tag *rcdefault;
	struct RCsetspeed_tag *setspeed;
	struct RCsetprom_tag *setprom;
	struct RCsetbroadcast_tag *setbroadcast;
	struct RCsetipnmask_tag *setipandmask;
	struct RCsetmac_tag *setmac;
} _RC_user_data;		/* declare as a global, so the defines below will work */

/* 3) Structure short-cut entry */
/* define structure short-cuts *//* structure names are taken from RC_user_tag structure above */
#define RCUS_GETINFO           data.RCgetinfo;
#define RCUS_GETSPEED          data.RCgetspeed;
#define RCUS_GETPROM           data.RCgetprom;
#define RCUS_GETBROADCAST      data.RCgetbroadcast;
#define RCUS_GETFWVER          data.RCgetfwver;
#define RCUS_GETIPANDMASK      data.RCgetipandmask;
#define RCUS_GETMAC            data.RCgetmac;
#define RCUS_GETLINKSTATUS     data.RCgetlnkstatus;
#define RCUS_GETLINKSTATISTICS data.RCgetlinkstats;
#define RCUS_DEFAULT           data.RCdefault;
#define RCUS_SETSPEED          data.RCsetspeed;
#define RCUS_SETPROM           data.RCsetprom;
#define RCUS_SETBROADCAST      data.RCsetbroadcast;
#define RCUS_SETIPANDMASK      data.RCsetipandmask;
#define RCUS_SETMAC            data.RCsetmac;

/* 4) Data short-cut entry */
/* define data short-cuts *//* pointer names are from RC_user_data_tag union (just below RC_user_tag) */
#define RCUD_GETINFO           _RC_user_data.getinfo
#define RCUD_GETSPEED          _RC_user_data.getspeed
#define RCUD_GETPROM           _RC_user_data.getprom
#define RCUD_GETBROADCAST      _RC_user_data.getbroadcast
#define RCUD_GETFWVER          _RC_user_data.getfwver
#define RCUD_GETIPANDMASK      _RC_user_data.getipandmask
#define RCUD_GETMAC            _RC_user_data.getmac
#define RCUD_GETLINKSTATUS     _RC_user_data.getlinkstatus
#define RCUD_GETLINKSTATISTICS _RC_user_data.getlinkstatistics
#define RCUD_DEFAULT           _RC_user_data.rcdefault
#define RCUD_SETSPEED          _RC_user_data.setspeed
#define RCUD_SETPROM           _RC_user_data.setprom
#define RCUD_SETBROADCAST      _RC_user_data.setbroadcast
#define RCUD_SETIPANDMASK      _RC_user_data.setipandmask
#define RCUD_SETMAC            _RC_user_data.setmac

/* 5) Command identifier entry */
/* define command identifiers */
#define RCUC_GETINFO            0x01
#define RCUC_GETSPEED           0x02
#define RCUC_GETFWVER           0x03
#define RCUC_GETIPANDMASK       0x04
#define RCUC_GETMAC             0x05
#define RCUC_GETLINKSTATUS      0x06
#define RCUC_GETLINKSTATISTICS  0x07
#define RCUC_GETPROM            0x14
#define RCUC_GETBROADCAST       0x15
#define RCUC_DEFAULT            0xff
#define RCUC_SETSPEED           0x08
#define RCUC_SETIPANDMASK       0x09
#define RCUC_SETMAC             0x0a
#define RCUC_SETPROM            0x16
#define RCUC_SETBROADCAST       0x17

/* define ioctl commands to use, when talking to RC 45/PCI driver */
#define RCU_PROTOCOL_REV         SIOCDEVPRIVATE
#define RCU_COMMAND              SIOCDEVPRIVATE+1

/*
   Intended use for the above defines is shown below (GETINFO, as this example):

      RCuser_struct RCuser;           // declare RCuser structure
      struct ifreq ifr;               // declare an interface request structure

      RCuser.cmd = RCUC_GETINFO;           // set user command to GETINFO
      ifr->ifr_data = (caddr_t) &RCuser;   // set point to user structure

      sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);   // get a socket
      ioctl(sock, RCU_COMMAND, &ifr);                  // do ioctl on socket

      RCUD_GETINFO = &RCuser.RCUS_GETINFO;   // set data pointer for GETINFO

      // print results
      printf("memory 0x%lx-0x%lx, base address 0x%x, irq 0x%x\n",
              RCUD_GETINFO->mem_start, RCUD_GETINFO->mem_end,
              RCUD_GETINFO->base_addr, RCUD_GETINFO->irq);
*/

#endif				/* RCIF_H */
