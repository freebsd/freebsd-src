/* Copyright (c) 2008-2011 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*

 @File          error.c

 @Description   General errors and events reporting utilities.
*//***************************************************************************/

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))

const char *dbgLevelStrings[] =
{
     "CRITICAL"
    ,"MAJOR"
    ,"MINOR"
    ,"WARNING"
    ,"INFO"
    ,"TRACE"
};

const char *errTypeStrings[] =
{
     "Invalid State"                        /* E_INVALID_STATE */
    ,"Invalid Operation"                    /* E_INVALID_OPERATION */
    ,"Unsupported Operation"                /* E_NOT_SUPPORTED */
    ,"No Device"                            /* E_NO_DEVICE */
    ,"Invalid Handle"                       /* E_INVALID_HANDLE */
    ,"Invalid ID"                           /* E_INVALID_ID */
    ,"Unexpected NULL Pointer"              /* E_NULL_POINTER */
    ,"Invalid Value"                        /* E_INVALID_VALUE */
    ,"Invalid Selection"                    /* E_INVALID_SELECTION */
    ,"Invalid Communication Mode"           /* E_INVALID_COMM_MODE */
    ,"Invalid Byte Order"                   /* E_INVALID_BYTE_ORDER */
    ,"Invalid Memory Type"                  /* E_INVALID_MEMORY_TYPE */
    ,"Invalid Interrupt Queue"              /* E_INVALID_INTR_QUEUE */
    ,"Invalid Priority"                     /* E_INVALID_PRIORITY */
    ,"Invalid Clock"                        /* E_INVALID_CLOCK */
    ,"Invalid Rate"                         /* E_INVALID_RATE */
    ,"Invalid Address"                      /* E_INVALID_ADDRESS */
    ,"Invalid Bus"                          /* E_INVALID_BUS */
    ,"Conflict In Bus Selection"            /* E_BUS_CONFLICT */
    ,"Conflict In Settings"                 /* E_CONFLICT */
    ,"Incorrect Alignment"                  /* E_NOT_ALIGNED */
    ,"Value Out Of Range"                   /* E_NOT_IN_RANGE */
    ,"Invalid Frame"                        /* E_INVALID_FRAME */
    ,"Frame Is Empty"                       /* E_EMPTY_FRAME */
    ,"Buffer Is Empty"                      /* E_EMPTY_BUFFER */
    ,"Memory Allocation Failed"             /* E_NO_MEMORY */
    ,"Resource Not Found"                   /* E_NOT_FOUND */
    ,"Resource Is Unavailable"              /* E_NOT_AVAILABLE */
    ,"Resource Already Exists"              /* E_ALREADY_EXISTS */
    ,"Resource Is Full"                     /* E_FULL */
    ,"Resource Is Empty"                    /* E_EMPTY */
    ,"Resource Is Busy"                     /* E_BUSY */
    ,"Resource Already Free"                /* E_ALREADY_FREE */
    ,"Read Access Failed"                   /* E_READ_FAILED */
    ,"Write Access Failed"                  /* E_WRITE_FAILED */
    ,"Send Operation Failed"                /* E_SEND_FAILED */
    ,"Receive Operation Failed"             /* E_RECEIVE_FAILED */
    ,"Operation Timed Out"                  /* E_TIMEOUT */
};


#if (defined(REPORT_EVENTS) && (REPORT_EVENTS > 0))

const char *eventStrings[] =
{
     "Rx Discard"                           /* EV_RX_DISCARD */
    ,"Rx Error"                             /* EV_RX_ERROR */
    ,"Tx Error"                             /* EV_TX_ERROR */
    ,"No Buffer Objects"                    /* EV_NO_BUFFERS */
    ,"No MB-Frame Objects"                  /* EV_NO_MB_FRAMES */
    ,"No SB-Frame Objects"                  /* EV_NO_SB_FRAMES */
    ,"Tx Queue Is Full"                     /* EV_TX_QUEUE_FULL */
    ,"Rx Queue Is Full"                     /* EV_RX_QUEUE_FULL */
    ,"Interrupts Queue Is Full"             /* EV_INTR_QUEUE_FULL */
    ,"Data Buffer Is Unavailable"           /* EV_NO_DATA_BUFFER */
    ,"Objects Pool Is Empty"                /* EV_OBJ_POOL_EMPTY */
    ,"Illegal bus access"                   /* EV_BUS_ERROR */
    ,"PTP Tx Timestamps Queue Is Full"      /* EV_PTP_TXTS_QUEUE_FULL */
    ,"PTP Rx Timestamps Queue Is Full"      /* EV_PTP_RXTS_QUEUE_FULL */
};

#endif /* (defined(REPORT_EVENTS) && (REPORT_EVENTS > 0)) */

#endif /* (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0)) */

