/*
* \file       ext_dcd_echo_test_fact.c
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ext_dcd_echo_test_fact.h"
#include "ext_dcd_echo_test.h"

/** Decoder factory implementation. 
    Permits registration of the decoder with the library.

    Provides creation and deletion functionality for decoder instances.
*/

/*-- functions meeting the ext decode factory structure requirements */

/** Required function to create a decoder instance - fills in the decoder struct supplied. */
static ocsd_err_t ext_echo_create(const int create_flags, const void *decoder_cfg, const ocsd_extern_dcd_cb_fns *p_lib_callbacks, ocsd_extern_dcd_inst_t *p_decoder_inst);

/** Required Function to destroy a decoder instance - indicated by decoder handle */
static ocsd_err_t ext_echo_destroy(const void *decoder_handle);

/** Required Function to extract the CoreSight Trace ID from the configuration structure */
static ocsd_err_t ext_echo_csid_from_cfg(const void *decoder_cfg, unsigned char *p_csid);

/** Optional Function to convert a protocol specific trace packet to human readable string */
static ocsd_err_t ext_echo_pkt_to_str(const void *trc_pkt, char *buffer, const int buflen);

static ocsd_extern_dcd_fact_t echo_test_decoder_fact;

ocsd_extern_dcd_fact_t *ext_echo_get_dcd_fact()
{
    echo_test_decoder_fact.createDecoder = ext_echo_create;
    echo_test_decoder_fact.destroyDecoder = ext_echo_destroy;
    echo_test_decoder_fact.csidFromConfig = ext_echo_csid_from_cfg;
    echo_test_decoder_fact.pktToString = ext_echo_pkt_to_str;
    echo_test_decoder_fact.protocol_id = OCSD_PROTOCOL_END;
    return &echo_test_decoder_fact;
}

ocsd_err_t ext_echo_create(const int create_flags, const void *decoder_cfg, const ocsd_extern_dcd_cb_fns *p_lib_callbacks, ocsd_extern_dcd_inst_t *p_decoder_inst)
{
    echo_decoder_t *decoder = NULL;

    if ((decoder = (echo_decoder_t *)malloc(sizeof(echo_decoder_t))) == NULL)
        return OCSD_ERR_MEM;

    echo_dcd_init(decoder,p_decoder_inst,(echo_dcd_cfg_t *)decoder_cfg, p_lib_callbacks);
    
    decoder->createFlags = create_flags;

    return OCSD_OK;
}

ocsd_err_t ext_echo_destroy(const void *decoder_handle)
{
    free((echo_decoder_t *)decoder_handle);
    return OCSD_OK;
}

ocsd_err_t ext_echo_csid_from_cfg(const void *decoder_cfg, unsigned char *p_csid)
{
    *p_csid = ((echo_dcd_cfg_t *)decoder_cfg)->cs_id;
    return OCSD_OK;
}

ocsd_err_t ext_echo_pkt_to_str(const void *trc_pkt, char *buffer, const int buflen)
{
    echo_dcd_pkt_tostr((echo_dcd_pkt_t*)trc_pkt, buffer, buflen);    
    return OCSD_OK;
}
