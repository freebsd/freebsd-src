/******************************************************************************
 * vm_event.h
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

#ifndef _XEN_PUBLIC_VM_EVENT_H
#define _XEN_PUBLIC_VM_EVENT_H

#include "xen.h"

#define VM_EVENT_INTERFACE_VERSION 0x00000006

#if defined(__XEN__) || defined(__XEN_TOOLS__)

#include "io/ring.h"

/*
 * Memory event flags
 */

/*
 * VCPU_PAUSED in a request signals that the vCPU triggering the event has been
 *  paused
 * VCPU_PAUSED in a response signals to unpause the vCPU
 */
#define VM_EVENT_FLAG_VCPU_PAUSED        (1 << 0)
/* Flags to aid debugging vm_event */
#define VM_EVENT_FLAG_FOREIGN            (1 << 1)
/*
 * The following flags can be set in response to a mem_access event.
 *
 * Emulate the fault-causing instruction (if set in the event response flags).
 * This will allow the guest to continue execution without lifting the page
 * access restrictions.
 */
#define VM_EVENT_FLAG_EMULATE            (1 << 2)
/*
 * Same as VM_EVENT_FLAG_EMULATE, but with write operations or operations
 * potentially having side effects (like memory mapped or port I/O) disabled.
 */
#define VM_EVENT_FLAG_EMULATE_NOWRITE    (1 << 3)
/*
 * Toggle singlestepping on vm_event response.
 * Requires the vCPU to be paused already (synchronous events only).
 */
#define VM_EVENT_FLAG_TOGGLE_SINGLESTEP  (1 << 4)
/*
 * Data is being sent back to the hypervisor in the event response, to be
 * returned by the read function when emulating an instruction.
 * This flag is only useful when combined with VM_EVENT_FLAG_EMULATE
 * and takes precedence if combined with VM_EVENT_FLAG_EMULATE_NOWRITE
 * (i.e. if both VM_EVENT_FLAG_EMULATE_NOWRITE and
 * VM_EVENT_FLAG_SET_EMUL_READ_DATA are set, only the latter will be honored).
 */
#define VM_EVENT_FLAG_SET_EMUL_READ_DATA (1 << 5)
/*
 * Deny completion of the operation that triggered the event.
 * Currently only useful for MSR and control-register write events.
 * Requires the vCPU to be paused already (synchronous events only).
 */
#define VM_EVENT_FLAG_DENY               (1 << 6)
/*
 * This flag can be set in a request or a response
 *
 * On a request, indicates that the event occurred in the alternate p2m
 * specified by the altp2m_idx request field.
 *
 * On a response, indicates that the VCPU should resume in the alternate p2m
 * specified by the altp2m_idx response field if possible.
 */
#define VM_EVENT_FLAG_ALTERNATE_P2M      (1 << 7)
/*
 * Set the vCPU registers to the values in the  vm_event response.
 * At the moment x86-only, applies to EAX-EDX, ESP, EBP, ESI, EDI, R8-R15,
 * EFLAGS, and EIP.
 * Requires the vCPU to be paused already (synchronous events only).
 */
#define VM_EVENT_FLAG_SET_REGISTERS      (1 << 8)
/*
 * Instruction cache is being sent back to the hypervisor in the event response
 * to be used by the emulator. This flag is only useful when combined with
 * VM_EVENT_FLAG_EMULATE and does not take presedence if combined with
 * VM_EVENT_FLAG_EMULATE_NOWRITE or VM_EVENT_FLAG_SET_EMUL_READ_DATA, (i.e.
 * if any of those flags are set, only those will be honored).
 */
#define VM_EVENT_FLAG_SET_EMUL_INSN_DATA (1 << 9)
/*
 * Have a one-shot VM_EVENT_REASON_INTERRUPT event sent for the first
 * interrupt pending after resuming the VCPU.
 */
#define VM_EVENT_FLAG_GET_NEXT_INTERRUPT (1 << 10)
/*
 * Execute fast singlestepping on vm_event response.
 * Requires the vCPU to be paused already (synchronous events only).
 *
 * On a response requires setting the  p2midx field of fast_singlestep to which
 * Xen will switch the vCPU to on the occurance of the first singlestep, after
 * which singlestep gets automatically disabled.
 */
#define VM_EVENT_FLAG_FAST_SINGLESTEP    (1 << 11)

/*
 * Reasons for the vm event request
 */

/* Default case */
#define VM_EVENT_REASON_UNKNOWN                 0
/* Memory access violation */
#define VM_EVENT_REASON_MEM_ACCESS              1
/* Memory sharing event */
#define VM_EVENT_REASON_MEM_SHARING             2
/* Memory paging event */
#define VM_EVENT_REASON_MEM_PAGING              3
/* A control register was updated */
#define VM_EVENT_REASON_WRITE_CTRLREG           4
/* An MSR was updated. */
#define VM_EVENT_REASON_MOV_TO_MSR              5
/* Debug operation executed (e.g. int3) */
#define VM_EVENT_REASON_SOFTWARE_BREAKPOINT     6
/* Single-step (e.g. MTF) */
#define VM_EVENT_REASON_SINGLESTEP              7
/* An event has been requested via HVMOP_guest_request_vm_event. */
#define VM_EVENT_REASON_GUEST_REQUEST           8
/* A debug exception was caught */
#define VM_EVENT_REASON_DEBUG_EXCEPTION         9
/* CPUID executed */
#define VM_EVENT_REASON_CPUID                   10
/*
 * Privileged call executed (e.g. SMC).
 * Note: event may be generated even if SMC condition check fails on some CPUs.
 *       As this behavior is CPU-specific, users are advised to not rely on it.
 *       These kinds of events will be filtered out in future versions.
 */
#define VM_EVENT_REASON_PRIVILEGED_CALL         11
/* An interrupt has been delivered. */
#define VM_EVENT_REASON_INTERRUPT               12
/* A descriptor table register was accessed. */
#define VM_EVENT_REASON_DESCRIPTOR_ACCESS       13
/* Current instruction is not implemented by the emulator */
#define VM_EVENT_REASON_EMUL_UNIMPLEMENTED      14

/* Supported values for the vm_event_write_ctrlreg index. */
#define VM_EVENT_X86_CR0    0
#define VM_EVENT_X86_CR3    1
#define VM_EVENT_X86_CR4    2
#define VM_EVENT_X86_XCR0   3

/* The limit field is right-shifted by 12 bits if .ar.g is set. */
struct vm_event_x86_selector_reg {
    uint32_t limit  :    20;
    uint32_t ar     :    12;
};

/*
 * Using custom vCPU structs (i.e. not hvm_hw_cpu) for both x86 and ARM
 * so as to not fill the vm_event ring buffer too quickly.
 */
struct vm_event_regs_x86 {
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbx;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rflags;
    uint64_t dr6;
    uint64_t dr7;
    uint64_t rip;
    uint64_t cr0;
    uint64_t cr2;
    uint64_t cr3;
    uint64_t cr4;
    uint64_t sysenter_cs;
    uint64_t sysenter_esp;
    uint64_t sysenter_eip;
    uint64_t msr_efer;
    uint64_t msr_star;
    uint64_t msr_lstar;
    uint64_t gdtr_base;
    uint32_t cs_base;
    uint32_t ss_base;
    uint32_t ds_base;
    uint32_t es_base;
    uint64_t fs_base;
    uint64_t gs_base;
    struct vm_event_x86_selector_reg cs;
    struct vm_event_x86_selector_reg ss;
    struct vm_event_x86_selector_reg ds;
    struct vm_event_x86_selector_reg es;
    struct vm_event_x86_selector_reg fs;
    struct vm_event_x86_selector_reg gs;
    uint64_t shadow_gs;
    uint16_t gdtr_limit;
    uint16_t cs_sel;
    uint16_t ss_sel;
    uint16_t ds_sel;
    uint16_t es_sel;
    uint16_t fs_sel;
    uint16_t gs_sel;
    uint16_t _pad;
};

/*
 * Only the register 'pc' can be set on a vm_event response using the
 * VM_EVENT_FLAG_SET_REGISTERS flag.
 */
struct vm_event_regs_arm {
    uint64_t ttbr0;
    uint64_t ttbr1;
    uint64_t ttbcr;
    uint64_t pc;
    uint32_t cpsr;
    uint32_t _pad;
};

/*
 * mem_access flag definitions
 *
 * These flags are set only as part of a mem_event request.
 *
 * R/W/X: Defines the type of violation that has triggered the event
 *        Multiple types can be set in a single violation!
 * GLA_VALID: If the gla field holds a guest VA associated with the event
 * FAULT_WITH_GLA: If the violation was triggered by accessing gla
 * FAULT_IN_GPT: If the violation was triggered during translating gla
 */
#define MEM_ACCESS_R                (1 << 0)
#define MEM_ACCESS_W                (1 << 1)
#define MEM_ACCESS_X                (1 << 2)
#define MEM_ACCESS_RWX              (MEM_ACCESS_R | MEM_ACCESS_W | MEM_ACCESS_X)
#define MEM_ACCESS_RW               (MEM_ACCESS_R | MEM_ACCESS_W)
#define MEM_ACCESS_RX               (MEM_ACCESS_R | MEM_ACCESS_X)
#define MEM_ACCESS_WX               (MEM_ACCESS_W | MEM_ACCESS_X)
#define MEM_ACCESS_GLA_VALID        (1 << 3)
#define MEM_ACCESS_FAULT_WITH_GLA   (1 << 4)
#define MEM_ACCESS_FAULT_IN_GPT     (1 << 5)

struct vm_event_mem_access {
    uint64_t gfn;
    uint64_t offset;
    uint64_t gla;   /* if flags has MEM_ACCESS_GLA_VALID set */
    uint32_t flags; /* MEM_ACCESS_* */
    uint32_t _pad;
};

struct vm_event_write_ctrlreg {
    uint32_t index;
    uint32_t _pad;
    uint64_t new_value;
    uint64_t old_value;
};

struct vm_event_singlestep {
    uint64_t gfn;
};

struct vm_event_fast_singlestep {
    uint16_t p2midx;
};

struct vm_event_debug {
    uint64_t gfn;
    uint64_t pending_dbg; /* Behaves like the VT-x PENDING_DBG field. */
    uint32_t insn_length;
    uint8_t type;        /* HVMOP_TRAP_* */
    uint8_t _pad[3];
};

struct vm_event_mov_to_msr {
    uint64_t msr;
    uint64_t new_value;
    uint64_t old_value;
};

#define VM_EVENT_DESC_IDTR           1
#define VM_EVENT_DESC_GDTR           2
#define VM_EVENT_DESC_LDTR           3
#define VM_EVENT_DESC_TR             4

struct vm_event_desc_access {
    union {
        struct {
            uint32_t instr_info;         /* VMX: VMCS Instruction-Information */
            uint32_t _pad1;
            uint64_t exit_qualification; /* VMX: VMCS Exit Qualification */
        } vmx;
    } arch;
    uint8_t descriptor;                  /* VM_EVENT_DESC_* */
    uint8_t is_write;
    uint8_t _pad[6];
};

struct vm_event_cpuid {
    uint32_t insn_length;
    uint32_t leaf;
    uint32_t subleaf;
    uint32_t _pad;
};

struct vm_event_interrupt_x86 {
    uint32_t vector;
    uint32_t type;
    uint32_t error_code;
    uint32_t _pad;
    uint64_t cr2;
};

#define MEM_PAGING_DROP_PAGE       (1 << 0)
#define MEM_PAGING_EVICT_FAIL      (1 << 1)

struct vm_event_paging {
    uint64_t gfn;
    uint32_t p2mt;
    uint32_t flags;
};

struct vm_event_sharing {
    uint64_t gfn;
    uint32_t p2mt;
    uint32_t _pad;
};

struct vm_event_emul_read_data {
    uint32_t size;
    /* The struct is used in a union with vm_event_regs_x86. */
    uint8_t  data[sizeof(struct vm_event_regs_x86) - sizeof(uint32_t)];
};

struct vm_event_emul_insn_data {
    uint8_t data[16]; /* Has to be completely filled */
};

typedef struct vm_event_st {
    uint32_t version;   /* VM_EVENT_INTERFACE_VERSION */
    uint32_t flags;     /* VM_EVENT_FLAG_* */
    uint32_t reason;    /* VM_EVENT_REASON_* */
    uint32_t vcpu_id;
    uint16_t altp2m_idx; /* may be used during request and response */
    uint16_t _pad[3];

    union {
        struct vm_event_paging                mem_paging;
        struct vm_event_sharing               mem_sharing;
        struct vm_event_mem_access            mem_access;
        struct vm_event_write_ctrlreg         write_ctrlreg;
        struct vm_event_mov_to_msr            mov_to_msr;
        struct vm_event_desc_access           desc_access;
        struct vm_event_singlestep            singlestep;
        struct vm_event_fast_singlestep       fast_singlestep;
        struct vm_event_debug                 software_breakpoint;
        struct vm_event_debug                 debug_exception;
        struct vm_event_cpuid                 cpuid;
        union {
            struct vm_event_interrupt_x86     x86;
        } interrupt;
    } u;

    union {
        union {
            struct vm_event_regs_x86 x86;
            struct vm_event_regs_arm arm;
        } regs;

        union {
            struct vm_event_emul_read_data read;
            struct vm_event_emul_insn_data insn;
        } emul;
    } data;
} vm_event_request_t, vm_event_response_t;

DEFINE_RING_TYPES(vm_event, vm_event_request_t, vm_event_response_t);

#endif /* defined(__XEN__) || defined(__XEN_TOOLS__) */
#endif /* _XEN_PUBLIC_VM_EVENT_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
