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

/******************************************************************************
 @File          fm_kg.c

 @Description   FM PCD ...
*//***************************************************************************/
#include "std_ext.h"
#include "error_ext.h"
#include "string_ext.h"
#include "debug_ext.h"
#include "net_ext.h"
#include "fm_port_ext.h"

#include "fm_common.h"
#include "fm_pcd.h"
#include "fm_hc.h"

#include "fm_pcd_ipc.h"


static t_Error WriteKgarWait(t_FmPcd *p_FmPcd, uint32_t kgar)
{
    WRITE_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->kgar, kgar);
    /* Wait for GO to be idle and read error */
    while ((kgar = GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->kgar)) & FM_PCD_KG_KGAR_GO) ;
    if (kgar & FM_PCD_KG_KGAR_ERR)
        RETURN_ERROR(MINOR, E_INVALID_STATE, ("Keygen scheme access violation"));
    return E_OK;
}

static e_FmPcdKgExtractDfltSelect GetGenericSwDefault(t_FmPcdKgExtractDflt swDefaults[], uint8_t numOfSwDefaults, uint8_t code)
{
    int i;

    switch(code)
    {
        case( KG_SCH_GEN_PARSE_RESULT_N_FQID):
        case( KG_SCH_GEN_DEFAULT):
        case( KG_SCH_GEN_NEXTHDR):
            for(i=0 ; i<numOfSwDefaults ; i++)
                if(swDefaults[i].type == e_FM_PCD_KG_GENERIC_NOT_FROM_DATA)
                    return swDefaults[i].dfltSelect;
            ASSERT_COND(FALSE);
        case( KG_SCH_GEN_SHIM1):
        case( KG_SCH_GEN_SHIM2):
        case( KG_SCH_GEN_IP_PID_NO_V):
        case( KG_SCH_GEN_ETH_NO_V):
        case( KG_SCH_GEN_SNAP_NO_V):
        case( KG_SCH_GEN_VLAN1_NO_V):
        case( KG_SCH_GEN_VLAN2_NO_V):
        case( KG_SCH_GEN_ETH_TYPE_NO_V):
        case( KG_SCH_GEN_PPP_NO_V):
        case( KG_SCH_GEN_MPLS1_NO_V):
        case( KG_SCH_GEN_MPLS_LAST_NO_V):
        case( KG_SCH_GEN_L3_NO_V):
        case( KG_SCH_GEN_IP2_NO_V):
        case( KG_SCH_GEN_GRE_NO_V):
        case( KG_SCH_GEN_L4_NO_V):
            for(i=0 ; i<numOfSwDefaults ; i++)
                if(swDefaults[i].type == e_FM_PCD_KG_GENERIC_FROM_DATA_NO_V)
                    return swDefaults[i].dfltSelect;

        case( KG_SCH_GEN_START_OF_FRM):
        case( KG_SCH_GEN_ETH):
        case( KG_SCH_GEN_SNAP):
        case( KG_SCH_GEN_VLAN1):
        case( KG_SCH_GEN_VLAN2):
        case( KG_SCH_GEN_ETH_TYPE):
        case( KG_SCH_GEN_PPP):
        case( KG_SCH_GEN_MPLS1):
        case( KG_SCH_GEN_MPLS2):
        case( KG_SCH_GEN_MPLS3):
        case( KG_SCH_GEN_MPLS_LAST):
        case( KG_SCH_GEN_IPV4):
        case( KG_SCH_GEN_IPV6):
        case( KG_SCH_GEN_IPV4_TUNNELED):
        case( KG_SCH_GEN_IPV6_TUNNELED):
        case( KG_SCH_GEN_MIN_ENCAP):
        case( KG_SCH_GEN_GRE):
        case( KG_SCH_GEN_TCP):
        case( KG_SCH_GEN_UDP):
        case( KG_SCH_GEN_IPSEC_AH):
        case( KG_SCH_GEN_SCTP):
        case( KG_SCH_GEN_DCCP):
        case( KG_SCH_GEN_IPSEC_ESP):
            for(i=0 ; i<numOfSwDefaults ; i++)
                if(swDefaults[i].type == e_FM_PCD_KG_GENERIC_FROM_DATA)
                    return swDefaults[i].dfltSelect;
        default:
            return e_FM_PCD_KG_DFLT_ILLEGAL;
    }
}

static uint8_t GetGenCode(e_FmPcdExtractFrom src, uint8_t *p_Offset)
{
    *p_Offset = 0;

    switch(src)
    {
        case(e_FM_PCD_EXTRACT_FROM_FRAME_START):
            return KG_SCH_GEN_START_OF_FRM;
        case(e_FM_PCD_EXTRACT_FROM_DFLT_VALUE):
            return KG_SCH_GEN_DEFAULT;
        case(e_FM_PCD_EXTRACT_FROM_PARSE_RESULT):
            return KG_SCH_GEN_PARSE_RESULT_N_FQID;
        case(e_FM_PCD_EXTRACT_FROM_ENQ_FQID):
            *p_Offset = 32;
            return KG_SCH_GEN_PARSE_RESULT_N_FQID;
        case(e_FM_PCD_EXTRACT_FROM_CURR_END_OF_PARSE):
            return KG_SCH_GEN_NEXTHDR;
        default:
            REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("Illegal 'extract from' src"));
            return 0;
    }
}

static uint8_t GetGenHdrCode(e_NetHeaderType hdr, e_FmPcdHdrIndex hdrIndex, bool ignoreProtocolValidation)
{
    if(!ignoreProtocolValidation)
        switch(hdr)
        {
            case(HEADER_TYPE_NONE):
                ASSERT_COND(FALSE);
            case(HEADER_TYPE_ETH):
                return KG_SCH_GEN_ETH;
            case(HEADER_TYPE_LLC_SNAP):
                return KG_SCH_GEN_SNAP;
            case(HEADER_TYPE_PPPoE):
                return KG_SCH_GEN_PPP;
            case(HEADER_TYPE_MPLS):
                if((hdrIndex == e_FM_PCD_HDR_INDEX_NONE) || (hdrIndex == e_FM_PCD_HDR_INDEX_1))
                    return KG_SCH_GEN_MPLS1;
                if(hdrIndex == e_FM_PCD_HDR_INDEX_2)
                    return KG_SCH_GEN_MPLS2;
                if(hdrIndex == e_FM_PCD_HDR_INDEX_3)
                    return KG_SCH_GEN_MPLS3;
                if(hdrIndex == e_FM_PCD_HDR_INDEX_LAST)
                    return KG_SCH_GEN_MPLS_LAST;
                REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal MPLS header index"));
                return 0;
            case(HEADER_TYPE_IPv4):
                if((hdrIndex == e_FM_PCD_HDR_INDEX_NONE) || (hdrIndex == e_FM_PCD_HDR_INDEX_1))
                    return KG_SCH_GEN_IPV4;
                if(hdrIndex == e_FM_PCD_HDR_INDEX_2)
                    return KG_SCH_GEN_IPV4_TUNNELED;
                REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv4 header index"));
                return 0;
            case(HEADER_TYPE_IPv6):
                if((hdrIndex == e_FM_PCD_HDR_INDEX_NONE) || (hdrIndex == e_FM_PCD_HDR_INDEX_1))
                    return KG_SCH_GEN_IPV6;
                if(hdrIndex == e_FM_PCD_HDR_INDEX_2)
                    return KG_SCH_GEN_IPV6_TUNNELED;
                REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv6 header index"));
                return 0;
            case(HEADER_TYPE_GRE):
                return KG_SCH_GEN_GRE;
            case(HEADER_TYPE_TCP):
                return KG_SCH_GEN_TCP;
            case(HEADER_TYPE_UDP):
                return KG_SCH_GEN_UDP;
            case(HEADER_TYPE_IPSEC_AH):
                return KG_SCH_GEN_IPSEC_AH;
            case(HEADER_TYPE_IPSEC_ESP):
                return KG_SCH_GEN_IPSEC_ESP;
            case(HEADER_TYPE_SCTP):
                return KG_SCH_GEN_SCTP;
            case(HEADER_TYPE_DCCP):
                return KG_SCH_GEN_DCCP;
            default:
                REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                return 0;
        }
    else
        switch(hdr)
        {
            case(HEADER_TYPE_NONE):
                ASSERT_COND(FALSE);
            case(HEADER_TYPE_ETH):
                return KG_SCH_GEN_ETH_NO_V;
            case(HEADER_TYPE_LLC_SNAP):
                return KG_SCH_GEN_SNAP_NO_V;
            case(HEADER_TYPE_PPPoE):
                return KG_SCH_GEN_PPP_NO_V;
            case(HEADER_TYPE_MPLS):
                 if((hdrIndex == e_FM_PCD_HDR_INDEX_NONE) || (hdrIndex == e_FM_PCD_HDR_INDEX_1))
                    return KG_SCH_GEN_MPLS1_NO_V;
                if(hdrIndex == e_FM_PCD_HDR_INDEX_LAST)
                    return KG_SCH_GEN_MPLS_LAST_NO_V;
                if((hdrIndex == e_FM_PCD_HDR_INDEX_2) || (hdrIndex == e_FM_PCD_HDR_INDEX_3) )
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Indexed MPLS Extraction not supported"));
                else
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal MPLS header index"));
                return 0;
            case(HEADER_TYPE_IPv4):
            case(HEADER_TYPE_IPv6):
              if((hdrIndex == e_FM_PCD_HDR_INDEX_NONE) || (hdrIndex == e_FM_PCD_HDR_INDEX_1))
                    return KG_SCH_GEN_L3_NO_V;
                if(hdrIndex == e_FM_PCD_HDR_INDEX_2)
                    return KG_SCH_GEN_IP2_NO_V;
                REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IP header index"));
            case(HEADER_TYPE_MINENCAP):
                return KG_SCH_GEN_IP2_NO_V;
            case(HEADER_TYPE_USER_DEFINED_L3):
                return KG_SCH_GEN_L3_NO_V;
            case(HEADER_TYPE_GRE):
                return KG_SCH_GEN_GRE_NO_V;
            case(HEADER_TYPE_TCP):
            case(HEADER_TYPE_UDP):
            case(HEADER_TYPE_IPSEC_AH):
            case(HEADER_TYPE_IPSEC_ESP):
            case(HEADER_TYPE_SCTP):
            case(HEADER_TYPE_DCCP):
                return KG_SCH_GEN_L4_NO_V;
            case(HEADER_TYPE_USER_DEFINED_SHIM1):
                return KG_SCH_GEN_SHIM1;
            case(HEADER_TYPE_USER_DEFINED_SHIM2):
                return KG_SCH_GEN_SHIM2;
            default:
                REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                return 0;
        }
}
static t_GenericCodes GetGenFieldCode(e_NetHeaderType hdr, t_FmPcdFields field, bool ignoreProtocolValidation, e_FmPcdHdrIndex hdrIndex)
{
    if (!ignoreProtocolValidation)
        switch(hdr)
        {
            case(HEADER_TYPE_NONE):
                ASSERT_COND(FALSE);
            case(HEADER_TYPE_ETH):
                switch(field.eth)
                {
                    case(NET_HEADER_FIELD_ETH_TYPE):
                        return KG_SCH_GEN_ETH_TYPE;
                    default:
                        REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                        return 0;
                }
            case(HEADER_TYPE_VLAN):
                switch(field.vlan)
                {
                    case(NET_HEADER_FIELD_VLAN_TCI):
                        if((hdrIndex == e_FM_PCD_HDR_INDEX_NONE) || (hdrIndex == e_FM_PCD_HDR_INDEX_1))
                            return KG_SCH_GEN_VLAN1;
                        if(hdrIndex == e_FM_PCD_HDR_INDEX_LAST)
                            return KG_SCH_GEN_VLAN2;
                        REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal VLAN header index"));
                        return 0;
                }
            case(HEADER_TYPE_MPLS):
            case(HEADER_TYPE_IPSEC_AH):
            case(HEADER_TYPE_IPSEC_ESP):
            case(HEADER_TYPE_LLC_SNAP):
            case(HEADER_TYPE_PPPoE):
            case(HEADER_TYPE_IPv4):
            case(HEADER_TYPE_IPv6):
            case(HEADER_TYPE_GRE):
            case(HEADER_TYPE_MINENCAP):
            case(HEADER_TYPE_USER_DEFINED_L3):
            case(HEADER_TYPE_TCP):
            case(HEADER_TYPE_UDP):
            case(HEADER_TYPE_SCTP):
            case(HEADER_TYPE_DCCP):
            case(HEADER_TYPE_USER_DEFINED_L4):
                REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
            default:
                REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Header not supported"));
                return 0;
        }
        else
            switch(hdr)
            {
                case(HEADER_TYPE_NONE):
                    ASSERT_COND(FALSE);
                case(HEADER_TYPE_ETH):
                switch(field.eth)
                {
                    case(NET_HEADER_FIELD_ETH_TYPE):
                        return KG_SCH_GEN_ETH_TYPE_NO_V;
                    default:
                        REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                        return 0;
                }
                case(HEADER_TYPE_VLAN):
                    switch(field.vlan)
                    {
                        case(NET_HEADER_FIELD_VLAN_TCI) :
                            if((hdrIndex == e_FM_PCD_HDR_INDEX_NONE) || (hdrIndex == e_FM_PCD_HDR_INDEX_1))
                                return KG_SCH_GEN_VLAN1_NO_V;
                            if(hdrIndex == e_FM_PCD_HDR_INDEX_LAST)
                                return KG_SCH_GEN_VLAN2_NO_V;
                            REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal VLAN header index"));
                            return 0;
                    }
                case(HEADER_TYPE_IPv4):
                    switch(field.ipv4)
                    {
                        case(NET_HEADER_FIELD_IPv4_PROTO):
                            return KG_SCH_GEN_IP_PID_NO_V;
                        default:
                            REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                            return 0;
                    }
                case(HEADER_TYPE_IPv6):
                   switch(field.ipv6)
                    {
                        case(NET_HEADER_FIELD_IPv6_NEXT_HDR):
                            return KG_SCH_GEN_IP_PID_NO_V;
                        default:
                            REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                            return 0;
                    }
                case(HEADER_TYPE_MPLS):
                case(HEADER_TYPE_LLC_SNAP):
                case(HEADER_TYPE_PPPoE):
                case(HEADER_TYPE_GRE):
                case(HEADER_TYPE_MINENCAP):
                case(HEADER_TYPE_USER_DEFINED_L3):
                case(HEADER_TYPE_TCP):
                case(HEADER_TYPE_UDP):
                case(HEADER_TYPE_IPSEC_AH):
                case(HEADER_TYPE_IPSEC_ESP):
                case(HEADER_TYPE_SCTP):
                case(HEADER_TYPE_DCCP):
                case(HEADER_TYPE_USER_DEFINED_L4):
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Header not supported"));
                    return 0;
            }
}

static t_KnownFieldsMasks GetKnownProtMask(e_NetHeaderType hdr, e_FmPcdHdrIndex index, t_FmPcdFields field)
{
    switch(hdr)
    {
        case(HEADER_TYPE_NONE):
            ASSERT_COND(FALSE);
        case(HEADER_TYPE_ETH):
            switch(field.eth)
            {
                case(NET_HEADER_FIELD_ETH_DA):
                    return KG_SCH_KN_MACDST;
                case(NET_HEADER_FIELD_ETH_SA):
                    return KG_SCH_KN_MACSRC;
                case(NET_HEADER_FIELD_ETH_TYPE):
                    return KG_SCH_KN_ETYPE;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
           }
        case(HEADER_TYPE_LLC_SNAP):
            switch(field.llcSnap)
            {
                case(NET_HEADER_FIELD_LLC_SNAP_TYPE):
                    return KG_SCH_KN_ETYPE;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
           }
        case(HEADER_TYPE_VLAN):
            switch(field.vlan)
            {
                case(NET_HEADER_FIELD_VLAN_TCI):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return KG_SCH_KN_TCI1;
                    if(index == e_FM_PCD_HDR_INDEX_LAST)
                        return KG_SCH_KN_TCI2;
                    else
                    {
                        REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                        return 0;
                    }
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
            }
        case(HEADER_TYPE_MPLS):
            switch(field.mpls)
            {
                case(NET_HEADER_FIELD_MPLS_LABEL_STACK):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return KG_SCH_KN_MPLS1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return KG_SCH_KN_MPLS2;
                    if(index == e_FM_PCD_HDR_INDEX_LAST)
                        return KG_SCH_KN_MPLS_LAST;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal MPLS index"));
                    return 0;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
            }
        case(HEADER_TYPE_IPv4):
            switch(field.ipv4)
            {
                case(NET_HEADER_FIELD_IPv4_SRC_IP):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return KG_SCH_KN_IPSRC1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return KG_SCH_KN_IPSRC2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv4 index"));
                    return 0;
                case(NET_HEADER_FIELD_IPv4_DST_IP):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return KG_SCH_KN_IPDST1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return KG_SCH_KN_IPDST2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv4 index"));
                    return 0;
                case(NET_HEADER_FIELD_IPv4_PROTO):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return KG_SCH_KN_PTYPE1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return KG_SCH_KN_PTYPE2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv4 index"));
                    return 0;
                case(NET_HEADER_FIELD_IPv4_TOS):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return KG_SCH_KN_IPTOS_TC1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return KG_SCH_KN_IPTOS_TC2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv4 index"));
                    return 0;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
            }
        case(HEADER_TYPE_IPv6):
             switch(field.ipv6)
            {
                case(NET_HEADER_FIELD_IPv6_SRC_IP):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return KG_SCH_KN_IPSRC1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return KG_SCH_KN_IPSRC2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv6 index"));
                    return 0;
                case(NET_HEADER_FIELD_IPv6_DST_IP):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return KG_SCH_KN_IPDST1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return KG_SCH_KN_IPDST2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv6 index"));
                    return 0;
                case(NET_HEADER_FIELD_IPv6_NEXT_HDR):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return KG_SCH_KN_PTYPE1;
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return KG_SCH_KN_PTYPE2;
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv6 index"));
                    return 0;
                case(NET_HEADER_FIELD_IPv6_VER | NET_HEADER_FIELD_IPv6_FL | NET_HEADER_FIELD_IPv6_TC):
                    if((index == e_FM_PCD_HDR_INDEX_NONE) || (index == e_FM_PCD_HDR_INDEX_1))
                        return (KG_SCH_KN_IPV6FL1 | KG_SCH_KN_IPTOS_TC1);
                    if(index == e_FM_PCD_HDR_INDEX_2)
                        return (KG_SCH_KN_IPV6FL2 | KG_SCH_KN_IPTOS_TC2);
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal IPv6 index"));
                    return 0;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
            }
        case(HEADER_TYPE_GRE):
            switch(field.gre)
            {
                case(NET_HEADER_FIELD_GRE_TYPE):
                    return KG_SCH_KN_GREPTYPE;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
           }
        case(HEADER_TYPE_MINENCAP):
            switch(field.minencap)
            {
                case(NET_HEADER_FIELD_MINENCAP_SRC_IP):
                    return KG_SCH_KN_IPSRC2;
                case(NET_HEADER_FIELD_MINENCAP_DST_IP):
                    return KG_SCH_KN_IPDST2;
                case(NET_HEADER_FIELD_MINENCAP_TYPE):
                    return KG_SCH_KN_PTYPE2;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
           }
        case(HEADER_TYPE_TCP):
            switch(field.tcp)
            {
                case(NET_HEADER_FIELD_TCP_PORT_SRC):
                    return KG_SCH_KN_L4PSRC;
                case(NET_HEADER_FIELD_TCP_PORT_DST):
                    return KG_SCH_KN_L4PDST;
                case(NET_HEADER_FIELD_TCP_FLAGS):
                    return KG_SCH_KN_TFLG;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
            }
        case(HEADER_TYPE_UDP):
            switch(field.udp)
            {
                case(NET_HEADER_FIELD_UDP_PORT_SRC):
                    return KG_SCH_KN_L4PSRC;
                case(NET_HEADER_FIELD_UDP_PORT_DST):
                    return KG_SCH_KN_L4PDST;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
            }
        case(HEADER_TYPE_IPSEC_AH):
            switch(field.ipsecAh)
            {
                case(NET_HEADER_FIELD_IPSEC_AH_SPI):
                    return KG_SCH_KN_IPSEC_SPI;
                case(NET_HEADER_FIELD_IPSEC_AH_NH):
                    return KG_SCH_KN_IPSEC_NH;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
            }
        case(HEADER_TYPE_IPSEC_ESP):
            switch(field.ipsecEsp)
            {
                case(NET_HEADER_FIELD_IPSEC_ESP_SPI):
                    return KG_SCH_KN_IPSEC_SPI;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
            }
        case(HEADER_TYPE_SCTP):
            switch(field.sctp)
            {
                case(NET_HEADER_FIELD_SCTP_PORT_SRC):
                    return KG_SCH_KN_L4PSRC;
                case(NET_HEADER_FIELD_SCTP_PORT_DST):
                    return KG_SCH_KN_L4PDST;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
            }
        case(HEADER_TYPE_DCCP):
            switch(field.dccp)
            {
                case(NET_HEADER_FIELD_DCCP_PORT_SRC):
                    return KG_SCH_KN_L4PSRC;
                case(NET_HEADER_FIELD_DCCP_PORT_DST):
                    return KG_SCH_KN_L4PDST;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
            }
        case(HEADER_TYPE_PPPoE):
            switch(field.pppoe)
            {
                case(NET_HEADER_FIELD_PPPoE_PID):
                    return KG_SCH_KN_PPPID;
                case(NET_HEADER_FIELD_PPPoE_SID):
                    return KG_SCH_KN_PPPSID;
                default:
                    REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
                    return 0;
            }
        default:
            REPORT_ERROR(MAJOR, E_NOT_SUPPORTED, ("Extraction not supported"));
            return 0;
    }
}


static uint8_t GetKnownFieldId(uint32_t bitMask)
{
    uint8_t cnt = 0;

    while (bitMask)
        if(bitMask & 0x80000000)
            break;
        else
        {
            cnt++;
            bitMask <<= 1;
        }
    return cnt;

}

static uint8_t GetExtractedOrMask(uint8_t bitOffset, bool fqid)
{
    uint8_t i, mask, numOfOnesToClear, walking1Mask = 1;

    /* bitOffset 1-7 --> mask 0x1-0x7F */
    if(bitOffset<8)
    {
        mask = 0;
        for(i = 0 ; i < bitOffset ; i++, walking1Mask <<= 1)
            mask |= walking1Mask;
    }
    else
    {
       mask = 0xFF;
       numOfOnesToClear = 0;
       if(fqid && bitOffset>24)
           /* bitOffset 25-31 --> mask 0xFE-0x80 */
           numOfOnesToClear = (uint8_t)(bitOffset-24);
       else
          /* bitOffset 9-15 --> mask 0xFE-0x80 */
          if(!fqid && bitOffset>8)
               numOfOnesToClear = (uint8_t)(bitOffset-8);
       for(i = 0 ; i < numOfOnesToClear ; i++, walking1Mask <<= 1)
           mask &= ~walking1Mask;
       /* bitOffset 8-24 for FQID, 8 for PP --> no mask (0xFF)*/
    }
    return mask;
}


t_Error FmPcdKgBuildClsPlanGrp(t_Handle h_FmPcd, t_FmPcdKgInterModuleClsPlanGrpParams *p_Grp, t_FmPcdKgInterModuleClsPlanSet *p_ClsPlanSet)
{
    t_FmPcd                         *p_FmPcd = (t_FmPcd*)h_FmPcd;
    t_FmPcdKgClsPlanGrp             *p_ClsPlanGrp;
    t_FmPcdIpcKgClsPlanParams       kgAlloc;
    t_Error                         err = E_OK;
    uint32_t                        oredVectors = 0;
    uint32_t                        intFlags;
    int                             i, j;

    if (p_Grp->numOfOptions >= FM_PCD_MAX_NUM_OF_OPTIONS(FM_PCD_MAX_NUM_OF_CLS_PLANS))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE,("Too many classification plan basic options selected."));

    intFlags = FmPcdLock(p_FmPcd);

    /* find a new clsPlan group */
    for(i = 0;i<FM_MAX_NUM_OF_PORTS;i++)
        if(!p_FmPcd->p_FmPcdKg->clsPlanGrps[i].used)
            break;
    if(i== FM_MAX_NUM_OF_PORTS)
    {
        FmPcdUnlock(p_FmPcd, intFlags);
        RETURN_ERROR(MAJOR, E_FULL,("No classification plan groups available."));
    }
    p_FmPcd->p_FmPcdKg->clsPlanGrps[i].used = TRUE;
    p_Grp->clsPlanGrpId = (uint8_t)i;

    if(p_Grp->numOfOptions == 0)
        p_FmPcd->p_FmPcdKg->emptyClsPlanGrpId = (uint8_t)i;

    if (!TRY_LOCK(NULL, &p_FmPcd->p_FmPcdKg->clsPlanGrps[p_Grp->clsPlanGrpId].lock))
    {
        FmPcdUnlock(p_FmPcd, intFlags);
        return ERROR_CODE(E_BUSY);
    }
    FmPcdUnlock(p_FmPcd, intFlags);

    p_ClsPlanGrp = &p_FmPcd->p_FmPcdKg->clsPlanGrps[i];
    p_ClsPlanGrp->netEnvId = p_Grp->netEnvId;
    p_ClsPlanGrp->owners = 0;
    FmPcdSetClsPlanGrpId(p_FmPcd, p_Grp->netEnvId, p_Grp->clsPlanGrpId);
    FmPcdIncNetEnvOwners(p_FmPcd, p_Grp->netEnvId);

    p_ClsPlanGrp->sizeOfGrp = (uint16_t)(1<<p_Grp->numOfOptions);
    /* a minimal group of 8 is required */
    if(p_ClsPlanGrp->sizeOfGrp < CLS_PLAN_NUM_PER_GRP)
        p_ClsPlanGrp->sizeOfGrp = CLS_PLAN_NUM_PER_GRP;
    if(p_FmPcd->guestId == NCSW_MASTER_ID)
    {
        err = KgAllocClsPlanEntries(h_FmPcd, p_ClsPlanGrp->sizeOfGrp, p_FmPcd->guestId, &p_ClsPlanGrp->baseEntry);

        if(err)
        {
            RELEASE_LOCK(p_FmPcd->p_FmPcdKg->clsPlanGrps[p_Grp->clsPlanGrpId].lock);
            RETURN_ERROR(MINOR, E_INVALID_STATE, NO_MSG);
        }

    }
    else
    {
        t_FmPcdIpcMsg   msg;
        uint32_t        replyLength;
        t_FmPcdIpcReply reply;

        /* in GUEST_PARTITION, we use the IPC, to also set a private driver group if required */
        memset(&reply, 0, sizeof(reply));
        memset(&msg, 0, sizeof(msg));
        memset(&kgAlloc, 0, sizeof(kgAlloc));
        kgAlloc.guestId = p_FmPcd->guestId;
        kgAlloc.numOfClsPlanEntries = p_ClsPlanGrp->sizeOfGrp;
        msg.msgId = FM_PCD_ALLOC_KG_CLSPLAN;
        memcpy(msg.msgBody, &kgAlloc, sizeof(kgAlloc));
        replyLength = (sizeof(uint32_t) + sizeof(p_ClsPlanGrp->baseEntry));
        if ((err = XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) + sizeof(kgAlloc),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
        {
            RELEASE_LOCK(p_FmPcd->p_FmPcdKg->clsPlanGrps[p_Grp->clsPlanGrpId].lock);
            RETURN_ERROR(MAJOR, err, NO_MSG);
        }

        if (replyLength != (sizeof(uint32_t) + sizeof(p_ClsPlanGrp->baseEntry)))
        {
            RELEASE_LOCK(p_FmPcd->p_FmPcdKg->clsPlanGrps[p_Grp->clsPlanGrpId].lock);
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
        }
        if ((t_Error)reply.error != E_OK)
        {
            RELEASE_LOCK(p_FmPcd->p_FmPcdKg->clsPlanGrps[p_Grp->clsPlanGrpId].lock);
            RETURN_ERROR(MINOR, (t_Error)reply.error, NO_MSG);
        }

        p_ClsPlanGrp->baseEntry = *(uint8_t*)(reply.replyBody);
    }

    /* build classification plan entries parameters */
    p_ClsPlanSet->baseEntry = p_ClsPlanGrp->baseEntry;
    p_ClsPlanSet->numOfClsPlanEntries = p_ClsPlanGrp->sizeOfGrp;

    oredVectors = 0;
    for(i = 0; i<p_Grp->numOfOptions; i++)
    {
        oredVectors |= p_Grp->optVectors[i];
        /* save an array of used options - the indexes represent the power of 2 index */
        p_ClsPlanGrp->optArray[i] = p_Grp->options[i];
    }
    /* set the classification plan relevant entries so that all bits
     * relevant to the list of options is cleared
     */
    for(j = 0; j<p_ClsPlanGrp->sizeOfGrp; j++)
        p_ClsPlanSet->vectors[j] = ~oredVectors;

    for(i = 0; i<p_Grp->numOfOptions; i++)
    {
       /* option i got the place 2^i in the clsPlan array. all entries that
         * have bit i set, should have the vector bit cleared. So each option
         * has one location that it is exclusive (1,2,4,8...) and represent the
         * presence of that option only, and other locations that represent a
         * combination of options.
         * e.g:
         * If ethernet-BC is option 1 it gets entry 2 in the table. Entry 2
         * now represents a frame with ethernet-BC header - so the bit
         * representing ethernet-BC should be set and all other option bits
         * should be cleared.
         * Entries 2,3,6,7,10... also have ethernet-BC and therefore have bit
         * vector[1] set, but they also have other bits set:
         * 3=1+2, options 0 and 1
         * 6=2+4, options 1 and 2
         * 7=1+2+4, options 0,1,and 2
         * 10=2+8, options 1 and 3
         * etc.
         * */

        /* now for each option (i), we set their bits in all entries (j)
         * that contain bit 2^i.
         */
        for(j = 0; j<p_ClsPlanGrp->sizeOfGrp; j++)
        {
            if(j & (1<<i))
                p_ClsPlanSet->vectors[j] |= p_Grp->optVectors[i];
        }
    }

    RELEASE_LOCK(p_FmPcd->p_FmPcdKg->clsPlanGrps[p_Grp->clsPlanGrpId].lock);

    return E_OK;
}

void FmPcdKgDestroyClsPlanGrp(t_Handle h_FmPcd, uint8_t grpId)
{
    t_FmPcd                         *p_FmPcd = (t_FmPcd*)h_FmPcd;
    t_FmPcdIpcKgClsPlanParams       kgAlloc;
    t_Error                         err;
    t_FmPcdIpcMsg                   msg;
    uint32_t                        replyLength;
    t_FmPcdIpcReply                 reply;

    /* check that no port is bound to this clsPlan */
    if(p_FmPcd->p_FmPcdKg->clsPlanGrps[grpId].owners)
    {
        REPORT_ERROR(MINOR, E_INVALID_STATE, ("Trying to delete a clsPlan grp that has ports bound to"));
        return;
    }

    FmPcdDecNetEnvOwners(p_FmPcd, p_FmPcd->p_FmPcdKg->clsPlanGrps[grpId].netEnvId);

    /* free blocks */
    if(p_FmPcd->guestId == NCSW_MASTER_ID)
    {
        KgFreeClsPlanEntries(h_FmPcd,
                             p_FmPcd->p_FmPcdKg->clsPlanGrps[grpId].sizeOfGrp,
                             p_FmPcd->guestId,
                             p_FmPcd->p_FmPcdKg->clsPlanGrps[grpId].baseEntry);
    }
    else    /* in GUEST_PARTITION, we use the IPC, to also set a private driver group if required */
    {
        memset(&reply, 0, sizeof(reply));
        memset(&msg, 0, sizeof(msg));
        kgAlloc.guestId = p_FmPcd->guestId;
        kgAlloc.numOfClsPlanEntries = p_FmPcd->p_FmPcdKg->clsPlanGrps[grpId].sizeOfGrp;
        kgAlloc.clsPlanBase = p_FmPcd->p_FmPcdKg->clsPlanGrps[grpId].baseEntry;
        msg.msgId = FM_PCD_FREE_KG_CLSPLAN;
        memcpy(msg.msgBody, &kgAlloc, sizeof(kgAlloc));
        replyLength = sizeof(uint32_t);
        if ((err = XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                     (uint8_t*)&msg,
                                     sizeof(msg.msgId) + sizeof(kgAlloc),
                                     (uint8_t*)&reply,
                                     &replyLength,
                                     NULL,
                                     NULL)) != E_OK)
        {
            REPORT_ERROR(MINOR, err, NO_MSG);
            return;
        }
        if (replyLength != sizeof(uint32_t))
        {
            REPORT_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
            return;
        }
        if((t_Error)reply.error != E_OK)
        {
            REPORT_ERROR(MAJOR, E_INVALID_STATE, ("Free KG clsPlan failed"));
            return;
        }
    }

    if(grpId == p_FmPcd->p_FmPcdKg->emptyClsPlanGrpId)
        p_FmPcd->p_FmPcdKg->emptyClsPlanGrpId = ILLEGAL_CLS_PLAN;
    /* clear clsPlan driver structure */
    memset(&p_FmPcd->p_FmPcdKg->clsPlanGrps[grpId], 0, sizeof(t_FmPcdKgClsPlanGrp));
}

t_Error FmPcdKgBuildBindPortToSchemes(t_Handle h_FmPcd , t_FmPcdKgInterModuleBindPortToSchemes *p_BindPort, uint32_t *p_SpReg, bool add)
{
    t_FmPcd                 *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t                j, schemesPerPortVector = 0;
    t_FmPcdKgScheme         *p_Scheme;
    uint8_t                 i, relativeSchemeId;
    uint32_t                tmp, walking1Mask;
    uint8_t                 swPortIndex = 0;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdKg, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE);

    /* for each scheme */
    for(i = 0; i<p_BindPort->numOfSchemes; i++)
    {
        relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmPcd, p_BindPort->schemesIds[i]);
        if(relativeSchemeId >= FM_PCD_KG_NUM_OF_SCHEMES)
            RETURN_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);

        if(add)
        {
            if (!FmPcdKgIsSchemeValidSw(h_FmPcd, relativeSchemeId))
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Requested scheme is invalid."));

            p_Scheme = &p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId];
            /* check netEnvId  of the port against the scheme netEnvId */
            if((p_Scheme->netEnvId != p_BindPort->netEnvId) && (p_Scheme->netEnvId != ILLEGAL_NETENV))
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Port may not be bound to requested scheme - differ in netEnvId"));

            /* if next engine is private port policer profile, we need to check that it is valid */
            HW_PORT_ID_TO_SW_PORT_INDX(swPortIndex, p_BindPort->hardwarePortId);
            if(p_Scheme->nextRelativePlcrProfile)
            {
                for(j = 0;j<p_Scheme->numOfProfiles;j++)
                {
                    ASSERT_COND(p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].h_FmPort);
                    if(p_Scheme->relativeProfileId+j >= p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].numOfProfiles)
                        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Relative profile not in range"));
                     if(!FmPcdPlcrIsProfileValid(p_FmPcd, (uint16_t)(p_FmPcd->p_FmPcdPlcr->portsMapping[swPortIndex].profilesBase + p_Scheme->relativeProfileId + j)))
                        RETURN_ERROR(MINOR, E_INVALID_STATE, ("Relative profile not valid."));
                }
            }
            if(!p_BindPort->useClsPlan)
            {
                /* if this port does not use clsPlan, it may not be bound to schemes with units that contain
                cls plan options. Schemes that are used only directly, should not be checked.
                it also may not be bound to schemes that go to CC with units that are options  - so we OR
                the match vector and the grpBits (= ccUnits) */
                if ((p_Scheme->matchVector != SCHEME_ALWAYS_DIRECT) || p_Scheme->ccUnits)
                {
                    walking1Mask = 0x80000000;
                    tmp = (p_Scheme->matchVector == SCHEME_ALWAYS_DIRECT)? 0:p_Scheme->matchVector;
                    tmp |= p_Scheme->ccUnits;
                    while (tmp)
                    {
                        if(tmp & walking1Mask)
                        {
                            tmp &= ~walking1Mask;
                            if(!PcdNetEnvIsUnitWithoutOpts(p_FmPcd, p_Scheme->netEnvId, walking1Mask))
                                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Port (without clsPlan) may not be bound to requested scheme - uses clsPlan options"));
                        }
                        walking1Mask >>= 1;
                    }
                }
            }
        }
        /* build vector */
        schemesPerPortVector |= 1 << (31 - p_BindPort->schemesIds[i]);
    }

    *p_SpReg = schemesPerPortVector;

    return E_OK;
}

void FmPcdKgIncSchemeOwners(t_Handle h_FmPcd , t_FmPcdKgInterModuleBindPortToSchemes *p_BindPort)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    int             i;
    t_FmPcdKgScheme *p_Scheme;

    /* for each scheme - update owners counters */
    for(i = 0; i<p_BindPort->numOfSchemes; i++)
    {
        p_Scheme = &p_FmPcd->p_FmPcdKg->schemes[p_BindPort->schemesIds[i]];

        /* increment owners number */
        p_Scheme->owners++;
    }
}

void FmPcdKgDecSchemeOwners(t_Handle h_FmPcd , t_FmPcdKgInterModuleBindPortToSchemes *p_BindPort)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    int             i;
    t_FmPcdKgScheme *p_Scheme;

    /* for each scheme - update owners counters */
    for(i = 0; i<p_BindPort->numOfSchemes; i++)
    {
        p_Scheme = &p_FmPcd->p_FmPcdKg->schemes[p_BindPort->schemesIds[i]];

        /* increment owners number */
        ASSERT_COND(p_Scheme->owners);
        p_Scheme->owners--;
    }
}

static t_Error KgWriteSp(t_FmPcd *p_FmPcd, uint8_t hardwarePortId, uint32_t spReg, bool add)
{
    t_FmPcdKgPortConfigRegs *p_FmPcdKgPortRegs;
    uint32_t                tmpKgarReg = 0, tmpKgpeSp, intFlags;
    t_Error                 err = E_OK;

    if (p_FmPcd->h_Hc)
        return FmHcKgWriteSp(p_FmPcd->h_Hc, hardwarePortId, spReg, add);

    p_FmPcdKgPortRegs = &p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.portRegs;

    tmpKgarReg = FmPcdKgBuildReadPortSchemeBindActionReg(hardwarePortId);
    intFlags = FmPcdLock(p_FmPcd);
    err = WriteKgarWait(p_FmPcd, tmpKgarReg);
    if(err)
    {
        FmPcdUnlock(p_FmPcd, intFlags);
        RETURN_ERROR(MINOR, err, NO_MSG);
    }

    tmpKgpeSp = GET_UINT32(p_FmPcdKgPortRegs->kgoe_sp);

    if(add)
        tmpKgpeSp |= spReg;
    else /* clear */
        tmpKgpeSp &= ~spReg;

    WRITE_UINT32(p_FmPcdKgPortRegs->kgoe_sp, tmpKgpeSp);

    tmpKgarReg = FmPcdKgBuildWritePortSchemeBindActionReg(hardwarePortId);

    err = WriteKgarWait(p_FmPcd, tmpKgarReg);
    FmPcdUnlock(p_FmPcd, intFlags);
    return err;
}

static t_Error KgWriteCpp(t_FmPcd *p_FmPcd, uint8_t hardwarePortId, uint32_t cppReg)
{
    t_FmPcdKgPortConfigRegs *p_FmPcdKgPortRegs;
    uint32_t                tmpKgarReg, intFlags;
    t_Error                 err;

    if (p_FmPcd->h_Hc)
        return FmHcKgWriteCpp(p_FmPcd->h_Hc, hardwarePortId, cppReg);

    p_FmPcdKgPortRegs = &p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.portRegs;
    intFlags = FmPcdLock(p_FmPcd);
    WRITE_UINT32(p_FmPcdKgPortRegs->kgoe_cpp, cppReg);

    tmpKgarReg = FmPcdKgBuildWritePortClsPlanBindActionReg(hardwarePortId);
    err = WriteKgarWait(p_FmPcd, tmpKgarReg);
    FmPcdUnlock(p_FmPcd, intFlags);

    return err;
}

static void FmPcdKgUnbindPortToClsPlanGrp(t_FmPcd *p_FmPcd, uint8_t hardwarePortId)
{
    KgWriteCpp(p_FmPcd, hardwarePortId, 0);
}

static t_Error KgBindPortToClsPlanGrp(t_FmPcd *p_FmPcd, uint8_t hardwarePortId, uint8_t clsPlanGrpId)
{
    uint32_t                tmpKgpeCpp = 0;

    tmpKgpeCpp = FmPcdKgBuildCppReg(p_FmPcd, clsPlanGrpId);
    return KgWriteCpp(p_FmPcd, hardwarePortId, tmpKgpeCpp);
}

t_Error FmPcdKgBindPortToSchemes(t_Handle h_FmPcd , t_FmPcdKgInterModuleBindPortToSchemes  *p_SchemeBind)
{
    t_FmPcd                 *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t                spReg;
    t_Error                 err = E_OK;

    err = FmPcdKgBuildBindPortToSchemes(h_FmPcd, p_SchemeBind, &spReg, TRUE);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    err = KgWriteSp(p_FmPcd, p_SchemeBind->hardwarePortId, spReg, TRUE);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    FmPcdKgIncSchemeOwners(h_FmPcd, p_SchemeBind);

    return E_OK;
}

t_Error FmPcdKgUnbindPortToSchemes(t_Handle h_FmPcd ,  t_FmPcdKgInterModuleBindPortToSchemes *p_SchemeBind)
{
    t_FmPcd                 *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t                spReg;
    t_Error                 err = E_OK;

    err = FmPcdKgBuildBindPortToSchemes(h_FmPcd, p_SchemeBind, &spReg, FALSE);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    err = KgWriteSp(p_FmPcd, p_SchemeBind->hardwarePortId, spReg, FALSE);
    if(err)
        RETURN_ERROR(MAJOR, err, NO_MSG);

    FmPcdKgDecSchemeOwners(h_FmPcd, p_SchemeBind);

    return E_OK;
}

bool     FmPcdKgIsSchemeValidSw(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    return p_FmPcd->p_FmPcdKg->schemes[schemeId].valid;
}

bool     KgIsSchemeAlwaysDirect(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    if(p_FmPcd->p_FmPcdKg->schemes[schemeId].matchVector == SCHEME_ALWAYS_DIRECT)
        return TRUE;
    else
        return FALSE;
}

t_Error  FmPcdKgAllocSchemes(t_Handle h_FmPcd, uint8_t numOfSchemes, uint8_t guestId, uint8_t *p_SchemesIds)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t            intFlags;
    uint8_t             i,j;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdKg, E_INVALID_HANDLE);

    intFlags = FmPcdLock(p_FmPcd);
    for(j=0,i=0;i<FM_PCD_KG_NUM_OF_SCHEMES && j<numOfSchemes;i++)
    {
        if(!p_FmPcd->p_FmPcdKg->schemesMng[i].allocated)
        {
            p_FmPcd->p_FmPcdKg->schemesMng[i].allocated = TRUE;
            p_FmPcd->p_FmPcdKg->schemesMng[i].ownerId = guestId;
            p_SchemesIds[j] = i;
            j++;
        }
    }

    if (j != numOfSchemes)
    {
        /* roll back */
        for(j--; j; j--)
        {
            p_FmPcd->p_FmPcdKg->schemesMng[p_SchemesIds[j]].allocated = FALSE;
            p_FmPcd->p_FmPcdKg->schemesMng[p_SchemesIds[j]].ownerId = 0;
            p_SchemesIds[j] = 0;
        }
        FmPcdUnlock(p_FmPcd, intFlags);
        RETURN_ERROR(MAJOR, E_NOT_AVAILABLE, ("No schemes found"));
    }
    FmPcdUnlock(p_FmPcd, intFlags);

    return E_OK;
}

t_Error  FmPcdKgFreeSchemes(t_Handle h_FmPcd, uint8_t numOfSchemes, uint8_t guestId, uint8_t *p_SchemesIds)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t            intFlags;
    uint8_t             i;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdKg, E_INVALID_HANDLE);

    intFlags = FmPcdLock(p_FmPcd);

    for(i=0;i<numOfSchemes;i++)
    {
        if(!p_FmPcd->p_FmPcdKg->schemesMng[p_SchemesIds[i]].allocated)
        {
            FmPcdUnlock(p_FmPcd, intFlags);
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Scheme was not previously allocated"));
        }
        if(p_FmPcd->p_FmPcdKg->schemesMng[p_SchemesIds[i]].ownerId != guestId)
        {
            FmPcdUnlock(p_FmPcd, intFlags);
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Scheme is not owned by caller. "));
        }
        p_FmPcd->p_FmPcdKg->schemesMng[p_SchemesIds[i]].allocated = FALSE;
        p_FmPcd->p_FmPcdKg->schemesMng[p_SchemesIds[i]].ownerId = 0;
    }

    FmPcdUnlock(p_FmPcd, intFlags);
    return E_OK;
}

t_Error  KgAllocClsPlanEntries(t_Handle h_FmPcd, uint16_t numOfClsPlanEntries, uint8_t guestId, uint8_t *p_First)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t    intFlags;
    uint8_t     numOfBlocks, blocksFound=0, first=0;
    uint8_t     i, j;

    intFlags = FmPcdLock(p_FmPcd);

    if(!numOfClsPlanEntries)
    {
        FmPcdUnlock(p_FmPcd, intFlags);
        return E_OK;
    }

    if ((numOfClsPlanEntries % CLS_PLAN_NUM_PER_GRP) || (!POWER_OF_2(numOfClsPlanEntries)))
    {
        FmPcdUnlock(p_FmPcd, intFlags);
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("numOfClsPlanEntries must be a power of 2 and divisible by 8"));
    }

    numOfBlocks =  (uint8_t)(numOfClsPlanEntries/CLS_PLAN_NUM_PER_GRP);

    /* try to find consequent blocks */
    first = 0;
    for(i=0;i<FM_PCD_MAX_NUM_OF_CLS_PLANS/CLS_PLAN_NUM_PER_GRP;)
    {
        if(!p_FmPcd->p_FmPcdKg->clsPlanBlocksMng[i].allocated)
        {
            blocksFound++;
            i++;
            if(blocksFound == numOfBlocks)
                break;
        }
        else
        {
            blocksFound = 0;
            /* advance i to the next aligned address */
            first = i = (uint8_t)(first + numOfBlocks);
        }
    }

    if(blocksFound == numOfBlocks)
    {
        *p_First = (uint8_t)(first*CLS_PLAN_NUM_PER_GRP);
        for(j = first; j<first + numOfBlocks; j++)
        {
            p_FmPcd->p_FmPcdKg->clsPlanBlocksMng[j].allocated = TRUE;
            p_FmPcd->p_FmPcdKg->clsPlanBlocksMng[j].ownerId = guestId;
        }
        FmPcdUnlock(p_FmPcd, intFlags);

        return E_OK;
    }
    else
    {
        FmPcdUnlock(p_FmPcd, intFlags);
        RETURN_ERROR(MINOR, E_FULL, ("No recources for clsPlan"));
    }
}

void  KgFreeClsPlanEntries(t_Handle h_FmPcd, uint16_t numOfClsPlanEntries, uint8_t guestId, uint8_t base)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t    intFlags;
    uint8_t     numOfBlocks;
    uint8_t     i, baseBlock;

    UNUSED( guestId);

    intFlags = FmPcdLock(p_FmPcd);

    numOfBlocks =  (uint8_t)(numOfClsPlanEntries/CLS_PLAN_NUM_PER_GRP);
    ASSERT_COND(!(base%CLS_PLAN_NUM_PER_GRP));

    baseBlock = (uint8_t)(base/CLS_PLAN_NUM_PER_GRP);
    for(i=baseBlock;i<baseBlock+numOfBlocks;i++)
    {
        ASSERT_COND(p_FmPcd->p_FmPcdKg->clsPlanBlocksMng[i].allocated);
        ASSERT_COND(guestId == p_FmPcd->p_FmPcdKg->clsPlanBlocksMng[i].ownerId);
        p_FmPcd->p_FmPcdKg->clsPlanBlocksMng[i].allocated = FALSE;
        p_FmPcd->p_FmPcdKg->clsPlanBlocksMng[i].ownerId = 0;
    }
    FmPcdUnlock(p_FmPcd, intFlags);
}

void KgEnable(t_FmPcd *p_FmPcd)
{
    t_FmPcdKgRegs               *p_Regs = p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs;

    ASSERT_COND(FmIsMaster(p_FmPcd->h_Fm));
    WRITE_UINT32(p_Regs->kggcr,GET_UINT32(p_Regs->kggcr) | FM_PCD_KG_KGGCR_EN);
}

void KgDisable(t_FmPcd *p_FmPcd)
{
    t_FmPcdKgRegs               *p_Regs = p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs;

    ASSERT_COND(FmIsMaster(p_FmPcd->h_Fm));
    WRITE_UINT32(p_Regs->kggcr,GET_UINT32(p_Regs->kggcr) & ~FM_PCD_KG_KGGCR_EN);
}

void KgSetClsPlan(t_Handle h_FmPcd, t_FmPcdKgInterModuleClsPlanSet *p_Set)
{
    t_FmPcd                 *p_FmPcd = (t_FmPcd*)h_FmPcd;
    t_FmPcdKgClsPlanRegs    *p_FmPcdKgPortRegs;
    uint32_t                tmpKgarReg=0, intFlags;
    uint16_t                i, j;

    SANITY_CHECK_RETURN(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN(p_FmPcd->p_FmPcdKg, E_INVALID_HANDLE);

    ASSERT_COND(FmIsMaster(p_FmPcd->h_Fm));
    p_FmPcdKgPortRegs = &p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.clsPlanRegs;

    intFlags = FmPcdLock(p_FmPcd);
    for(i=p_Set->baseEntry;i<p_Set->baseEntry+p_Set->numOfClsPlanEntries;i+=8)
    {
        tmpKgarReg = FmPcdKgBuildWriteClsPlanBlockActionReg((uint8_t)(i / CLS_PLAN_NUM_PER_GRP));

        for (j = i; j < i+8; j++)
        {
            ASSERT_COND(IN_RANGE(0, (j - p_Set->baseEntry), FM_PCD_MAX_NUM_OF_CLS_PLANS-1));
            WRITE_UINT32(p_FmPcdKgPortRegs->kgcpe[j % CLS_PLAN_NUM_PER_GRP],p_Set->vectors[j - p_Set->baseEntry]);
        }

        if(WriteKgarWait(p_FmPcd, tmpKgarReg) != E_OK)
        {
            REPORT_ERROR(MAJOR, E_INVALID_STATE, ("WriteKgarWait FAILED"));
            return;
        }
    }
    FmPcdUnlock(p_FmPcd, intFlags);
}

static void PcdKgErrorException(t_Handle h_FmPcd)
{
    t_FmPcd                 *p_FmPcd = (t_FmPcd *)h_FmPcd;
    uint32_t                event, force, schemeIndexes = 0,index = 0, mask = 0;

    ASSERT_COND(FmIsMaster(p_FmPcd->h_Fm));
    event = GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->kgeer);
    mask = GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->kgeeer);

    schemeIndexes = GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->kgseer);
    schemeIndexes &= GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->kgseeer);

    event &= mask;

    /* clear the forced events */
    force = GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->kgfeer);
    if(force & event)
        WRITE_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->kgfeer, force & ~event);

    WRITE_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->kgeer, event);
    WRITE_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->kgseer, schemeIndexes);

    if(event & FM_PCD_KG_DOUBLE_ECC)
        p_FmPcd->f_Exception(p_FmPcd->h_App,e_FM_PCD_KG_EXCEPTION_DOUBLE_ECC);
    if(event & FM_PCD_KG_KEYSIZE_OVERFLOW)
    {
        if(schemeIndexes)
        {
            while(schemeIndexes)
            {
                if(schemeIndexes & 0x1)
                    p_FmPcd->f_FmPcdIndexedException(p_FmPcd->h_App,e_FM_PCD_KG_EXCEPTION_KEYSIZE_OVERFLOW, (uint16_t)(31 - index));
                schemeIndexes >>= 1;
                index+=1;
            }
        }
        else /* this should happen only when interrupt is forced. */
            p_FmPcd->f_Exception(p_FmPcd->h_App,e_FM_PCD_KG_EXCEPTION_KEYSIZE_OVERFLOW);
    }
}

static t_Error KgInitGuest(t_FmPcd *p_FmPcd)
{
    t_Error                     err = E_OK;
    t_FmPcdIpcKgSchemesParams   kgAlloc;
    uint32_t                    replyLength;
    t_FmPcdIpcReply             reply;
    t_FmPcdIpcMsg               msg;

    ASSERT_COND(p_FmPcd->guestId != NCSW_MASTER_ID);

    /* in GUEST_PARTITION, we use the IPC  */
    memset(&reply, 0, sizeof(reply));
    memset(&msg, 0, sizeof(msg));
    memset(&kgAlloc, 0, sizeof(t_FmPcdIpcKgSchemesParams));
    kgAlloc.numOfSchemes = p_FmPcd->p_FmPcdKg->numOfSchemes;
    kgAlloc.guestId = p_FmPcd->guestId;
    msg.msgId = FM_PCD_ALLOC_KG_SCHEMES;
    memcpy(msg.msgBody, &kgAlloc, sizeof(kgAlloc));
    replyLength = sizeof(uint32_t) + p_FmPcd->p_FmPcdKg->numOfSchemes*sizeof(uint8_t);
    if ((err = XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                 (uint8_t*)&msg,
                                 sizeof(msg.msgId) + sizeof(kgAlloc),
                                 (uint8_t*)&reply,
                                 &replyLength,
                                 NULL,
                                 NULL)) != E_OK)
        RETURN_ERROR(MAJOR, err, NO_MSG);
    if(replyLength != (sizeof(uint32_t) + p_FmPcd->p_FmPcdKg->numOfSchemes*sizeof(uint8_t)))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));
    memcpy(p_FmPcd->p_FmPcdKg->schemesIds, (uint8_t*)(reply.replyBody),p_FmPcd->p_FmPcdKg->numOfSchemes*sizeof(uint8_t));

    return (t_Error)reply.error;
}

static t_Error KgInitMaster(t_FmPcd *p_FmPcd)
{
    t_Error                     err = E_OK;
    t_FmPcdKgRegs               *p_Regs = p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs;
    int                         i;
    uint8_t                     hardwarePortId = 0;
    uint32_t                    tmpReg;

    ASSERT_COND(p_FmPcd->guestId == NCSW_MASTER_ID);

    /**********************KGEER******************/
    WRITE_UINT32(p_Regs->kgeer, (FM_PCD_KG_DOUBLE_ECC | FM_PCD_KG_KEYSIZE_OVERFLOW));
    /**********************KGEER******************/

    /**********************KGEEER******************/
    tmpReg = 0;
    if(p_FmPcd->exceptions & FM_PCD_EX_KG_DOUBLE_ECC)
    {
        FmEnableRamsEcc(p_FmPcd->h_Fm);
        tmpReg |= FM_PCD_KG_DOUBLE_ECC;
    }
    if(p_FmPcd->exceptions & FM_PCD_EX_KG_KEYSIZE_OVERFLOW)
        tmpReg |= FM_PCD_KG_KEYSIZE_OVERFLOW;
    WRITE_UINT32(p_Regs->kgeeer,tmpReg);
    /**********************KGEEER******************/

    /**********************KGFDOR******************/
    WRITE_UINT32(p_Regs->kgfdor,0);
    /**********************KGFDOR******************/

    /**********************KGGDV0R******************/
    WRITE_UINT32(p_Regs->kggdv0r,0);
    /**********************KGGDV0R******************/

    /**********************KGGDV1R******************/
    WRITE_UINT32(p_Regs->kggdv1r,0);
    /**********************KGGDV1R******************/

    /**********************KGGCR******************/
    WRITE_UINT32(p_Regs->kggcr, NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME);
    /**********************KGGCR******************/

    /* register even if no interrupts enabled, to allow future enablement */
    FmRegisterIntr(p_FmPcd->h_Fm, e_FM_MOD_KG, 0, e_FM_INTR_TYPE_ERR, PcdKgErrorException, p_FmPcd);

    /* clear binding between ports to schemes so that all ports are not bound to any schemes */
    for (i=0;i<FM_MAX_NUM_OF_PORTS;i++)
    {
        SW_PORT_INDX_TO_HW_PORT_ID(hardwarePortId, i);

        err = KgWriteSp(p_FmPcd, hardwarePortId, 0xffffffff, FALSE);
        if(err)
            RETURN_ERROR(MINOR, err, NO_MSG);

        err = KgWriteCpp(p_FmPcd, hardwarePortId, 0);
        if(err)
            RETURN_ERROR(MINOR, err, NO_MSG);
    }

    /* enable and enable all scheme interrupts */
    WRITE_UINT32(p_Regs->kgseer, 0xFFFFFFFF);
    WRITE_UINT32(p_Regs->kgseeer, 0xFFFFFFFF);

    if(p_FmPcd->p_FmPcdKg->numOfSchemes)
    {
        err = FmPcdKgAllocSchemes(p_FmPcd,
                                  p_FmPcd->p_FmPcdKg->numOfSchemes,
                                  p_FmPcd->guestId,
                                  p_FmPcd->p_FmPcdKg->schemesIds);
        if(err)
            RETURN_ERROR(MINOR, err, NO_MSG);
    }

    return E_OK;
}


/****************************************/
/*  API routines                        */
/****************************************/
t_Error FM_PCD_KgSetAdditionalDataAfterParsing(t_Handle h_FmPcd, uint8_t payloadOffset)
{
   t_FmPcd              *p_FmPcd = (t_FmPcd*)h_FmPcd;
   t_FmPcdKgRegs        *p_Regs;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdKg, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs, E_NULL_POINTER);

    p_Regs = p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs;
    if(!FmIsMaster(p_FmPcd->h_Fm))
        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("FM_PCD_KgSetAdditionalDataAfterParsing - guest mode!"));

/* not needed
    if(payloadOffset > 256)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("data exatraction offset from parseing end can not be more than 256"));
*/

    WRITE_UINT32(p_Regs->kgfdor,payloadOffset);

    return E_OK;
}

t_Error FM_PCD_KgSetDfltValue(t_Handle h_FmPcd, uint8_t valueId, uint32_t value)
{
   t_FmPcd              *p_FmPcd = (t_FmPcd*)h_FmPcd;
   t_FmPcdKgRegs        *p_Regs;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(((valueId == 0) || (valueId == 1)), E_INVALID_VALUE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdKg, E_NULL_POINTER);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs, E_NULL_POINTER);

    p_Regs = p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs;

    if(!FmIsMaster(p_FmPcd->h_Fm))
        RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("FM_PCD_KgSetDfltValue - guest mode!"));

    if(valueId == 0)
        WRITE_UINT32(p_Regs->kggdv0r,value);
    else
        WRITE_UINT32(p_Regs->kggdv1r,value);
    return E_OK;
}

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
t_Error FM_PCD_KgDumpRegs(t_Handle h_FmPcd)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    int                 i = 0, j = 0;
    uint8_t             hardwarePortId = 0;
    uint32_t            tmpKgarReg, intFlags;
    t_Error             err = E_OK;
    t_FmPcdIpcMsg       msg;

    DECLARE_DUMP;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdKg, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE);

    if(p_FmPcd->guestId != NCSW_MASTER_ID)
    {
        memset(&msg, 0, sizeof(msg));
        msg.msgId = FM_PCD_KG_DUMP_REGS;
        return XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                 (uint8_t*)&msg,
                                 sizeof(msg.msgId),
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
    }
    DUMP_SUBTITLE(("\n"));
    DUMP_TITLE(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs, ("FmPcdKgRegs Regs"));

    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kggcr);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kgeer);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kgeeer);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kgseer);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kgseeer);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kggsr);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kgtpc);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kgserc);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kgfdor);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kggdv0r);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kggdv1r);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kgfer);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kgfeer);
    DUMP_VAR(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs,kgar);

    DUMP_SUBTITLE(("\n"));
    intFlags = FmPcdLock(p_FmPcd);
    for(j = 0;j<FM_PCD_KG_NUM_OF_SCHEMES;j++)
    {
        tmpKgarReg = FmPcdKgBuildReadSchemeActionReg((uint8_t)j);
        if(WriteKgarWait(p_FmPcd, tmpKgarReg) != E_OK)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);

        DUMP_TITLE(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs, ("FmPcdKgIndirectAccessSchemeRegs Scheme %d Regs", j));

        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_mode);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_ekfc);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_ekdv);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_bmch);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_bmcl);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_fqb);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_hc);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_ppc);

        DUMP_TITLE(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs.kgse_gec, ("kgse_gec"));
        DUMP_SUBSTRUCT_ARRAY(i, FM_PCD_KG_NUM_OF_GENERIC_REGS)
        {
            DUMP_MEMORY(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs.kgse_gec[i], sizeof(uint32_t));
        }

        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_spc);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_dv0);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_dv1);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_ccbs);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs,kgse_mv);
    }
    DUMP_SUBTITLE(("\n"));

    for (i=0;i<FM_MAX_NUM_OF_PORTS;i++)
    {
        SW_PORT_INDX_TO_HW_PORT_ID(hardwarePortId, i);

        tmpKgarReg = FmPcdKgBuildReadPortSchemeBindActionReg(hardwarePortId);

        err = WriteKgarWait(p_FmPcd, tmpKgarReg);
        if(err)
            RETURN_ERROR(MINOR, err, NO_MSG);

        DUMP_TITLE(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.portRegs, ("FmPcdKgIndirectAccessPortRegs PCD Port %d regs", hardwarePortId));

        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.portRegs, kgoe_sp);
        DUMP_VAR(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.portRegs, kgoe_cpp);
    }

    DUMP_SUBTITLE(("\n"));
    for(j=0;j<FM_PCD_MAX_NUM_OF_CLS_PLANS/CLS_PLAN_NUM_PER_GRP;j++)
    {
        DUMP_TITLE(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.clsPlanRegs, ("FmPcdKgIndirectAccessClsPlanRegs Regs group %d", j));
        DUMP_TITLE(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.clsPlanRegs.kgcpe, ("kgcpe"));

        tmpKgarReg = FmPcdKgBuildReadClsPlanBlockActionReg((uint8_t)j);
        err = WriteKgarWait(p_FmPcd, tmpKgarReg);
        if(err)
            RETURN_ERROR(MINOR, err, NO_MSG);
        DUMP_SUBSTRUCT_ARRAY(i, 8)
            DUMP_MEMORY(&p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.clsPlanRegs.kgcpe[i], sizeof(uint32_t));
    }
    FmPcdUnlock(p_FmPcd, intFlags);

    return E_OK;
}
#endif /* (defined(DEBUG_ERRORS) && ... */

t_Handle KgConfig( t_FmPcd *p_FmPcd, t_FmPcdParams *p_FmPcdParams)
{
    t_FmPcdKg   *p_FmPcdKg;

    UNUSED(p_FmPcd);

    if (p_FmPcdParams->numOfSchemes > FM_PCD_KG_NUM_OF_SCHEMES)
    {
        REPORT_ERROR(MAJOR, E_INVALID_VALUE,
                     ("numOfSchemes should not exceed %d", FM_PCD_KG_NUM_OF_SCHEMES));
        return NULL;
    }

    p_FmPcdKg = (t_FmPcdKg *)XX_Malloc(sizeof(t_FmPcdKg));
    if (!p_FmPcdKg)
    {
        REPORT_ERROR(MAJOR, E_NO_MEMORY, ("FM Keygen allocation FAILED"));
        return NULL;
    }
    memset(p_FmPcdKg, 0, sizeof(t_FmPcdKg));

    if(FmIsMaster(p_FmPcd->h_Fm))
    {
        p_FmPcdKg->p_FmPcdKgRegs  = (t_FmPcdKgRegs *)UINT_TO_PTR(FmGetPcdKgBaseAddr(p_FmPcdParams->h_Fm));
        p_FmPcd->exceptions |= DEFAULT_fmPcdKgErrorExceptions;
    }

    p_FmPcdKg->numOfSchemes = p_FmPcdParams->numOfSchemes;
    if((p_FmPcd->guestId == NCSW_MASTER_ID) && !p_FmPcdKg->numOfSchemes)
    {
        p_FmPcdKg->numOfSchemes = FM_PCD_KG_NUM_OF_SCHEMES;
        DBG(WARNING, ("numOfSchemes was defined 0 by user, re-defined by driver to FM_PCD_KG_NUM_OF_SCHEMES"));
    }

    p_FmPcdKg->emptyClsPlanGrpId = ILLEGAL_CLS_PLAN;

    return p_FmPcdKg;
}

t_Error KgInit(t_FmPcd *p_FmPcd)
{
    if (p_FmPcd->guestId == NCSW_MASTER_ID)
        return KgInitMaster(p_FmPcd);
    else
        return KgInitGuest(p_FmPcd);
}

t_Error KgFree(t_FmPcd *p_FmPcd)
{
    t_FmPcdIpcKgSchemesParams       kgAlloc;
    t_Error                         err = E_OK;
    t_FmPcdIpcMsg                   msg;
    uint32_t                        replyLength;
    t_FmPcdIpcReply                 reply;

    FmUnregisterIntr(p_FmPcd->h_Fm, e_FM_MOD_KG, 0, e_FM_INTR_TYPE_ERR);

    if(p_FmPcd->guestId == NCSW_MASTER_ID)
        return FmPcdKgFreeSchemes(p_FmPcd,
                                    p_FmPcd->p_FmPcdKg->numOfSchemes,
                                    p_FmPcd->guestId,
                                    p_FmPcd->p_FmPcdKg->schemesIds);

    /* guest */
    memset(&reply, 0, sizeof(reply));
    memset(&msg, 0, sizeof(msg));
    kgAlloc.numOfSchemes = p_FmPcd->p_FmPcdKg->numOfSchemes;
    kgAlloc.guestId = p_FmPcd->guestId;
    ASSERT_COND(kgAlloc.numOfSchemes < FM_PCD_KG_NUM_OF_SCHEMES);
    memcpy(kgAlloc.schemesIds, p_FmPcd->p_FmPcdKg->schemesIds , (sizeof(uint8_t))*kgAlloc.numOfSchemes);
    msg.msgId = FM_PCD_FREE_KG_SCHEMES;
    memcpy(msg.msgBody, &kgAlloc, sizeof(kgAlloc));
    replyLength = sizeof(uint32_t);
    if ((err = XX_IpcSendMessage(p_FmPcd->h_IpcSession,
                                 (uint8_t*)&msg,
                                 sizeof(msg.msgId) + sizeof(kgAlloc),
                                 (uint8_t*)&reply,
                                 &replyLength,
                                 NULL,
                                 NULL)) != E_OK)
        RETURN_ERROR(MAJOR, err, NO_MSG);
    if (replyLength != sizeof(uint32_t))
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("IPC reply length mismatch"));

    return (t_Error)reply.error;
}

t_Error FmPcdKgSetOrBindToClsPlanGrp(t_Handle h_FmPcd, uint8_t hardwarePortId, uint8_t netEnvId, protocolOpt_t *p_OptArray, uint8_t *p_ClsPlanGrpId, bool *p_IsEmptyClsPlanGrp)
{
    t_FmPcd                                 *p_FmPcd = (t_FmPcd *)h_FmPcd;
    t_FmPcdKgInterModuleClsPlanGrpParams    grpParams, *p_GrpParams;
    t_FmPcdKgClsPlanGrp                     *p_ClsPlanGrp;
    t_FmPcdKgInterModuleClsPlanSet          *p_ClsPlanSet;
    t_Error                                 err;

    memset(&grpParams, 0, sizeof(grpParams));
    grpParams.clsPlanGrpId = ILLEGAL_CLS_PLAN;
    p_GrpParams = &grpParams;

    p_GrpParams->netEnvId = netEnvId;
    err = PcdGetClsPlanGrpParams(h_FmPcd, p_GrpParams);
    if(err)
        RETURN_ERROR(MINOR,err,NO_MSG);
    if(p_GrpParams->grpExists)
        *p_ClsPlanGrpId = p_GrpParams->clsPlanGrpId;
    else
    {
        p_ClsPlanSet = (t_FmPcdKgInterModuleClsPlanSet *)XX_Malloc(sizeof(t_FmPcdKgInterModuleClsPlanSet));
        if (!p_ClsPlanSet)
            RETURN_ERROR(MAJOR, E_NO_MEMORY, ("memory allocation failed for p_ClsPlanSet"));
        memset(p_ClsPlanSet, 0, sizeof(t_FmPcdKgInterModuleClsPlanSet));
        err = FmPcdKgBuildClsPlanGrp(h_FmPcd, p_GrpParams, p_ClsPlanSet);
        if (err)
        {
            XX_Free(p_ClsPlanSet);
            RETURN_ERROR(MINOR,err,NO_MSG);
        }
        *p_ClsPlanGrpId = p_GrpParams->clsPlanGrpId;

        if (p_FmPcd->h_Hc)
        {
            /* write clsPlan entries to memory */
            err = FmHcPcdKgSetClsPlan(p_FmPcd->h_Hc, p_ClsPlanSet);
            if (err)
            {
                XX_Free(p_ClsPlanSet);
                RETURN_ERROR(MAJOR, err, NO_MSG);
            }
        }
        else
            /* write clsPlan entries to memory */
            KgSetClsPlan(p_FmPcd, p_ClsPlanSet);

        XX_Free(p_ClsPlanSet);
    }

    /* mark if this is an empty classification group */
    if(*p_ClsPlanGrpId == p_FmPcd->p_FmPcdKg->emptyClsPlanGrpId)
        *p_IsEmptyClsPlanGrp = TRUE;
    else
        *p_IsEmptyClsPlanGrp = FALSE;

    p_ClsPlanGrp = &p_FmPcd->p_FmPcdKg->clsPlanGrps[*p_ClsPlanGrpId];

    /* increment owners number */
    p_ClsPlanGrp->owners++;

    /* copy options array for port */
    memcpy(p_OptArray, &p_FmPcd->p_FmPcdKg->clsPlanGrps[*p_ClsPlanGrpId].optArray, FM_PCD_MAX_NUM_OF_OPTIONS(FM_PCD_MAX_NUM_OF_CLS_PLANS)*sizeof(protocolOpt_t));

    /* bind port to the new or existing group */
    err = KgBindPortToClsPlanGrp(p_FmPcd, hardwarePortId, p_GrpParams->clsPlanGrpId);
    if(err)
        RETURN_ERROR(MINOR, err, NO_MSG);

    return E_OK;
}

t_Error FmPcdKgDeleteOrUnbindPortToClsPlanGrp(t_Handle h_FmPcd, uint8_t hardwarePortId, uint8_t clsPlanGrpId)
{
    t_FmPcd                         *p_FmPcd = (t_FmPcd *)h_FmPcd;
    t_FmPcdKgClsPlanGrp             *p_ClsPlanGrp = &p_FmPcd->p_FmPcdKg->clsPlanGrps[clsPlanGrpId];
    t_FmPcdKgInterModuleClsPlanSet  *p_ClsPlanSet;

    FmPcdKgUnbindPortToClsPlanGrp(p_FmPcd, hardwarePortId);

    /* decrement owners number */
    ASSERT_COND(p_ClsPlanGrp->owners);
    p_ClsPlanGrp->owners--;

    if(!p_ClsPlanGrp->owners)
    {
        if (p_FmPcd->h_Hc)
            return FmHcPcdKgDeleteClsPlan(p_FmPcd->h_Hc, clsPlanGrpId);
        else
        {
            /* clear clsPlan entries in memory */
            p_ClsPlanSet = (t_FmPcdKgInterModuleClsPlanSet *)XX_Malloc(sizeof(t_FmPcdKgInterModuleClsPlanSet));
            if (!p_ClsPlanSet)
                RETURN_ERROR(MAJOR, E_NO_MEMORY, ("memory allocation failed for p_ClsPlanSet"));
            memset(p_ClsPlanSet, 0, sizeof(t_FmPcdKgInterModuleClsPlanSet));
            p_ClsPlanSet->baseEntry = p_FmPcd->p_FmPcdKg->clsPlanGrps[clsPlanGrpId].baseEntry;
            p_ClsPlanSet->numOfClsPlanEntries = p_FmPcd->p_FmPcdKg->clsPlanGrps[clsPlanGrpId].sizeOfGrp;
            KgSetClsPlan(p_FmPcd, p_ClsPlanSet);
            XX_Free(p_ClsPlanSet);
            FmPcdKgDestroyClsPlanGrp(h_FmPcd, clsPlanGrpId);
        }
    }
    return E_OK;
}

t_Error FmPcdKgBuildScheme(t_Handle h_FmPcd,  t_FmPcdKgSchemeParams *p_Scheme, t_FmPcdKgInterModuleSchemeRegs *p_SchemeRegs)
{
    t_FmPcd                             *p_FmPcd = (t_FmPcd *)h_FmPcd;
    uint32_t                            grpBits = 0;
    uint8_t                             grpBase;
    bool                                direct=TRUE, absolute=FALSE;
    uint16_t                            profileId=0, numOfProfiles=0, relativeProfileId;
    t_Error                             err = E_OK;
    int                                 i = 0;
    t_NetEnvParams                      netEnvParams;
    uint32_t                            tmpReg, fqbTmp = 0, ppcTmp = 0, selectTmp, maskTmp, knownTmp, genTmp;
    t_FmPcdKgKeyExtractAndHashParams    *p_KeyAndHash = NULL;
    uint8_t                             j, curr, idx;
    uint8_t                             id, shift=0, code=0, offset=0, size=0;
    t_FmPcdExtractEntry                 *p_Extract = NULL;
    t_FmPcdKgExtractedOrParams          *p_ExtractOr;
    bool                                generic = FALSE;
    t_KnownFieldsMasks                  bitMask;
    e_FmPcdKgExtractDfltSelect          swDefault = (e_FmPcdKgExtractDfltSelect)0;
    t_FmPcdKgSchemesExtracts            *p_LocalExtractsArray;
    uint8_t                             numOfSwDefaults = 0;
    t_FmPcdKgExtractDflt                swDefaults[NUM_OF_SW_DEFAULTS];
    uint8_t                             currGenId = 0, relativeSchemeId;

    if(!p_Scheme->modify)
        relativeSchemeId = p_Scheme->id.relativeSchemeId;
    else
        relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmPcd, (uint8_t)(PTR_TO_UINT(p_Scheme->id.h_Scheme)-1));

    memset(&p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId], 0, sizeof(t_FmPcdKgScheme));
    memset(swDefaults, 0, NUM_OF_SW_DEFAULTS*sizeof(t_FmPcdKgExtractDflt));
    memset(p_SchemeRegs, 0, sizeof(t_FmPcdKgInterModuleSchemeRegs));

    if (p_Scheme->netEnvParams.numOfDistinctionUnits > FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE,
                     ("numOfDistinctionUnits should not exceed %d", FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS));

    /* by netEnv parameters, get match vector */
    if(!p_Scheme->alwaysDirect)
    {
        p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].netEnvId =
            (uint8_t)(PTR_TO_UINT(p_Scheme->netEnvParams.h_NetEnv)-1);
        netEnvParams.netEnvId = p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].netEnvId;
        netEnvParams.numOfDistinctionUnits = p_Scheme->netEnvParams.numOfDistinctionUnits;
        memcpy(netEnvParams.unitIds, p_Scheme->netEnvParams.unitIds, (sizeof(uint8_t))*p_Scheme->netEnvParams.numOfDistinctionUnits);
        err = PcdGetUnitsVector(p_FmPcd, &netEnvParams);
        if(err)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, NO_MSG);
        p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].matchVector = netEnvParams.vector;
    }
    else
    {
        p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].matchVector = SCHEME_ALWAYS_DIRECT;
        p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].netEnvId = ILLEGAL_NETENV;
    }

    if(p_Scheme->nextEngine == e_FM_PCD_INVALID)
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Next Engine of the scheme is not Valid"));

    if(p_Scheme->bypassFqidGeneration)
    {
#ifdef FM_KG_NO_BYPASS_FQID_GEN
        {
            t_FmRevisionInfo    revInfo;

            FM_GetRevision(p_FmPcd->h_Fm, &revInfo);
            if (revInfo.majorRev != 4)
                RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("bypassFqidGeneration."));
        }
#endif /* FM_KG_NO_BYPASS_FQID_GEN */
        if(p_Scheme->baseFqid)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("baseFqid set for a scheme that does not generate an FQID"));
    }
    else
        if(!p_Scheme->baseFqid)
            DBG(WARNING, ("baseFqid is 0."));

    if(p_Scheme->nextEngine == e_FM_PCD_PLCR)
    {
        direct = p_Scheme->kgNextEngineParams.plcrProfile.direct;
        p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].directPlcr = direct;
        absolute = (bool)(p_Scheme->kgNextEngineParams.plcrProfile.sharedProfile ? TRUE : FALSE);
        if(!direct && absolute)
            RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Indirect policing is not available when profile is shared."));

        if(direct)
        {
            profileId = p_Scheme->kgNextEngineParams.plcrProfile.profileSelect.directRelativeProfileId;
            numOfProfiles = 1;
        }
        else
        {
            profileId = p_Scheme->kgNextEngineParams.plcrProfile.profileSelect.indirectProfile.fqidOffsetRelativeProfileIdBase;
            shift = p_Scheme->kgNextEngineParams.plcrProfile.profileSelect.indirectProfile.fqidOffsetShift;
            numOfProfiles = p_Scheme->kgNextEngineParams.plcrProfile.profileSelect.indirectProfile.numOfProfiles;
        }
    }

    if(p_Scheme->nextEngine == e_FM_PCD_CC)
    {
#ifdef FM_KG_NO_BYPASS_PLCR_PROFILE_GEN
        if((p_Scheme->kgNextEngineParams.cc.plcrNext) && (p_Scheme->kgNextEngineParams.cc.bypassPlcrProfileGeneration))
        {
            t_FmRevisionInfo    revInfo;

            FM_GetRevision(p_FmPcd->h_Fm, &revInfo);
            if (revInfo.majorRev != 4)
                RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("bypassPlcrProfileGeneration."));
        }
#endif /* FM_KG_NO_BYPASS_PLCR_PROFILE_GEN */

        err = FmPcdCcGetGrpParams(p_Scheme->kgNextEngineParams.cc.h_CcTree,
                             p_Scheme->kgNextEngineParams.cc.grpId,
                             &grpBits,
                             &grpBase);
        if(err)
            RETURN_ERROR(MAJOR, err, NO_MSG);
        p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].ccUnits = grpBits;

        if((p_Scheme->kgNextEngineParams.cc.plcrNext) &&
           (!p_Scheme->kgNextEngineParams.cc.bypassPlcrProfileGeneration))
        {
                if(p_Scheme->kgNextEngineParams.cc.plcrProfile.sharedProfile)
                    RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Shared profile may not be used after Coarse classification."));
                absolute = FALSE;
                direct = p_Scheme->kgNextEngineParams.cc.plcrProfile.direct;
                if(direct)
                {
                    profileId = p_Scheme->kgNextEngineParams.cc.plcrProfile.profileSelect.directRelativeProfileId;
                    numOfProfiles = 1;
                }
                else
                {
                    profileId = p_Scheme->kgNextEngineParams.cc.plcrProfile.profileSelect.indirectProfile.fqidOffsetRelativeProfileIdBase;
                    shift = p_Scheme->kgNextEngineParams.cc.plcrProfile.profileSelect.indirectProfile.fqidOffsetShift;
                    numOfProfiles = p_Scheme->kgNextEngineParams.cc.plcrProfile.profileSelect.indirectProfile.numOfProfiles;
                }
        }
    }

    /* if policer is used directly after KG, or after CC */
    if((p_Scheme->nextEngine == e_FM_PCD_PLCR)  ||
       ((p_Scheme->nextEngine == e_FM_PCD_CC) &&
        (p_Scheme->kgNextEngineParams.cc.plcrNext) &&
        (!p_Scheme->kgNextEngineParams.cc.bypassPlcrProfileGeneration)))
    {
        /* if private policer profile, it may be uninitialized yet, therefore no checks are done at this stage */
        if(absolute)
        {
            /* for absolute direct policy only, */
            relativeProfileId = profileId;
            err = FmPcdPlcrGetAbsoluteProfileId(h_FmPcd,e_FM_PCD_PLCR_SHARED,NULL, relativeProfileId, &profileId);
            if(err)
                RETURN_ERROR(MAJOR, err, ("Shared profile not valid offset"));
            if(!FmPcdPlcrIsProfileValid(p_FmPcd, profileId))
                RETURN_ERROR(MINOR, E_INVALID_STATE, ("Shared profile not valid."));
            p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].relativeProfileId = profileId;
        }
        else
        {
            /* save relative profile id's for later check */
            p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].nextRelativePlcrProfile = TRUE;
            p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].relativeProfileId = profileId;
            p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].numOfProfiles = numOfProfiles;
        }
    }
    else
    {
        /* if policer is NOT going to be used after KG at all than if bypassFqidGeneration
        is set, we do not need numOfUsedExtractedOrs and hashDistributionNumOfFqids */
        if(p_Scheme->bypassFqidGeneration && p_Scheme->numOfUsedExtractedOrs)
            RETURN_ERROR(MAJOR, E_INVALID_STATE,
                    ("numOfUsedExtractedOrs is set in a scheme that does not generate FQID or policer profile ID"));
        if(p_Scheme->bypassFqidGeneration &&
                p_Scheme->useHash &&
                p_Scheme->keyExtractAndHashParams.hashDistributionNumOfFqids)
            RETURN_ERROR(MAJOR, E_INVALID_STATE,
                    ("hashDistributionNumOfFqids is set in a scheme that does not generate FQID or policer profile ID"));
    }

    /* configure all 21 scheme registers */
    tmpReg =  KG_SCH_MODE_EN;
    switch(p_Scheme->nextEngine)
    {
        case(e_FM_PCD_PLCR):
            /* add to mode register - NIA */
            tmpReg |= KG_SCH_MODE_NIA_PLCR;
            tmpReg |= NIA_ENG_PLCR;
            tmpReg |= (uint32_t)(p_Scheme->kgNextEngineParams.plcrProfile.sharedProfile ? NIA_PLCR_ABSOLUTE:0);
            /* initialize policer profile command - */
            /*  configure kgse_ppc  */
            if(direct)
            /* use profileId as base, other fields are 0 */
                p_SchemeRegs->kgse_ppc = (uint32_t)profileId;
            else
            {
                if(shift > MAX_PP_SHIFT)
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("fqidOffsetShift may not be larger than %d", MAX_PP_SHIFT));

                if(!numOfProfiles || !POWER_OF_2(numOfProfiles))
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("numOfProfiles must not be 0 and must be a power of 2"));

                ppcTmp = ((uint32_t)shift << KG_SCH_PP_SHIFT_HIGH_SHIFT) & KG_SCH_PP_SHIFT_HIGH;
                ppcTmp |= ((uint32_t)shift << KG_SCH_PP_SHIFT_LOW_SHIFT) & KG_SCH_PP_SHIFT_LOW;
                ppcTmp |= ((uint32_t)(numOfProfiles-1) << KG_SCH_PP_MASK_SHIFT);
                ppcTmp |= (uint32_t)profileId;

                p_SchemeRegs->kgse_ppc = ppcTmp;
            }
            break;
        case(e_FM_PCD_CC):
            /* mode reg - define NIA */
            tmpReg |= (NIA_ENG_FM_CTL | NIA_FM_CTL_AC_CC);

            p_SchemeRegs->kgse_ccbs = grpBits;
            tmpReg |= (uint32_t)(grpBase << KG_SCH_MODE_CCOBASE_SHIFT);

            if(p_Scheme->kgNextEngineParams.cc.plcrNext)
            {
                if(!p_Scheme->kgNextEngineParams.cc.bypassPlcrProfileGeneration)
                {
                    /* find out if absolute or relative */
                    if(absolute)
                         RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("It is illegal to request a shared profile in a scheme that is in a KG->CC->PLCR flow"));
                    if(direct)
                    {
                        /* mask = 0, base = directProfileId */
                        p_SchemeRegs->kgse_ppc = (uint32_t)profileId;
                    }
                    else
                    {
                        if(shift > MAX_PP_SHIFT)
                            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("fqidOffsetShift may not be larger than %d", MAX_PP_SHIFT));
                        if(!numOfProfiles || !POWER_OF_2(numOfProfiles))
                            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("numOfProfiles must not be 0 and must be a power of 2"));

                        ppcTmp = ((uint32_t)shift << KG_SCH_PP_SHIFT_HIGH_SHIFT) & KG_SCH_PP_SHIFT_HIGH;
                        ppcTmp |= ((uint32_t)shift << KG_SCH_PP_SHIFT_LOW_SHIFT) & KG_SCH_PP_SHIFT_LOW;
                        ppcTmp |= ((uint32_t)(numOfProfiles-1) << KG_SCH_PP_MASK_SHIFT);
                        ppcTmp |= (uint32_t)profileId;

                        p_SchemeRegs->kgse_ppc = ppcTmp;
                    }
                }
                else
                    ppcTmp = KG_SCH_PP_NO_GEN;
            }
            break;
        case(e_FM_PCD_DONE):
            if(p_Scheme->kgNextEngineParams.doneAction == e_FM_PCD_DROP_FRAME)
                tmpReg |= (NIA_ENG_BMI | NIA_BMI_AC_DISCARD);
            else
                tmpReg |= (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME);
            break;
        default:
             RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Next engine not supported"));
    }
    p_SchemeRegs->kgse_mode = tmpReg;

    p_SchemeRegs->kgse_mv = p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].matchVector;

    if(p_Scheme->useHash)
    {
        p_KeyAndHash = &p_Scheme->keyExtractAndHashParams;

        if (p_KeyAndHash->numOfUsedExtracts >= FM_PCD_KG_MAX_NUM_OF_EXTRACTS_PER_KEY)
             RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("numOfUsedExtracts out of range"));

        /*  configure kgse_dv0  */
        p_SchemeRegs->kgse_dv0 = p_KeyAndHash->privateDflt0;

        /*  configure kgse_dv1  */
        p_SchemeRegs->kgse_dv1 = p_KeyAndHash->privateDflt1;

        if(!p_Scheme->bypassFqidGeneration)
        {
            if(!p_KeyAndHash->hashDistributionNumOfFqids || !POWER_OF_2(p_KeyAndHash->hashDistributionNumOfFqids))
                RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("hashDistributionNumOfFqids must not be 0 and must be a power of 2"));
            if((p_KeyAndHash->hashDistributionNumOfFqids-1) & p_Scheme->baseFqid)
                DBG(WARNING, ("baseFqid unaligned. Distribution may result in less than hashDistributionNumOfFqids queues."));
        }

        /*  configure kgse_ekdv  */
        tmpReg = 0;
        for( i=0 ;i<p_KeyAndHash->numOfUsedDflts ; i++)
        {
            switch(p_KeyAndHash->dflts[i].type)
            {
                case(e_FM_PCD_KG_MAC_ADDR):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_MAC_ADDR_SHIFT);
                    break;
                case(e_FM_PCD_KG_TCI):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_TCI_SHIFT);
                    break;
                case(e_FM_PCD_KG_ENET_TYPE):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_ENET_TYPE_SHIFT);
                    break;
                case(e_FM_PCD_KG_PPP_SESSION_ID):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_PPP_SESSION_ID_SHIFT);
                    break;
                case(e_FM_PCD_KG_PPP_PROTOCOL_ID):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_PPP_PROTOCOL_ID_SHIFT);
                    break;
                case(e_FM_PCD_KG_MPLS_LABEL):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_MPLS_LABEL_SHIFT);
                    break;
                case(e_FM_PCD_KG_IP_ADDR):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_IP_ADDR_SHIFT);
                    break;
                case(e_FM_PCD_KG_PROTOCOL_TYPE):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_PROTOCOL_TYPE_SHIFT);
                    break;
                case(e_FM_PCD_KG_IP_TOS_TC):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_IP_TOS_TC_SHIFT);
                    break;
                case(e_FM_PCD_KG_IPV6_FLOW_LABEL):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_L4_PORT_SHIFT);
                    break;
                case(e_FM_PCD_KG_IPSEC_SPI):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_IPSEC_SPI_SHIFT);
                    break;
                case(e_FM_PCD_KG_L4_PORT):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_L4_PORT_SHIFT);
                    break;
                case(e_FM_PCD_KG_TCP_FLAG):
                    tmpReg |= (p_KeyAndHash->dflts[i].dfltSelect << KG_SCH_DEF_TCP_FLAG_SHIFT);
                    break;
                case(e_FM_PCD_KG_GENERIC_FROM_DATA):
                    swDefaults[numOfSwDefaults].type = e_FM_PCD_KG_GENERIC_FROM_DATA;
                    swDefaults[numOfSwDefaults].dfltSelect = p_KeyAndHash->dflts[i].dfltSelect;
                    numOfSwDefaults ++;
                    break;
                case(e_FM_PCD_KG_GENERIC_FROM_DATA_NO_V):
                    swDefaults[numOfSwDefaults].type = e_FM_PCD_KG_GENERIC_FROM_DATA_NO_V;
                    swDefaults[numOfSwDefaults].dfltSelect = p_KeyAndHash->dflts[i].dfltSelect;
                    numOfSwDefaults ++;
                    break;
                case(e_FM_PCD_KG_GENERIC_NOT_FROM_DATA):
                    swDefaults[numOfSwDefaults].type = e_FM_PCD_KG_GENERIC_NOT_FROM_DATA;
                    swDefaults[numOfSwDefaults].dfltSelect = p_KeyAndHash->dflts[i].dfltSelect;
                    numOfSwDefaults ++;
                   break;
                default:
                    RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
            }
        }
        p_SchemeRegs->kgse_ekdv = tmpReg;

        p_LocalExtractsArray = (t_FmPcdKgSchemesExtracts *)XX_Malloc(sizeof(t_FmPcdKgSchemesExtracts));
        if(!p_LocalExtractsArray)
            RETURN_ERROR(MAJOR, E_NO_MEMORY, ("No memory"));

        /*  configure kgse_ekfc and  kgse_gec */
        knownTmp = 0;
        for( i=0 ;i<p_KeyAndHash->numOfUsedExtracts ; i++)
        {
            p_Extract = &p_KeyAndHash->extractArray[i];
            switch(p_Extract->type)
            {
                case(e_FM_PCD_KG_EXTRACT_PORT_PRIVATE_INFO):
                    knownTmp |= KG_SCH_KN_PORT_ID;
                    /* save in driver structure */
                    p_LocalExtractsArray->extractsArray[i].id = GetKnownFieldId(KG_SCH_KN_PORT_ID);
                    p_LocalExtractsArray->extractsArray[i].known = TRUE;
                    break;
                case(e_FM_PCD_EXTRACT_BY_HDR):
                    switch(p_Extract->extractByHdr.hdr)
                    {
                        case(HEADER_TYPE_UDP_ENCAP_ESP):
                            switch(p_Extract->extractByHdr.type)
                            {
                                case(e_FM_PCD_EXTRACT_FROM_HDR):
                                    /* case where extraction from ESP only */
                                    if (p_Extract->extractByHdr.extractByHdrType.fromHdr.offset >= UDP_HEADER_SIZE)
                                    {
                                        p_Extract->extractByHdr.hdr = FmPcdGetAliasHdr(p_FmPcd, p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].netEnvId, HEADER_TYPE_UDP_ENCAP_ESP);
                                        p_Extract->extractByHdr.extractByHdrType.fromHdr.offset -= UDP_HEADER_SIZE;
                                        p_Extract->extractByHdr.ignoreProtocolValidation = TRUE;
                                    }
                                    else
                                    {
                                        p_Extract->extractByHdr.hdr = HEADER_TYPE_UDP;
                                        p_Extract->extractByHdr.ignoreProtocolValidation = FALSE;
                                    }
                                    break;
                                case(e_FM_PCD_EXTRACT_FROM_FIELD):
                                    switch(p_Extract->extractByHdr.extractByHdrType.fromField.field.udpEncapEsp)
                                    {
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_PORT_SRC):
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_PORT_DST):
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_LEN):
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_CKSUM):
                                            p_Extract->extractByHdr.hdr = HEADER_TYPE_UDP;
                                            break;
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_SPI):
                                            p_Extract->extractByHdr.type = e_FM_PCD_EXTRACT_FROM_HDR;
                                            p_Extract->extractByHdr.hdr = FmPcdGetAliasHdr(p_FmPcd, p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].netEnvId, HEADER_TYPE_UDP_ENCAP_ESP);
                                            p_Extract->extractByHdr.extractByHdrType.fromField.size = p_Extract->extractByHdr.extractByHdrType.fromField.size;
                                            /*p_Extract->extractByHdr.extractByHdrType.fromField.offset += ESP_SPI_OFFSET;*/
                                            p_Extract->extractByHdr.ignoreProtocolValidation = TRUE;
                                            break;
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_SEQUENCE_NUM):
                                            p_Extract->extractByHdr.type = e_FM_PCD_EXTRACT_FROM_HDR;
                                            p_Extract->extractByHdr.hdr = FmPcdGetAliasHdr(p_FmPcd, p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].netEnvId, HEADER_TYPE_UDP_ENCAP_ESP);
                                            p_Extract->extractByHdr.extractByHdrType.fromField.size = p_Extract->extractByHdr.extractByHdrType.fromField.size;
                                            p_Extract->extractByHdr.extractByHdrType.fromField.offset += ESP_SEQ_NUM_OFFSET;
                                            p_Extract->extractByHdr.ignoreProtocolValidation = TRUE;
                                            break;
                                    }
                                    break;
                                case(e_FM_PCD_EXTRACT_FULL_FIELD):
                                    switch(p_Extract->extractByHdr.extractByHdrType.fullField.udpEncapEsp)
                                    {
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_PORT_SRC):
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_PORT_DST):
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_LEN):
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_CKSUM):
                                            p_Extract->extractByHdr.hdr = HEADER_TYPE_UDP;
                                            break;
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_SPI):
                                            p_Extract->extractByHdr.type = e_FM_PCD_EXTRACT_FROM_HDR;
                                            p_Extract->extractByHdr.hdr = FmPcdGetAliasHdr(p_FmPcd, p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].netEnvId, HEADER_TYPE_UDP_ENCAP_ESP);
                                            p_Extract->extractByHdr.extractByHdrType.fromHdr.size = ESP_SPI_SIZE;
                                            p_Extract->extractByHdr.extractByHdrType.fromHdr.offset = ESP_SPI_OFFSET;
                                            p_Extract->extractByHdr.ignoreProtocolValidation = TRUE;
                                            break;
                                        case(NET_HEADER_FIELD_UDP_ENCAP_ESP_SEQUENCE_NUM):
                                            p_Extract->extractByHdr.type = e_FM_PCD_EXTRACT_FROM_HDR;
                                            p_Extract->extractByHdr.hdr = FmPcdGetAliasHdr(p_FmPcd, p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].netEnvId, HEADER_TYPE_UDP_ENCAP_ESP);
                                            p_Extract->extractByHdr.extractByHdrType.fromHdr.size = ESP_SEQ_NUM_SIZE;
                                            p_Extract->extractByHdr.extractByHdrType.fromHdr.offset = ESP_SEQ_NUM_OFFSET;
                                            p_Extract->extractByHdr.ignoreProtocolValidation = TRUE;
                                            break;
                                    }
                                    break;
                            }
                            break;
                        default:
                            break;
                    }
                    switch(p_Extract->extractByHdr.type)
                    {
                        case(e_FM_PCD_EXTRACT_FROM_HDR):
                            generic = TRUE;
                            /* get the header code for the generic extract */
                            code = GetGenHdrCode(p_Extract->extractByHdr.hdr, p_Extract->extractByHdr.hdrIndex, p_Extract->extractByHdr.ignoreProtocolValidation);
                            /* set generic register fields */
                            offset = p_Extract->extractByHdr.extractByHdrType.fromHdr.offset;
                            size = p_Extract->extractByHdr.extractByHdrType.fromHdr.size;
                            break;
                        case(e_FM_PCD_EXTRACT_FROM_FIELD):
                            generic = TRUE;
                            /* get the field code for the generic extract */
                            code = GetGenFieldCode(p_Extract->extractByHdr.hdr,
                                        p_Extract->extractByHdr.extractByHdrType.fromField.field, p_Extract->extractByHdr.ignoreProtocolValidation,p_Extract->extractByHdr.hdrIndex);
                            offset = p_Extract->extractByHdr.extractByHdrType.fromField.offset;
                            size = p_Extract->extractByHdr.extractByHdrType.fromField.size;
                            break;
                        case(e_FM_PCD_EXTRACT_FULL_FIELD):
                            if(!p_Extract->extractByHdr.ignoreProtocolValidation)
                            {
                                /* if we have a known field for it - use it, otherwise use generic */
                                bitMask = GetKnownProtMask(p_Extract->extractByHdr.hdr, p_Extract->extractByHdr.hdrIndex,
                                            p_Extract->extractByHdr.extractByHdrType.fullField);
                                if(bitMask)
                                {
                                    knownTmp |= bitMask;
                                    /* save in driver structure */
                                    p_LocalExtractsArray->extractsArray[i].id = GetKnownFieldId(bitMask);
                                    p_LocalExtractsArray->extractsArray[i].known = TRUE;
                                }
                                else
                                    generic = TRUE;

                            }
                            else
                                generic = TRUE;
                            if(generic)
                            {
                                /* tmp - till we cover more headers under generic */
                                RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Full header selection not supported"));
                            }
                            break;
                        default:
                            RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
                    }
                    break;
                case(e_FM_PCD_EXTRACT_NON_HDR):
                    /* use generic */
                    generic = TRUE;
                    offset = 0;
                    /* get the field code for the generic extract */
                    code = GetGenCode(p_Extract->extractNonHdr.src, &offset);
                    offset += p_Extract->extractNonHdr.offset;
                    size = p_Extract->extractNonHdr.size;
                    break;
                default:
                    RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
            }

            if(generic)
            {
                /* set generic register fields */
                if(currGenId >= FM_PCD_KG_NUM_OF_GENERIC_REGS)
                    RETURN_ERROR(MAJOR, E_FULL, ("Generic registers are fully used"));
                if(!code)
                    RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, NO_MSG);

                genTmp = KG_SCH_GEN_VALID;
                genTmp |= (uint32_t)(code << KG_SCH_GEN_HT_SHIFT);
                genTmp |= offset;
                if((size > MAX_KG_SCH_SIZE) || (size < 1))
                      RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("Illegal extraction (size out of range)"));
                genTmp |= (uint32_t)((size - 1) << KG_SCH_GEN_SIZE_SHIFT);
                swDefault = GetGenericSwDefault(swDefaults, numOfSwDefaults, code);
                if(swDefault == e_FM_PCD_KG_DFLT_ILLEGAL)
                    DBG(WARNING, ("No sw default configured"));

                genTmp |= swDefault << KG_SCH_GEN_DEF_SHIFT;
                genTmp |= KG_SCH_GEN_MASK;
                p_SchemeRegs->kgse_gec[currGenId] = genTmp;
                /* save in driver structure */
                p_LocalExtractsArray->extractsArray[i].id = currGenId++;
                p_LocalExtractsArray->extractsArray[i].known = FALSE;
                generic = FALSE;
            }
        }
        p_SchemeRegs->kgse_ekfc = knownTmp;

        selectTmp = 0;
        maskTmp = 0xFFFFFFFF;
        /*  configure kgse_bmch, kgse_bmcl and kgse_fqb */

        if(p_KeyAndHash->numOfUsedMasks >= FM_PCD_KG_NUM_OF_EXTRACT_MASKS)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Only %d masks supported", FM_PCD_KG_NUM_OF_EXTRACT_MASKS));
        for( i=0 ;i<p_KeyAndHash->numOfUsedMasks ; i++)
        {
            /* Get the relative id of the extract (for known 0-0x1f, for generic 0-7) */
            id = p_LocalExtractsArray->extractsArray[p_KeyAndHash->masks[i].extractArrayIndex].id;
            /* Get the shift of the select field (depending on i) */
            GET_MASK_SEL_SHIFT(shift,i);
            if (p_LocalExtractsArray->extractsArray[p_KeyAndHash->masks[i].extractArrayIndex].known)
                selectTmp |= id << shift;
            else
                selectTmp |= (id + MASK_FOR_GENERIC_BASE_ID) << shift;

            /* Get the shift of the offset field (depending on i) - may
               be in  kgse_bmch or in kgse_fqb (depending on i) */
            GET_MASK_OFFSET_SHIFT(shift,i);
            if (i<=1)
                selectTmp |= p_KeyAndHash->masks[i].offset << shift;
            else
                fqbTmp |= p_KeyAndHash->masks[i].offset << shift;

            /* Get the shift of the mask field (depending on i) */
            GET_MASK_SHIFT(shift,i);
            /* pass all bits */
            maskTmp |= KG_SCH_BITMASK_MASK << shift;
            /* clear bits that need masking */
            maskTmp &= ~(0xFF << shift) ;
            /* set mask bits */
            maskTmp |= (p_KeyAndHash->masks[i].mask << shift) ;
        }
        p_SchemeRegs->kgse_bmch = selectTmp;
        p_SchemeRegs->kgse_bmcl = maskTmp;
        /* kgse_fqb will be written t the end of the routine */

        /*  configure kgse_hc  */
        if(p_KeyAndHash->hashShift > MAX_HASH_SHIFT)
             RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("hashShift must not be larger than %d", MAX_HASH_SHIFT));
        if(p_KeyAndHash->hashDistributionFqidsShift > MAX_DIST_FQID_SHIFT)
             RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("hashDistributionFqidsShift must not be larger than %d", MAX_DIST_FQID_SHIFT));

        tmpReg = 0;

        tmpReg |= ((p_KeyAndHash->hashDistributionNumOfFqids - 1) << p_KeyAndHash->hashDistributionFqidsShift);
        tmpReg |= p_KeyAndHash->hashShift << KG_SCH_HASH_CONFIG_SHIFT_SHIFT;

        if(p_KeyAndHash->symmetricHash)
        {
            if((!!(p_SchemeRegs->kgse_ekfc & KG_SCH_KN_MACSRC) != !!(p_SchemeRegs->kgse_ekfc & KG_SCH_KN_MACDST)) ||
                    (!!(p_SchemeRegs->kgse_ekfc & KG_SCH_KN_IPSRC1) != !!(p_SchemeRegs->kgse_ekfc & KG_SCH_KN_IPDST1)) ||
                    (!!(p_SchemeRegs->kgse_ekfc & KG_SCH_KN_IPSRC2) != !!(p_SchemeRegs->kgse_ekfc & KG_SCH_KN_IPDST2)) ||
                    (!!(p_SchemeRegs->kgse_ekfc & KG_SCH_KN_L4PSRC) != !!(p_SchemeRegs->kgse_ekfc & KG_SCH_KN_L4PDST)))
                RETURN_ERROR(MAJOR, E_INVALID_STATE, ("symmetricHash set but src/dest extractions missing"));
            tmpReg |= KG_SCH_HASH_CONFIG_SYM;
        }
        p_SchemeRegs->kgse_hc = tmpReg;

        /* build the return array describing the order of the extractions */

        /* the last currGenId places of the array
           are for generic extracts that are always last.
           We now sort for the calculation of the order of the known
           extractions we sort the known extracts between orderedArray[0] and
           orderedArray[p_KeyAndHash->numOfUsedExtracts - currGenId - 1].
           for the calculation of the order of the generic extractions we use:
           num_of_generic - currGenId
           num_of_known - p_KeyAndHash->numOfUsedExtracts - currGenId
           first_generic_index = num_of_known */
        curr = 0;
        for (i=0;i<p_KeyAndHash->numOfUsedExtracts ; i++)
        {
            if(p_LocalExtractsArray->extractsArray[i].known)
            {
                ASSERT_COND(curr<(p_KeyAndHash->numOfUsedExtracts - currGenId));
                j = curr;
                /* id is the extract id (port id = 0, mac src = 1 etc.). the value in the array is the original
                index in the user's extractions array */
                /* we compare the id of the current extract with the id of the extract in the orderedArray[j-1]
                location */
                while((j > 0) && (p_LocalExtractsArray->extractsArray[i].id <
                      p_LocalExtractsArray->extractsArray[p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].orderedArray[j-1]].id))
                {
                    p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].orderedArray[j] =
                        p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].orderedArray[j-1];
                    j--;
                }
                p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].orderedArray[j] = (uint8_t)i;
                curr++;
            }
            else
            {
                /* index is first_generic_index + generic index (id) */
                idx = (uint8_t)(p_KeyAndHash->numOfUsedExtracts - currGenId + p_LocalExtractsArray->extractsArray[i].id);
                ASSERT_COND(idx < FM_PCD_KG_MAX_NUM_OF_EXTRACTS_PER_KEY);
                p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].orderedArray[idx]= (uint8_t)i;
            }
        }
        XX_Free(p_LocalExtractsArray);
        p_LocalExtractsArray = NULL;

    }
    else
    {
        /* clear all unused registers: */
        p_SchemeRegs->kgse_ekfc = 0;
        p_SchemeRegs->kgse_ekdv = 0;
        p_SchemeRegs->kgse_bmch = 0;
        p_SchemeRegs->kgse_bmcl = 0;
        p_SchemeRegs->kgse_hc = 0;
        p_SchemeRegs->kgse_dv0 = 0;
        p_SchemeRegs->kgse_dv1 = 0;
    }

    if(p_Scheme->bypassFqidGeneration)
        p_SchemeRegs->kgse_hc |= KG_SCH_HASH_CONFIG_NO_FQID;

    /*  configure kgse_spc  */
    if( p_Scheme->schemeCounter.update)
        p_SchemeRegs->kgse_spc = p_Scheme->schemeCounter.value;


    /* check that are enough generic registers */
    if(p_Scheme->numOfUsedExtractedOrs + currGenId > FM_PCD_KG_NUM_OF_GENERIC_REGS)
        RETURN_ERROR(MAJOR, E_FULL, ("Generic registers are fully used"));

    /* extracted OR mask on Qid */
    for( i=0 ;i<p_Scheme->numOfUsedExtractedOrs ; i++)
    {

        p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].extractedOrs = TRUE;
        /*  configure kgse_gec[i]  */
        p_ExtractOr = &p_Scheme->extractedOrs[i];
        switch(p_ExtractOr->type)
        {
            case(e_FM_PCD_KG_EXTRACT_PORT_PRIVATE_INFO):
                code = KG_SCH_GEN_PARSE_RESULT_N_FQID;
                offset = 0;
                break;
            case(e_FM_PCD_EXTRACT_BY_HDR):
                /* get the header code for the generic extract */
                code = GetGenHdrCode(p_ExtractOr->extractByHdr.hdr, p_ExtractOr->extractByHdr.hdrIndex, p_ExtractOr->extractByHdr.ignoreProtocolValidation);
                /* set generic register fields */
                offset = p_ExtractOr->extractionOffset;
                break;
            case(e_FM_PCD_EXTRACT_NON_HDR):
                /* get the field code for the generic extract */
                offset = 0;
                code = GetGenCode(p_ExtractOr->src, &offset);
                offset += p_ExtractOr->extractionOffset;
                break;
            default:
                RETURN_ERROR(MAJOR, E_INVALID_SELECTION, NO_MSG);
        }

        /* set generic register fields */
        if(!code)
            RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, NO_MSG);
        genTmp = KG_SCH_GEN_EXTRACT_TYPE | KG_SCH_GEN_VALID;
        genTmp |= (uint32_t)(code << KG_SCH_GEN_HT_SHIFT);
        genTmp |= offset;
        if(!!p_ExtractOr->bitOffsetInFqid == !!p_ExtractOr->bitOffsetInPlcrProfile)
            RETURN_ERROR(MAJOR, E_INVALID_VALUE, (" extracted byte must effect either FQID or Policer profile"));

        /************************************************************************************
            bitOffsetInFqid and bitOffsetInPolicerProfile are translated to rotate parameter
            in the following way:

            Driver API and implementation:
            ==============================
            FQID: extracted OR byte may be shifted right 1-31 bits to effect parts of the FQID.
            if shifted less than 8 bits, or more than 24 bits a mask is set on the bits that
            are not overlapping FQID.
                     ------------------------
                    |      FQID (24)         |
                     ------------------------
            --------
           |        |  extracted OR byte
            --------

            Policer Profile: extracted OR byte may be shifted right 1-15 bits to effect parts of the
            PP id. Unless shifted exactly 8 bits to overlap the PP id, a mask is set on the bits that
            are not overlapping PP id.

                     --------
                    | PP (8) |
                     --------
            --------
           |        |  extracted OR byte
            --------

            HW implementation
            =================
            FQID and PP construct a 32 bit word in the way describe below. Extracted byte is located
            as the highest byte of that word and may be rotated to effect any part os the FQID or
            the PP.
             ------------------------  --------
            |      FQID (24)         || PP (8) |
             ------------------------  --------
             --------
            |        |  extracted OR byte
             --------

        ************************************************************************************/

        if(p_ExtractOr->bitOffsetInFqid)
        {
            if(p_ExtractOr->bitOffsetInFqid > MAX_KG_SCH_FQID_BIT_OFFSET )
              RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Illegal extraction (bitOffsetInFqid out of range)"));
            if(p_ExtractOr->bitOffsetInFqid<8)
                genTmp |= (uint32_t)((p_ExtractOr->bitOffsetInFqid+24) << KG_SCH_GEN_SIZE_SHIFT);
            else
                genTmp |= (uint32_t)((p_ExtractOr->bitOffsetInFqid-8) << KG_SCH_GEN_SIZE_SHIFT);
            p_ExtractOr->mask &= GetExtractedOrMask(p_ExtractOr->bitOffsetInFqid, TRUE);
        }
        else /* effect policer profile */
        {
            if(p_ExtractOr->bitOffsetInPlcrProfile > MAX_KG_SCH_PP_BIT_OFFSET )
              RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("Illegal extraction (bitOffsetInPlcrProfile out of range)"));
            p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].bitOffsetInPlcrProfile = p_ExtractOr->bitOffsetInPlcrProfile;
            genTmp |= (uint32_t)((p_ExtractOr->bitOffsetInPlcrProfile+16) << KG_SCH_GEN_SIZE_SHIFT);
            p_ExtractOr->mask &= GetExtractedOrMask(p_ExtractOr->bitOffsetInPlcrProfile, FALSE);
        }

        genTmp |= (uint32_t)(p_ExtractOr->extractionOffset << KG_SCH_GEN_DEF_SHIFT);
        /* clear bits that need masking */
        genTmp &= ~KG_SCH_GEN_MASK ;
        /* set mask bits */
        genTmp |= (uint32_t)(p_ExtractOr->mask << KG_SCH_GEN_MASK_SHIFT);
        p_SchemeRegs->kgse_gec[currGenId++] = genTmp;

    }
    /* clear all unused GEC registers */
    for( i=currGenId ;i<FM_PCD_KG_NUM_OF_GENERIC_REGS ; i++)
        p_SchemeRegs->kgse_gec[i] = 0;

    /* add base Qid for this scheme */
    /* add configuration for kgse_fqb */
    if(p_Scheme->baseFqid & ~0x00FFFFFF)
        RETURN_ERROR(MAJOR, E_INVALID_VALUE, ("baseFqid must be between 1 and 2^24-1"));

    fqbTmp |= p_Scheme->baseFqid;
    p_SchemeRegs->kgse_fqb = fqbTmp;

    p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].nextEngine = p_Scheme->nextEngine;
    p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].doneAction = p_Scheme->kgNextEngineParams.doneAction;
    return E_OK;
}

void  FmPcdKgValidateSchemeSw(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    ASSERT_COND(!p_FmPcd->p_FmPcdKg->schemes[schemeId].valid);

    if(p_FmPcd->p_FmPcdKg->schemes[schemeId].netEnvId != ILLEGAL_NETENV)
        FmPcdIncNetEnvOwners(p_FmPcd, p_FmPcd->p_FmPcdKg->schemes[schemeId].netEnvId);
    p_FmPcd->p_FmPcdKg->schemes[schemeId].valid = TRUE;
}

void  FmPcdKgInvalidateSchemeSw(t_Handle h_FmPcd, uint8_t schemeId)
{

    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    if(p_FmPcd->p_FmPcdKg->schemes[schemeId].netEnvId != ILLEGAL_NETENV)
        FmPcdDecNetEnvOwners(h_FmPcd, p_FmPcd->p_FmPcdKg->schemes[schemeId].netEnvId);
    p_FmPcd->p_FmPcdKg->schemes[schemeId].valid = FALSE;
}

uint32_t FmPcdKgGetRequiredAction(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    ASSERT_COND(p_FmPcd->p_FmPcdKg->schemes[schemeId].valid);

    return p_FmPcd->p_FmPcdKg->schemes[schemeId].requiredAction;
}

uint32_t FmPcdKgGetPointedOwners(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

   ASSERT_COND(p_FmPcd->p_FmPcdKg->schemes[schemeId].valid);

    return p_FmPcd->p_FmPcdKg->schemes[schemeId].pointedOwners;
}

bool FmPcdKgIsDirectPlcr(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

   ASSERT_COND(p_FmPcd->p_FmPcdKg->schemes[schemeId].valid);

    return p_FmPcd->p_FmPcdKg->schemes[schemeId].directPlcr;
}


uint16_t FmPcdKgGetRelativeProfileId(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

   ASSERT_COND(p_FmPcd->p_FmPcdKg->schemes[schemeId].valid);

    return p_FmPcd->p_FmPcdKg->schemes[schemeId].relativeProfileId;
}


bool FmPcdKgIsDistrOnPlcrProfile(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

   ASSERT_COND(p_FmPcd->p_FmPcdKg->schemes[schemeId].valid);

    if((p_FmPcd->p_FmPcdKg->schemes[schemeId].extractedOrs &&
        p_FmPcd->p_FmPcdKg->schemes[schemeId].bitOffsetInPlcrProfile) ||
        p_FmPcd->p_FmPcdKg->schemes[schemeId].nextRelativePlcrProfile)
        return TRUE;
    else
        return FALSE;

}
void FmPcdKgUpatePointedOwner(t_Handle h_FmPcd, uint8_t schemeId, bool add)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

   ASSERT_COND(p_FmPcd->p_FmPcdKg->schemes[schemeId].valid);

    if(add)
        p_FmPcd->p_FmPcdKg->schemes[schemeId].pointedOwners++;
    else
        p_FmPcd->p_FmPcdKg->schemes[schemeId].pointedOwners--;
}

e_FmPcdEngine FmPcdKgGetNextEngine(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    ASSERT_COND(p_FmPcd->p_FmPcdKg->schemes[schemeId].valid);

    return p_FmPcd->p_FmPcdKg->schemes[schemeId].nextEngine;
}

e_FmPcdDoneAction FmPcdKgGetDoneAction(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    ASSERT_COND(p_FmPcd->p_FmPcdKg->schemes[schemeId].valid);

    return p_FmPcd->p_FmPcdKg->schemes[schemeId].doneAction;
}

void FmPcdKgUpdateRequiredAction(t_Handle h_FmPcd, uint8_t schemeId, uint32_t requiredAction)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    ASSERT_COND(p_FmPcd->p_FmPcdKg->schemes[schemeId].valid);

    p_FmPcd->p_FmPcdKg->schemes[schemeId].requiredAction = requiredAction;
}

t_Error FmPcdKgCheckInvalidateSchemeSw(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    if(schemeId >= FM_PCD_KG_NUM_OF_SCHEMES)
        REPORT_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);

   /* check that no port is bound to this scheme */
    if(p_FmPcd->p_FmPcdKg->schemes[schemeId].owners)
       RETURN_ERROR(MINOR, E_INVALID_STATE, ("Trying to delete a scheme that has ports bound to"));
    if(!p_FmPcd->p_FmPcdKg->schemes[schemeId].valid)
       RETURN_ERROR(MINOR, E_INVALID_STATE, ("Trying to delete an invalid scheme"));
    return E_OK;
}

uint32_t FmPcdKgBuildCppReg(t_Handle h_FmPcd, uint8_t clsPlanGrpId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t    tmpKgpeCpp;

    tmpKgpeCpp = (uint32_t)(p_FmPcd->p_FmPcdKg->clsPlanGrps[clsPlanGrpId].baseEntry / 8);
    tmpKgpeCpp |= (uint32_t)(((p_FmPcd->p_FmPcdKg->clsPlanGrps[clsPlanGrpId].sizeOfGrp / 8) - 1) << FM_PCD_KG_PE_CPP_MASK_SHIFT);
    return tmpKgpeCpp;
}

bool    FmPcdKgHwSchemeIsValid(uint32_t schemeModeReg)
{

    if(schemeModeReg & KG_SCH_MODE_EN)
        return TRUE;
    else
        return FALSE;
}

uint32_t    FmPcdKgBuildWriteSchemeActionReg(uint8_t schemeId, bool updateCounter)
{
    return     (uint32_t)(((uint32_t)schemeId << FM_PCD_KG_KGAR_NUM_SHIFT)|
                            FM_PCD_KG_KGAR_GO |
                            FM_PCD_KG_KGAR_WRITE |
                            FM_PCD_KG_KGAR_SEL_SCHEME_ENTRY |
                            DUMMY_PORT_ID |
                            (updateCounter ? FM_PCD_KG_KGAR_SCHEME_WSEL_UPDATE_CNT:0));

}

uint32_t    FmPcdKgBuildReadSchemeActionReg(uint8_t schemeId)
{
    return     (uint32_t)(((uint32_t)schemeId << FM_PCD_KG_KGAR_NUM_SHIFT)|
                            FM_PCD_KG_KGAR_GO |
                            FM_PCD_KG_KGAR_READ |
                            FM_PCD_KG_KGAR_SEL_SCHEME_ENTRY |
                            DUMMY_PORT_ID |
                            FM_PCD_KG_KGAR_SCHEME_WSEL_UPDATE_CNT);

}


uint32_t    FmPcdKgBuildWriteClsPlanBlockActionReg(uint8_t grpId)
{
    return (uint32_t)(FM_PCD_KG_KGAR_GO |
                        FM_PCD_KG_KGAR_WRITE |
                        FM_PCD_KG_KGAR_SEL_CLS_PLAN_ENTRY |
                        DUMMY_PORT_ID |
                        ((uint32_t)grpId << FM_PCD_KG_KGAR_NUM_SHIFT) |
                        FM_PCD_KG_KGAR_WSEL_MASK);


        /* if we ever want to write 1 by 1, use:
        sel = (uint8_t)(0x01 << (7- (entryId % CLS_PLAN_NUM_PER_GRP)));*/
}

uint32_t    FmPcdKgBuildReadClsPlanBlockActionReg(uint8_t grpId)
{
    return (uint32_t)(FM_PCD_KG_KGAR_GO |
                        FM_PCD_KG_KGAR_READ |
                        FM_PCD_KG_KGAR_SEL_CLS_PLAN_ENTRY |
                        DUMMY_PORT_ID |
                        ((uint32_t)grpId << FM_PCD_KG_KGAR_NUM_SHIFT) |
                        FM_PCD_KG_KGAR_WSEL_MASK);


        /* if we ever want to write 1 by 1, use:
        sel = (uint8_t)(0x01 << (7- (entryId % CLS_PLAN_NUM_PER_GRP)));*/
}

uint32_t        FmPcdKgBuildWritePortSchemeBindActionReg(uint8_t hardwarePortId)
{

    return (uint32_t)(FM_PCD_KG_KGAR_GO |
                        FM_PCD_KG_KGAR_WRITE |
                        FM_PCD_KG_KGAR_SEL_PORT_ENTRY |
                        hardwarePortId |
                        FM_PCD_KG_KGAR_SEL_PORT_WSEL_SP);
}

uint32_t        FmPcdKgBuildReadPortSchemeBindActionReg(uint8_t hardwarePortId)
{

    return (uint32_t)(FM_PCD_KG_KGAR_GO |
                        FM_PCD_KG_KGAR_READ |
                        FM_PCD_KG_KGAR_SEL_PORT_ENTRY |
                        hardwarePortId |
                        FM_PCD_KG_KGAR_SEL_PORT_WSEL_SP);
}
uint32_t        FmPcdKgBuildWritePortClsPlanBindActionReg(uint8_t hardwarePortId)
{

    return (uint32_t)(FM_PCD_KG_KGAR_GO |
                        FM_PCD_KG_KGAR_WRITE |
                        FM_PCD_KG_KGAR_SEL_PORT_ENTRY |
                        hardwarePortId |
                        FM_PCD_KG_KGAR_SEL_PORT_WSEL_CPP);
}

uint8_t FmPcdKgGetClsPlanGrpBase(t_Handle h_FmPcd, uint8_t clsPlanGrp)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    return p_FmPcd->p_FmPcdKg->clsPlanGrps[clsPlanGrp].baseEntry;
}

uint16_t FmPcdKgGetClsPlanGrpSize(t_Handle h_FmPcd, uint8_t clsPlanGrp)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;

    return p_FmPcd->p_FmPcdKg->clsPlanGrps[clsPlanGrp].sizeOfGrp;
}

uint8_t FmPcdKgGetSchemeSwId(t_Handle h_FmPcd, uint8_t schemeHwId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint8_t     i;

    for(i=0;i<p_FmPcd->p_FmPcdKg->numOfSchemes;i++)
        if(p_FmPcd->p_FmPcdKg->schemesIds[i] == schemeHwId)
            return i;
    ASSERT_COND(i!=p_FmPcd->p_FmPcdKg->numOfSchemes);
    return FM_PCD_KG_NUM_OF_SCHEMES;
}

uint8_t FmPcdKgGetNumOfPartitionSchemes(t_Handle h_FmPcd)
{
    return ((t_FmPcd*)h_FmPcd)->p_FmPcdKg->numOfSchemes;
}

uint8_t FmPcdKgGetPhysicalSchemeId(t_Handle h_FmPcd, uint8_t relativeSchemeId)
{
    return ((t_FmPcd*)h_FmPcd)->p_FmPcdKg->schemesIds[relativeSchemeId];
}

uint8_t FmPcdKgGetRelativeSchemeId(t_Handle h_FmPcd, uint8_t schemeId)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint8_t     i;

    for(i = 0;i<p_FmPcd->p_FmPcdKg->numOfSchemes;i++)
        if(p_FmPcd->p_FmPcdKg->schemesIds[i] == schemeId)
            return i;

    if(i == p_FmPcd->p_FmPcdKg->numOfSchemes)
        REPORT_ERROR(MAJOR, E_NOT_IN_RANGE, ("Scheme is out of partition range"));

    return FM_PCD_KG_NUM_OF_SCHEMES;
}

t_Error FmPcdKgCcGetSetParams(t_Handle h_FmPcd, t_Handle  h_Scheme, uint32_t requiredAction)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint8_t             relativeSchemeId, physicalSchemeId;
    uint32_t            tmpKgarReg, tmpReg32 = 0, intFlags;
    t_Error             err;

    SANITY_CHECK_RETURN_VALUE(h_FmPcd, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(p_FmPcd->p_FmPcdKg, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE, 0);

    if (p_FmPcd->h_Hc)
        return FmHcPcdKgCcGetSetParams(p_FmPcd->h_Hc,  h_Scheme,  requiredAction);

    physicalSchemeId = (uint8_t)(PTR_TO_UINT(h_Scheme)-1);

    relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmPcd, physicalSchemeId);
    if(relativeSchemeId >= FM_PCD_KG_NUM_OF_SCHEMES)
        RETURN_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);

    if (FmPcdKgSchemeTryLock(p_FmPcd, relativeSchemeId, FALSE))
        RETURN_ERROR(MAJOR, E_INVALID_STATE, ("Lock of the scheme FAILED"));

    if(!p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].pointedOwners ||
       !(p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].requiredAction & requiredAction))
    {
        if(requiredAction & UPDATE_NIA_ENQ_WITHOUT_DMA)
        {
            switch(p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].nextEngine)
            {
                case(e_FM_PCD_DONE):
                    if(p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].doneAction == e_FM_PCD_ENQ_FRAME)
                    {
                        tmpKgarReg = FmPcdKgBuildReadSchemeActionReg(physicalSchemeId);
                        intFlags = FmPcdLock(p_FmPcd);
                        WriteKgarWait(p_FmPcd, tmpKgarReg);
                        if (!(GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs.kgse_mode) & KG_SCH_MODE_EN))
                        {
                            RELEASE_LOCK(p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].lock);
                            RETURN_ERROR(MAJOR, E_ALREADY_EXISTS, ("Scheme is Invalid"));
                        }
                        tmpReg32 = GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs.kgse_mode);
                        ASSERT_COND(tmpReg32 & (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME));
                        WRITE_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs.kgse_mode, tmpReg32 | NIA_BMI_AC_ENQ_FRAME_WITHOUT_DMA);
                        /* call indirect command for scheme write */
                        tmpKgarReg = FmPcdKgBuildWriteSchemeActionReg(physicalSchemeId, FALSE);
                        WriteKgarWait(p_FmPcd, tmpKgarReg);
                        FmPcdUnlock(p_FmPcd, intFlags);
                    }
                break;
                case(e_FM_PCD_PLCR):
                    if(!p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].directPlcr ||
                       (p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].extractedOrs &&
                        p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].bitOffsetInPlcrProfile) ||
                        p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].nextRelativePlcrProfile)
                        {
                            RELEASE_LOCK(p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].lock);
                            RETURN_ERROR(MAJOR, E_NOT_SUPPORTED, ("In this situation PP can not be with distribution and has to be shared"));
                        }
                        err = FmPcdPlcrCcGetSetParams(h_FmPcd, p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].relativeProfileId, requiredAction);
                        if(err)
                        {
                            RELEASE_LOCK(p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].lock);
                            RETURN_ERROR(MAJOR, err, NO_MSG);
                        }
               break;
               default:
                    RETURN_ERROR(MAJOR, E_INVALID_VALUE,("in this situation the next engine after scheme can be or PLCR or ENQ_FRAME"));
            }
        }
    }
    p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].pointedOwners += 1;
    p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].requiredAction |= requiredAction;

    RELEASE_LOCK(p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].lock);
    return E_OK;
}

t_Error FmPcdKgSchemeTryLock(t_Handle h_FmPcd, uint8_t schemeId, bool intr)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    bool        ans;

    if (intr)
        ans = TRY_LOCK(NULL, &p_FmPcd->p_FmPcdKg->schemes[schemeId].lock);
    else
        ans = TRY_LOCK(p_FmPcd->h_Spinlock, &p_FmPcd->p_FmPcdKg->schemes[schemeId].lock);
    if (ans)
        return E_OK;
    return ERROR_CODE(E_BUSY);
}

void FmPcdKgReleaseSchemeLock(t_Handle h_FmPcd, uint8_t schemeId)
{
    RELEASE_LOCK(((t_FmPcd*)h_FmPcd)->p_FmPcdKg->schemes[schemeId].lock);
}

t_Handle FM_PCD_KgSetScheme(t_Handle h_FmPcd,  t_FmPcdKgSchemeParams *p_Scheme)
{
    t_FmPcd                             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t                            tmpReg;
    t_FmPcdKgInterModuleSchemeRegs      schemeRegs;
    t_FmPcdKgInterModuleSchemeRegs      *p_MemRegs;
    uint8_t                             i;
    t_Error                             err = E_OK;
    uint32_t                            tmpKgarReg;
    uint32_t                            intFlags;
    uint8_t                             physicalSchemeId, relativeSchemeId;

    SANITY_CHECK_RETURN_VALUE(p_FmPcd, E_INVALID_HANDLE, NULL);
    SANITY_CHECK_RETURN_VALUE(p_FmPcd->p_FmPcdKg, E_INVALID_HANDLE, NULL);
    SANITY_CHECK_RETURN_VALUE(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE, NULL);

    if (p_FmPcd->h_Hc)
        return FmHcPcdKgSetScheme(p_FmPcd->h_Hc, p_Scheme);

    /* if not called for modification, check first that this scheme is unused */
    if(!p_Scheme->modify)
    {
        /* check that schemeId is in range */
        if(p_Scheme->id.relativeSchemeId >= p_FmPcd->p_FmPcdKg->numOfSchemes)
        {
            REPORT_ERROR(MAJOR, E_NOT_IN_RANGE, ("Scheme is out of range"));
            return NULL;
        }
        relativeSchemeId = p_Scheme->id.relativeSchemeId;

        if (FmPcdKgSchemeTryLock(p_FmPcd, relativeSchemeId, FALSE))
            return NULL;

        physicalSchemeId = p_FmPcd->p_FmPcdKg->schemesIds[relativeSchemeId];

        /* read specified scheme into scheme registers */
        tmpKgarReg = FmPcdKgBuildReadSchemeActionReg(physicalSchemeId);
        intFlags = FmPcdLock(p_FmPcd);
        WriteKgarWait(p_FmPcd, tmpKgarReg);
        tmpReg = GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs.kgse_mode);
        FmPcdUnlock(p_FmPcd, intFlags);

        if (tmpReg & KG_SCH_MODE_EN)
        {
            REPORT_ERROR(MAJOR, E_ALREADY_EXISTS,
                         ("Scheme %d(phys %d) is already used", relativeSchemeId, physicalSchemeId));
            RELEASE_LOCK(p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].lock);
            return NULL;
        }
    }
    else
    {
        SANITY_CHECK_RETURN_VALUE(p_Scheme->id.h_Scheme, E_INVALID_HANDLE, NULL);

        intFlags = FmPcdLock(p_FmPcd);
        physicalSchemeId = (uint8_t)(PTR_TO_UINT(p_Scheme->id.h_Scheme)-1);
        relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmPcd, physicalSchemeId);

        /* check that schemeId is in range */
        if(relativeSchemeId >= FM_PCD_KG_NUM_OF_SCHEMES)
        {
            REPORT_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);
            FmPcdUnlock(p_FmPcd, intFlags);
            return NULL;
        }

        err = FmPcdKgSchemeTryLock(p_FmPcd, relativeSchemeId, TRUE);
        FmPcdUnlock(p_FmPcd, intFlags);
        if (err)
            return NULL;
    }

    err = FmPcdKgBuildScheme(h_FmPcd, p_Scheme, &schemeRegs);
    if(err)
    {
        REPORT_ERROR(MAJOR, err, NO_MSG);
        FmPcdKgInvalidateSchemeSw(h_FmPcd, relativeSchemeId);
        RELEASE_LOCK(p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].lock);
        return NULL;
    }

    /* configure all 21 scheme registers */
    p_MemRegs = &p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs;
    intFlags = FmPcdLock(p_FmPcd);
    WRITE_UINT32(p_MemRegs->kgse_ppc, schemeRegs.kgse_ppc);
    WRITE_UINT32(p_MemRegs->kgse_ccbs, schemeRegs.kgse_ccbs);
    WRITE_UINT32(p_MemRegs->kgse_mode, schemeRegs.kgse_mode);
    WRITE_UINT32(p_MemRegs->kgse_mv, schemeRegs.kgse_mv);
    WRITE_UINT32(p_MemRegs->kgse_dv0, schemeRegs.kgse_dv0);
    WRITE_UINT32(p_MemRegs->kgse_dv1, schemeRegs.kgse_dv1);
    WRITE_UINT32(p_MemRegs->kgse_ekdv, schemeRegs.kgse_ekdv);
    WRITE_UINT32(p_MemRegs->kgse_ekfc, schemeRegs.kgse_ekfc);
    WRITE_UINT32(p_MemRegs->kgse_bmch, schemeRegs.kgse_bmch);
    WRITE_UINT32(p_MemRegs->kgse_bmcl, schemeRegs.kgse_bmcl);
    WRITE_UINT32(p_MemRegs->kgse_hc, schemeRegs.kgse_hc);
    WRITE_UINT32(p_MemRegs->kgse_spc, schemeRegs.kgse_spc);
    WRITE_UINT32(p_MemRegs->kgse_fqb, schemeRegs.kgse_fqb);
    for(i=0 ; i<FM_PCD_KG_NUM_OF_GENERIC_REGS ; i++)
        WRITE_UINT32(p_MemRegs->kgse_gec[i], schemeRegs.kgse_gec[i]);

    /* call indirect command for scheme write */
    tmpKgarReg = FmPcdKgBuildWriteSchemeActionReg(physicalSchemeId, p_Scheme->schemeCounter.update);

    WriteKgarWait(p_FmPcd, tmpKgarReg);
    FmPcdUnlock(p_FmPcd, intFlags);

    FmPcdKgValidateSchemeSw(h_FmPcd, relativeSchemeId);

    RELEASE_LOCK(p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].lock);

    return UINT_TO_PTR((uint64_t)physicalSchemeId+1);
}

t_Error  FM_PCD_KgDeleteScheme(t_Handle h_FmPcd, t_Handle h_Scheme)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint8_t             physicalSchemeId;
    uint32_t            tmpKgarReg, intFlags;
    t_Error             err = E_OK;
    uint8_t             relativeSchemeId;

    SANITY_CHECK_RETURN_ERROR(p_FmPcd, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(p_FmPcd->p_FmPcdKg, E_INVALID_HANDLE);
    SANITY_CHECK_RETURN_ERROR(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE);

    if (p_FmPcd->h_Hc)
        return FmHcPcdKgDeleteScheme(p_FmPcd->h_Hc, h_Scheme);

    physicalSchemeId = (uint8_t)(PTR_TO_UINT(h_Scheme)-1);
    relativeSchemeId = FmPcdKgGetRelativeSchemeId(p_FmPcd, physicalSchemeId);

    if(relativeSchemeId >= FM_PCD_KG_NUM_OF_SCHEMES)
        RETURN_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);

    if ((err = FmPcdKgSchemeTryLock(p_FmPcd, relativeSchemeId, FALSE)) != E_OK)
       RETURN_ERROR(MINOR, err, NO_MSG);

    /* check that no port is bound to this scheme */
    err = FmPcdKgCheckInvalidateSchemeSw(h_FmPcd, relativeSchemeId);
    if(err)
       RETURN_ERROR(MINOR, err, NO_MSG);

    intFlags = FmPcdLock(p_FmPcd);
    /* clear mode register, including enable bit */
    WRITE_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs.kgse_mode, 0);

    /* call indirect command for scheme write */
    tmpKgarReg = FmPcdKgBuildWriteSchemeActionReg(physicalSchemeId, FALSE);

    WriteKgarWait(p_FmPcd, tmpKgarReg);
    FmPcdUnlock(p_FmPcd, intFlags);

    FmPcdKgInvalidateSchemeSw(h_FmPcd, relativeSchemeId);

    RELEASE_LOCK(p_FmPcd->p_FmPcdKg->schemes[relativeSchemeId].lock);

    return E_OK;
}

uint32_t  FM_PCD_KgGetSchemeCounter(t_Handle h_FmPcd, t_Handle h_Scheme)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t            tmpKgarReg, spc, intFlags;
    uint8_t             physicalSchemeId;

    SANITY_CHECK_RETURN_VALUE(h_FmPcd, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(p_FmPcd->p_FmPcdKg, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE, 0);

    if (p_FmPcd->h_Hc)
        return FmHcPcdKgGetSchemeCounter(p_FmPcd->h_Hc, h_Scheme);

    physicalSchemeId = (uint8_t)(PTR_TO_UINT(h_Scheme)-1);

    if(FmPcdKgGetRelativeSchemeId(p_FmPcd, physicalSchemeId) == FM_PCD_KG_NUM_OF_SCHEMES)
        REPORT_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);

    tmpKgarReg = FmPcdKgBuildReadSchemeActionReg(physicalSchemeId);
    intFlags = FmPcdLock(p_FmPcd);
    WriteKgarWait(p_FmPcd, tmpKgarReg);
    if (!(GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs.kgse_mode) & KG_SCH_MODE_EN))
       REPORT_ERROR(MAJOR, E_ALREADY_EXISTS, ("Scheme is Invalid"));
    spc = GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs.kgse_spc);
    FmPcdUnlock(p_FmPcd, intFlags);

    return spc;
}

t_Error  FM_PCD_KgSetSchemeCounter(t_Handle h_FmPcd, t_Handle h_Scheme, uint32_t value)
{
    t_FmPcd             *p_FmPcd = (t_FmPcd*)h_FmPcd;
    uint32_t            tmpKgarReg, intFlags;
    uint8_t             physicalSchemeId;

    SANITY_CHECK_RETURN_VALUE(h_FmPcd, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(p_FmPcd->p_FmPcdKg, E_INVALID_HANDLE, 0);
    SANITY_CHECK_RETURN_VALUE(!p_FmPcd->p_FmPcdDriverParam, E_INVALID_STATE, 0);

    if (p_FmPcd->h_Hc)
        return FmHcPcdKgSetSchemeCounter(p_FmPcd->h_Hc, h_Scheme, value);

    physicalSchemeId = (uint8_t)(PTR_TO_UINT(h_Scheme)-1);
    /* check that schemeId is in range */
    if(FmPcdKgGetRelativeSchemeId(p_FmPcd, physicalSchemeId) == FM_PCD_KG_NUM_OF_SCHEMES)
        REPORT_ERROR(MAJOR, E_NOT_IN_RANGE, NO_MSG);

    /* read specified scheme into scheme registers */
    tmpKgarReg = FmPcdKgBuildReadSchemeActionReg(physicalSchemeId);
    intFlags = FmPcdLock(p_FmPcd);
    WriteKgarWait(p_FmPcd, tmpKgarReg);
    if (!(GET_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs.kgse_mode) & KG_SCH_MODE_EN))
    {
       FmPcdUnlock(p_FmPcd, intFlags);
       RETURN_ERROR(MAJOR, E_ALREADY_EXISTS, ("Scheme is Invalid"));
    }

    /* change counter value */
    WRITE_UINT32(p_FmPcd->p_FmPcdKg->p_FmPcdKgRegs->indirectAccessRegs.schemeRegs.kgse_spc, value);

    /* call indirect command for scheme write */
    tmpKgarReg = FmPcdKgBuildWriteSchemeActionReg(physicalSchemeId, TRUE);

    WriteKgarWait(p_FmPcd, tmpKgarReg);
    FmPcdUnlock(p_FmPcd, intFlags);

    return E_OK;
}

