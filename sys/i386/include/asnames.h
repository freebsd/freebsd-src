/*-
 * Copyright (c) 1997 John D. Polstra
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: asnames.h,v 1.13 1997/12/26 20:41:59 phk Exp $
 */

#ifndef _MACHINE_ASNAMES_H_
#define _MACHINE_ASNAMES_H_

/*
 * This file is used by the kernel assembly language sources to provide
 * the proper mapping between the global names used in assembly language
 * code and the corresponding C symbols.  By convention, all C symbols
 * that are referenced from assembly language are prefixed with `_'.
 * That happens to be the same prefix that the a.out compiler attaches
 * to each C symbol.
 *
 * When using the ELF compiler, C symbols are identical to the corresponding
 * assembly language symbols.  Thus the extra underscores cause problems.
 * The defines in this file map the underscore names back to the proper
 * unadorned names.
 *
 * Every global symbol that is referenced from both C source and assembly
 * language source must have an entry in this file, or the kernel will
 * not build properly using the ELF compiler.
 *
 * This file is included by <machine/asmacros.h>, and it is OK to rely
 * on that.
 */

#ifdef __ELF__

#define _APTD				APTD
#define _APTDpde			APTDpde
#define _APTmap				APTmap
#define _CONST_QNaN			CONST_QNaN
#define _IdlePTD			IdlePTD
#define _KPTphys			KPTphys
#define _MP_GDT				MP_GDT
#define _MPgetlock			MPgetlock
#define _MPrellock			MPrellock
#define _MPtrylock			MPtrylock
#define _PTD				PTD
#define _PTDpde				PTDpde
#define _PTmap				PTmap
#define _SMP_ioapic			SMP_ioapic
#define _SMP_prvpt			SMP_prvpt
#define _Xalign				Xalign
#define _Xbnd				Xbnd
#define _Xbpt				Xbpt
#define _Xcpuast			Xcpuast
#define _Xcpustop			Xcpustop
#define _Xdbg				Xdbg
#define _Xdiv				Xdiv
#define _Xdna				Xdna
#define _Xfastintr0			Xfastintr0
#define _Xfastintr1			Xfastintr1
#define _Xfastintr10			Xfastintr10
#define _Xfastintr11			Xfastintr11
#define _Xfastintr12			Xfastintr12
#define _Xfastintr13			Xfastintr13
#define _Xfastintr14			Xfastintr14
#define _Xfastintr15			Xfastintr15
#define _Xfastintr16			Xfastintr16
#define _Xfastintr17			Xfastintr17
#define _Xfastintr18			Xfastintr18
#define _Xfastintr19			Xfastintr19
#define _Xfastintr2			Xfastintr2
#define _Xfastintr20			Xfastintr20
#define _Xfastintr21			Xfastintr21
#define _Xfastintr22			Xfastintr22
#define _Xfastintr23			Xfastintr23
#define _Xfastintr3			Xfastintr3
#define _Xfastintr4			Xfastintr4
#define _Xfastintr5			Xfastintr5
#define _Xfastintr6			Xfastintr6
#define _Xfastintr7			Xfastintr7
#define _Xfastintr8			Xfastintr8
#define _Xfastintr9			Xfastintr9
#define _Xfpu				Xfpu
#define _Xfpusegm			Xfpusegm
#define _Xill				Xill
#define _Xint0x80_syscall		Xint0x80_syscall
#define _Xintr0				Xintr0
#define _Xintr1				Xintr1
#define _Xintr10			Xintr10
#define _Xintr11			Xintr11
#define _Xintr12			Xintr12
#define _Xintr13			Xintr13
#define _Xintr14			Xintr14
#define _Xintr15			Xintr15
#define _Xintr16			Xintr16
#define _Xintr17			Xintr17
#define _Xintr18			Xintr18
#define _Xintr19			Xintr19
#define _Xintr2				Xintr2
#define _Xintr20			Xintr20
#define _Xintr21			Xintr21
#define _Xintr22			Xintr22
#define _Xintr23			Xintr23
#define _Xintr3				Xintr3
#define _Xintr4				Xintr4
#define _Xintr5				Xintr5
#define _Xintr6				Xintr6
#define _Xintr7				Xintr7
#define _Xintr8				Xintr8
#define _Xintr8254			Xintr8254
#define _Xintr9				Xintr9
#define _XintrRTC			XintrRTC
#define _Xinvltlb			Xinvltlb
#define _Xcpucheckstate			Xcpucheckstate
#define _Xmchk				Xmchk
#define _Xmissing			Xmissing
#define _Xnmi				Xnmi
#define _Xofl				Xofl
#define _Xpage				Xpage
#define _Xprot				Xprot
#define _Xrsvd				Xrsvd
#define _Xspuriousint			Xspuriousint
#define _Xstk				Xstk
#define _Xsyscall			Xsyscall
#define _Xtss				Xtss
#define __default_ldt			_default_ldt
#define __ucodesel			_ucodesel
#define __udatasel			_udatasel
#define _alltraps			alltraps
#define _ap_init			ap_init
#define _apic_base			apic_base
#define _apic_id_to_logical		apic_id_to_logical
#define _apic_imen			apic_imen
#define _apic_pin_trigger		apic_pin_trigger
#define _apm_addr			apm_addr
#define _apm_bios_call			apm_bios_call
#define _apm_cs16_base			apm_cs16_base
#define _apm_cs32_base			apm_cs32_base
#define _apm_cs_entry			apm_cs_entry
#define _apm_cs_limit			apm_cs_limit
#define _apm_current_gdt_pdesc		apm_current_gdt_pdesc
#define _apm_ds_base			apm_ds_base
#define _apm_ds_limit			apm_ds_limit
#define _apm_flags			apm_flags
#define _apm_init_image			apm_init_image
#define _apm_init_image_size		apm_init_image_size
#define _apm_setup			apm_setup
#define _apm_version			apm_version
#define _arith_invalid			arith_invalid
#define _arith_overflow			arith_overflow
#define _arith_underflow		arith_underflow
#define _bcopy				bcopy
#define _bcopy_vector			bcopy_vector
#define _bigJump			bigJump
#define _bio_imask			bio_imask
#define _bluetrap			bluetrap
#define _bootCodeSeg			bootCodeSeg
#define _bootDataSeg			bootDataSeg
#define _bootMP				bootMP
#define _bootMP_size			bootMP_size
#define _bootPTD			bootPTD
#define _boot_get_mplock		boot_get_mplock
#define _bootdev			bootdev
#define _boothowto			boothowto
#define _bootinfo			bootinfo
#define _bootstrap_gdt			bootstrap_gdt
#define _bzero				bzero
#define _cam_imask			cam_imask
#define _checkstate_probed_cpus		checkstate_probed_cpus
#define _clock_lock			clock_lock
#define _cnt				cnt
#define _common_tss			common_tss
#define _common_tssd			common_tssd
#define _copyin_vector			copyin_vector
#define _copyout_vector			copyout_vector
#define _cpl				cpl
#define _cpl_lock			cpl_lock
#define _cpu				cpu
#define _cpu0prvpage			cpu0prvpage
#define _cpu0prvpt			cpu0prvpt
#define _cpu_apic_versions		cpu_apic_versions
#define _cpu_class			cpu_class
#define _cpu_feature			cpu_feature
#define _cpu_high			cpu_high
#define _cpu_id				cpu_id
#define _cpu_switch			cpu_switch
#define _cpu_vendor			cpu_vendor
#define _cpuid				cpuid
#define _curpcb				curpcb
#define _curproc			curproc
#define _currentldt			currentldt
#define _cypoll				cypoll
#define _default_halt			default_halt
#define _denormal_operand		denormal_operand
#define _div_small			div_small
#define _divide_by_zero			divide_by_zero
#define _divide_kernel			divide_kernel
#define _do_page_zero_idle		do_page_zero_idle
#define _edata				edata
#define _eintrcnt			eintrcnt
#define _eintrnames			eintrnames
#define _end				end
#define _etext				etext
#define _exception			exception
#define _fast_intr_lock			fast_intr_lock
#define _fastmove			fastmove
#define _gdt				gdt
#define _generic_bcopy			generic_bcopy
#define _generic_bzero			generic_bzero
#define _generic_copyin			generic_copyin
#define _generic_copyout		generic_copyout
#define _get_align_lock			get_align_lock
#define _get_altsyscall_lock		get_altsyscall_lock
#define _get_fpu_lock			get_fpu_lock
#define _get_isrlock			get_isrlock
#define _get_mplock			get_mplock
#define _get_syscall_lock		get_syscall_lock
#define _idqs				idqs
#define _imen				imen
#define _imen_lock			imen_lock
#define _init386			init386
#define _init_secondary			init_secondary
#define _inside_intr			inside_intr
#define _intr_countp			intr_countp
#define _intr_handler			intr_handler
#define _intr_mask			intr_mask
#define _intr_nesting_level		intr_nesting_level
#define _intr_unit			intr_unit
#define _intrcnt			intrcnt
#define _intrnames			intrnames
#define _ioapic				ioapic
#define _ipending			ipending
#define _isr_lock			isr_lock
#define _ivectors			ivectors
#define _kernelname			kernelname
#define _kstack				kstack
#define _lapic				lapic
#define _linux_sigcode			linux_sigcode
#define _linux_szsigcode		linux_szsigcode
#define _main				main
#define _mask8254			mask8254
#define _maskRTC			maskRTC
#define _microtime			microtime
#define _mp_gdtbase			mp_gdtbase
#define _mp_lock			mp_lock
#define _mp_ncpus			mp_ncpus
#define _mul64				mul64
#define _my_idlePTD			my_idlePTD
#define _my_tr				my_tr
#define _net_imask			net_imask
#define _netisr				netisr
#define _netisrs			netisrs
#define _nfs_diskless			nfs_diskless
#define _nfs_diskless_valid		nfs_diskless_valid
#define _normalize			normalize
#define _normalize_nuo			normalize_nuo
#define _npx_intrs_while_probing	npx_intrs_while_probing
#define _npx_traps_while_probing	npx_traps_while_probing
#define _npxintr			npxintr
#define _npxproc			npxproc
#define _npxsave			npxsave
#define _other_cpus			other_cpus
#define _ovbcopy_vector			ovbcopy_vector
#define _panic				panic
#define _poly_div16			poly_div16
#define _poly_div2			poly_div2
#define _poly_div4			poly_div4
#define _polynomial			polynomial
#define _private_tss			private_tss
#define _probeintr			probeintr
#define _probetrap			probetrap
#define _proc0paddr			proc0paddr
#define _prv_CMAP1			prv_CMAP1
#define _prv_CMAP2			prv_CMAP2
#define _prv_CMAP3			prv_CMAP3
#define _prv_CPAGE1			prv_CPAGE1
#define _prv_CPAGE2			prv_CPAGE2
#define _prv_CPAGE3			prv_CPAGE3
#define _qs				qs
#define _rcpoll				rcpoll
#define _real_2op_NaN			real_2op_NaN
#define _reg_div			reg_div
#define _reg_u_add			reg_u_add
#define _reg_u_div			reg_u_div
#define _reg_u_mul			reg_u_mul
#define _reg_u_sub			reg_u_sub
#define _rel_mplock			rel_mplock
#define _round_reg			round_reg
#define _rtqs				rtqs
#define _runtime			runtime
#define _s_lock				s_lock
#define _s_unlock			s_unlock
#define _secondary_main			secondary_main
#define _set_precision_flag_down	set_precision_flag_down
#define _set_precision_flag_up		set_precision_flag_up
#define _set_user_ldt			set_user_ldt
#define _shrx				shrx
#define _shrxs				shrxs
#define _sigcode			sigcode
#define _siopoll			siopoll
#define _smp_active			smp_active
#define _soft_imask			soft_imask
#define _softclock			softclock
#define _softnet_imask			softnet_imask
#define _softtty_imask			softtty_imask
#define _spl0				spl0
#define _splz				splz
#define _ss_lock			ss_lock
#define _ss_unlock			ss_unlock
#define _started_cpus			started_cpus
#define _stopped_cpus			stopped_cpus
#define _checkstate_cpus		checkstate_cpus
#define _checkstate_cpustate		checkstate_cpustate
#define _checkstate_curproc		checkstate_curproc
#define _checkstate_pc			checkstate_pc
#define _checkstate_need_ast		checkstate_need_ast
#define _checkstate_pending_ast		checkstate_pending_ast
#define _syscall			syscall
#define _szsigcode			szsigcode
#define _time				time
#define _timer0_max_count		timer0_max_count
#define _timer0_overflow_threshold	timer0_overflow_threshold
#define _timer0_prescaler_count		timer0_prescaler_count
#define _trap				trap
#define _trap_by_wrmsr			trap_by_wrmsr
#define _trapwrite			trapwrite
#define _tsc_bias			tsc_bias
#define _tsc_freq			tsc_freq
#define _tsc_multiplier			tsc_multiplier
#define _tty_imask			tty_imask
#define _userconfig_from_boot		userconfig_from_boot
#define _vec				vec
#define _vec8254			vec8254
#define _vecRTC				vecRTC
#define _vm_page_zero_idle		vm_page_zero_idle
#define _want_resched			want_resched
#define _whichidqs			whichidqs
#define _whichqs			whichqs
#define _whichrtqs			whichrtqs
#define _wm_sqrt			wm_sqrt

#endif /* __ELF__ */

#endif /* !_MACHINE_ASNAMES_H_ */
