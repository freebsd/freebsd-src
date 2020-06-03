/** @file
  IPMI 2.0 definitions from the IPMI Specification Version 2.0, Revision 1.1.
  IPMI Intelligent Chassis Management Bus Bridge Specification Version 1.0,
  Revision 1.3.

  See IPMI specification, Appendix G, Command Assignments
  and Appendix H, Sub-function Assignments.

  Copyright (c) 1999 - 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _IPMI_NET_FN_BRIDGE_H_
#define _IPMI_NET_FN_BRIDGE_H_

//
// Net function definition for Bridge command
//
#define IPMI_NETFN_BRIDGE 0x02

//
//  Definitions for Get Bridge State command
//
#define IPMI_BRIDGE_GET_STATE  0x00

//
//  Constants and Structure definitions for "Get Bridge State" command to follow here
//

//
//  Definitions for Set Bridge State command
//
#define IPMI_BRIDGE_SET_STATE  0x01

//
//  Constants and Structure definitions for "Set Bridge State" command to follow here
//

//
//  Definitions for Get ICMB Address command
//
#define IPMI_BRIDGE_GET_ICMB_ADDRESS 0x02

//
//  Constants and Structure definitions for "Get ICMB Address" command to follow here
//

//
//  Definitions for Set ICMB Address command
//
#define IPMI_BRIDGE_SET_ICMB_ADDRESS 0x03

//
//  Constants and Structure definitions for "Set ICMB Address" command to follow here
//

//
//  Definitions for Set Bridge Proxy Address command
//
#define IPMI_BRIDGE_SET_PROXY_ADDRESS  0x04

//
//  Constants and Structure definitions for "Set Bridge Proxy Address" command to follow here
//

//
//  Definitions for Get Bridge Statistics command
//
#define IPMI_BRIDGE_GET_BRIDGE_STATISTICS  0x05

//
//  Constants and Structure definitions for "Get Bridge Statistics" command to follow here
//

//
//  Definitions for Get ICMB Capabilities command
//
#define IPMI_BRIDGE_GET_ICMB_CAPABILITIES  0x06

//
//  Constants and Structure definitions for "Get ICMB Capabilities" command to follow here
//

//
//  Definitions for Clear Bridge Statistics command
//
#define IPMI_BRIDGE_CLEAR_STATISTICS 0x08

//
//  Constants and Structure definitions for "Clear Bridge Statistics" command to follow here
//

//
//  Definitions for Get Bridge Proxy Address command
//
#define IPMI_BRIDGE_GET_PROXY_ADDRESS  0x09

//
//  Constants and Structure definitions for "Get Bridge Proxy Address" command to follow here
//

//
//  Definitions for Get ICMB Connector Info command
//
#define IPMI_BRIDGE_GET_ICMB_CONNECTOR_INFO  0x0A

//
//  Constants and Structure definitions for "Get ICMB Connector Info " command to follow here
//

//
//  Definitions for Get ICMB Connection ID command
//
#define IPMI_BRIDGE_GET_ICMB_CONNECTION_ID 0x0B

//
//  Constants and Structure definitions for "Get ICMB Connection ID" command to follow here
//

//
//  Definitions for Get ICMB Connection ID command
//
#define IPMI_BRIDGE_SEND_ICMB_CONNECTION_ID  0x0C

//
//  Constants and Structure definitions for "Send ICMB Connection ID" command to follow here
//

//
//  Definitions for Prepare for Discovery command
//
#define IPMI_BRIDGE_PREPARE_FOR_DISCOVERY  0x10

//
//  Constants and Structure definitions for "Prepare for Discovery" command to follow here
//

//
//  Definitions for Get Addresses command
//
#define IPMI_BRIDGE_GET_ADDRESSES  0x11

//
//  Constants and Structure definitions for "Get Addresses" command to follow here
//

//
//  Definitions for Set Discovered command
//
#define IPMI_BRIDGE_SET_DISCOVERED 0x12

//
//  Constants and Structure definitions for "Set Discovered" command to follow here
//

//
//  Definitions for Get Chassis Device ID command
//
#define IPMI_BRIDGE_GET_CHASSIS_DEVICEID 0x13

//
//  Constants and Structure definitions for "Get Chassis Device ID" command to follow here
//

//
//  Definitions for Set Chassis Device ID command
//
#define IPMI_BRIDGE_SET_CHASSIS_DEVICEID 0x14

//
//  Constants and Structure definitions for "Set Chassis Device ID" command to follow here
//

//
//  Definitions for Bridge Request command
//
#define IPMI_BRIDGE_REQUEST  0x20

//
//  Constants and Structure definitions for "Bridge Request" command to follow here
//

//
//  Definitions for Bridge Message command
//
#define IPMI_BRIDGE_MESSAGE  0x21

//
//  Constants and Structure definitions for "Bridge Message" command to follow here
//

//
//  Definitions for Get Event Count command
//
#define IPMI_BRIDGE_GET_EVENT_COUNT  0x30

//
//  Constants and Structure definitions for "Get Event Count" command to follow here
//

//
//  Definitions for Set Event Destination command
//
#define IPMI_BRIDGE_SET_EVENT_DESTINATION  0x31

//
//  Constants and Structure definitions for "Set Event Destination" command to follow here
//

//
//  Definitions for Set Event Reception State command
//
#define IPMI_BRIDGE_SET_EVENT_RECEPTION_STATE  0x32

//
//  Constants and Structure definitions for "Set Event Reception State" command to follow here
//

//
//  Definitions for Set Event Reception State command
//
#define IPMI_BRIDGE_SET_EVENT_RECEPTION_STATE  0x32

//
//  Constants and Structure definitions for "Set Event Reception State" command to follow here
//

//
//  Definitions for Send ICMB Event Message command
//
#define IPMI_BRIDGE_SEND_ICMB_EVENT_MESSAGE  0x33

//
//  Constants and Structure definitions for "Send ICMB Event Message" command to follow here
//

#endif
