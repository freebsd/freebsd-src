/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_q932fac.c - Q932 facility handling
 *	--------------------------------------
 *
 *	$Id: i4b_q932fac.c,v 1.8 1999/12/13 21:25:27 hm Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer3/i4b_q932fac.c,v 1.6 1999/12/14 20:48:32 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 22:05:51 1999]
 *
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__
#include "i4bq931.h"
#else
#define	NI4BQ931	1
#endif
#if NI4BQ931 > 0

#include <sys/param.h>

#if defined(__FreeBSD__)
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif

#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/include/i4b_isdnq931.h>
#include <i4b/include/i4b_l2l3.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer3/i4b_l3.h>
#include <i4b/layer3/i4b_l3fsm.h>
#include <i4b/layer3/i4b_q931.h>
#include <i4b/layer3/i4b_q932fac.h>

#include <i4b/layer4/i4b_l4.h>

static int do_component(int length);
static void next_state(int class, int form, int code, int val);

static int byte_len;
static unsigned char *byte_buf;
static int state;

static int units;
static int operation_value;

/*---------------------------------------------------------------------------*
 *	decode Q.931/Q.932 facility info element
 *---------------------------------------------------------------------------*/
int
i4b_aoc(unsigned char *buf, call_desc_t *cd)
{
	int len;

	cd->units_type = CHARGE_INVALID;
	cd->units = -1;			
	
	buf++;		/* length */

	len = *buf;

	buf++;		/* protocol profile */

	switch(*buf & 0x1f)
	{
		case FAC_PROTO_ROP:
			break;
			
		case FAC_PROTO_CMIP:
			DBGL3(L3_A_MSG, "i4b_facility", ("CMIP Protocol (Q.941), UNSUPPORTED\n"));
			return(-1);
			break;

		case FAC_PROTO_ACSE:
			DBGL3(L3_A_MSG, "i4b_facility", ("ACSE Protocol (X.217/X.227), UNSUPPORTED!\n"));
			return(-1);
			break;

		default:
			DBGL3(L3_A_ERR, "i4b_facility", ("Unknown Protocol, UNSUPPORTED!\n"));
			return(-1);
			break;
	}

	DBGL3(L3_A_MSG, "i4b_facility", ("Remote Operations Protocol\n"));

	/* next byte */
	
	buf++;
	len--;

	/* initialize variables for do_component */
	
	byte_len = 0;
	byte_buf = buf;
	state = ST_EXP_COMP_TYP;	

	/* decode facility */
	
	do_component(len);

	switch(operation_value)
	{
		case FAC_OPVAL_AOC_D_CUR:
			cd->units_type = CHARGE_AOCD;
			cd->units = 0;
			return(0);
			break;
			
		case FAC_OPVAL_AOC_D_UNIT:
			cd->units_type = CHARGE_AOCD;
			cd->units = units;
			return(0);
			break;
			
		case FAC_OPVAL_AOC_E_CUR:
			cd->units_type = CHARGE_AOCE;
			cd->units = 0;
			return(0);
			break;
			
		case FAC_OPVAL_AOC_E_UNIT:
			cd->units_type = CHARGE_AOCE;
			cd->units = units;
			return(0);
			break;

		default:
			cd->units_type = CHARGE_INVALID;
			cd->units = -1;
			return(-1);
			break;
	}
	return(-1);	
}

/*---------------------------------------------------------------------------*
 *	handle a component recursively
 *---------------------------------------------------------------------------*/
static int
do_component(int length)
{
	int comp_tag_class;	/* component tag class */
	int comp_tag_form;	/* component form: constructor or primitive */
	int comp_tag_code;	/* component code depending on class */
	int comp_length = 0;	/* component length */

again:

	/*----------------------------------------*/
	/* first component element: component tag */
	/*----------------------------------------*/
	
	/* tag class bits */

	comp_tag_class = (*byte_buf & 0xc0) >> 6;
	
	switch(comp_tag_class)
	{
		case FAC_TAGCLASS_UNI:
			break;
		case FAC_TAGCLASS_APW:
			break;
		case FAC_TAGCLASS_COS:
			break;
		case FAC_TAGCLASS_PRU:
			break;
	}

	/* tag form bit */

	comp_tag_form = (*byte_buf & 0x20) > 5;
	
	/* tag code bits */

	comp_tag_code = *byte_buf & 0x1f;
	
	if(comp_tag_code == 0x1f)
	{
		comp_tag_code = 0;
		
		byte_buf++;
		byte_len++;

		while(*byte_buf & 0x80)
		{
			comp_tag_code += (*byte_buf & 0x7f);
			byte_buf++;
			byte_len++;
		}
		comp_tag_code += (*byte_buf & 0x7f);
	}
	else
	{
		comp_tag_code = (*byte_buf & 0x1f);
	}

	byte_buf++;
	byte_len++;
	
	/*--------------------------------------------*/
	/* second component element: component length */
	/*--------------------------------------------*/
	
	comp_length = 0;
	
	if(*byte_buf & 0x80)
	{
		int i = *byte_buf & 0x7f;

		byte_len += i;
		
		for(;i > 0;i++)
		{
			byte_buf++;
			comp_length += (*byte_buf * (i*256));
		}	
	}
	else
	{
		comp_length = *byte_buf & 0x7f;
	}

	next_state(comp_tag_class, comp_tag_form, comp_tag_code, -1);
	
	byte_len++;
	byte_buf++;
	
	/*---------------------------------------------*/
	/* third component element: component contents */
	/*---------------------------------------------*/

	if(comp_tag_form)	/* == constructor */
	{
		do_component(comp_length);
	}
	else 
	{
		int val = 0;		
		if(comp_tag_class == FAC_TAGCLASS_UNI)
		{
			switch(comp_tag_code)
			{
				case FAC_CODEUNI_INT:
				case FAC_CODEUNI_ENUM:
				case FAC_CODEUNI_BOOL:				
					if(comp_length)
					{
						int i;
				
						for(i = comp_length-1; i >= 0; i--)
						{
							val += (*byte_buf + (i*255));
							byte_buf++;
							byte_len++;
						}
					}
					break;
				default:	
					if(comp_length)
					{
						int i;
				
						for(i = comp_length-1; i >= 0; i--)
						{
							byte_buf++;
							byte_len++;
						}
					}
					break;
			}
		}

		else	/* comp_tag_class != FAC_TAGCLASS_UNI */
		{
			if(comp_length)
			{
				int i;
		
				for(i = comp_length-1; i >= 0; i--)
				{
					val += (*byte_buf + (i*255)); 
					byte_buf++;
					byte_len++;
				}
			}
		}
		next_state(comp_tag_class, comp_tag_form, comp_tag_code, val);
	}

	if(byte_len < length)
		goto again;

	return(byte_len);
}

/*---------------------------------------------------------------------------*
 *	invoke component
 *---------------------------------------------------------------------------*/
static void
F_1_1(int val)
{
	if(val == -1)
	{
		state = ST_EXP_INV_ID;
	}
}

/*---------------------------------------------------------------------------*
 *	return result
 *---------------------------------------------------------------------------*/
static void
F_1_2(int val)
{
	if(val == -1)
		state = ST_EXP_NIX;
}
/*---------------------------------------------------------------------------*
 *	return error
 *---------------------------------------------------------------------------*/
static void
F_1_3(int val)
{
	if(val == -1)
		state = ST_EXP_NIX;
}
/*---------------------------------------------------------------------------*
 *	reject
 *---------------------------------------------------------------------------*/
static void
F_1_4(int val)
{
	if(val == -1)
		state = ST_EXP_NIX;
}

/*---------------------------------------------------------------------------*
 *	invoke id
 *---------------------------------------------------------------------------*/
static void
F_2(int val)
{
	if(val != -1)
	{
		DBGL3(L3_A_MSG, "i4b_facility", ("Invoke ID = %d\n", val));
		state = ST_EXP_OP_VAL;
	}
}

/*---------------------------------------------------------------------------*
 *	operation value
 *---------------------------------------------------------------------------*/
static void
F_3(int val)
{
	if(val != -1)
	{
		DBGL3(L3_A_MSG, "i4b_facility", ("Operation Value = %d\n", val));
	
		operation_value = val;
		
		if((val == FAC_OPVAL_AOC_D_UNIT) || (val == FAC_OPVAL_AOC_E_UNIT))
		{
			units = 0;
			state = ST_EXP_INFO;
		}
		else
		{
			state = ST_EXP_NIX;
		}
	}
}

/*---------------------------------------------------------------------------*
 *	specific charging units
 *---------------------------------------------------------------------------*/
static void
F_4(int val)
{
	if(val == -1)
		state = ST_EXP_RUL;
}

/*---------------------------------------------------------------------------*
 *	free of charge
 *---------------------------------------------------------------------------*/
static void
F_4_1(int val)
{
	if(val == -1)
	{
		DBGL3(L3_A_MSG, "i4b_facility", ("Free of Charge\n"));
		/* units = 0; XXXX */
		state = ST_EXP_NIX;
	}
}

/*---------------------------------------------------------------------------*
 *	charge not available
 *---------------------------------------------------------------------------*/
static void
F_4_2(int val)
{
	if(val == -1)
	{
		DBGL3(L3_A_MSG, "i4b_facility", ("Charge not available\n"));
		/* units = -1; 	XXXXXX ??? */
		state = ST_EXP_NIX;
	}
}

/*---------------------------------------------------------------------------*
 *	recorded units list
 *---------------------------------------------------------------------------*/
static void
F_5(int val)
{
	if(val == -1)
		state = ST_EXP_RU;
}

/*---------------------------------------------------------------------------*
 *	recorded units
 *---------------------------------------------------------------------------*/
static void
F_6(int val)
{
	if(val == -1)
		state = ST_EXP_RNOU;
}

/*---------------------------------------------------------------------------*
 *	number of units
 *---------------------------------------------------------------------------*/
static void
F_7(int val)
{
	if(val != -1)
	{
		DBGL3(L3_A_MSG, "i4b_facility", ("Number of Units = %d\n", val));
		units = val;
		state = ST_EXP_TOCI;
	}
}

/*---------------------------------------------------------------------------*
 *	subtotal/total
 *---------------------------------------------------------------------------*/
static void
F_8(int val)
{
	if(val != -1)
	{
		DBGL3(L3_A_MSG, "i4b_facility", ("Subtotal/Total = %d\n", val));
		/* type_of_charge = val; */
		state = ST_EXP_DBID;
	}
}

/*---------------------------------------------------------------------------*
 *	billing_id
 *---------------------------------------------------------------------------*/
static void
F_9(int val)
{
	if(val != -1)
	{
		DBGL3(L3_A_MSG, "i4b_facility", ("Billing ID = %d\n", val));
		/* billing_id = val; */
		state = ST_EXP_NIX;
	}
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static struct statetab {
	int currstate;		/* input: current state we are in */
	int form;		/* input: current tag form */
	int class;		/* input: current tag class */
	int code;		/* input: current tag code */
	void (*func)(int);	/* output: func to exec */
} statetab[] = {

/*	 current state		tag form		tag class		tag code		function	*/
/*	 ---------------------  ----------------------  ----------------------  ---------------------- 	----------------*/
	{ST_EXP_COMP_TYP,	FAC_TAGFORM_CON,	FAC_TAGCLASS_COS,	1,			F_1_1		},
	{ST_EXP_COMP_TYP,	FAC_TAGFORM_CON,	FAC_TAGCLASS_COS,	2,			F_1_2		},
	{ST_EXP_COMP_TYP,	FAC_TAGFORM_CON,	FAC_TAGCLASS_COS,	3,			F_1_3		},
	{ST_EXP_COMP_TYP,	FAC_TAGFORM_CON,	FAC_TAGCLASS_COS,	4,			F_1_4		},
	{ST_EXP_INV_ID,		FAC_TAGFORM_PRI,	FAC_TAGCLASS_UNI,	FAC_CODEUNI_INT,	F_2		},
	{ST_EXP_OP_VAL,		FAC_TAGFORM_PRI,	FAC_TAGCLASS_UNI,	FAC_CODEUNI_INT,	F_3		},
	{ST_EXP_INFO,		FAC_TAGFORM_CON,	FAC_TAGCLASS_UNI,	FAC_CODEUNI_SEQ,	F_4		},
	{ST_EXP_INFO,		FAC_TAGFORM_PRI,	FAC_TAGCLASS_UNI,	FAC_CODEUNI_NULL,	F_4_1		},
	{ST_EXP_INFO,		FAC_TAGFORM_PRI,	FAC_TAGCLASS_COS,	1,			F_4_2		},
	{ST_EXP_RUL,		FAC_TAGFORM_CON,	FAC_TAGCLASS_COS,	1,			F_5		},
	{ST_EXP_RU,		FAC_TAGFORM_CON,	FAC_TAGCLASS_UNI,	FAC_CODEUNI_SEQ,	F_6		},
	{ST_EXP_RNOU,		FAC_TAGFORM_PRI,	FAC_TAGCLASS_UNI,	FAC_CODEUNI_INT,	F_7		},
	{ST_EXP_TOCI,		FAC_TAGFORM_PRI,	FAC_TAGCLASS_COS,	2,			F_8		},
	{ST_EXP_DBID,		FAC_TAGFORM_PRI,	FAC_TAGCLASS_COS,	3,			F_9		},
	{-1,			-1,			-1,			-1,			NULL		}
};	
	
/*---------------------------------------------------------------------------*
 *	state decode for do_component
 *---------------------------------------------------------------------------*/
static void
next_state(int class, int form, int code, int val)
{
	int i;

	for(i=0; ; i++)
	{
		if((statetab[i].currstate > state) ||
		   (statetab[i].currstate == -1))
		{
			break;
		}

		if((statetab[i].currstate == state) 	&&
		   (statetab[i].form == form)		&&
		   (statetab[i].class == class)		&&
		   (statetab[i].code == code))
		{
			(*statetab[i].func)(val);
			break;
		}
	}
}

#endif /* NI4BQ931 > 0 */
