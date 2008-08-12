/*
 * ioreq.h: I/O request definitions for device models
 * Copyright (c) 2004, Intel Corporation.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _IOREQ_H_
#define _IOREQ_H_

#define IOREQ_READ      1
#define IOREQ_WRITE     0

#define STATE_IOREQ_NONE        0
#define STATE_IOREQ_READY       1
#define STATE_IOREQ_INPROCESS   2
#define STATE_IORESP_READY      3

#define IOREQ_TYPE_PIO          0 /* pio */
#define IOREQ_TYPE_COPY         1 /* mmio ops */
#define IOREQ_TYPE_AND          2
#define IOREQ_TYPE_OR           3
#define IOREQ_TYPE_XOR          4
#define IOREQ_TYPE_XCHG         5
#define IOREQ_TYPE_ADD          6
#define IOREQ_TYPE_TIMEOFFSET   7
#define IOREQ_TYPE_INVALIDATE   8 /* mapcache */
#define IOREQ_TYPE_SUB          9

/*
 * VMExit dispatcher should cooperate with instruction decoder to
 * prepare this structure and notify service OS and DM by sending
 * virq
 */
struct ioreq {
    uint64_t addr;          /*  physical address            */
    uint64_t size;          /*  size in bytes               */
    uint64_t count;         /*  for rep prefixes            */
    uint64_t data;          /*  data (or paddr of data)     */
    uint8_t state:4;
    uint8_t data_is_ptr:1;  /*  if 1, data above is the guest paddr 
                             *   of the real data to use.   */
    uint8_t dir:1;          /*  1=read, 0=write             */
    uint8_t df:1;
    uint8_t type;           /* I/O type                     */
    uint8_t _pad0[6];
    uint64_t io_count;      /* How many IO done on a vcpu   */
};
typedef struct ioreq ioreq_t;

struct vcpu_iodata {
    struct ioreq vp_ioreq;
    /* Event channel port, used for notifications to/from the device model. */
    uint32_t vp_eport;
    uint32_t _pad0;
};
typedef struct vcpu_iodata vcpu_iodata_t;

struct shared_iopage {
    struct vcpu_iodata   vcpu_iodata[1];
};
typedef struct shared_iopage shared_iopage_t;

#define IOREQ_BUFFER_SLOT_NUM     80
struct buffered_iopage {
    unsigned int    read_pointer;
    unsigned int    write_pointer;
    ioreq_t         ioreq[IOREQ_BUFFER_SLOT_NUM];
}; /* NB. Size of this structure must be no greater than one page. */
typedef struct buffered_iopage buffered_iopage_t;

#if defined(__ia64__)
struct pio_buffer {
    uint32_t page_offset;
    uint32_t pointer;
    uint32_t data_end;
    uint32_t buf_size;
    void *opaque;
};

#define PIO_BUFFER_IDE_PRIMARY   0 /* I/O port = 0x1F0 */
#define PIO_BUFFER_IDE_SECONDARY 1 /* I/O port = 0x170 */
#define PIO_BUFFER_ENTRY_NUM     2
struct buffered_piopage {
    struct pio_buffer pio[PIO_BUFFER_ENTRY_NUM];
    uint8_t buffer[1];
};
#endif /* defined(__ia64__) */

#if defined(__i386__) || defined(__x86_64__)
#define ACPI_PM1A_EVT_BLK_ADDRESS           0x0000000000001f40
#define ACPI_PM1A_CNT_BLK_ADDRESS           (ACPI_PM1A_EVT_BLK_ADDRESS + 0x04)
#define ACPI_PM_TMR_BLK_ADDRESS             (ACPI_PM1A_EVT_BLK_ADDRESS + 0x08)
#endif /* defined(__i386__) || defined(__x86_64__) */

#endif /* _IOREQ_H_ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
