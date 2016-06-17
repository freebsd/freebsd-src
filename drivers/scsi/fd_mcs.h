/* fd_mcs.h -- Header for Future Domain MCS 600/700 (or IBM OEM) driver
 * 
 * fd_mcs.h v0.2 03/11/1998 ZP Gu (zpg@castle.net)
 *

 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.

 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.

 */

#ifndef _FD_MCS_H
#define _FD_MCS_H

extern int fd_mcs_detect( Scsi_Host_Template * );
extern int fd_mcs_release( struct Scsi_Host * );
extern int fd_mcs_command( Scsi_Cmnd * );
extern int fd_mcs_abort( Scsi_Cmnd * );
extern int fd_mcs_reset( Scsi_Cmnd *, unsigned int );
extern int fd_mcs_queue( Scsi_Cmnd *, void (*done)(Scsi_Cmnd *) );
extern int fd_mcs_biosparam( Disk *, kdev_t, int * );
extern int fd_mcs_proc_info( char *, char **, off_t, int, int, int );
extern const char *fd_mcs_info(struct Scsi_Host *);

#define FD_MCS {\
                    proc_name:      "fd_mcs",                   \
                    proc_info:      fd_mcs_proc_info,           \
		    detect:         fd_mcs_detect,              \
		    release:        fd_mcs_release,             \
		    info:           fd_mcs_info,                \
		    command:        fd_mcs_command,             \
		    queuecommand:   fd_mcs_queue,               \
		    abort:          fd_mcs_abort,               \
		    reset:          fd_mcs_reset,               \
		    bios_param:     fd_mcs_biosparam,           \
		    can_queue:      1, 				\
		    this_id:        7, 				\
		    sg_tablesize:   64, 			\
		    cmd_per_lun:    1, 				\
		    use_clustering: DISABLE_CLUSTERING }

#endif /* _FD_MCS_H */
