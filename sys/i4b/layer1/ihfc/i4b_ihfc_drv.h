/*
 * Copyright (c) 2000 Hans Petter Selasky. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	i4b_ihfc_drv.h - include file for the HFC-1/S/SP driver
 *	-------------------------------------------------------
 *
 *	last edit-date: [Wed Jul 19 09:40:55 2000]
 *
 *	$Id: i4b_ihfc_drv.h,v 1.7 2000/09/19 13:50:36 hm Exp $
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/
#ifndef	I4B_IHFC_DRV_H_
#define I4B_IHFC_DRV_H_

/*---------------------------------------------------------------------------*
 *	Ramptables related fifo					(HFC-1/S/SP)
 *
 *	The HFC-SP chip only uses ihfc_xxx[2] values for D-channel!
 *	NOTE: These tables are not used anymore.
 *---------------------------------------------------------------------------*
 * 
 *             w - write, r - read:   D1_w  D1_r  B1_w  B1_r  B2_w  B2_r
 * const u_char ihfc_readtable[6]  = {0xa6, 0xa7, 0xbc, 0xbd, 0xbe, 0xbf};
 * const u_char ihfc_writetable[6] = {0x96, 0x97, 0xac, 0xad, 0xae, 0xaf};
 * const u_char ihfc_f1inctable[6] = {0x92, 0x93, 0xa8, 0xa9, 0xaa, 0xab};	
 * const u_char ihfc_f2inctable[6] = {0xa2, 0xa3, 0xb8, 0xb9, 0xba, 0xbb};
 * 
 * const	struct { u_char z1L, z1H, z2L, z2H, f1, f2, dummy; } 
 * 	ihfc_countertable[6] = {
 * 	{0x90, 0x94, 0x98, 0x9c, 0x9a, 0x9e, 0x00},	D1_w
 *	{0x91, 0x95, 0x99, 0x9d, 0x9b, 0x9f, 0x00},	D1_r
 * 	{0x80, 0x84, 0x88, 0x8c, 0xb0, 0xb4, 0x00},	B1_w
 * 	{0x81, 0x85, 0x89, 0x8d, 0xb1, 0xb5, 0x00},	B1_r
 * 	{0x82, 0x86, 0x8a, 0x8e, 0xb2, 0xb6, 0x00},	B2_w
 * 	{0x83, 0x87, 0x8b, 0x8f, 0xb3, 0xb7, 0x00}	B2_r
 * 	};
 *---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 *	Ramptables related to configuration			(HFC-1/S/SP)
 *
 *	NOTE: Write registers only
 *---------------------------------------------------------------------------*/
const u_char ihfc_configtable[11] =
{
	0x18, 0x19, 0x1a,	/* cirm, ctmt, int_m1		*/
	0x1b, 0x2e, 0x37,	/* int_m2, mst_mode, clkdel	*/
	0x31, 0x2f, 0x32,	/* sctrl, connect, test/sctrl_e */
	0x33, 0x00		/* sctrl_r			*/
};
const u_char isac_configtable[9] =
{
	0x39, 0x30, 0x3b,	/* adf2, spcr, sqxr	*/
	0x38, 0x37, 0x22,	/* adf1, stcr, mode	*/
	0x20, 0x2b, 0x00	/* mask, star2		*/
};

/*---------------------------------------------------------------------------*
 *	Ramptables related to statemachine			(HFC-1/S/SP)
 *
 * state:
 *	0 = deactivated
 * 	1 = pending
 *	2 = syncronized
 *	3 = activated
 *	4 = error
 *	5 = reset
 *     -1 = illegal
 *---------------------------------------------------------------------------*/

const struct ihfc_FSMtable { u_char state, *string; } 

	ihfc_TEtable[16] = 	/* HFC-S/SP	- TE */
{
	{ 0x05 ,"Reset"					},
	{ 0xff , 0 					},
	{ 0x01 ,"Sensing"				},
	{ 0x00 ,"Deactivated"				},
	{ 0x01 ,"Awaiting signal"			},
	{ 0x01 ,"Identifying input"			},
	{ 0x02 ,"Syncronized"				},
	{ 0x03 ,"Activated"				},
	{ 0x04 ,"Lost framing"				},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0	 				},
	{ 0xff , 0 					},
	{ 0xff , 0 					}
}, 
	ihfc_NTtable[16] = 	/* HFC-S/SP	- NT */
{
	{ 0x05 ,"Reset"					},
	{ 0x00 ,"Deactive"				},
	{ 0x02 ,"Pending activation"			},
	{ 0x03 ,"Active"				},
	{ 0x01 ,"Pending deactivation"			},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					}
},
	ihfc_TEtable2[16] =	/* HFC-1/ISAC 	- TE */
{
	{ 0x00 ,"Deactivate request"			},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0xff , 0 					},
	{ 0x01 ,"Level detected"			},
	{ 0xff , 0 					},
	{ 0x04 ,"Error indication"			},
	{ 0x00 ,"Power-up"		 		},
	{ 0x02 ,"Activate request downstream" 		},
	{ 0xff , 0 					},
	{ 0x00 ,"Test indication"			},
	{ 0x00 ,"Awake test indication"			},
	{ 0x03 ,"Activate ind. with priority class 8" 	},
	{ 0x03 ,"Activate ind. with priority class 10"	},
	{ 0xff , 0 					},
	{ 0x00 ,"Deactivate indication downstream" 	}
};

/*---------------------------------------------------------------------------*
 *	Ramptable related to ISAC EXIR				(HFC-1)
 *
 *	cmd: command to execute, if any.
 *
 *---------------------------------------------------------------------------*/
const struct ihfc_EXIRtable { u_char cmd, *string; }

	ihfc_EXIRtable[8] =
{
	{ 0x00 ,"Watchdog Timer Overflow"		},
	{ 0x00 ,"Subscriber Awake"			},
	{ 0x00 ,"Monitor Status"			},
	{ 0x00 ,"Rx Sync Xfer Overflow"			},
	{ 0xc0 ,"Rx Frame Overflow"			}, /* RMC + RRES */
	{ 0x00 ,"Protocol Error"			},
	{ 0x01 ,"Tx Data Underrun"			}, /* XRES */
	{ 0x01 ,"Tx Message Repeat"			}, /* XRES */
};

/*---------------------------------------------------------------------------*
 *	Ramptables related to S/Q - channel			(HFC-1/S/SP)
 *
 *	From TE's viewpoint:
 *	Q: commands to NT
 *	S: indications from NT
 *
 *	From NT's viewpoint:
 *	Q: indications from TE
 *	S: commands to TE
 *	
 *	cmd: not used
 *---------------------------------------------------------------------------*/
const struct ihfc_SQtable { u_char cmd, *string; }

	ihfc_Qtable[16] =
{
	{ 0x00, "Loss of Power indication"		},
	{ 0x00, "ST request"				},
	{ 0x00, 0					},
	{ 0x00, "LoopBack request (B1/B2)"		},
	{ 0x00, 0					},
	{ 0x00, 0					},
	{ 0x00, 0					},
	{ 0x00, "LoopBack request (B1)"			},
	{ 0x00, 0					},
	{ 0x00, 0					},
	{ 0x00, 0					},
	{ 0x00, "LoopBack request (B2)"			},
	{ 0x00, "V-DCE slave mode"			},
	{ 0x00, "V-DTE slave mode"			},
	{ 0x00, 0					},
	{ 0x00, "Idle"					}
},
	ihfc_Stable[16] =
{
	{ 0x00, "Idle"					},
	{ 0x00, "ST Fail"				},
	{ 0x00, "ST Pass"				},
	{ 0x00, "Disruptive Operation Indication"	},
	{ 0x00, "DTSE-OUT"				},
	{ 0x00, "V-DCE master mode"			},
	{ 0x00, "ST Indication"				},
	{ 0x00, "DTSE-IN"				},
	{ 0x00, "LoopBack indication (B1/B2)"		},
	{ 0x00, "Loss of Received Signal indication"	},
	{ 0x00, "LoopBack indication (B2)"		},
	{ 0x00, "DTSE-IN and OUT"			},
	{ 0x00, "LoopBack indication (B1)"		},
	{ 0x00, "Loss of power indication"		}
};


#endif /* I4B_IHFC_DRV_H_ */

