/*
* \file       ext_dcd_echo_test_fact.h
* \brief      OpenCSD : Echo test custom decoder factory
*
* \copyright  Copyright (c) 2016, ARM Limited. All Rights Reserved.
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
#ifndef ARM_EXT_DCD_ECHO_TEST_FACT_H_INCLUDED
#define ARM_EXT_DCD_ECHO_TEST_FACT_H_INCLUDED

/* Creates echo test decoders to test the external decoder C-API infrastructure. 
   Echo decoders can be attached to any CS byte stream and will mainly echo the data
   back with a simple "protocol" decoded - not based on real protocol.
  */

#include "opencsd/c_api/ocsd_c_api_custom.h"

/* return an initialised structure with the factory functions */
extern ocsd_extern_dcd_fact_t *ext_echo_get_dcd_fact();


#endif /*ARM_EXT_DCD_ECHO_TEST_FACT_H_INCLUDED */
