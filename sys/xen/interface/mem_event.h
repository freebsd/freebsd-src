/******************************************************************************
 * mem_event.h
 *
 * Memory event common structures.
 *
 * Copyright (c) 2009 by Citrix Systems, Inc. (Patrick Colp)
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

#ifndef _XEN_PUBLIC_MEM_EVENT_H
#define _XEN_PUBLIC_MEM_EVENT_H

#include "xen.h"
#include "io/ring.h"

/* Memory event flags */
#define MEM_EVENT_FLAG_VCPU_PAUSED  (1 << 0)
#define MEM_EVENT_FLAG_DROP_PAGE    (1 << 1)
#define MEM_EVENT_FLAG_EVICT_FAIL   (1 << 2)
#define MEM_EVENT_FLAG_FOREIGN      (1 << 3)
#define MEM_EVENT_FLAG_DUMMY        (1 << 4)

/* Reasons for the memory event request */
#define MEM_EVENT_REASON_UNKNOWN     0    /* typical reason */
#define MEM_EVENT_REASON_VIOLATION   1    /* access violation, GFN is address */
#define MEM_EVENT_REASON_CR0         2    /* CR0 was hit: gfn is CR0 value */
#define MEM_EVENT_REASON_CR3         3    /* CR3 was hit: gfn is CR3 value */
#define MEM_EVENT_REASON_CR4         4    /* CR4 was hit: gfn is CR4 value */
#define MEM_EVENT_REASON_INT3        5    /* int3 was hit: gla/gfn are RIP */
#define MEM_EVENT_REASON_SINGLESTEP  6    /* single step was invoked: gla/gfn are RIP */

typedef struct mem_event_st {
    uint32_t flags;
    uint32_t vcpu_id;

    uint64_t gfn;
    uint64_t offset;
    uint64_t gla; /* if gla_valid */

    uint32_t p2mt;

    uint16_t access_r:1;
    uint16_t access_w:1;
    uint16_t access_x:1;
    uint16_t gla_valid:1;
    uint16_t available:12;

    uint16_t reason;
} mem_event_request_t, mem_event_response_t;

DEFINE_RING_TYPES(mem_event, mem_event_request_t, mem_event_response_t);

#endif

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
