#ifndef _GDTH_PROC_H
#define _GDTH_PROC_H

/* gdth_proc.h 
 * $Id: gdth_proc.h,v 1.11 2001/07/25 15:37:40 achim Exp $
 */

static int gdth_set_info(char *buffer,int length,int vh,int hanum,int busnum);
static int gdth_get_info(char *buffer,char **start,off_t offset,
                         int length,int vh,int hanum,int busnum);

#if LINUX_VERSION_CODE >= 0x020322
static int gdth_set_asc_info(char *buffer,int length,int hanum,Scsi_Cmnd *scp);
static int gdth_set_bin_info(char *buffer,int length,int hanum,Scsi_Cmnd *scp);
#else 
static int gdth_set_asc_info(char *buffer,int length,int hanum,Scsi_Cmnd scp);
static int gdth_set_bin_info(char *buffer,int length,int hanum,Scsi_Cmnd scp);
#endif
static void gdth_do_cmd(Scsi_Cmnd *scp, gdth_cmd_str *cmd, 
                        char *cmnd, int timeout);

static char *gdth_ioctl_alloc(int hanum, ushort size, int scratch);
static void gdth_ioctl_free(int hanum, char *buf);
static int gdth_ioctl_check_bin(int hanum, ushort size);
static void gdth_wait_completion(int hanum, int busnum, int id);
static void gdth_stop_timeout(int hanum, int busnum, int id);
static void gdth_start_timeout(int hanum, int busnum, int id);
static int gdth_update_timeout(int hanum, Scsi_Cmnd *scp, int timeout);

void gdth_scsi_done(Scsi_Cmnd *scp);

#endif

