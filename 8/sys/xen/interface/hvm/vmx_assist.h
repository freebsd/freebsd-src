/*
 * vmx_assist.h: Context definitions for the VMXASSIST world switch.
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
 * Leendert van Doorn, leendert@watson.ibm.com
 * Copyright (c) 2005, International Business Machines Corporation.
 */

#ifndef _VMX_ASSIST_H_
#define _VMX_ASSIST_H_

#define VMXASSIST_BASE         0xD0000
#define VMXASSIST_MAGIC        0x17101966
#define VMXASSIST_MAGIC_OFFSET (VMXASSIST_BASE+8)

#define VMXASSIST_NEW_CONTEXT (VMXASSIST_BASE + 12)
#define VMXASSIST_OLD_CONTEXT (VMXASSIST_NEW_CONTEXT + 4)

#ifndef __ASSEMBLY__

#define NR_EXCEPTION_HANDLER    32
#define NR_INTERRUPT_HANDLERS   16
#define NR_TRAPS        (NR_EXCEPTION_HANDLER+NR_INTERRUPT_HANDLERS)

union vmcs_arbytes {
    struct arbyte_fields {
        unsigned int seg_type : 4,
            s         : 1,
            dpl       : 2,
            p         : 1,
            reserved0 : 4,
            avl       : 1,
            reserved1 : 1,
            default_ops_size: 1,
            g         : 1,
            null_bit  : 1,
            reserved2 : 15;
    } fields;
    unsigned int bytes;
};

/*
 * World switch state
 */
struct vmx_assist_context {
    uint32_t  eip;        /* execution pointer */
    uint32_t  esp;        /* stack pointer */
    uint32_t  eflags;     /* flags register */
    uint32_t  cr0;
    uint32_t  cr3;        /* page table directory */
    uint32_t  cr4;
    uint32_t  idtr_limit; /* idt */
    uint32_t  idtr_base;
    uint32_t  gdtr_limit; /* gdt */
    uint32_t  gdtr_base;
    uint32_t  cs_sel;     /* cs selector */
    uint32_t  cs_limit;
    uint32_t  cs_base;
    union vmcs_arbytes cs_arbytes;
    uint32_t  ds_sel;     /* ds selector */
    uint32_t  ds_limit;
    uint32_t  ds_base;
    union vmcs_arbytes ds_arbytes;
    uint32_t  es_sel;     /* es selector */
    uint32_t  es_limit;
    uint32_t  es_base;
    union vmcs_arbytes es_arbytes;
    uint32_t  ss_sel;     /* ss selector */
    uint32_t  ss_limit;
    uint32_t  ss_base;
    union vmcs_arbytes ss_arbytes;
    uint32_t  fs_sel;     /* fs selector */
    uint32_t  fs_limit;
    uint32_t  fs_base;
    union vmcs_arbytes fs_arbytes;
    uint32_t  gs_sel;     /* gs selector */
    uint32_t  gs_limit;
    uint32_t  gs_base;
    union vmcs_arbytes gs_arbytes;
    uint32_t  tr_sel;     /* task selector */
    uint32_t  tr_limit;
    uint32_t  tr_base;
    union vmcs_arbytes tr_arbytes;
    uint32_t  ldtr_sel;   /* ldtr selector */
    uint32_t  ldtr_limit;
    uint32_t  ldtr_base;
    union vmcs_arbytes ldtr_arbytes;

    unsigned char rm_irqbase[2];
};
typedef struct vmx_assist_context vmx_assist_context_t;

#endif /* __ASSEMBLY__ */

#endif /* _VMX_ASSIST_H_ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
