/*
* \file       ocsd_gen_elem_stack.h
* \brief      OpenCSD : Generic element output stack.
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

#include "trc_gen_elem.h"
#include "comp_attach_pt_t.h"
#include "interfaces/trc_gen_elem_in_i.h"

/* element stack to handle cases where a trace element can generate multiple output packets 
  
   maintains the "current" element, which might be sent independently of this stack, and also
   ensures that persistent data in the output elements is maintained between elements.
*/
class OcsdGenElemStack
{
public:
    OcsdGenElemStack();
    ~OcsdGenElemStack();

    void initSendIf(componentAttachPt<ITrcGenElemIn> *pGenElemIf);
    void initCSID(const uint8_t CSID) { m_CSID = CSID; };

    OcsdTraceElement &getCurrElem();    //!< get the current element. 
    ocsd_err_t resetElemStack();        //!< set pointers to base of stack
    ocsd_err_t addElem(const ocsd_trc_index_t trc_pkt_idx);    //!< add elem to stack and set current.
    void setCurrElemIdx(const ocsd_trc_index_t trc_pkt_idx);   //!< packet index for this element
    ocsd_err_t addElemType(const ocsd_trc_index_t trc_pkt_idx, ocsd_gen_trc_elem_t elem_type);

    ocsd_datapath_resp_t sendElements();    //!< send elements on the stack
    const int numElemToSend() const;

private:
    typedef struct _elemPtr {
        OcsdTraceElement *pElem;        //!< pointer to the listed trace element
        ocsd_trc_index_t trc_pkt_idx;   //!< packet index in the trace stream
    } elemPtr_t;

    const bool isInit();              //!< check correctly initialised.

    ocsd_err_t growArray();
    void copyPersistentData(int src, int dst);  //!< copy across persistent state data between elements
    void resetIndexes();    //!< clear down all indexes - reset or send complete.

    elemPtr_t *m_pElemArray;    //!< an array of pointers to elements.
    int m_elemArraySize;        //!< number of element pointers in the array 

    int m_elem_to_send;     //!< number of live elements in the stack - init to 1.
    int m_curr_elem_idx;    //!< index into the element array.
    int m_send_elem_idx;    //!< next element to send.

    //!< send packet info
    uint8_t m_CSID;
    componentAttachPt<ITrcGenElemIn> *m_sendIf; //!< element send interface.

    bool m_is_init;
};

inline const int OcsdGenElemStack::numElemToSend() const
{
    return m_elem_to_send;
}

inline void OcsdGenElemStack::initSendIf(componentAttachPt<ITrcGenElemIn> *pGenElemIf)
{
    m_sendIf = pGenElemIf;
}

inline void OcsdGenElemStack::setCurrElemIdx(const ocsd_trc_index_t trc_pkt_idx)
{
    m_pElemArray[m_curr_elem_idx].trc_pkt_idx = trc_pkt_idx;
}

inline OcsdTraceElement &OcsdGenElemStack::getCurrElem()
{
    return *(m_pElemArray[m_curr_elem_idx].pElem);
}


/* End of File ocsd_gen_elem_stack.h */
