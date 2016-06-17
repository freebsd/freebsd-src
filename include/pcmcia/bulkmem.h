/*
 * Definitions for bulk memory services
 *
 * bulkmem.h 1.12 2000/06/12 21:55:41
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License. 
 *
 * The initial developer of the original code is David A. Hinds
 * <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
 * are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the MPL or the GPL.
 * bulkmem.h 1.3 1995/05/27 04:49:49
 */

#ifndef _LINUX_BULKMEM_H
#define _LINUX_BULKMEM_H

/* For GetFirstRegion and GetNextRegion */
typedef struct region_info_t {
    u_int		Attributes;
    u_int		CardOffset;
    u_int		RegionSize;
    u_int		AccessSpeed;
    u_int		BlockSize;
    u_int		PartMultiple;
    u_char		JedecMfr, JedecInfo;
    memory_handle_t	next;
} region_info_t;

#define REGION_TYPE		0x0001
#define REGION_TYPE_CM		0x0000
#define REGION_TYPE_AM		0x0001
#define REGION_PREFETCH		0x0008
#define REGION_CACHEABLE	0x0010
#define REGION_BAR_MASK		0xe000
#define REGION_BAR_SHIFT	13

/* For OpenMemory */
typedef struct open_mem_t {
    u_int		Attributes;
    u_int		Offset;
} open_mem_t;

/* Attributes for OpenMemory */
#define MEMORY_TYPE		0x0001
#define MEMORY_TYPE_CM		0x0000
#define MEMORY_TYPE_AM		0x0001
#define MEMORY_EXCLUSIVE	0x0002
#define MEMORY_PREFETCH		0x0008
#define MEMORY_CACHEABLE	0x0010
#define MEMORY_BAR_MASK		0xe000
#define MEMORY_BAR_SHIFT	13

typedef struct eraseq_entry_t {
    memory_handle_t	Handle;
    u_char		State;
    u_int		Size;
    u_int		Offset;
    void		*Optional;
} eraseq_entry_t;

typedef struct eraseq_hdr_t {
    int			QueueEntryCnt;
    eraseq_entry_t	*QueueEntryArray;
} eraseq_hdr_t;

#define ERASE_QUEUED		0x00
#define ERASE_IN_PROGRESS(n)	(((n) > 0) && ((n) < 0x80))
#define ERASE_IDLE		0xff
#define ERASE_PASSED		0xe0
#define ERASE_FAILED		0xe1

#define ERASE_MISSING		0x80
#define ERASE_MEDIA_WRPROT	0x84
#define ERASE_NOT_ERASABLE	0x85
#define ERASE_BAD_OFFSET	0xc1
#define ERASE_BAD_TECH		0xc2
#define ERASE_BAD_SOCKET	0xc3
#define ERASE_BAD_VCC		0xc4
#define ERASE_BAD_VPP		0xc5
#define ERASE_BAD_SIZE		0xc6

/* For CopyMemory */
typedef struct copy_op_t {
    u_int		Attributes;
    u_int		SourceOffset;
    u_int		DestOffset;
    u_int		Count;
} copy_op_t;

/* For ReadMemory and WriteMemory */
typedef struct mem_op_t {
    u_int	Attributes;
    u_int	Offset;
    u_int	Count;
} mem_op_t;

#define MEM_OP_BUFFER		0x01
#define MEM_OP_BUFFER_USER	0x00
#define MEM_OP_BUFFER_KERNEL	0x01
#define MEM_OP_DISABLE_ERASE	0x02
#define MEM_OP_VERIFY		0x04

/* For RegisterMTD */
typedef struct mtd_reg_t {
    u_int	Attributes;
    u_int	Offset;
    u_long	MediaID;
} mtd_reg_t;

/*
 *  Definitions for MTD requests
 */

typedef struct mtd_request_t {
    u_int	SrcCardOffset;
    u_int	DestCardOffset;
    u_int	TransferLength;
    u_int	Function;
    u_long	MediaID;
    u_int	Status;
    u_int	Timeout;
} mtd_request_t;

/* Fields in MTD Function */
#define MTD_REQ_ACTION		0x003
#define MTD_REQ_ERASE		0x000
#define MTD_REQ_READ		0x001
#define MTD_REQ_WRITE		0x002
#define MTD_REQ_COPY		0x003
#define MTD_REQ_NOERASE		0x004
#define MTD_REQ_VERIFY		0x008
#define MTD_REQ_READY		0x010
#define MTD_REQ_TIMEOUT		0x020
#define MTD_REQ_LAST		0x040
#define MTD_REQ_FIRST		0x080
#define MTD_REQ_KERNEL		0x100

/* Status codes */
#define MTD_WAITREQ	0x00
#define MTD_WAITTIMER	0x01
#define MTD_WAITRDY	0x02
#define MTD_WAITPOWER	0x03

/*
 *  Definitions for MTD helper functions
 */

/* For MTDModifyWindow */
typedef struct mtd_mod_win_t {
    u_int	Attributes;
    u_int	AccessSpeed;
    u_int	CardOffset;
} mtd_mod_win_t;

/* For MTDSetVpp */
typedef struct mtd_vpp_req_t {
    u_char	Vpp1, Vpp2;
} mtd_vpp_req_t;

/* For MTDRDYMask */
typedef struct mtd_rdy_req_t {
    u_int	Mask;
} mtd_rdy_req_t;

enum mtd_helper {
    MTDRequestWindow, MTDModifyWindow, MTDReleaseWindow,
    MTDSetVpp, MTDRDYMask
};

#ifdef IN_CARD_SERVICES
extern int MTDHelperEntry(int func, void *a1, void *a2);
#else
extern int MTDHelperEntry(int func, ...);
#endif

int pcmcia_get_first_region(client_handle_t handle, region_info_t *rgn);
int pcmcia_get_next_region(client_handle_t handle, region_info_t *rgn);
int pcmcia_register_mtd(client_handle_t handle, mtd_reg_t *reg);
int pcmcia_register_erase_queue(client_handle_t *handle, eraseq_hdr_t *header, eraseq_handle_t *e);
int pcmcia_deregister_erase_queue(eraseq_handle_t eraseq);
int pcmcia_check_erase_queue(eraseq_handle_t eraseq);
int pcmcia_open_memory(client_handle_t *handle, open_mem_t *open, memory_handle_t *m);
int pcmcia_close_memory(memory_handle_t handle);
int pcmcia_read_memory(memory_handle_t handle, mem_op_t *req, caddr_t buf);
int pcmcia_write_memory(memory_handle_t handle, mem_op_t *req, caddr_t buf);
int pcmcia_copy_memory(memory_handle_t handle, copy_op_t *req);

#endif /* _LINUX_BULKMEM_H */
