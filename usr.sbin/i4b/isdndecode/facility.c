/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *	facility.c - decode Q.932 facilities
 *	------------------------------------
 *
 *	$Id: facility.c,v 1.5 2000/02/21 15:17:17 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Feb 21 16:15:43 2000]
 *
 *---------------------------------------------------------------------------
 *
 *	- Q.932 (03/93) Generic Procedures for the Control of
 *		ISDN Supplementaty Services
 *	- Q.950 (03/93) Supplementary Services Protocols, Structure and
 *		General Principles
 *	- ETS 300 179 (10/92) Advice Of Charge: charging information during
 *		the call (AOC-D) supplementary service Service description
 *	- ETS 300 180 (10/92) Advice Of Charge: charging information at the
 *		end of call (AOC-E) supplementary service Service description
 *	- ETS 300 181 (04/93) Advice Of Charge (AOC) supplementary service
 *		Functional capabilities and information flows
 *	- ETS 300 182 (04/93) Advice Of Charge (AOC) supplementary service
 *		Digital Subscriber Signalling System No. one (DSS1) protocol
 *	- X.208 Specification of Abstract Syntax Notation One (ASN.1)
 *	- X.209 Specification of Basic Encoding Rules for
 *		Abstract Syntax Notation One (ASN.1) 
 *	- "ASN.1 Abstract Syntax Notation One", Walter Gora, DATACOM-Verlag
 *		1992, 3rd Edition (ISBN 3-89238-062-7) (german !)
 *
 *---------------------------------------------------------------------------*/

#include "decode.h"
#include "facility.h"

static int do_component(int length, char *pbuf);
static char *uni_str(int code);
static char *opval_str(int val);
static char *bid_str(int val);
static void next_state(char *pbuf, int class, int form, int code, int val);

static int byte_len;
static unsigned char *byte_buf;
static int state;

/*---------------------------------------------------------------------------*
 *	decode Q.931/Q.932 facility info element
 *---------------------------------------------------------------------------*/
int
q932_facility(char *pbuf, unsigned char *buf)
{
	int len;

	sprintf((pbuf+strlen(pbuf)), "[facility (Q.932): ");
	
	buf++;		/* length */

	len = *buf;

	buf++;		/* protocol profile */

	sprintf((pbuf+strlen(pbuf)), "Protocol=");
	
	switch(*buf & 0x1f)
	{
		case FAC_PROTO_ROP:
			sprintf((pbuf+strlen(pbuf)), "Remote Operations Protocol\n");
			break;

		case FAC_PROTO_CMIP:
			sprintf((pbuf+strlen(pbuf)), "CMIP Protocol (Q.941), UNSUPPORTED!\n");
			return(len+2);
			break;

		case FAC_PROTO_ACSE:
			sprintf((pbuf+strlen(pbuf)), "ACSE Protocol (X.217/X.227), UNSUPPORTED!\n");
			return(len+2);
			break;

		default:
			sprintf((pbuf+strlen(pbuf)), "Unknown Protocol (val = 0x%x), UNSUPPORTED!\n", *buf & 0x1f);
			return(len+2);
			break;
	}

	/* next byte */
	
	buf++;
	len--;

	/* initialize variables for do_component */
	
	byte_len = 0;
	byte_buf = buf;
	state = ST_EXP_COMP_TYP;	

	/* decode facility */
	
	do_component(len, pbuf);

	sprintf((pbuf+(strlen(pbuf)-1)), "]");	/* XXX replace last newline */

	return(len+3);
}

/*---------------------------------------------------------------------------*
 *	handle a component recursively
 *---------------------------------------------------------------------------*/
static int
do_component(int length, char *pbuf)
{
	int comp_tag_class;	/* component tag class */
	int comp_tag_form;	/* component form: constructor or primitive */
	int comp_tag_code;	/* component code depending on class */
	int comp_length = 0;	/* component length */

#ifdef FAC_DEBUG
	sprintf((pbuf+strlen(pbuf)), "ENTER - comp_length = %d, byte_len = %d, length =%d\n", comp_length, byte_len, length);	
#endif

again:

#ifdef FAC_DEBUG
	sprintf((pbuf+strlen(pbuf)), "AGAIN - comp_length = %d, byte_len = %d, length =%d\n", comp_length, byte_len, length);
#endif

	/*----------------------------------------*/
	/* first component element: component tag */
	/*----------------------------------------*/
	
	/* tag class bits */

	sprintf((pbuf+strlen(pbuf)), "\t0x%02x Tag: ", *byte_buf);	

	comp_tag_class = (*byte_buf & 0xc0) >> 6;
	
	switch(comp_tag_class)
	{
		case FAC_TAGCLASS_UNI:
			sprintf((pbuf+strlen(pbuf)), "Universal");
			break;
		case FAC_TAGCLASS_APW:
			sprintf((pbuf+strlen(pbuf)), "Applic-wide");
			break;
		case FAC_TAGCLASS_COS:
			sprintf((pbuf+strlen(pbuf)), "Context-spec");
			break;
		case FAC_TAGCLASS_PRU:
			sprintf((pbuf+strlen(pbuf)), "Private");
			break;
	}

	/* tag form bit */

	comp_tag_form = (*byte_buf & 0x20) > 5;
	
	sprintf((pbuf+strlen(pbuf)), ", ");

	if(comp_tag_form == FAC_TAGFORM_CON)
	{
		sprintf((pbuf+strlen(pbuf)), "Constructor");
	}
	else
	{
		sprintf((pbuf+strlen(pbuf)), "Primitive");
	}

	/* tag code bits */

	comp_tag_code = *byte_buf & 0x1f;
	
	sprintf((pbuf+strlen(pbuf)), ", ");	

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
		sprintf((pbuf+strlen(pbuf)), "%d (ext)\n", comp_tag_code);
	}
	else
	{
		comp_tag_code = (*byte_buf & 0x1f);

		if(comp_tag_class == FAC_TAGCLASS_UNI)
		{
			sprintf((pbuf+strlen(pbuf)), "%s (%d)\n", uni_str(comp_tag_code), comp_tag_code);
		}
		else 
		{
			sprintf((pbuf+strlen(pbuf)), "code = %d\n", comp_tag_code);
		}
	}

	byte_buf++;
	byte_len++;
	
	/*--------------------------------------------*/
	/* second component element: component length */
	/*--------------------------------------------*/
	
	sprintf((pbuf+strlen(pbuf)), "\t0x%02x Len: ", *byte_buf);

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
		sprintf((pbuf+strlen(pbuf)), "%d (long form)\n", comp_length);
	}
	else
	{
		comp_length = *byte_buf & 0x7f;
		sprintf((pbuf+strlen(pbuf)), "%d (short form)\n", comp_length);
	}

	next_state(pbuf, comp_tag_class, comp_tag_form, comp_tag_code, -1);
	
	byte_len++;
	byte_buf++;
	
	if(comp_length)
	{

		/*---------------------------------------------*/
		/* third component element: component contents */
		/*---------------------------------------------*/
			
		if(comp_tag_form)	/* == constructor */
		{
			do_component(comp_length, pbuf);
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
					
							sprintf((pbuf+strlen(pbuf)), "\t");
							
							for(i = comp_length-1; i >= 0; i--)
							{
								sprintf((pbuf+strlen(pbuf)), "0x%02x ", *byte_buf);
								val += (*byte_buf + (i*255));
								byte_buf++;
								byte_len++;
								if(i)
									sprintf((pbuf+strlen(pbuf)), "\n\t");
							}
							sprintf((pbuf+strlen(pbuf)), "Val: %d\n", val);
						}
						break;
					default:	
						if(comp_length)
						{
							int i;
					
							sprintf((pbuf+strlen(pbuf)), "\t");
							
							for(i = comp_length-1; i >= 0; i--)
							{
								sprintf((pbuf+strlen(pbuf)), "0x%02x = %d", *byte_buf, *byte_buf);
								if(isprint(*byte_buf))
									sprintf((pbuf+strlen(pbuf)), " = '%c'", *byte_buf);
								byte_buf++;
								byte_len++;
								if(i)
									sprintf((pbuf+strlen(pbuf)), "\n\t");
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
			
					sprintf((pbuf+strlen(pbuf)), "\t");
					
					for(i = comp_length-1; i >= 0; i--)
					{
						sprintf((pbuf+strlen(pbuf)), "0x%02x", *byte_buf);
						val += (*byte_buf + (i*255)); 
						byte_buf++;
						byte_len++;
						if(i)
							sprintf((pbuf+strlen(pbuf)), "\n\t");
					}
					sprintf((pbuf+strlen(pbuf)), "\n");
				}
			}
			next_state(pbuf, comp_tag_class, comp_tag_form, comp_tag_code, val);
		}
	}
	
#ifdef FAC_DEBUG
	sprintf((pbuf+strlen(pbuf)), "PREGOTO - comp_length = %d, byte_len = %d, length =%d\n", comp_length, byte_len, length);
#endif	
	if(byte_len < length)
		goto again;
#ifdef FAC_DEBUG
	sprintf((pbuf+strlen(pbuf)), "RETURN - comp_length = %d, byte_len = %d, length =%d\n", comp_length, byte_len, length);		
#endif
	return(byte_len);
}

/*---------------------------------------------------------------------------*
 *	print universal id type
 *---------------------------------------------------------------------------*/
static char *uni_str(int code)
{
	static char *tbl[] = {
		"BOOLEAN",
		"INTEGER",
		"BIT STRING",
		"OCTET STRING",
		"NULL",
		"OBJECT IDENTIFIER",
		"OBJECT DESCRIPTOR",
		"EXTERNAL",
		"REAL",
		"ENUMERATED",
		"RESERVED11",
		"RESERVED12",
		"RESERVED13",
		"RESERVED14",
		"RESERVED15",
		"SEQUENCE",
		"SET",
		"NUMERIC STRING",
		"PRINTABLE STRING",
		"TELETEX STRING",
		"ISO646 STRING",
		"IA5 STRING",
		"GRAPHIC STRING",
		"GENERAL STRING"
	};

	if(code >= 1 && code <=	FAC_CODEUNI_GNSTR)
		return(tbl[code-1]);
	else
		return("ERROR, Value out of Range!");
}

/*---------------------------------------------------------------------------*
 *	print operation value 
 *---------------------------------------------------------------------------*/
static char *opval_str(int val)
{
	static char buffer[80];
	char *r;
	
	switch(val)
	{
		case FAC_OPVAL_UUS:
			r = "uUs";
			break;
		case FAC_OPVAL_CUG:
			r = "cUGCall";
			break;
		case FAC_OPVAL_MCID:
			r = "mCIDRequest";
			break;
		case FAC_OPVAL_BTPY:
			r = "beginTPY";
			break;
		case FAC_OPVAL_ETPY:
			r = "endTPY";
			break;
		case FAC_OPVAL_ECT:
			r = "eCTRequest";
			break;
		case FAC_OPVAL_DIV_ACT:
			r = "activationDiversion";
			break;
		case FAC_OPVAL_DIV_DEACT:
			r = "deactivationDiversion";
			break;
		case FAC_OPVAL_DIV_ACTSN:
			r = "activationStatusNotificationDiv";
			break;
		case FAC_OPVAL_DIV_DEACTSN:
			r = "deactivationStatusNotificationDiv";
			break;
		case FAC_OPVAL_DIV_INTER:
			r = "interrogationDiversion";
			break;
		case FAC_OPVAL_DIV_INFO:
			r = "diversionInformation";
			break;
		case FAC_OPVAL_DIV_CALLDEF:
			r = "callDeflection";
			break;
		case FAC_OPVAL_DIV_CALLRER:
			r = "callRerouting";
			break;
		case FAC_OPVAL_DIV_LINF2:
			r = "divertingLegInformation2";
			break;
		case FAC_OPVAL_DIV_INVS:
			r = "invokeStatus";
			break;
		case FAC_OPVAL_DIV_INTER1:
			r = "interrogationDiversion1";
			break;
		case FAC_OPVAL_DIV_LINF1:
			r = "divertingLegInformation1";
			break;
		case FAC_OPVAL_DIV_LINF3:
			r = "divertingLegInformation3";
			break;
		case FAC_OPVAL_ER_CRCO:
			r = "explicitReservationCreationControl";
			break;
		case FAC_OPVAL_ER_MGMT:
			r = "explicitReservationManagement";
			break;
		case FAC_OPVAL_ER_CANC:
			r = "explicitReservationCancel";
			break;
		case FAC_OPVAL_MLPP_QUERY:
			r = "mLPP lfb Query";
			break;
		case FAC_OPVAL_MLPP_CALLR:
			r = "mLPP Call Request";
			break;
		case FAC_OPVAL_MLPP_CALLP:
			r = "mLPP Call Preemption";
			break;
		case FAC_OPVAL_AOC_REQ:
			r = "chargingRequest";
			break;
		case FAC_OPVAL_AOC_S_CUR:
			r = "aOCSCurrency";
			break;
		case FAC_OPVAL_AOC_S_SPC:
			r = "aOCSSpecialArrangement";
			break;
		case FAC_OPVAL_AOC_D_CUR:
			r = "aOCDCurrency";
			break;
		case FAC_OPVAL_AOC_D_UNIT:
			r = "aOCDChargingUnit";
			break;
		case FAC_OPVAL_AOC_E_CUR:
			r = "aOCECurrency";
			break;
		case FAC_OPVAL_AOC_E_UNIT:
			r = "aOCEChargingUnit";
			break;
		case FAC_OPVAL_AOC_IDOFCRG:
			r = "identificationOfCharge";
			break;
		case FAC_OPVAL_CONF_BEG:
			r = "beginConf";
			break;
		case FAC_OPVAL_CONF_ADD:
			r = "addConf";
			break;
		case FAC_OPVAL_CONF_SPLIT:
			r = "splitConf";
			break;
		case FAC_OPVAL_CONF_DROP:
			r = "dropConf";
			break;
		case FAC_OPVAL_CONF_ISOLATE:
			r = "isolateConf";
			break;
		case FAC_OPVAL_CONF_REATT:
			r = "reattachConf";
			break;
		case FAC_OPVAL_CONF_PDISC:
			r = "partyDISC";
			break;
		case FAC_OPVAL_CONF_FCONF:
			r = "floatConf";
			break;
		case FAC_OPVAL_CONF_END:
			r = "endConf";
			break;
		case FAC_OPVAL_CONF_IDCFE:
			r = "indentifyConferee";
			break;
		case FAC_OPVAL_REVC_REQ:
			r = "requestREV";
			break;
		default:
			sprintf(buffer, "unknown operation value %d!", val);
			r = buffer;
	}
	return(r);
}

/*---------------------------------------------------------------------------*
 *	billing id string
 *---------------------------------------------------------------------------*/
static char *bid_str(int val)
{
	static char buffer[80];
	char *r;
	
	switch(val)
	{
		case 0:
			r = "normalCharging";
			break;
		case 1:
			r = "reverseCharging";
			break;
		case 2:
			r = "creditCardCharging";
			break;
		case 3:
			r = "callForwardingUnconditional";
			break;
		case 4:
			r = "callForwardingBusy";
			break;
		case 5:
			r = "callForwardingNoReply";
			break;
		case 6:
			r = "callDeflection";
			break;
		case 7:
			r = "callTransfer";
			break;
		default:
			sprintf(buffer, "unknown billing-id value %d!", val);
			r = buffer;
	}
	return(r);
}

/*---------------------------------------------------------------------------*
 *	invoke component
 *---------------------------------------------------------------------------*/
static void
F_1_1(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_1_1, val = %d\n", val);
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          invokeComponent\n");
		state = ST_EXP_INV_ID;
	}
}

/*---------------------------------------------------------------------------*
 *	return result
 *---------------------------------------------------------------------------*/
static void
F_1_2(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_1_2, val = %d\n", val);
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          returnResult\n");
		state = ST_EXP_RR_INV_ID;
	}
}
/*---------------------------------------------------------------------------*
 *	return error
 *---------------------------------------------------------------------------*/
static void
F_1_3(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_1_3, val = %d\n", val);
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          returnError\n");
		state = ST_EXP_NIX;
	}
}
/*---------------------------------------------------------------------------*
 *	reject
 *---------------------------------------------------------------------------*/
static void
F_1_4(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_1_4, val = %d\n", val);
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          reject\n");
		state = ST_EXP_REJ_INV_ID;
	}
}

/*---------------------------------------------------------------------------*
 *	return result: invoke id
 *---------------------------------------------------------------------------*/
static void
F_RJ2(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_RJ2, val = %d\n", val);
#endif
	if(val != -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          InvokeIdentifier = %d\n", val);
		state = ST_EXP_REJ_OP_VAL;
	}
}

/*---------------------------------------------------------------------------*
 *	reject, general problem
 *---------------------------------------------------------------------------*/
static void
F_RJ30(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_RJ30, val = %d\n", val);
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          General problem\n");
	}
	else
	{
		switch(val)
		{
			case 0:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unrecognized component\n");
				break;
			case 1:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = mistyped component\n");
				break;
			case 2:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = badly structured component\n");
				break;
			default:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unknown problem code 0x%x\n", val);
				break;
		}
		state = ST_EXP_NIX;
	}
}

/*---------------------------------------------------------------------------*
 *	reject, invoke problem
 *---------------------------------------------------------------------------*/
static void
F_RJ31(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_RJ31, val = %d\n", val);
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          Invoke problem\n");
	}
	else
	{
		switch(val)
		{
			case 0:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = duplicate invocation\n");
				break;
			case 1:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unrecognized operation\n");
				break;
			case 2:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = mistyped argument\n");
				break;
			case 3:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = resource limitation\n");
				break;
			case 4:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = initiator releasing\n");
				break;
			case 5:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unrecognized linked identifier\n");
				break;
			case 6:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = linked resonse unexpected\n");
				break;
			case 7:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unexpected child operation\n");
				break;
			default:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unknown problem code 0x%x\n", val);
				break;
		}
		state = ST_EXP_NIX;
	}
}

/*---------------------------------------------------------------------------*
 *	reject, return result problem
 *---------------------------------------------------------------------------*/
static void
F_RJ32(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_RJ32, val = %d\n", val);
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          Return result problem\n");
	}
	else
	{
		switch(val)
		{
			case 0:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unrecognized invocation\n");
				break;
			case 1:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = return response unexpected\n");
				break;
			case 2:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = mistyped result\n");
				break;
			default:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unknown problem code 0x%x\n", val);
				break;
		}
		state = ST_EXP_NIX;
	}
}

/*---------------------------------------------------------------------------*
 *	reject, return error problem
 *---------------------------------------------------------------------------*/
static void
F_RJ33(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_RJ33, val = %d\n", val);
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          Return error problem\n");
	}
	else
	{
		switch(val)
		{
			case 0:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unrecognized invocation\n");
				break;
			case 1:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = error response unexpected\n");
				break;
			case 2:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unrecognized error\n");
				break;
			case 3:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unexpected error\n");
				break;
			case 4:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = mistyped parameter\n");
				break;
			default:
				sprintf((pbuf+strlen(pbuf)), "\t          problem = unknown problem code 0x%x\n", val);
				break;
		}
		state = ST_EXP_NIX;
	}
}

/*---------------------------------------------------------------------------*
 *	invoke component: invoke id
 *---------------------------------------------------------------------------*/
static void
F_2(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_2, val = %d\n", val);
#endif
	if(val != -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          InvokeIdentifier = %d\n", val);		
		state = ST_EXP_OP_VAL;
	}
}

/*---------------------------------------------------------------------------*
 *	return result: invoke id
 *---------------------------------------------------------------------------*/
static void
F_RR2(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_RR2, val = %d\n", val);
#endif
	if(val != -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          InvokeIdentifier = %d\n", val);
		state = ST_EXP_RR_OP_VAL;
	}
}

/*---------------------------------------------------------------------------*
 *	invoke component: operation value
 *---------------------------------------------------------------------------*/
static void
F_3(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_3, val = %d\n", val);
#endif
	if(val != -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          Operation Value = %s (%d)\n", opval_str(val), val);
		state = ST_EXP_INFO;
	}
}

/*---------------------------------------------------------------------------*
 *	return result: operation value
 *---------------------------------------------------------------------------*/
static void
F_RR3(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_RR3, val = %d\n", val);
#endif
	if(val != -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          Operation Value = %s (%d)\n", opval_str(val), val);
		state = ST_EXP_RR_RESULT;
	}
}

/*---------------------------------------------------------------------------*
 *	return result: RESULT
 *---------------------------------------------------------------------------*/
static void
F_RRR(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_RRR, val = %d\n", val);
#endif
	state = ST_EXP_NIX;
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_4(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_4, val = %d\n", val);	
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          specificChargingUnits\n");
		state = ST_EXP_RUL;
	}
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_4_1(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_4_1, val = %d\n", val);	
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          freeOfCharge\n");
		state = ST_EXP_NIX;
	}
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_4_2(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_4_2, val = %d\n", val);	
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          chargeNotAvailable\n");
		state = ST_EXP_NIX;
	}
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_5(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_5, val = %d\n", val);	
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          recordedUnitsList [1]\n");
		state = ST_EXP_RU;
	}
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_6(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_6, val = %d\n", val);	
#endif
	if(val == -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          RecordedUnits\n");
		state = ST_EXP_RNOU;
	}
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_7(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_7, val = %d\n", val);	
#endif
	if(val != -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          NumberOfUnits = %d\n", val);
		state = ST_EXP_TOCI;
	}
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_8(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_8, val = %d\n", val);	
#endif
	if(val != -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          typeOfChargingInfo = %s\n", val == 0 ? "subTotal" : "total");
		state = ST_EXP_DBID;
	}
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_9(char *pbuf, int val)
{
#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: exec F_9, val = %d\n", val);	
#endif
	if(val != -1)
	{
		sprintf((pbuf+strlen(pbuf)), "\t          AOCDBillingId = %s (%d)\n", bid_str(val), val);
		state = ST_EXP_NIX;
	}
}

/*---------------------------------------------------------------------------*
 *	state table
 *---------------------------------------------------------------------------*/
static struct statetab {
	int currstate;			/* input: current state we are in */
	int form;			/* input: current tag form */
	int class;			/* input: current tag class */
	int code;			/* input: current tag code */
	void (*func)(char *,int);	/* output: func to exec */
} statetab[] = {

/*	 current state		tag form		tag class		tag code		function	*/
/*	 ---------------------  ----------------------  ----------------------  ---------------------- 	----------------*/

/* invoke */

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

/* return result */
	
	{ST_EXP_RR_INV_ID,	FAC_TAGFORM_PRI,	FAC_TAGCLASS_UNI,	FAC_CODEUNI_INT,	F_RR2		},
	{ST_EXP_RR_OP_VAL,	FAC_TAGFORM_PRI,	FAC_TAGCLASS_UNI,	FAC_CODEUNI_INT,	F_RR3		},
	{ST_EXP_RR_RESULT,	FAC_TAGFORM_CON,	FAC_TAGCLASS_UNI,	FAC_CODEUNI_SET,	F_RRR		},

/*	 current state		tag form		tag class		tag code		function	*/
/*	 ---------------------  ----------------------  ----------------------  ---------------------- 	----------------*/
/* reject */
	
	{ST_EXP_REJ_INV_ID,	FAC_TAGFORM_PRI,	FAC_TAGCLASS_UNI,	FAC_CODEUNI_INT,	F_RJ2		},
	{ST_EXP_REJ_OP_VAL,	FAC_TAGFORM_PRI,	FAC_TAGCLASS_COS,	0,			F_RJ30		},
	{ST_EXP_REJ_OP_VAL,	FAC_TAGFORM_PRI,	FAC_TAGCLASS_COS,	1,			F_RJ31		},
	{ST_EXP_REJ_OP_VAL,	FAC_TAGFORM_PRI,	FAC_TAGCLASS_COS,	2,			F_RJ32		},
	{ST_EXP_REJ_OP_VAL,	FAC_TAGFORM_PRI,	FAC_TAGCLASS_COS,	3,			F_RJ33		},

/* end */
	
	{-1,			-1,			-1,			-1,			NULL		}
};	
	
/*---------------------------------------------------------------------------*
 *	state decode for do_component
 *---------------------------------------------------------------------------*/
static void
next_state(char *pbuf, int class, int form, int code, int val)
{
	int i;

#ifdef ST_DEBUG
	sprintf((pbuf+strlen(pbuf)), "next_state: class=%d, form=%d, code=%d, val=%d\n", class, form, code, val);
#endif

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
			(*statetab[i].func)(pbuf, val);
			break;
		}
	}
}

/* EOF */

