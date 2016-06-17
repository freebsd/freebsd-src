#ifndef __ALPHA_TSUNAMI__H__
#define __ALPHA_TSUNAMI__H__

#include <linux/types.h>
#include <asm/compiler.h>

/*
 * TSUNAMI/TYPHOON are the internal names for the core logic chipset which
 * provides memory controller and PCI access for the 21264 based systems.
 *
 * This file is based on:
 *
 * Tsunami System Programmers Manual
 * Preliminary, Chapters 2-5
 *
 */

/* XXX: Do we need to conditionalize on this?  */
#ifdef USE_48_BIT_KSEG
#define TS_BIAS 0x80000000000UL
#else
#define TS_BIAS 0x10000000000UL
#endif

/*
 * CChip, DChip, and PChip registers
 */

typedef struct {
	volatile unsigned long csr __attribute__((aligned(64)));
} tsunami_64;

typedef struct {
	tsunami_64	csc;
	tsunami_64	mtr;
	tsunami_64	misc;
	tsunami_64	mpd;
	tsunami_64	aar0;
	tsunami_64	aar1;
	tsunami_64	aar2;
	tsunami_64	aar3;
	tsunami_64	dim0;
	tsunami_64	dim1;
	tsunami_64	dir0;
	tsunami_64	dir1;
	tsunami_64	drir;
	tsunami_64	prben;
	tsunami_64	iic;	/* a.k.a. iic0 */
	tsunami_64	wdr;	/* a.k.a. iic1 */
	tsunami_64	mpr0;
	tsunami_64	mpr1;
	tsunami_64	mpr2;
	tsunami_64	mpr3;
	tsunami_64	mctl;
	tsunami_64	__pad1;
	tsunami_64	ttr;
	tsunami_64	tdr;
	tsunami_64	dim2;
	tsunami_64	dim3;
	tsunami_64	dir2;
	tsunami_64	dir3;
	tsunami_64	iic2;
	tsunami_64	iic3;
} tsunami_cchip;

typedef struct {
	tsunami_64	dsc;
	tsunami_64	str;
	tsunami_64	drev;
} tsunami_dchip;

typedef struct {
	tsunami_64	wsba[4];
	tsunami_64	wsm[4];
	tsunami_64	tba[4];
	tsunami_64	pctl;
	tsunami_64	plat;
	tsunami_64	reserved;
	tsunami_64	perror;
	tsunami_64	perrmask;
	tsunami_64	perrset;
	tsunami_64	tlbiv;
	tsunami_64	tlbia;
	tsunami_64	pmonctl;
	tsunami_64	pmoncnt;
} tsunami_pchip;

#define TSUNAMI_cchip  ((tsunami_cchip *)(IDENT_ADDR+TS_BIAS+0x1A0000000UL))
#define TSUNAMI_dchip  ((tsunami_dchip *)(IDENT_ADDR+TS_BIAS+0x1B0000800UL))
#define TSUNAMI_pchip0 ((tsunami_pchip *)(IDENT_ADDR+TS_BIAS+0x180000000UL))
#define TSUNAMI_pchip1 ((tsunami_pchip *)(IDENT_ADDR+TS_BIAS+0x380000000UL))
extern int TSUNAMI_bootcpu;

/*
 * TSUNAMI Pchip Error register.
 */

#define perror_m_lost 0x1
#define perror_m_serr 0x2
#define perror_m_perr 0x4
#define perror_m_dcrto 0x8
#define perror_m_sge 0x10
#define perror_m_ape 0x20
#define perror_m_ta 0x40
#define perror_m_rdpe 0x80
#define perror_m_nds 0x100
#define perror_m_rto 0x200
#define perror_m_uecc 0x400
#define perror_m_cre 0x800
#define perror_m_addrl 0xFFFFFFFF0000UL
#define perror_m_addrh 0x7000000000000UL
#define perror_m_cmd 0xF0000000000000UL
#define perror_m_syn 0xFF00000000000000UL
union TPchipPERROR {   
	struct  {
		unsigned int perror_v_lost : 1;
		unsigned perror_v_serr : 1;
		unsigned perror_v_perr : 1;
		unsigned perror_v_dcrto : 1;
		unsigned perror_v_sge : 1;
		unsigned perror_v_ape : 1;
		unsigned perror_v_ta : 1;
		unsigned perror_v_rdpe : 1;
		unsigned perror_v_nds : 1;
		unsigned perror_v_rto : 1;
		unsigned perror_v_uecc : 1;
		unsigned perror_v_cre : 1;                 
		unsigned perror_v_rsvd1 : 4;
		unsigned perror_v_addrl : 32;
		unsigned perror_v_addrh : 3;
		unsigned perror_v_rsvd2 : 1;
		unsigned perror_v_cmd : 4;
		unsigned perror_v_syn : 8;
	} perror_r_bits;
	int perror_q_whole [2];
};                       

/*
 * TSUNAMI Pchip Window Space Base Address register.
 */
#define wsba_m_ena 0x1                
#define wsba_m_sg 0x2
#define wsba_m_ptp 0x4
#define wsba_m_addr 0xFFF00000  
#define wmask_k_sz1gb 0x3FF00000                   
union TPchipWSBA {
	struct  {
		unsigned wsba_v_ena : 1;
		unsigned wsba_v_sg : 1;
		unsigned wsba_v_ptp : 1;
		unsigned wsba_v_rsvd1 : 17;
		unsigned wsba_v_addr : 12;
		unsigned wsba_v_rsvd2 : 32;
	} wsba_r_bits;
	int wsba_q_whole [2];
};

/*
 * TSUNAMI Pchip Control Register
 */
#define pctl_m_fdsc 0x1
#define pctl_m_fbtb 0x2
#define pctl_m_thdis 0x4
#define pctl_m_chaindis 0x8
#define pctl_m_tgtlat 0x10
#define pctl_m_hole 0x20
#define pctl_m_mwin 0x40
#define pctl_m_arbena 0x80
#define pctl_m_prigrp 0x7F00
#define pctl_m_ppri 0x8000
#define pctl_m_rsvd1 0x30000
#define pctl_m_eccen 0x40000
#define pctl_m_padm 0x80000
#define pctl_m_cdqmax 0xF00000
#define pctl_m_rev 0xFF000000
#define pctl_m_crqmax 0xF00000000UL
#define pctl_m_ptpmax 0xF000000000UL
#define pctl_m_pclkx 0x30000000000UL
#define pctl_m_fdsdis 0x40000000000UL
#define pctl_m_fdwdis 0x80000000000UL
#define pctl_m_ptevrfy 0x100000000000UL
#define pctl_m_rpp 0x200000000000UL
#define pctl_m_pid 0xC00000000000UL
#define pctl_m_rsvd2 0xFFFF000000000000UL

union TPchipPCTL {
	struct {
		unsigned pctl_v_fdsc : 1;
		unsigned pctl_v_fbtb : 1;
		unsigned pctl_v_thdis : 1;
		unsigned pctl_v_chaindis : 1;
		unsigned pctl_v_tgtlat : 1;
		unsigned pctl_v_hole : 1;
		unsigned pctl_v_mwin : 1;
		unsigned pctl_v_arbena : 1;
		unsigned pctl_v_prigrp : 7;
		unsigned pctl_v_ppri : 1;
		unsigned pctl_v_rsvd1 : 2;
		unsigned pctl_v_eccen : 1;
		unsigned pctl_v_padm : 1;
		unsigned pctl_v_cdqmax : 4;
		unsigned pctl_v_rev : 8;
		unsigned pctl_v_crqmax : 4;
		unsigned pctl_v_ptpmax : 4;
		unsigned pctl_v_pclkx : 2;
		unsigned pctl_v_fdsdis : 1;
		unsigned pctl_v_fdwdis : 1;
		unsigned pctl_v_ptevrfy : 1;
		unsigned pctl_v_rpp : 1;
		unsigned pctl_v_pid : 2;
		unsigned pctl_v_rsvd2 : 16;
	} pctl_r_bits;
	int pctl_q_whole [2];
};

/*
 * TSUNAMI Pchip Error Mask Register.
 */
#define perrmask_m_lost 0x1
#define perrmask_m_serr 0x2
#define perrmask_m_perr 0x4
#define perrmask_m_dcrto 0x8
#define perrmask_m_sge 0x10
#define perrmask_m_ape 0x20
#define perrmask_m_ta 0x40
#define perrmask_m_rdpe 0x80
#define perrmask_m_nds 0x100
#define perrmask_m_rto 0x200
#define perrmask_m_uecc 0x400
#define perrmask_m_cre 0x800
#define perrmask_m_rsvd 0xFFFFFFFFFFFFF000UL
union TPchipPERRMASK {   
	struct  {
		unsigned int perrmask_v_lost : 1;
		unsigned perrmask_v_serr : 1;
		unsigned perrmask_v_perr : 1;
		unsigned perrmask_v_dcrto : 1;
		unsigned perrmask_v_sge : 1;
		unsigned perrmask_v_ape : 1;
		unsigned perrmask_v_ta : 1;
		unsigned perrmask_v_rdpe : 1;
		unsigned perrmask_v_nds : 1;
		unsigned perrmask_v_rto : 1;
		unsigned perrmask_v_uecc : 1;
		unsigned perrmask_v_cre : 1;                 
		unsigned perrmask_v_rsvd1 : 20;
		unsigned perrmask_v_rsvd2 : 32;
	} perrmask_r_bits;
	int perrmask_q_whole [2];
};                       

/*
 * Memory spaces:
 */
#define TSUNAMI_HOSE(h)		(((unsigned long)(h)) << 33)
#define TSUNAMI_BASE		(IDENT_ADDR + TS_BIAS)

#define TSUNAMI_MEM(h)		(TSUNAMI_BASE+TSUNAMI_HOSE(h) + 0x000000000UL)
#define _TSUNAMI_IACK_SC(h)	(TSUNAMI_BASE+TSUNAMI_HOSE(h) + 0x1F8000000UL)
#define TSUNAMI_IO(h)		(TSUNAMI_BASE+TSUNAMI_HOSE(h) + 0x1FC000000UL)
#define TSUNAMI_CONF(h)		(TSUNAMI_BASE+TSUNAMI_HOSE(h) + 0x1FE000000UL)

#define TSUNAMI_IACK_SC		_TSUNAMI_IACK_SC(0) /* hack! */


/* 
 * The canonical non-remaped I/O and MEM addresses have these values
 * subtracted out.  This is arranged so that folks manipulating ISA
 * devices can use their familiar numbers and have them map to bus 0.
 */

#define TSUNAMI_IO_BIAS          TSUNAMI_IO(0)
#define TSUNAMI_MEM_BIAS         TSUNAMI_MEM(0)

/* The IO address space is larger than 0xffff */
#define TSUNAMI_IO_SPACE	(TSUNAMI_CONF(0) - TSUNAMI_IO(0))

/* Offset between ram physical addresses and pci64 DAC bus addresses.  */
#define TSUNAMI_DAC_OFFSET	(1UL << 40)

/*
 * Data structure for handling TSUNAMI machine checks:
 */
struct el_TSUNAMI_sysdata_mcheck {
};


#ifdef __KERNEL__

#ifndef __EXTERN_INLINE
#define __EXTERN_INLINE static inline
#define __IO_EXTERN_INLINE
#endif

/*
 * I/O functions:
 *
 * TSUNAMI, the 21??? PCI/memory support chipset for the EV6 (21264)
 * can only use linear accesses to get at PCI memory and I/O spaces.
 */

#define vucp	volatile unsigned char *
#define vusp	volatile unsigned short *
#define vuip	volatile unsigned int *
#define vulp	volatile unsigned long *

__EXTERN_INLINE u8 tsunami_inb(unsigned long addr)
{
	/* ??? I wish I could get rid of this.  But there's no ioremap
	   equivalent for I/O space.  PCI I/O can be forced into the
	   correct hose's I/O region, but that doesn't take care of
	   legacy ISA crap.  */

	addr += TSUNAMI_IO_BIAS;
	return __kernel_ldbu(*(vucp)addr);
}

__EXTERN_INLINE void tsunami_outb(u8 b, unsigned long addr)
{
	addr += TSUNAMI_IO_BIAS;
	__kernel_stb(b, *(vucp)addr);
	mb();
}

__EXTERN_INLINE u16 tsunami_inw(unsigned long addr)
{
	addr += TSUNAMI_IO_BIAS;
	return __kernel_ldwu(*(vusp)addr);
}

__EXTERN_INLINE void tsunami_outw(u16 b, unsigned long addr)
{
	addr += TSUNAMI_IO_BIAS;
	__kernel_stw(b, *(vusp)addr);
	mb();
}

__EXTERN_INLINE u32 tsunami_inl(unsigned long addr)
{
	addr += TSUNAMI_IO_BIAS;
	return *(vuip)addr;
}

__EXTERN_INLINE void tsunami_outl(u32 b, unsigned long addr)
{
	addr += TSUNAMI_IO_BIAS;
	*(vuip)addr = b;
	mb();
}

/*
 * Memory functions.  all accesses are done through linear space.
 */

__EXTERN_INLINE unsigned long tsunami_ioremap(unsigned long addr, 
					      unsigned long size
					      __attribute__((unused)))
{
	return addr + TSUNAMI_MEM_BIAS;
}

__EXTERN_INLINE void tsunami_iounmap(unsigned long addr)
{
	return;
}

__EXTERN_INLINE int tsunami_is_ioaddr(unsigned long addr)
{
	return addr >= TSUNAMI_BASE;
}

__EXTERN_INLINE u8 tsunami_readb(unsigned long addr)
{
	return __kernel_ldbu(*(vucp)addr);
}

__EXTERN_INLINE u16 tsunami_readw(unsigned long addr)
{
	return __kernel_ldwu(*(vusp)addr);
}

__EXTERN_INLINE u32 tsunami_readl(unsigned long addr)
{
	return *(vuip)addr;
}

__EXTERN_INLINE u64 tsunami_readq(unsigned long addr)
{
	return *(vulp)addr;
}

__EXTERN_INLINE void tsunami_writeb(u8 b, unsigned long addr)
{
	__kernel_stb(b, *(vucp)addr);
}

__EXTERN_INLINE void tsunami_writew(u16 b, unsigned long addr)
{
	__kernel_stw(b, *(vusp)addr);
}

__EXTERN_INLINE void tsunami_writel(u32 b, unsigned long addr)
{
	*(vuip)addr = b;
}

__EXTERN_INLINE void tsunami_writeq(u64 b, unsigned long addr)
{
	*(vulp)addr = b;
}

#undef vucp
#undef vusp
#undef vuip
#undef vulp

#ifdef __WANT_IO_DEF

#define __inb(p)		tsunami_inb((unsigned long)(p))
#define __inw(p)		tsunami_inw((unsigned long)(p))
#define __inl(p)		tsunami_inl((unsigned long)(p))
#define __outb(x,p)		tsunami_outb((x),(unsigned long)(p))
#define __outw(x,p)		tsunami_outw((x),(unsigned long)(p))
#define __outl(x,p)		tsunami_outl((x),(unsigned long)(p))
#define __readb(a)		tsunami_readb((unsigned long)(a))
#define __readw(a)		tsunami_readw((unsigned long)(a))
#define __readl(a)		tsunami_readl((unsigned long)(a))
#define __readq(a)		tsunami_readq((unsigned long)(a))
#define __writeb(x,a)		tsunami_writeb((x),(unsigned long)(a))
#define __writew(x,a)		tsunami_writew((x),(unsigned long)(a))
#define __writel(x,a)		tsunami_writel((x),(unsigned long)(a))
#define __writeq(x,a)		tsunami_writeq((x),(unsigned long)(a))
#define __ioremap(a,s)		tsunami_ioremap((unsigned long)(a),(s))
#define __iounmap(a)		tsunami_iounmap((unsigned long)(a))
#define __is_ioaddr(a)		tsunami_is_ioaddr((unsigned long)(a))

#define inb(p)			__inb(p)
#define inw(p)			__inw(p)
#define inl(p)			__inl(p)
#define outb(x,p)		__outb((x),(p))
#define outw(x,p)		__outw((x),(p))
#define outl(x,p)		__outl((x),(p))
#define __raw_readb(a)		__readb(a)
#define __raw_readw(a)		__readw(a)
#define __raw_readl(a)		__readl(a)
#define __raw_readq(a)		__readq(a)
#define __raw_writeb(v,a)	__writeb((v),(a))
#define __raw_writew(v,a)	__writew((v),(a))
#define __raw_writel(v,a)	__writel((v),(a))
#define __raw_writeq(v,a)	__writeq((v),(a))

#endif /* __WANT_IO_DEF */

#ifdef __IO_EXTERN_INLINE
#undef __EXTERN_INLINE
#undef __IO_EXTERN_INLINE
#endif

#endif /* __KERNEL__ */

#endif /* __ALPHA_TSUNAMI__H__ */
