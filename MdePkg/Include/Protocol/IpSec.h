/** @file
  EFI IPSEC Protocol Definition
  The EFI_IPSEC_PROTOCOL is used to abstract the ability to deal with the individual
  packets sent and received by the host and provide packet-level security for IP 
  datagram.
  The EFI_IPSEC2_PROTOCOL is used to abstract the ability to deal with the individual
  packets sent and received by the host and provide packet-level security for IP 
  datagram. In addition, it supports the Option (extension header) processing in 
  IPsec which doesn't support in EFI_IPSEC_PROTOCOL. It is also recommended to 
  use EFI_IPSEC2_PROTOCOL instead of EFI_IPSEC_PROTOCOL especially for IPsec Tunnel 
  Mode.

  Copyright (c) 2009 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Revision Reference:          
  The EFI_IPSEC2_PROTOCOL is introduced in UEFI Specification 2.3D.

**/

#ifndef __EFI_IPSEC_PROTOCOL_H__
#define __EFI_IPSEC_PROTOCOL_H__

#include <Protocol/IpSecConfig.h>

#define EFI_IPSEC_PROTOCOL_GUID \
  { \
    0xdfb386f7, 0xe100, 0x43ad, {0x9c, 0x9a, 0xed, 0x90, 0xd0, 0x8a, 0x5e, 0x12 } \
  }

#define EFI_IPSEC2_PROTOCOL_GUID \
  { \
    0xa3979e64, 0xace8, 0x4ddc, {0xbc, 0x7, 0x4d, 0x66, 0xb8, 0xfd, 0x9, 0x77 } \
  }

typedef struct _EFI_IPSEC_PROTOCOL  EFI_IPSEC_PROTOCOL;
typedef struct _EFI_IPSEC2_PROTOCOL EFI_IPSEC2_PROTOCOL;

///
/// EFI_IPSEC_FRAGMENT_DATA 
/// defines the instances of packet fragments.
///
typedef struct _EFI_IPSEC_FRAGMENT_DATA { 
  UINT32  FragmentLength;
  VOID    *FragmentBuffer;
} EFI_IPSEC_FRAGMENT_DATA; 


/**
  Handles IPsec packet processing for inbound and outbound IP packets. 

  The EFI_IPSEC_PROCESS process routine handles each inbound or outbound packet.
  The behavior is that it can perform one of the following actions: 
  bypass the packet, discard the packet, or protect the packet.       

  @param[in]      This             Pointer to the EFI_IPSEC_PROTOCOL instance.
  @param[in]      NicHandle        Instance of the network interface.
  @param[in]      IpVer            IPV4 or IPV6.
  @param[in, out] IpHead           Pointer to the IP Header.
  @param[in]      LastHead         The protocol of the next layer to be processed by IPsec.
  @param[in]      OptionsBuffer    Pointer to the options buffer. 
  @param[in]      OptionsLength    Length of the options buffer.
  @param[in, out] FragmentTable    Pointer to a list of fragments. 
  @param[in]      FragmentCount    Number of fragments.
  @param[in]      TrafficDirection Traffic direction.
  @param[out]     RecycleSignal    Event for recycling of resources.
 
  @retval EFI_SUCCESS              The packet was bypassed and all buffers remain the same.
  @retval EFI_SUCCESS              The packet was protected.
  @retval EFI_ACCESS_DENIED        The packet was discarded.

**/
typedef
EFI_STATUS
(EFIAPI  *EFI_IPSEC_PROCESS)(
  IN     EFI_IPSEC_PROTOCOL      *This,
  IN     EFI_HANDLE              NicHandle,
  IN     UINT8                   IpVer,
  IN OUT VOID                    *IpHead,
  IN     UINT8                   *LastHead,
  IN     VOID                    *OptionsBuffer,
  IN     UINT32                  OptionsLength,
  IN OUT EFI_IPSEC_FRAGMENT_DATA **FragmentTable,
  IN     UINT32                  *FragmentCount,
  IN     EFI_IPSEC_TRAFFIC_DIR   TrafficDirection,
     OUT EFI_EVENT               *RecycleSignal
  );

///
/// EFI_IPSEC_PROTOCOL 
/// provides the ability for  securing IP communications by authenticating
/// and/or encrypting each IP packet in a data stream. 
//  EFI_IPSEC_PROTOCOL can be consumed by both the IPv4 and IPv6 stack.
//  A user can employ this protocol for IPsec package handling in both IPv4
//  and IPv6 environment.
///
struct _EFI_IPSEC_PROTOCOL {
  EFI_IPSEC_PROCESS      Process;           ///< Handle the IPsec message.
  EFI_EVENT              DisabledEvent;     ///< Event signaled when the interface is disabled.
  BOOLEAN                DisabledFlag;      ///< State of the interface.
};

/**
  Handles IPsec processing for both inbound and outbound IP packets. Compare with 
  Process() in EFI_IPSEC_PROTOCOL, this interface has the capability to process 
  Option(Extension Header). 

  The EFI_IPSEC2_PROCESS process routine handles each inbound or outbound packet.
  The behavior is that it can perform one of the following actions: 
  bypass the packet, discard the packet, or protect the packet.  

  @param[in]       This               Pointer to the EFI_IPSEC2_PROTOCOL instance.
  @param[in]       NicHandle          Instance of the network interface. 
  @param[in]       IpVer              IP version.IPv4 or IPv6.
  @param[in, out]  IpHead             Pointer to the IP Header it is either 
                                      the EFI_IP4_HEADER or EFI_IP6_HEADER.
                                      On input, it contains the IP header. 
                                      On output, 1) in tunnel mode and the 
                                      traffic direction is inbound, the buffer 
                                      will be reset to zero by IPsec; 2) in 
                                      tunnel mode and the traffic direction 
                                      is outbound, the buffer will reset to 
                                      be the tunnel IP header.3) in transport 
                                      mode, the related fielders (like payload 
                                      length, Next header) in IP header will 
                                      be modified according to the condition.
  @param[in, out]  LastHead           For IP4, it is the next protocol in IP
                                      header. For IP6 it is the Next Header 
                                      of the last extension header.
  @param[in, out]  OptionsBuffer      On input, it contains the options 
                                      (extensions header) to be processed by 
                                      IPsec. On output, 1) in tunnel mode and
                                      the traffic direction is outbound, it 
                                      will be set to NULL, and that means this 
                                      contents was wrapped after inner header 
                                      and should not be concatenated after 
                                      tunnel header again; 2) in transport 
                                      mode and the traffic direction is inbound, 
                                      if there are IP options (extension headers) 
                                      protected by IPsec, IPsec will concatenate 
                                      the those options after the input options 
                                      (extension headers); 3) on other situations, 
                                      the output of contents of OptionsBuffer 
                                      might be same with input's. The caller 
                                      should take the responsibility to free 
                                      the buffer both on input and on output.
  @param[in, out]  OptionsLength      On input, the input length of the options 
                                      buffer. On output, the output length of 
                                      the options buffer.
  @param[in, out]  FragmentTable      Pointer to a list of fragments. On input, 
                                      these fragments contain the IP payload. 
                                      On output, 1) in tunnel mode and the traffic 
                                      direction is inbound, the fragments contain 
                                      the whole IP payload which is from the 
                                      IP inner header to the last byte of the 
                                      packet; 2) in tunnel mode and the traffic 
                                      direction is the outbound, the fragments 
                                      contains the whole encapsulated payload 
                                      which encapsulates the whole IP payload 
                                      between the encapsulated header and 
                                      encapsulated trailer fields. 3) in transport 
                                      mode and the traffic direction is inbound, 
                                      the fragments contains the IP payload 
                                      which is from the next layer protocol to 
                                      the last byte of the packet; 4) in transport 
                                      mode and the traffic direction is outbound, 
                                      the fragments contains the whole encapsulated 
                                      payload which encapsulates the next layer 
                                      protocol information between the encapsulated 
                                      header and encapsulated trailer fields.
  @param[in, out]  FragmentCount      Number of fragments.
  @param[in]       TrafficDirection   Traffic direction.
  @param[out]      RecycleSignal      Event for recycling of resources.

  @retval      EFI_SUCCESS           The packet was processed by IPsec successfully.
  @retval      EFI_ACCESS_DENIED     The packet was discarded.
  @retval      EFI_NOT_READY         The IKE negotiation is invoked and the packet 
                                     was discarded.
  @retval      EFI_INVALID_PARAMETER One or more of following are TRUE:
                                     If OptionsBuffer is NULL;
                                     If OptionsLength is NULL;
                                     If FragmentTable is NULL;
                                     If FragmentCount is NULL.

**/
typedef 
EFI_STATUS
(EFIAPI *EFI_IPSEC_PROCESSEXT) ( 
  IN EFI_IPSEC2_PROTOCOL         *This, 
  IN EFI_HANDLE                  NicHandle, 
  IN UINT8                       IpVer, 
  IN OUT VOID                    *IpHead, 
  IN OUT UINT8                   *LastHead, 
  IN OUT VOID                    **OptionsBuffer, 
  IN OUT UINT32                  *OptionsLength, 
  IN OUT EFI_IPSEC_FRAGMENT_DATA **FragmentTable, 
  IN OUT UINT32                  *FragmentCount, 
  IN EFI_IPSEC_TRAFFIC_DIR       TrafficDirection, 
     OUT EFI_EVENT               *RecycleSignal
  );

/// 
/// EFI_IPSEC2_PROTOCOL
/// supports the Option (extension header) processing in IPsec which doesn't support
/// in EFI_IPSEC_PROTOCOL. It is also recommended to use EFI_IPSEC2_PROTOCOL instead
/// of EFI_IPSEC_PROTOCOL especially for IPsec Tunnel Mode.
/// provides the ability for securing IP communications by authenticating and/or
/// encrypting each IP packet in a data stream.
///
struct _EFI_IPSEC2_PROTOCOL { 
EFI_IPSEC_PROCESSEXT ProcessExt;
EFI_EVENT            DisabledEvent; 
BOOLEAN              DisabledFlag; 
};

extern EFI_GUID gEfiIpSecProtocolGuid;
extern EFI_GUID gEfiIpSec2ProtocolGuid;
#endif
