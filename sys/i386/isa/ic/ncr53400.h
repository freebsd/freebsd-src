/*
 * Definitions for 53C400 SCSI-controller chip.
 *
 * Derived from Linux NCR-5380 generic driver sources (by Drew Eckhardt).
 *
 * Copyright (C) 1994 Serge Vakulenko (vak@cronyx.ru)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPERS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _IC_NCR_53C400_H_
#define _IC_NCR_53C400_H_

#define C400_CSR                0       /* rw - Control and Status Reg. */
#define CSR_5380_ENABLE                 0x80
#define CSR_TRANSFER_DIRECTION          0x40
#define CSR_TRANSFER_READY_INTR         0x20
#define CSR_5380_INTR                   0x10
#define CSR_SHARED_INTR                 0x08
#define CSR_HOST_BUF_NOT_READY          0x04 /* read only */
#define CSR_SCSI_BUF_READY              0x02 /* read only */
#define CSR_5380_GATED_IRQ              0x01 /* read only */
#define CSR_BITS "\20\1irq\2sbrdy\3hbrdy\4shintr\5intr\6tintr\7tdir\10enable"

#define C400_CCR                1       /* rw - Clock Counter Reg. */
#define C400_HBR                4       /* rw - Host Buffer Reg. */

#define C400_5380_REG_OFFSET    8       /* Offset of 5380 registers. */

#endif /* _IC_NCR_53C400_H_ */
