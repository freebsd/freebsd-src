/*
 *    telopts.h
 *  Used for telnet options
 ****************************************************************************
 *                                                                          *
 *                                                                          *
 *      NCSA Telnet                                                         *
 *      by Tim Krauskopf, VT100 by Gaige Paulsen, Tek by Aaron Contorer     *
 *	Additions by Kurt Mahan, Heeren Pathak, & Quincey Koziol            *
 *                                                                          *
 *      National Center for Supercomputing Applications                     *
 *      152 Computing Applications Building                                 *
 *      605 E. Springfield Ave.                                             *
 *      Champaign, IL  61820                                                *
 *                                                                          *
 ****************************************************************************
 *	Quincey Koziol
 *   Defines for telnet options and related things
 */

#ifndef TELOPTS_H
#define TELOPTS_H

#define NUMLMODEOPTIONS 30

/* Definitions for telnet protocol */

#define STNORM      0

/* Definition of the lowest telnet byte following an IAC byte */
#define LOW_TEL_OPT 236

#define TEL_EOF     236
#define SUSP        237
#define ABORT       238

#define SE			240
#define NOP			241
#define DM			242
#define BREAK			243
#define IP			244
#define AO			245
#define AYT			246
#define EC			247
#define EL			248
#define GOAHEAD 		249
#define SB			250
#define WILLTEL 		251
#define WONTTEL 		252
#define DOTEL	 		253
#define DONTTEL 		254
#define IAC		 	255

/* Assigned Telnet Options */
#define BINARY	 			0
#define ECHO				1
#define RECONNECT			2
#define SGA 				3
#define AMSN				4
#define STATUS				5
#define TIMING				6
#define RCTAN				7
#define OLW					8
#define OPS					9
#define OCRD				10
#define OHTS				11
#define OHTD				12
#define OFFD				13
#define OVTS				14
#define OVTD				15
#define OLFD				16
#define XASCII				17
#define LOGOUT				18
#define BYTEM				19
#define DET					20
#define SUPDUP				21
#define SUPDUPOUT			22
#define SENDLOC				23
#define TERMTYPE 			24
#define EOR					25
#define TACACSUID			26
#define OUTPUTMARK			27
#define TERMLOCNUM			28
#define REGIME3270			29
#define X3PAD				30
#define NAWS				31
#define TERMSPEED			32
#define TFLOWCNTRL			33
#define LINEMODE 			34

#define MODE 1
#define MODE_EDIT       1
#define MODE_TRAPSIG    2
#define MODE_ACK        4
#define MODE_SOFT_TAB   8
#define MODE_LIT_ECHO   16

#define FORWARDMASK 2

#define SLC 3
#define SLC_DEFAULT     3
#define SLC_VALUE       2
#define SLC_CANTCHANGE  1
#define SLC_NOSUPPORT   0
#define SLC_LEVELBITS   3

#define SLC_ACK         128
#define SLC_FLUSHIN     64
#define SLC_FLUSHOUT    32

#define SLC_SYNCH		1
#define SLC_BRK			2
#define SLC_IP			3
#define SLC_AO			4
#define SLC_AYT			5
#define SLC_EOR			6
#define SLC_ABORT		7
#define SLC_EOF			8
#define SLC_SUSP		9
#define SLC_EC			10
#define SLC_EL   		11
#define SLC_EW   		12
#define SLC_RP			13
#define SLC_LNEXT		14
#define SLC_XON			15
#define SLC_XOFF		16
#define SLC_FORW1		17
#define SLC_FORW2		18
#define SLC_MCL         19
#define SLC_MCR         20
#define SLC_MCWL        21
#define SLC_MCWR        22
#define SLC_MCBOL       23
#define SLC_MCEOL       24
#define SLC_INSRT       25
#define SLC_OVER        26
#define SLC_ECR         27
#define SLC_EWR         28
#define SLC_EBOL        29
#define SLC_EEOL        30

#define XDISPLOC		35
#define ENVIRONMENT		36
#define AUTHENTICATION	 	37
#define TELOPT_AUTHENTICATION      AUTHENTICATION
#define DATA_ENCRYPTION		38
#define XOPTIONS		255

#define LINEMODE_MODES_SUPPORTED    0x1B
/*
 * set this flag for linemode special functions which are supported by
 * Telnet, even though they are not currently active.  This is to allow
 * the other side to negotiate to a "No Support" state for an option
 * and then change later to supporting it, so we know it's ok to change
 * our "No Support" state to something else ("Can't Change", "Value",
 * whatever)
 */
#define SLC_SUPPORTED       0x10

#define ESCFOUND 5
#define IACFOUND 6
#define NEGOTIATE 1

#endif  /* telopts.h */
