/*
 * smccc.h
 *
 * SMC/HVC interface in accordance with SMC Calling Convention.
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
 *
 * Copyright 2017 (C) EPAM Systems
 */

#ifndef __XEN_PUBLIC_ARCH_ARM_SMCCC_H__
#define __XEN_PUBLIC_ARCH_ARM_SMCCC_H__

#include "public/xen.h"

/*
 * Hypervisor Service version.
 *
 * We can't use XEN version here, because of SMCCC requirements:
 * Major revision should change every time SMC/HVC function is removed.
 * Minor revision should change every time SMC/HVC function is added.
 * So, it is SMCCC protocol revision code, not XEN version.
 *
 * Those values are subjected to change, when interface will be extended.
 */
#define XEN_SMCCC_MAJOR_REVISION 0
#define XEN_SMCCC_MINOR_REVISION 1

/* Hypervisor Service UID. Randomly generated with uuidgen. */
#define XEN_SMCCC_UID XEN_DEFINE_UUID(0xa71812dc, 0xc698, 0x4369, 0x9acf, \
                                      0x79, 0xd1, 0x8d, 0xde, 0xe6, 0x67)

/* Standard Service Service Call version. */
#define SSSC_SMCCC_MAJOR_REVISION 0
#define SSSC_SMCCC_MINOR_REVISION 1

/* Standard Service Call UID. Randomly generated with uuidgen. */
#define SSSC_SMCCC_UID XEN_DEFINE_UUID(0xf863386f, 0x4b39, 0x4cbd, 0x9220,\
                                       0xce, 0x16, 0x41, 0xe5, 0x9f, 0x6f)

#endif /* __XEN_PUBLIC_ARCH_ARM_SMCCC_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:b
 */
