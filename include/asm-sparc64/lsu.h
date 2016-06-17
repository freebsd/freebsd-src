/* $Id: lsu.h,v 1.2 1997/04/04 00:50:22 davem Exp $ */
#ifndef _SPARC64_LSU_H
#define _SPARC64_LSU_H

/* LSU Control Register */
#define LSU_CONTROL_PM		0x000001fe00000000 /* Phys-watchpoint byte mask     */
#define LSU_CONTROL_VM		0x00000001fe000000 /* Virt-watchpoint byte mask     */
#define LSU_CONTROL_PR		0x0000000001000000 /* Phys-read watchpoint enable   */
#define LSU_CONTROL_PW		0x0000000000800000 /* Phys-write watchpoint enable  */
#define LSU_CONTROL_VR		0x0000000000400000 /* Virt-read watchpoint enable   */
#define LSU_CONTROL_VW		0x0000000000200000 /* Virt-write watchpoint enable  */
#define LSU_CONTROL_FM		0x00000000000ffff0 /* Parity mask enables.          */
#define LSU_CONTROL_DM		0x0000000000000008 /* Data MMU enable.              */
#define LSU_CONTROL_IM		0x0000000000000004 /* Instruction MMU enable.       */
#define LSU_CONTROL_DC		0x0000000000000002 /* Data cache enable.            */
#define LSU_CONTROL_IC		0x0000000000000001 /* Instruction cache enable.     */

#endif /* !(_SPARC64_LSU_H) */
