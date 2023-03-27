/*
* \file       ocsd_gen_elem_stack.cpp
* \brief      OpenCSD : List of Generic trace elements for output.
*
* \copyright  Copyright (c) 2020, ARM Limited. All Rights Reserved.
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

#include "common/ocsd_gen_elem_stack.h"

OcsdGenElemStack::OcsdGenElemStack() :
    m_pElemArray(0),
    m_elemArraySize(0),
    m_elem_to_send(0),
    m_curr_elem_idx(0),
    m_send_elem_idx(0),
    m_CSID(0),
    m_sendIf(NULL),
    m_is_init(false)
{

}

OcsdGenElemStack::~OcsdGenElemStack()
{
    for (int i = 0; i<m_elemArraySize; i++)
    {
        delete m_pElemArray[i].pElem;
    }
    delete [] m_pElemArray;
    m_pElemArray = 0;
}

ocsd_err_t OcsdGenElemStack::addElem(const ocsd_trc_index_t trc_pkt_idx)
{
    ocsd_err_t err = OCSD_OK;

    if (((m_curr_elem_idx + 1) == m_elemArraySize) || !m_pElemArray)
    {
        err = growArray();
        if (err)
            return err;
    }

    // if there is a least one element then copy and increment
    // otherwise we are at base of stack.
    if (m_elem_to_send)
    {
        copyPersistentData(m_curr_elem_idx, m_curr_elem_idx + 1);
        m_curr_elem_idx++;
    }
    m_pElemArray[m_curr_elem_idx].trc_pkt_idx = trc_pkt_idx;
    m_elem_to_send++;
    return err;
}

ocsd_err_t OcsdGenElemStack::addElemType(const ocsd_trc_index_t trc_pkt_idx, ocsd_gen_trc_elem_t elem_type)
{
    ocsd_err_t err = addElem(trc_pkt_idx);
    if (!err)
        getCurrElem().setType(elem_type);
    return err;
}

ocsd_err_t OcsdGenElemStack::resetElemStack()
{
    ocsd_err_t err = OCSD_OK;
    if (!m_pElemArray)
    {
        err = growArray();
        if (err)
            return err;
    }

    if (!isInit())
        return OCSD_ERR_NOT_INIT;

    resetIndexes();
    return err;
}

void OcsdGenElemStack::resetIndexes()
{
    // last time there was more than one element on stack
    if (m_curr_elem_idx > 0)
        copyPersistentData(m_curr_elem_idx, 0);

    // indexes to bottom of stack, nothing in use at present
    m_curr_elem_idx = 0;
    m_send_elem_idx = 0;
    m_elem_to_send = 0;
}

ocsd_datapath_resp_t OcsdGenElemStack::sendElements()
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    if (!isInit())
        return OCSD_RESP_FATAL_NOT_INIT;

    while (m_elem_to_send && OCSD_DATA_RESP_IS_CONT(resp))
    {
        resp = m_sendIf->first()->TraceElemIn(m_pElemArray[m_send_elem_idx].trc_pkt_idx, m_CSID, *(m_pElemArray[m_send_elem_idx].pElem));
        m_send_elem_idx++;
        m_elem_to_send--;
    }

    // clear the indexes if we are done.
    if (!m_elem_to_send)
        resetIndexes();
    return resp;
}

ocsd_err_t OcsdGenElemStack::growArray()
{
    elemPtr_t *p_new_array = 0;
    const int increment = 4;
    
    p_new_array = new (std::nothrow) elemPtr_t[m_elemArraySize + increment];

    if (p_new_array != 0)
    {
        OcsdTraceElement *pElem = 0;

        // fill the last increment elements with new objects
        for (int i = 0; i < increment; i++)
        {
            pElem = new (std::nothrow) OcsdTraceElement();
            if (!pElem)
                return OCSD_ERR_MEM;
            pElem->init();
            p_new_array[m_elemArraySize + i].pElem = pElem;
        }

        // copy the existing objects from the old array to the start of the new one
        if (m_elemArraySize > 0)
        {            
            for (int i = 0; i < m_elemArraySize; i++)
            {
                p_new_array[i].pElem = m_pElemArray[i].pElem;
                p_new_array[i].trc_pkt_idx = m_pElemArray[i].trc_pkt_idx;
            }
        }

        // delete the old pointer array.
        delete[] m_pElemArray;
        m_elemArraySize += increment;
        m_pElemArray = p_new_array;
    }
    else
        return OCSD_ERR_MEM;

    return OCSD_OK;
}

void OcsdGenElemStack::copyPersistentData(int src, int dst)
{
    m_pElemArray[dst].pElem->copyPersistentData(*(m_pElemArray[src].pElem));
}

const bool OcsdGenElemStack::isInit()
{
    if (!m_is_init) {
        if (m_elemArraySize && m_pElemArray && m_sendIf)
            m_is_init = true;
    }
    return m_is_init;
}


/* End of File ocsd_gen_elem_stack.cpp */
