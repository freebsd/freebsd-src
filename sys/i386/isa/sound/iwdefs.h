/*#######################################################################*/
/*  FILE   : iwdefs.h*/
/**/
/*  REMARKS: This file contains all defines used by DDK functions. It*/
/*           should be included by all applications and should be*/
/*           referenced by programmers to make their code easy to read*/
/*           and understand.*/
/**/
/*  UPDATE: 3/21/95*/
/*          7/10/95 --- added #def DRAM_HOLES*/
/*#######################################################################*/
#ifndef IWDEFS
#define IWDEFS
/*#######################################################################*/
/**/
/* Macros for use in loading Synth Addr Regs*/
/**/
/*#######################################################################*/
#define ADDR_HIGH(x)    (short)(x>>7)
#define ADDR_LOW(x)     (short)(x<<9)
/*#######################################################################*/
/**/
/*     Defines for DMA Controllers*/
/**/
/*#######################################################################*/
/* DMA Controler #1 (8-bit controller) */
#define DMA1_STAT         0x08          /* read status register */
#define DMA1_WCMD         0x08          /* write command register */
#define DMA1_WREQ         0x09          /* write request register */
#define DMA1_SNGL         0x0A          /* write single bit register */
#define DMA1_MODE         0x0B          /* write mode register */
#define DMA1_CLRFF        0x0C          /* clear byte ptr flip/flop */
#define DMA1_MCLR         0x0D          /* master clear register */
#define DMA1_CLRM         0x0E          /* clear mask register */
#define DMA1_WRTALL       0x0F          /* write all mask register */

/* DMA Controler #2 (16-bit controller) */
#define DMA2_STAT         0xD0          /* read status register */
#define DMA2_WCMD         0xD0          /* write command register */
#define DMA2_WREQ         0xD2          /* write request register */
#define DMA2_SNGL         0xD4          /* write single bit register */
#define DMA2_MODE         0xD6          /* write mode register */
#define DMA2_CLRFF        0xD8          /* clear byte ptr flip/flop */
#define DMA2_MCLR         0xDA          /* master clear register */
#define DMA2_CLRM         0xDC          /* clear mask register */
#define DMA2_WRTALL       0xDE          /* write all mask register */

#define DMA0_ADDR         0x00          /* chan 0 base adddress */
#define DMA0_CNT          0x01          /* chan 0 base count */
#define DMA1_ADDR         0x02          /* chan 1 base adddress */
#define DMA1_CNT          0x03          /* chan 1 base count */
#define DMA2_ADDR         0x04          /* chan 2 base adddress */
#define DMA2_CNT          0x05          /* chan 2 base count */
#define DMA3_ADDR         0x06          /* chan 3 base adddress */
#define DMA3_CNT          0x07          /* chan 3 base count */
#define DMA4_ADDR         0xC0          /* chan 4 base adddress */
#define DMA4_CNT          0xC2          /* chan 4 base count */
#define DMA5_ADDR         0xC4          /* chan 5 base adddress */
#define DMA5_CNT          0xC6          /* chan 5 base count */
#define DMA6_ADDR         0xC8          /* chan 6 base adddress */
#define DMA6_CNT          0xCA          /* chan 6 base count */
#define DMA7_ADDR         0xCC          /* chan 7 base adddress */
#define DMA7_CNT          0xCE          /* chan 7 base count */

#define DMA0_PAGE         0x87          /* chan 0 page register (refresh)*/
#define DMA1_PAGE         0x83          /* chan 1 page register */
#define DMA2_PAGE         0x81          /* chan 2 page register */
#define DMA3_PAGE         0x82          /* chan 3 page register */
#define DMA4_PAGE         0x8F          /* chan 4 page register (unusable)*/
#define DMA5_PAGE         0x8B          /* chan 5 page register */
#define DMA6_PAGE         0x89          /* chan 6 page register */
#define DMA7_PAGE         0x8A          /* chan 7 page register */
/*#######################################################################*/
/**/
/* Defines for register UISR (Interrupt Status)*/
/**/
/*#######################################################################*/
#define MIDI_TX_IRQ       0x01
#define MIDI_RX_IRQ       0x02
#define ALIB_TIMER1_IRQ   0x04
#define ALIB_TIMER2_IRQ   0x08
#define _UASBCI           0x45          /* UASBCI index */
#define SAMPLE_CONTROL    0x49          /* Not used by IW */
#define SET_VOICES        0x0E
#define WAVETABLE_IRQ     0x20
#define ENVELOPE_IRQ      0x40
#define DMA_TC_IRQ        0x80
/*#######################################################################*/
/**/
/*            Synthesizer-related defines*/
/**/
/*#######################################################################*/
#define GEN_INDEX         0x03           /* IGIDX offset into p3xr */
#define VOICE_SELECT      0x02           /* SVSR offset into p3xr */
#define VOICE_IRQS        0x8F           /* SVII index (read) */
#define _URSTI            0x4C           /* URSTI index */
#define GF1_SET           0x01           /* URSTI[0] */
#define GF1_OUT_ENABLE    0x02           /* URSTI[1] */
#define GF1_IRQ_ENABLE    0x04           /* URSTI[2] */
#define GF1_RESET         0xFE           /* URSTI[0]=0 */
#define VOICE_VOLUME_IRQ  0x04           /* SVII[2] */
#define VOICE_WAVE_IRQ    0x08           /* SVII[3] */
#define VC_IRQ_ENABLE     0x20           /* SACI[5] or SVCI[5]*/
#define VOICE_NUMBER      0x1F           /* Mask for SVII[4:0] */
#define VC_IRQ_PENDING    0x80           /* SACI[7] or SVCI[7] */
#define VC_DIRECT         0x40           /* SACI[6] or SVCI[6]*/
#define VC_DATA_WIDTH     0x04           /* SACI[2] */
#define VOICE_STOP        0x02           /* SACI[1] */
#define VOICE_STOPPED     0x01           /* SACI[0] */
#define VOLUME_STOP       0x02           /* SVCI[1] */
#define VOLUME_STOPPED    0x01           /* SVCI[0] */
#define VC_ROLLOVER       0x04           /* SVCI[2] */
#define VC_LOOP_ENABLE    0x08           /* SVCI[3] or SACI[3]*/
#define VC_BI_LOOP        0x10           /* SVCI[4] or SACI[4]*/
#define VOICE_OFFSET      0x20           /* SMSI[5] */
#define VOLUME_RATE0      0x00           /* SVRI[7:6]=(0,0) */
#define VOLUME_RATE1      0x40           /* SVRI[7:6]=(0,1) */
#define VOLUME_RATE2      0x80           /* SVRI[7:6]=(1,0) */
#define VOLUME_RATE3      0xC0           /* SVRI[7:6]=(1,1) */
/*#######################################################################*/
/**/
/*         Power-Mode Control Defines*/
/**/
/*#######################################################################*/
#define SHUT_DOWN         0x7E           /* shuts InterWave down */
#define POWER_UP          0xFE           /* enables all modules */
#define CODEC_PWR_UP      0x81           /* enables Codec Analog Ckts */
#define CODEC_PWR_DOWN    0x01           /* disables Codec Analog Ckts */
#define CODEC_REC_UP      0x82           /* Enables Record Path */
#define CODEC_REC_DOWN    0x02           /* Disables Record Path */
#define CODEC_PLAY_UP     0x84           /* Enables Playback Path */
#define CODEC_PLAY_DOWN   0x04           /* Disables Playback Path */
#define CODEC_IRQ_ENABLE  0x02           /* CEXTI[2] */
#define CODEC_TIMER_IRQ   0x40           /* CSR3I[6] */
#define CODEC_REC_IRQ     0x20           /* CSR3I[5] */
#define CODEC_PLAY_IRQ    0x10           /* CSR3I[4] */
#define CODEC_INT         0x01           /* CSR1R[0] */
#define MONO_INPUT        0x80           /* CMONOI[7] */
#define MONO_OUTPUT       0x40           /* CMONOI[6] */
#define MIDI_UP           0x88           /* Enables MIDI ports */
#define MIDI_DOWN         0x08           /* Disables MIDI ports */
#define SYNTH_UP          0x90           /* Enables Synthesizer */
#define SYNTH_DOWN        0x10           /* Disables Synthesizer */
#define LMC_UP            0xA0           /* Enables LM Module */
#define LMC_DOWN          0x20           /* Disbales LM Module */
#define XTAL24_UP         0xC0           /* Enables 24MHz Osc */
#define XTAL24_DOWN       0x40           /* Disables 24MHz Osc */
#define _PPWRI            0xF2           /* PPWRI index */
#define PLAY              0x0F
#define REC               0x1F
#define LEFT_AUX1_INPUT   0x02
#define RIGHT_AUX1_INPUT  0x03
#define LEFT_AUX2_INPUT   0x04
#define RIGHT_AUX2_INPUT  0x05
#define LEFT_LINE_IN      0x12
#define RIGHT_LINE_IN     0x13
#define LEFT_LINE_OUT     0x19
#define RIGHT_LINE_OUT    0x1B
#define LEFT_SOURCE       0x00
#define RIGHT_SOURCE      0x01
#define LINE_IN           0x00
#define AUX1_IN           0x40
#define MIC_IN            0x80
#define MIX_IN            0xC0
#define LEFT_DAC          0x06
#define RIGHT_DAC         0x07
#define LEFT_MIC_IN       0x16
#define RIGHT_MIC_IN      0x17
#define _CUPCTI           0x0E
#define _CLPCTI           0x0F
#define _CURCTI           0x1E
#define _CLRCTI           0x1F
#define _CLAX1I           0x02
#define _CRAX1I           0x03
#define _CLAX2I           0x04
#define _CRAX2I           0x05
#define _CLLICI           0x12
#define _CRLICI           0x13
#define _CLOAI            0x19
#define _CROAI            0x1B
#define _CLICI            0x00
#define _CRICI            0x01
#define _CLDACI           0x06
#define _CRDACI           0x07
#define _CPVFI            0x1D
/*#######################################################################*/
/**/
/* Defines for DMA transfer related operations*/
/**/
/*#######################################################################*/
#define MAX_DMA         0x07
#define DMA_DECREMENT   0x20
#define AUTO_INIT       0x10
#define DMA_READ        0x01
#define DMA_WRITE       0x02
#define AUTO_READ       0x03
#define AUTO_WRITE      0x04
#define IDMA_INV        0x0400
#define IDMA_WIDTH_16   0x0100
/*#######################################################################*/
/**/
/* Bits for dma flags within a DMA structure.*/
/**/
/*#######################################################################*/
#define DMA_USED        0x0001

#define DMA_SPLIT       0x0004    /* DMA Controller Page Crossover*/
#define CODEC_DMA       0x0008    /* Indicates a Codec DMA*/
#define DMA_WAIT        0x0020    /* Wait for DMA xfer to complete*/
#define DMA_DOWN        0x0040    /* DMA xfer from PC to InterWave*/
#define DRAM_HOLES      0x8000    /* Indicates Non-contiguous RAM configuration*/
#define DMA_UP          0xFFBF    /* DMA xfer from InterWave to PC */
/*#######################################################################*/
/**/
/* Bits for DMA Control Register (LDMACI)*/
/**/
/*#######################################################################*/
#define _LDMACI         0x41  /* Index */
#define DMA_INV         0x80
#define DMA_IRQ_ENABLE  0x20
#define DMA_IRQ_PENDING 0x40  /* on reads of LDMACI[6] */
#define DMA_DATA_16     0x40  /* on writes to LDMACI[6] */
#define DMA_WIDTH_16    0x04  /* 1=16-bit, 0=8-bit (DMA channel) */
#define DMA_RATE        0x18  /* 00=fastest,...,11=slowest */
#define DMA_UPLOAD      0x02  /* From LM to PC */
#define DMA_ENABLE      0x01
/*#######################################################################*/
/**/
/*    DMA Transfer Rates*/
/**/
/*#######################################################################*/
#define DMA_R0          0xE7    /* Fastest (use ANDing to set) */
#define DMA_R1          0x08
#define DMA_R2          0x10
#define DMA_R3          0x18    /* Slowest */
/*#######################################################################*/
/**/
/*    Interrupt Controller Defines*/
/**/
/*#######################################################################*/
#define IW_HANDLERS_ON  0x80    /* Flag for when IVT is modified */
#define EOI             0x20
#define OCR1            0x20    /* 8259-1 Operation Control Reg. */
#define IMR1            0x21    /* 8259-1 Interrupt Mask Reg. */
#define OCR2            0xA0    /* 8259-2 Operation Control Reg. */
#define IMR2            0xA1    /* 8259-2 Interrupt Mask Reg. */
#define IRQ0_UNMASK     0xFE    /* Mask to clear bit 0 in IMR */
#define IRQ1_UNMASK     0xFD
#define IRQ2_UNMASK     0xFB
#define IRQ3_UNMASK     0xF7
#define IRQ4_UNMASK     0xEF
#define IRQ5_UNMASK     0xDF
#define IRQ6_UNMASK     0xBF
#define IRQ7_UNMASK     0x7F
#define IRQ8_UNMASK     0xFE    /* Mask to clear bit 0 in IMR */
#define IRQ9_UNMASK     0xFD
#define IRQ10_UNMASK    0xFB
#define IRQ11_UNMASK    0xF7
#define IRQ12_UNMASK    0xEF
#define IRQ13_UNMASK    0xDF
#define IRQ14_UNMASK    0xBF
#define IRQ15_UNMASK    0x7F
#define IRQ0_EOI        0x60    /* Spec EOI for IRQ0 */
#define IRQ1_EOI        0x61
#define IRQ2_EOI        0x62
#define IRQ3_EOI        0x63
#define IRQ4_EOI        0x64
#define IRQ5_EOI        0x65
#define IRQ6_EOI        0x66
#define IRQ7_EOI        0x67
#define IRQ8_EOI        0x60    /* Spec EOI for IRQ8 */
#define IRQ9_EOI        0x61
#define IRQ10_EOI       0x62
#define IRQ11_EOI       0x63
#define IRQ12_EOI       0x64
#define IRQ13_EOI       0x65
#define IRQ14_EOI       0x66
#define IRQ15_EOI       0x67
/*#######################################################################*/
/**/
/*     Generic defines*/
/**/
/*#######################################################################*/
/**/
#define MEMBANK0      0L                 /* Addr of Memory Bank 0*/
#define MEMBANK1      4194304L           /* Addr of Memory Bank 1*/
#define MEMBANK2      8388608L           /* Addr of Memory Bank 2*/
#define MEMBANK3     12582912L           /* Addr of Memory Bank 3*/
#define IRQ_UNAVAIL       0x0000
#define IRQ_AVAIL         0x0001
#define IRQ_USED          0x0002
#define MAX_IRQ           16
#define NEXT_OFFSET       0L
#define PREV_OFFSET       4L
#define SIZE_OFFSET       8L
#define MEM_HEADER_SIZE  12L
#define GF1_POOL         (usigned long)(256L*1024L)
#define GUS_MODE          0x00           /* SGMI[0]=0*/
#define ENH_MODE          0x01           /* SGMI[0]=1*/
#define ENABLE_LFOS       0x02           /* SGMI[1]*/
#define NO_WAVETABLE      0x04           /* SGMI[2]*/
#define RAM_TEST          0x08           /* SGMI[3]*/
#define TRUE              1
#define FALSE             0
#define ON                1
#define OFF               0
#define AUDIO             0
#define EXT               1
#define GAME              2
#define EMULATION         3
#define MPU401            4
#define AUDIO_EXT         2
#define ALLOC_FAILURE     0xFFFFFFFFL
#define MEM_EXHAUSTED     0xFFFFFFFFL
#define RAM_MAX           16777216L
#define RAM_STEP          65536L
#define BANK_MAX          4194304L
#define ILLEGAL_SIZE      -1
#define MEM_INIT           1
#define NO_NEXT           0xFFFFFFFFL
#define NO_PREV           NO_NEXT
#define DMA_BAD_ADDR     -1
#define DMA_ON           -1
#define DMA_OK            1
#define MIDI_TX_IRQ       0x01
#define MIDI_RX_IRQ       0x02
#define ALIB_TIMER1_IRQ   0x04
#define ALIB_TIMER2_IRQ   0x08
#define WAVETABLE_IRQ     0x20
#define ENVELOPE_IRQ      0x40
#define DMA_TC_IRQ        0x80
#define DMA_SET_MASK      0x04
#define PNP_DATA_RDY       1          /* PRESSI[0] */
#define IWAVE_ABSENT      2
#define IWAVE_OPEN        4
#define IWAVE_OK          5
#define BAD_VOICES        -1
#define PNP_ABSENT        0xFF        /* No PNP cards in system */
#define DPMI_INT          0x31
#define _PCCCI            0x02
#define _PCSNI            0x06
#define _PIDXR            0x279
#define _PNPWRP           0xA79
#define _LDSALI           0x42
#define _LDSAHI           0x50
#define _LMALI            0x43
#define _LMAHI            0x44
#define _LMCFI            0x52
#define _LMCI             0x53
#define _LDIBI            0x58
#define _LDICI            0x57
#define _LMSBAI           0x51
#define _SVCI_RD          0x8D
#define _SVCI_WR          0x0D
#define _SACI_RD          0x80
#define _SACI_WR          0x00
#define _SALI_RD          0x8B
#define _SALI_WR          0x0B
#define _SAHI_RD          0x8A
#define _SAHI_WR          0x0A
#define _SASHI_RD         0x82
#define _SASHI_WR         0x02
#define _SASLI_RD         0x83
#define _SASLI_WR         0x03
#define _SAEHI_RD         0x84
#define _SAEHI_WR         0x04
#define _SAELI_RD         0x85
#define _SAELI_WR         0x05
#define _SVRI_RD          0x86
#define _SVRI_WR          0x06
#define _SVSI_RD          0x87
#define _SVSI_WR          0x07
#define _SVEI_RD          0x88
#define _SVEI_WR          0x08
#define _SVLI_RD          0x89
#define _SVLI_WR          0x09
#define _SROI_RD          0x8C
#define _SROI_WR          0x0C
#define _SLOI_RD          0x93
#define _SLOI_WR          0x13
#define _SMSI_RD          0x95
#define _SMSI_WR          0x15
#define _SGMI_RD          0x99
#define _SGMI_WR          0x19
#define _SFCI_RD          0x81
#define _SFCI_WR          0x01
#define _SUAI_RD          0x90
#define _SUAI_WR          0x10
#define _SVII             0x8F          
#define _CMODEI           0x0C        /* index for CMODEI */
#define _CFIG3I           0x11
#define _CFIG2I           0x10
#define _CLTIMI           0x14
#define _CUTIMI           0x15
#define _CSR3I            0x18        /* Index to CSR3I (Interrupt Status) */
#define _CEXTI            0x0A        /* Index to External Control Register */
#define _CFIG1I           0x09        /* Index to Codec Conf Reg 1 */
#define _CSR2I            0x0B        /* Index to Codec Stat Reg 2 */
#define _CPDFI            0x08        /* Index to Play Data Format Reg */
#define _CRDFI            0x1C        /* Index to Rec Data Format Reg */
#define _CLMICI           0x16        /* Index to Left Mic Input Ctrl Register */
#define _CRMICI           0x17        /* Index to Right Mic Input Ctrl Register */
#define _CLCI             0x0D        /* Index to Loopback Ctrl Register */
#define _IVERI            0x5B        /* Index to register IVERI */
#define CODEC_MODE1       0x00
#define CODEC_MODE2       0x40
#define CODEC_MODE3       0x6C        /* Enhanced Mode */
#define CODEC_STATUS1     0x01
#define CODEC_STATUS2     0x0B        /* Index to CSR2I */
#define CODEC_STATUS3     0x18        /* Index to CSR3I */
#define PLAYBACK          0x01        /* Enable playback path CFIG1I[0]=1*/
#define RECORD            0x02        /* Enable Record path CFIG1I[1]=1*/
#define TIMER_ENABLE      0x40        /* CFIG2I[6] */
#define CODEC_MCE         0x40        /* CIDXR[6] */
#define CALIB_IN_PROGRESS 0x20        /* CSR2I[5] */
#define CODEC_INIT        0x80        /* CIDXR[7] */
#define BIT16_BIG         0xC0        /* 16-bit signed, big endian */
#define IMA_ADPCM         0xA0        /* IMA-compliant ADPCM */
#define BIT8_ALAW         0x60        /* 8-bit A-law */
#define BIT16_LITTLE      0x40        /* 16-bit signed, lillte endian */
#define BIT8_ULAW         0x20        /* 8-bit u-law */
#define BIT8_LINEAR       0x00        /* 8-bit unsigned */
#define REC_DFORMAT       0x1C
#define PLAY_DFORMAT      0x08
#define DMA_ACCESS        0x00
#define PIO_ACCESS        0xC0
#define DMA_SIMPLEX       0x04
#define STEREO            0x10        /* CxDFI[4] */
#define XTAL1             0x00        /* CxDFI[4]=0 selects 24.5Mhz XTAL */
#define XTAL2             0x01        /* CxDFI[4]=1 selects 16.9Mhz XTAL */
#define AUTOCALIB         0x08        /* CFIG1I[3] */
#define ROM_IO            0x02        /* ROM I/O cycles - LMCI[1]=1 */
#define DRAM_IO           0x4D        /* DRAM I/O cycles - LMCI[1]=0 */
#define AUTOI             0x01        /* LMCI[0]=1 */
#define _PLDNI            0x07
#define ACTIVATE_DEV      0x30
#define _PWAKEI           0x03        /* Index for PWAKEI */
#define _PISOCI           0x01        /* Index for PISOCI */
#define _PSECI            0xF1        /* Index for PSECI */
#define RANGE_IOCHK       0x31        /* PURCI or PRRCI Index */
#define MIDI_RESET        0x03
#define IO_OK             5           /* No IO conflict flag */
#define IO_CONFLICT       6           /* IO Conflict detected */
#define IO_0x55           0x01
#define IO_0xAA           0xFE          
/*#######################################################################*/
/**/
/* Defines for Sound Handlers in "iw".*/
/**/
/*#######################################################################*/
#define PLAY_DMA_HANDLER     0x01
#define REC_DMA_HANDLER      0x02
#define MIDI_TX_HANDLER      0x03
#define MIDI_RX_HANDLER      0x04
#define TIMER1_HANDLER       0x05
#define TIMER2_HANDLER       0x06
#define WAVE_HANDLER         0x07
#define VOLUME_HANDLER       0x08
#define CODEC_TIMER_HANDLER  0x09
#define CODEC_PLAY_HANDLER   0x0A
#define CODEC_REC_HANDLER    0x0B
#define AUX_HANDLER          0x0C
/*#######################################################################*/
/**/
/*     Mapping for System Control Regs.*/
/**/
/*#######################################################################*/
#define UMCR            0x00010000    /* Mix Control Reg.*/
#define UISR            0x00020006    /* IRQ Stat Reg. (read) */
#define U2X6R           0x00030006    /* SB 2X6 reg */
#define UACWR           0x00040008    /* AdLib Command Write Reg */
#define UASRR           0x00050008    /* AdLib Stat Read Reg */
#define UADR            0x00060009    /* AdLib Data Register */
#define UACRR           0x0007000A    /* AdLib Cmd Read Reg */
#define UASWR           0x0008000A    /* AdLib Stat Write Reg */
#define UHRDP           0x0009000B    /* Hidden Reg Data Port */
#define UI2XCR          0x000A000C    /* SB IRQ 2xC Reg */
#define U2XCR           0x000B000D    /* SB 2xC Reg. (No IRQ) */
#define U2XER           0x000C000E    /* SB 2xE Reg. */
#define URCR            0x000D000F    /* Reg Control Register */
#define USRR            0x000E000F    /* Status Read Register */
#define UDCI            0x000F000B    /* DMA Channel Control Reg */
#define UICI            0x0010000B    /* Interrupt Ctrl Reg */
#define UGP1I           0x0011010B    /* GP Reg 1 (Back Door) */
#define UGP2I           0x0012020B    /* GP Reg 2 (Back Door) */
#define UGPA1I          0x0013030B    /* GP reg 1 Address */
#define UGPA2I          0x0014040B    /* GP reg 2 Address */
#define UCLRII          0x0015050B    /* Clear Interrupt Reg */
#define UJMPI           0x0016060B    /* Jumper Register */
#define UGP1II          0x0017000B    /* Gen. Purp Reg 1(Emulation) */
#define UGP2II          0x0018000B    /* Gen. Purp Reg 2(Emulation) */
#define GGCR            0x00190201    /* Game Control Register */
#define GMCR            0x001A0000    /* MIDI Control Register */
#define GMSR            0x001B0000    /* MIDI Status Reg. */
#define GMTDR           0x001C0001    /* MIDI xmit data reg */
#define GMRDR           0x001D0001    /* MIDI rcv data reg */
#define SVSR            0x001E0002    /* Synth Voice Select Reg */
#define IGIDXR          0x001F0003    /* General Index Register */
#define I16DP           0x00200004    /* General 16-bit Data Port */
#define I8DP            0x00210005    /* General 8-bit Data Port */
/*#######################################################################*/
/**/
/*             Synth defines*/
/**/
/*#######################################################################*/
#define SACI            0x00220005    /* Synth Addr Control */
#define SFCI            0x00230104    /* Synth Freq Control */
#define SASHI           0x00240204    /* Synth Addr Start High */
#define SASLI           0x00250304    /* Synth Addr Start Low */
#define SAEHI           0x00260404    /* Synth Addr End High */
#define SAELI           0x00270504    /* Synth Addr End Low */
#define SVRI            0x00280605    /* Synth Volume Rate */
#define SVSI            0x00290705    /* Synth Volume Start */
#define SVEI            0x002A0805    /* Synth Volume End */
#define SVLI            0x002B0904    /* Synth Volume Level */
#define SAHI            0x002C0A04    /* Synth Address High */
#define SALI            0x002D0B04    /* Synth Address Low */
#define SROI            0x002E0C04    /* Synth Right Offset */
#define SVCI            0x002F0D05    /* Synth Volume Control */
#define SAVI            0x00300E05    /* Synth Active Voices */
#define SVII            0x00318F05    /* Synth Voice IRQ */
#define SUAI            0x00321005    /* Synth Upper Addr */
#define SEAHI           0x00331104    /* Synth Effect Addr High */
#define SEALI           0x00341204    /* Synth Effect Addr Low */
#define SLOI            0x00351304    /* Synth Left Offset */
#define SEASI           0x00361405    /* Synth Effects Accum Sel */
#define SMSI            0x00371505    /* Synth Mode Select */
#define SEVI            0x00381604    /* Synth Effect Volume */
#define SFLFOI          0x00391705    /* Synth Freq LFO */
#define SVLFOI          0x003A1805    /* Synth Vol LFO */
#define SGMI            0x003B1905    /* Synth Global Mode */
#define SLFOBI          0x003C1A04    /* Synth LFO Base Address */
#define SROFI           0x003D1B04
#define SLOFI           0x003E1C04
#define SEVFI           0x003F1D04
#define SVIRI           0x00409F05    /* Synth Voice Read IRQ */
#define LDMACI          0x00414105    /* DMA Control Reg. */
#define LDSALI          0x00424204    /* LMC DMA Start Addr. Low Reg. */
#define LMALI           0x00434304    /* LMC Addr Low (I/O) */
#define LMAHI           0x00444405    /* LMC Addr High (I/O) */
#define UASBCI          0x00454505    /* Adlib-SB Control */
#define UAT1I           0x00464605    /* AdLib Timer 1 Count */
#define UAT2I           0x00474705    /* AdLib Timer 2 Count */
#define USCI            0x00484905    /* Sample Control Reg */
#define GJTDI           0x00494B05
#define URSTI           0x004A4C05
#define LDSAHI          0x004B5005
#define LMSBAI          0x004C5104
#define LMCFI           0x004D5204
#define LMCI            0x004E5305
#define LMRFAI          0x004F5404
#define LMPFAI          0x00505504
#define LMSFI           0x00515604
#define LDICI           0x00525704
#define LDIBI           0x00535804
#define ICMPTI          0x00545905
#define IDECI           0x00555A05
#define IVERI           0x00565B05
#define IEMUAI          0x00575C05
#define IEMUBI          0x00585D05
#define GMRFAI          0x00595E05
#define ITCI            0x005A5F05
#define IEIRQI          0x005B6005
#define LMBDR           0x005C0007
/*##########################################################*/
/* Mnemonics for Codec Registers*/
/*##########################################################*/
#define CIDXR           0x005D0000
#define CDATAP          0x005E0001
#define CSR1R           0x005F0002
#define CPDR            0x00600003
#define CRDR            0x00610003
#define CLICI           0x00620001
#define CRICI           0x00630101
#define CLAX1I          0x00640201
#define CRAX1I          0x00650301
#define CLAX2I          0x00660401
#define CRAX2I          0x00670501
#define CLDACI          0x00680601
#define CRDACI          0x00690701
#define CPDFI           0x006A0801
#define CFIG1I          0x006B0901
#define CEXTI           0x006C0A01
#define CSR2I           0x006D0B01
#define CMODEI          0x006E0C01
#define CLCI            0x006F0D01
#define CUPCTI          0x00700E01
#define CLPCTI          0x00710F01
#define CFIG2I          0x00721001
#define CFIG3I          0x00731101
#define CLLICI          0x00741201
#define CRLICI          0x00751301
#define CLTIMI          0x00761401
#define CUTIMI          0x00771501
#define CLMICI          0x00781601
#define CRMICI          0x00791701
#define CSR3I           0x007A1801
#define CLOAI           0x007B1901
#define CMONOI          0x007C1A01
#define CROAI           0x007D1B01
#define CRDFI           0x007E1C01
#define CPVFI           0x007F1D01
#define CURCTI          0x00801E01
#define CLRCTI          0x00811F01
/*##########################################################*/
/* Mnemonics for PnP Registers*/
/*##########################################################*/
#define PCSNBR          0x00820201
#define PIDXR           0x00830279
#define PNPWRP          0x00840A79
#define PNPRDP          0x00850000
#define PSRPAI          0x00860000
#define PISOCI          0x00870100
#define PCCCI           0x00880200
#define PWAKEI          0x00890300
#define PRESDI          0x008A0400
#define PRESSI          0x008B0500
#define PCSNI           0x008C0600
#define PLDNI           0x008D0700
#define PUACTI          0x008E3000
#define PURCI           0x008F3100
#define P2X0HI          0x00906000
#define P2X0LI          0x00916100
#define P3X0HI          0x00926200
#define P3X0LI          0x00936300
#define PHCAI           0x00946400
#define PLCAI           0x00956500
#define PUI1SI          0x00967000
#define PUI1TI          0x00977100
#define PUI2SI          0x00987200
#define PUI2TI          0x00997300
#define PUD1SI          0x009A7400
#define PUD2SI          0x009B7500
#define PSEENI          0x009CF000
#define PSECI           0x009DF100
#define PPWRI           0x009EF200
#define PRACTI          0x009F3001
#define PRRCI           0x00A03101
#define PRAHI           0x00A16001
#define PRALI           0x00A26101
#define PATAHI          0x00A36201
#define PATALI          0x00A46301
#define PRISI           0x00A57001
#define PRITI           0x00A67101
#define PRDSI           0x00A77401
#define PGACTI          0x00A83002
#define PGRCI           0x00A93102
#define P201HI          0x00AA6002
#define P201LI          0x00AB6102
#define PSACTI          0x00AC3003
#define PSRCI           0x00AD3103
#define P388HI          0x00AE6003
#define P388LI          0x00AF6103
#define PSBISI          0x00B07003
#define PSBITI          0x00B17103
#define PMACTI          0x00B23004
#define PMRCI           0x00B33104
#define P401HI          0x00B46004
#define P401LI          0x00B56104
#define PMISI           0x00B67004
#define PMITI           0x00B77104

typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned short PORT;
typedef unsigned long  DWORD;
typedef unsigned long  ADDRESS;
typedef int BOOL;
typedef int FLAG;

typedef struct
 {
	short flags;              /* InterWave stat flags */
	PORT pcodar;                      /* Base Port for Codec */
	PORT pcdrar;                      /* Base Port for Ext Device */
	PORT p2xr;                        /* Compatibility Base Port */
	PORT p3xr;                        /* MIDI and Synth Base Port */
	PORT p401ar;                      /* Gen Purpose Reg. 1 address */
	PORT p201ar;                      /* Game Ctrl normally at 0x201 */
	PORT pataar;                      /* Base Address for ATAPI I/O Space */
	PORT p388ar;                      /* Base Port for AdLib. It should be 388h */
	PORT pnprdp;                      /* PNP read data port */
	PORT igidxr;                      /* Gen Index Reg at P3XR+0x03 */
	PORT i16dp;                       /* 16-bit data port at P3XR+0x04  */
	PORT i8dp;                        /* 8-bit data port at P3XR+0x05 */
	PORT svsr;                        /* Synth Voice Select at P3XR+0x02 */
	PORT cdatap;                      /* Codec Indexed Data Port at PCODAR+0x01 */
	PORT csr1r;                       /* Codec Stat Reg 1 at PCODAR+0x02 */
	PORT cxdr;                        /* Play or Record Data Reg at PCODAR+0x03 */
	PORT gmxr;                        /* GMCR or GMSR at P3XR+0x00 */
	PORT gmxdr;                       /* GMTDR or GMRDR at P3XR+0x01 */
	PORT lmbdr;                       /* LMBDR at P3XR+0x07 */
	BYTE csn;                         /* Card Select Number */
	BYTE cmode;                       /* Codec Operation Mode */
	int dma1_chan;                    /* DMA channel 1 (local DMA & codec rec) */
	int dma2_chan;                    /* DMA channel 2 (codec play) */
	int ext_chan;                     /* Ext Dev DMA channel */
	BYTE voices;                      /* Number of active voices */
	DWORD vendor;                     /* Vendor ID and Product Identifier */
	int synth_irq;                    /* Synth IRQ number */
	int midi_irq;                     /* MIDI IRQ number */
	int ext_irq;                      /* Ext Dev IRQ  */
	int mpu_irq;                      /* MPU401 Dev IRQ */
	int emul_irq;                     /* Sound Blaster/AdLib Dev IRQ */
	ADDRESS  free_mem;                /* Address of First Free LM Block */
	DWORD reserved_mem;               /* Amount of LM reserved by app. */
	BYTE smode;                       /* Synth Mode */
	WORD  size_mem;                   /* Total LM in Kbytes */

 } IWAVE;

#endif
