/*
 * sound/awe_hw.h
 *
 * Access routines and definitions for the low level driver for the 
 * AWE32/Sound Blaster 32 wave table synth.
 *   version 0.2.0a; Oct. 30, 1996
 *
 * (C) 1996 Takashi Iwai
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef AWE_HW_H_DEF
#define AWE_HW_H_DEF

/*
 * user configuration:
 * if auto detection can't work properly, define the following values
 * for your machine.
 */
/*#define AWE_DEFAULT_BASE_ADDR	0x620*/	/* base port address */
/*#define AWE_DEFAULT_MEM_SIZE	512*/	/* kbytes */


/*
 * maximum size of sample table:
 * if your data overflow, increase the following values.
 */
#define AWE_MAX_SAMPLES		400
#define AWE_MAX_INFOS		900	/* GS presets has 801 infos! */


/*
 * Emu-8000 control registers
 * name(channel)	reg, port
 */

#define awe_cmd_idx(reg,ch)	(((reg)<< 5) | (ch))

#define Data0    0x620	/* doubleword r/w */
#define Data1    0xA20	/* doubleword r/w */
#define Data2    0xA22	/* word r/w */
#define Data3    0xE20	/* word r/w */
#define Pointer  0xE22	/* register pointer r/w */

#define AWE_CPF(ch)	awe_cmd_idx(0,ch), Data0	/* DW: current pitch and fractional address */
#define AWE_PTRX(ch)	awe_cmd_idx(1,ch), Data0	/* DW: pitch target and reverb send */
#define AWE_CVCF(ch)	awe_cmd_idx(2,ch), Data0	/* DW: current volume and filter cutoff */
#define AWE_VTFT(ch)	awe_cmd_idx(3,ch), Data0	/* DW: volume and filter cutoff targets */
#define AWE_PSST(ch)	awe_cmd_idx(6,ch), Data0	/* DW: pan send and loop start address */
#define AWE_CSL(ch)	awe_cmd_idx(7,ch), Data0	/* DW: chorus send and loop end address */
#define AWE_CCCA(ch)	awe_cmd_idx(0,ch), Data1	/* DW: Q, control bits, and current address */
#define AWE_HWCF4	awe_cmd_idx(1,9),  Data1	/* DW: config dw 4 */
#define AWE_HWCF5	awe_cmd_idx(1,10), Data1	/* DW: config dw 5 */
#define AWE_HWCF6	awe_cmd_idx(1,13), Data1	/* DW: config dw 6 */
#define AWE_SMALR	awe_cmd_idx(1,20), Data1	/* DW: sound memory address for left read */
#define AWE_SMARR	awe_cmd_idx(1,21), Data1	/* DW:    for right read */
#define AWE_SMALW	awe_cmd_idx(1,22), Data1	/* DW: sound memory address for left write */
#define AWE_SMARW	awe_cmd_idx(1,23), Data1	/* DW:    for right write */
#define AWE_SMLD	awe_cmd_idx(1,26), Data1	/* W: sound memory left data */
#define AWE_SMRD	awe_cmd_idx(1,26), Data2	/* W:    right data */
#define AWE_WC		awe_cmd_idx(1,27), Data2	/* W: sample counter */
#define AWE_WC_Cmd	awe_cmd_idx(1,27)
#define AWE_WC_Port	Data2
#define AWE_HWCF1	awe_cmd_idx(1,29), Data1	/* W: config w 1 */
#define AWE_HWCF2	awe_cmd_idx(1,30), Data1	/* W: config w 2 */
#define AWE_HWCF3	awe_cmd_idx(1,31), Data1	/* W: config w 3 */
#define AWE_INIT1(ch)	awe_cmd_idx(2,ch), Data1	/* W: init array 1 */
#define AWE_INIT2(ch)	awe_cmd_idx(2,ch), Data2	/* W: init array 2 */
#define AWE_INIT3(ch)	awe_cmd_idx(3,ch), Data1	/* W: init array 3 */
#define AWE_INIT4(ch)	awe_cmd_idx(3,ch), Data2	/* W: init array 4 */
#define AWE_ENVVOL(ch)	awe_cmd_idx(4,ch), Data1	/* W: volume envelope delay */
#define AWE_DCYSUSV(ch)	awe_cmd_idx(5,ch), Data1	/* W: volume envelope sustain and decay */
#define AWE_ENVVAL(ch)	awe_cmd_idx(6,ch), Data1	/* W: modulation envelope delay */
#define AWE_DCYSUS(ch)	awe_cmd_idx(7,ch), Data1	/* W: modulation envelope sustain and decay */
#define AWE_ATKHLDV(ch)	awe_cmd_idx(4,ch), Data2	/* W: volume envelope attack and hold */
#define AWE_LFO1VAL(ch)	awe_cmd_idx(5,ch), Data2	/* W: LFO#1 Delay */
#define AWE_ATKHLD(ch)	awe_cmd_idx(6,ch), Data2	/* W: modulation envelope attack and hold */
#define AWE_LFO2VAL(ch)	awe_cmd_idx(7,ch), Data2	/* W: LFO#2 Delay */
#define AWE_IP(ch)	awe_cmd_idx(0,ch), Data3	/* W: initial pitch */
#define AWE_IFATN(ch)	awe_cmd_idx(1,ch), Data3	/* W: initial filter cutoff and attenuation */
#define AWE_PEFE(ch)	awe_cmd_idx(2,ch), Data3	/* W: pitch and filter envelope heights */
#define AWE_FMMOD(ch)	awe_cmd_idx(3,ch), Data3	/* W: vibrato and filter modulation freq */
#define AWE_TREMFRQ(ch)	awe_cmd_idx(4,ch), Data3	/* W: LFO#1 tremolo amount and freq */
#define AWE_FM2FRQ2(ch)	awe_cmd_idx(5,ch), Data3	/* W: LFO#2 vibrato amount and freq */

/*  used during detection (returns ROM version ?)                    */
#define AWE_U1		0xE0, Data3	  /* (R)(W) used in initialization */
#define AWE_U2(ch)	0xC0+(ch), Data3  /* (W)(W) used in init envelope  */


#define AWE_MAX_VOICES		32
#define AWE_NORMAL_VOICES	30	/*30&31 are reserved for DRAM refresh*/

#define AWE_DRAM_OFFSET		0x200000

#endif
