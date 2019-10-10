/*
 * \file       ss_key_value_names.h
 * \brief      OpenCSD : Names and Value Strings needed to interpret snapshot .ini data
 * 
 * \copyright  Copyright (c) 2015, ARM Limited. All Rights Reserved.
 */

/* 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, 
 * this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation 
 * and/or other materials provided with the distribution. 
 * 
 * 3. Neither the name of the copyright holder nor the names of its contributors 
 * may be used to endorse or promote products derived from this software without 
 * specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */ 

#ifndef ARM_SS_KEY_VALUE_NAMES_H_INCLUDED
#define ARM_SS_KEY_VALUE_NAMES_H_INCLUDED

/*** Core Profile Prefixes ***/
const char * const CPUprofileA("Cortex-A");
const char * const CPUprofileR("Cortex-R");
const char * const CPUprofileM("Cortex-M");

/*** Trace Buffer formats ***/
const char * const BuffFmtCS("coresight");  // coresight frame formatted.

/***ETM v4 ***/
const char * const ETMv4Protocol("ETM4");
const char * const ETMv4RegCfg("TRCCONFIGR");
const char * const ETMv4RegIDR("TRCTRACEIDR");
const char * const ETMv4RegAuth("TRCAUTHSTATUS");
const char * const ETMv4RegIDR0("TRCIDR0");
const char * const ETMv4RegIDR1("TRCIDR1");
const char * const ETMv4RegIDR2("TRCIDR2");
const char * const ETMv4RegIDR8("TRCIDR8");
const char * const ETMv4RegIDR9("TRCIDR9");
const char * const ETMv4RegIDR10("TRCIDR10");
const char * const ETMv4RegIDR11("TRCIDR11");
const char * const ETMv4RegIDR12("TRCIDR12");
const char * const ETMv4RegIDR13("TRCIDR13");

/*** ETMv3/PTM ***/
const char * const ETMv3Protocol("ETM3");
const char * const PTMProtocol("PTM1");
const char * const PFTProtocol("PFT1");
const char * const ETMv3PTMRegIDR("ETMIDR");
const char * const ETMv3PTMRegCR("ETMCR");
const char * const ETMv3PTMRegCCER("ETMCCER");
const char * const ETMv3PTMRegTraceIDR("ETMTRACEIDR");

/*** STM/ITM **/
const char * const STMProtocol("STM");
const char * const STMRegTCSR("STMTCSR");

#endif // ARM_SS_KEY_VALUE_NAMES_H_INCLUDED

/* End of File ss_key_value_names.h */
