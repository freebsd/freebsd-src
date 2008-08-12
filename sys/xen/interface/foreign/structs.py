# configuration: what needs translation

structs = [ "start_info",
            "trap_info",
            "pt_fpreg",
            "cpu_user_regs",
            "xen_ia64_boot_param",
            "ia64_tr_entry",
            "vcpu_extra_regs",
            "vcpu_guest_context",
            "arch_vcpu_info",
            "vcpu_time_info",
            "vcpu_info",
            "arch_shared_info",
            "shared_info" ];

defines = [ "__i386__",
            "__x86_64__",

            "FLAT_RING1_CS",
            "FLAT_RING1_DS",
            "FLAT_RING1_SS",

            "FLAT_RING3_CS64",
            "FLAT_RING3_DS64",
            "FLAT_RING3_SS64",
            "FLAT_KERNEL_CS64",
            "FLAT_KERNEL_DS64",
            "FLAT_KERNEL_SS64",

            "FLAT_KERNEL_CS",
            "FLAT_KERNEL_DS",
            "FLAT_KERNEL_SS",

            # x86_{32,64}
            "_VGCF_i387_valid",
            "VGCF_i387_valid",
            "_VGCF_in_kernel",
            "VGCF_in_kernel",
            "_VGCF_failsafe_disables_events",
            "VGCF_failsafe_disables_events",
            "_VGCF_syscall_disables_events",
            "VGCF_syscall_disables_events",
            "_VGCF_online",
            "VGCF_online",

            # ia64
            "VGCF_EXTRA_REGS",

            # all archs
            "xen_pfn_to_cr3",
            "MAX_VIRT_CPUS",
            "MAX_GUEST_CMDLINE" ];

