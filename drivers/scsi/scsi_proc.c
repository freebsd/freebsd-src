/*
 * linux/drivers/scsi/scsi_proc.c
 *
 * The functions in this file provide an interface between
 * the PROC file system and the SCSI device drivers
 * It is mainly used for debugging, statistics and to pass 
 * information directly to the lowlevel driver.
 *
 * (c) 1995 Michael Neuffer neuffer@goofy.zdv.uni-mainz.de 
 * Version: 0.99.8   last change: 95/09/13
 * 
 * generic command parser provided by: 
 * Andreas Heilwagen <crashcar@informatik.uni-koblenz.de>
 *
 * generic_proc_info() support of xxxx_info() by:
 * Michael A. Griffith <grif@acm.org>
 */

#include <linux/config.h>	/* for CONFIG_PROC_FS */
#define __NO_VERSION__
#include <linux/module.h>

#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/blk.h>

#include <asm/uaccess.h>

#include "scsi.h"
#include "hosts.h"

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#ifdef CONFIG_PROC_FS

/* generic_proc_info
 * Used if the driver currently has no own support for /proc/scsi
 */
int generic_proc_info(char *buffer, char **start, off_t offset, int length, 
		      const char *(*info) (struct Scsi_Host *),
		      struct Scsi_Host *sh)
{
	int len, pos, begin;

	begin = 0;
	if (info && sh) {
		pos = len = sprintf(buffer, "%s\n", info(sh));
	} else {
		pos = len = sprintf(buffer,
			"The driver does not yet support the proc-fs\n");
	}
	if (pos < offset) {
		len = 0;
		begin = pos;
	}
	*start = buffer + (offset - begin);	/* Start of wanted data */
	len -= (offset - begin);
	if (len > length)
		len = length;

	return (len);
}

/* dispatch_scsi_info is the central dispatcher 
 * It is the interface between the proc-fs and the SCSI subsystem code
 */
static int proc_scsi_read(char *buffer, char **start, off_t offset,
	int length, int *eof, void *data)
{
	struct Scsi_Host *hpnt = data;
	int n;

	if (hpnt->hostt->proc_info == NULL)
		n = generic_proc_info(buffer, start, offset, length,
				      hpnt->hostt->info, hpnt);
	else
		n = (hpnt->hostt->proc_info(buffer, start, offset,
					   length, hpnt->host_no, 0));
	*eof = (n<length);
	return n;
}

#define PROC_BLOCK_SIZE (3*1024)     /* 4K page size, but our output routines 
				      * use some slack for overruns 
				      */

static int proc_scsi_write(struct file * file, const char * buf,
                           unsigned long count, void *data)
{
	struct Scsi_Host *hpnt = data;
	ssize_t ret = 0;
	char * page;
	char *start;
    
	if (hpnt->hostt->proc_info == NULL)
		ret = -ENOSYS;

	if (count > PROC_BLOCK_SIZE)
		return -EOVERFLOW;

	if (!(page = (char *) __get_free_page(GFP_KERNEL)))
		return -ENOMEM;
	if(copy_from_user(page, buf, count))
	{
		free_page((ulong) page);
		return -EFAULT;
	}

	ret = hpnt->hostt->proc_info(page, &start, 0, count,
				     hpnt->host_no, 1);

	free_page((ulong) page);
	return(ret);
}

void build_proc_dir_entries(Scsi_Host_Template * tpnt)
{
	struct Scsi_Host *hpnt;
	char name[10];	/* see scsi_unregister_host() */

	tpnt->proc_dir = proc_mkdir(tpnt->proc_name, proc_scsi);
        if (!tpnt->proc_dir) {
                printk(KERN_ERR "Unable to proc_mkdir in scsi.c/build_proc_dir_entries");
                return;
        }
	tpnt->proc_dir->owner = tpnt->module;

	hpnt = scsi_hostlist;
	while (hpnt) {
		if (tpnt == hpnt->hostt) {
			struct proc_dir_entry *p;
			sprintf(name,"%d",hpnt->host_no);
			p = create_proc_read_entry(name,
					S_IFREG | S_IRUGO | S_IWUSR,
					tpnt->proc_dir,
					proc_scsi_read,
					(void *)hpnt);
			if (!p)
				panic("Not enough memory to register SCSI HBA in /proc/scsi !\n");
			p->write_proc=proc_scsi_write;
			p->owner = tpnt->module;
		}
		hpnt = hpnt->next;
	}
}

/*
 *  parseHandle *parseInit(char *buf, char *cmdList, int cmdNum); 
 *              gets a pointer to a null terminated data buffer
 *              and a list of commands with blanks as delimiter 
 *      in between. 
 *      The commands have to be alphanumerically sorted. 
 *      cmdNum has to contain the number of commands.
 *              On success, a pointer to a handle structure
 *              is returned, NULL on failure
 *
 *      int parseOpt(parseHandle *handle, char **param);
 *              processes the next parameter. On success, the
 *              index of the appropriate command in the cmdList
 *              is returned, starting with zero.
 *              param points to the null terminated parameter string.
 *              On failure, -1 is returned.
 *
 *      The databuffer buf may only contain pairs of commands
 *          options, separated by blanks:
 *              <Command> <Parameter> [<Command> <Parameter>]*
 */

typedef struct {
	char *buf,		/* command buffer  */
	*cmdList,		/* command list    */
	*bufPos,		/* actual position */
	**cmdPos,		/* cmdList index   */
	 cmdNum;		/* cmd number      */
} parseHandle;

inline int parseFree(parseHandle * handle)
{				/* free memory     */
	kfree(handle->cmdPos);
	kfree(handle);

	return -1;
}

parseHandle *parseInit(char *buf, char *cmdList, int cmdNum)
{
	char *ptr;		/* temp pointer    */
	parseHandle *handle;	/* new handle      */

	if (!buf || !cmdList)	/* bad input ?     */
		return NULL;
	handle = (parseHandle *) kmalloc(sizeof(parseHandle), GFP_KERNEL);
	if (!handle)
		return NULL;	/* out of memory   */
	handle->cmdPos = (char **) kmalloc(sizeof(int) * cmdNum, GFP_KERNEL);
	if (!handle->cmdPos) {
		kfree(handle);
		return NULL;	/* out of memory   */
	}
	handle->buf = handle->bufPos = buf;	/* init handle     */
	handle->cmdList = cmdList;
	handle->cmdNum = cmdNum;

	handle->cmdPos[cmdNum = 0] = cmdList;
	for (ptr = cmdList; *ptr; ptr++) {	/* scan command string */
		if (*ptr == ' ') {	/* and insert zeroes   */
			*ptr++ = 0;
			handle->cmdPos[++cmdNum] = ptr++;
		}
	}
	return handle;
}

int parseOpt(parseHandle * handle, char **param)
{
	int cmdIndex = 0, cmdLen = 0;
	char *startPos;

	if (!handle)		/* invalid handle  */
		return (parseFree(handle));
	/* skip spaces     */
	for (; *(handle->bufPos) && *(handle->bufPos) == ' '; handle->bufPos++);
	if (!*(handle->bufPos))
		return (parseFree(handle));	/* end of data     */

	startPos = handle->bufPos;	/* store cmd start */
	for (; handle->cmdPos[cmdIndex][cmdLen] && *(handle->bufPos); handle->bufPos++) {	/* no string end?  */
		for (;;) {
			if (*(handle->bufPos) == handle->cmdPos[cmdIndex][cmdLen])
				break;	/* char matches ?  */
			else if (memcmp(startPos, (char *) (handle->cmdPos[++cmdIndex]), cmdLen))
				return (parseFree(handle));	/* unknown command */

			if (cmdIndex >= handle->cmdNum)
				return (parseFree(handle));	/* unknown command */
		}

		cmdLen++;	/* next char       */
	}

	/* Get param. First skip all blanks, then insert zero after param  */

	for (; *(handle->bufPos) && *(handle->bufPos) == ' '; handle->bufPos++);
	*param = handle->bufPos;

	for (; *(handle->bufPos) && *(handle->bufPos) != ' '; handle->bufPos++);
	*(handle->bufPos++) = 0;

	return (cmdIndex);
}

void proc_print_scsidevice(Scsi_Device * scd, char *buffer, int *size, int len)
{

	int x, y = *size;
	extern const char *const scsi_device_types[MAX_SCSI_DEVICE_CODE];

	y = sprintf(buffer + len,
	     "Host: scsi%d Channel: %02d Id: %02d Lun: %02d\n  Vendor: ",
		    scd->host->host_no, scd->channel, scd->id, scd->lun);
	for (x = 0; x < 8; x++) {
		if (scd->vendor[x] >= 0x20)
			y += sprintf(buffer + len + y, "%c", scd->vendor[x]);
		else
			y += sprintf(buffer + len + y, " ");
	}
	y += sprintf(buffer + len + y, " Model: ");
	for (x = 0; x < 16; x++) {
		if (scd->model[x] >= 0x20)
			y += sprintf(buffer + len + y, "%c", scd->model[x]);
		else
			y += sprintf(buffer + len + y, " ");
	}
	y += sprintf(buffer + len + y, " Rev: ");
	for (x = 0; x < 4; x++) {
		if (scd->rev[x] >= 0x20)
			y += sprintf(buffer + len + y, "%c", scd->rev[x]);
		else
			y += sprintf(buffer + len + y, " ");
	}
	y += sprintf(buffer + len + y, "\n");

	y += sprintf(buffer + len + y, "  Type:   %s ",
		     scd->type < MAX_SCSI_DEVICE_CODE ?
	       scsi_device_types[(int) scd->type] : "Unknown          ");
	y += sprintf(buffer + len + y, "               ANSI"
		     " SCSI revision: %02x", (scd->scsi_level - 1) ? scd->scsi_level - 1 : 1);
	if (scd->scsi_level == 2)
		y += sprintf(buffer + len + y, " CCS\n");
	else
		y += sprintf(buffer + len + y, "\n");

	*size = y;
	return;
}

#else				/* if !CONFIG_PROC_FS */

void proc_print_scsidevice(Scsi_Device * scd, char *buffer, int *size, int len)
{
}

#endif				/* CONFIG_PROC_FS */
