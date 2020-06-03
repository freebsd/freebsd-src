/** @file
  This file provides management service interfaces of 802.11 MAC layer. It is used by
  network applications (and drivers) to establish wireless connection with an access
  point (AP).

  Copyright (c) 2015 - 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.5

**/

#ifndef __EFI_WIFI_PROTOCOL_H__
#define __EFI_WIFI_PROTOCOL_H__

#include <Protocol/WiFi2.h>

#define EFI_WIRELESS_MAC_CONNECTION_PROTOCOL_GUID \
  { \
    0xda55bc9, 0x45f8, 0x4bb4, {0x87, 0x19, 0x52, 0x24, 0xf1, 0x8a, 0x4d, 0x45 } \
  }

typedef struct _EFI_WIRELESS_MAC_CONNECTION_PROTOCOL EFI_WIRELESS_MAC_CONNECTION_PROTOCOL;

///
/// EFI_80211_ACC_NET_TYPE
///
typedef enum {
  IeeePrivate           = 0,
  IeeePrivatewithGuest  = 1,
  IeeeChargeablePublic  = 2,
  IeeeFreePublic        = 3,
  IeeePersonal          = 4,
  IeeeEmergencyServOnly = 5,
  IeeeTestOrExp         = 14,
  IeeeWildcard          = 15
} EFI_80211_ACC_NET_TYPE;

///
/// EFI_80211_ASSOCIATE_RESULT_CODE
///
typedef enum {
  AssociateSuccess,
  AssociateRefusedReasonUnspecified,
  AssociateRefusedCapsMismatch,
  AssociateRefusedExtReason,
  AssociateRefusedAPOutOfMemory,
  AssociateRefusedBasicRatesMismatch,
  AssociateRejectedEmergencyServicesNotSupported,
  AssociateRefusedTemporarily
} EFI_80211_ASSOCIATE_RESULT_CODE;

///
/// EFI_80211_SCAN_RESULT_CODE
///
typedef enum {
  ///
  /// The scan operation finished successfully.
  ///
  ScanSuccess,
  ///
  /// The scan operation is not supported in current implementation.
  ///
  ScanNotSupported
} EFI_80211_SCAN_RESULT_CODE;

///
/// EFI_80211_REASON_CODE
///
typedef enum {
  Ieee80211UnspecifiedReason           = 1,
  Ieee80211PreviousAuthenticateInvalid = 2,
  Ieee80211DeauthenticatedSinceLeaving = 3,
  Ieee80211DisassociatedDueToInactive  = 4,
  Ieee80211DisassociatedSinceApUnable  = 5,
  Ieee80211Class2FrameNonauthenticated = 6,
  Ieee80211Class3FrameNonassociated    = 7,
  Ieee80211DisassociatedSinceLeaving   = 8,
  // ...
} EFI_80211_REASON_CODE;

///
/// EFI_80211_DISASSOCIATE_RESULT_CODE
///
typedef enum {
  ///
  /// Disassociation process completed successfully.
  ///
  DisassociateSuccess,
  ///
  /// Disassociation failed due to any input parameter is invalid.
  ///
  DisassociateInvalidParameters
} EFI_80211_DISASSOCIATE_RESULT_CODE;

///
/// EFI_80211_AUTHENTICATION_TYPE
///
typedef enum {
  ///
  /// Open system authentication, admits any STA to the DS.
  ///
  OpenSystem,
  ///
  /// Shared Key authentication relies on WEP to demonstrate knowledge of a WEP
  /// encryption key.
  ///
  SharedKey,
  ///
  /// FT authentication relies on keys derived during the initial mobility domain
  /// association to authenticate the stations.
  ///
  FastBSSTransition,
  ///
  /// SAE authentication uses finite field cryptography to prove knowledge of a shared
  /// password.
  ///
  SAE
} EFI_80211_AUTHENTICATION_TYPE;

///
/// EFI_80211_AUTHENTICATION_RESULT_CODE
///
typedef enum {
  AuthenticateSuccess,
  AuthenticateRefused,
  AuthenticateAnticLoggingTokenRequired,
  AuthenticateFiniteCyclicGroupNotSupported,
  AuthenticationRejected,
  AuthenticateInvalidParameter
} EFI_80211_AUTHENTICATE_RESULT_CODE;

///
/// EFI_80211_ELEMENT_HEADER
///
typedef struct {
  ///
  /// A unique element ID defined in IEEE 802.11 specification.
  ///
  UINT8                              ElementID;
  ///
  /// Specifies the number of octets in the element body.
  ///
  UINT8                              Length;
} EFI_80211_ELEMENT_HEADER;

///
/// EFI_80211_ELEMENT_REQ
///
typedef struct {
  ///
  /// Common header of an element.
  ///
  EFI_80211_ELEMENT_HEADER           Hdr;
  ///
  /// Start of elements that are requested to be included in the Probe Response frame.
  /// The elements are listed in order of increasing element ID.
  ///
  UINT8                              RequestIDs[1];
} EFI_80211_ELEMENT_REQ;

///
/// EFI_80211_ELEMENT_SSID
///
typedef struct {
  ///
  /// Common header of an element.
  ///
  EFI_80211_ELEMENT_HEADER           Hdr;
  ///
  /// Service set identifier. If Hdr.Length is zero, this field is ignored.
  ///
  UINT8                              SSId[32];
} EFI_80211_ELEMENT_SSID;

///
/// EFI_80211_SCAN_DATA
///
typedef struct {
  ///
  /// Determines whether infrastructure BSS, IBSS, MBSS, or all, are included in the
  /// scan.
  ///
  EFI_80211_BSS_TYPE                 BSSType;
  ///
  /// Indicates a specific or wildcard BSSID. Use all binary 1s to represent all SSIDs.
  ///
  EFI_80211_MAC_ADDRESS              BSSId;
  ///
  /// Length in bytes of the SSId. If zero, ignore SSId field.
  ///
  UINT8                              SSIdLen;
  ///
  /// Specifies the desired SSID or the wildcard SSID. Use NULL to represent all SSIDs.
  ///
  UINT8                              *SSId;
  ///
  /// Indicates passive scanning if TRUE.
  ///
  BOOLEAN                            PassiveMode;
  ///
  /// The delay in microseconds to be used prior to transmitting a Probe frame during
  /// active scanning. If zero, the value can be overridden by an
  /// implementation-dependent default value.
  ///
  UINT32                             ProbeDelay;
  ///
  /// Specifies a list of channels that are examined when scanning for a BSS. If set to
  /// NULL, all valid channels will be scanned.
  ///
  UINT32                             *ChannelList;
  ///
  /// Indicates the minimum time in TU to spend on each channel when scanning. If zero,
  /// the value can be overridden by an implementation-dependent default value.
  ///
  UINT32                             MinChannelTime;
  ///
  /// Indicates the maximum time in TU to spend on each channel when scanning. If zero,
  /// the value can be overridden by an implementation-dependent default value.
  ///
  UINT32                             MaxChannelTime;
  ///
  /// Points to an optionally present element. This is an optional parameter and may be
  /// NULL.
  ///
  EFI_80211_ELEMENT_REQ              *RequestInformation;
  ///
  /// Indicates one or more SSID elements that are optionally present. This is an
  /// optional parameter and may be NULL.
  ///
  EFI_80211_ELEMENT_SSID             *SSIDList;
  ///
  /// Specifies a desired specific access network type or the wildcard access network
  /// type. Use 15 as wildcard access network type.
  ///
  EFI_80211_ACC_NET_TYPE             AccessNetworkType;
  ///
  ///  Specifies zero or more elements. This is an optional parameter and may be NULL.
  ///
  UINT8                              *VendorSpecificInfo;
} EFI_80211_SCAN_DATA;

///
/// EFI_80211_COUNTRY_TRIPLET_SUBBAND
///
typedef struct {
  ///
  /// Indicates the lowest channel number in the subband. It has a positive integer
  /// value less than 201.
  ///
  UINT8                              FirstChannelNum;
  ///
  /// Indicates the number of channels in the subband.
  ///
  UINT8                              NumOfChannels;
  ///
  /// Indicates the maximum power in dBm allowed to be transmitted.
  ///
  UINT8                              MaxTxPowerLevel;
} EFI_80211_COUNTRY_TRIPLET_SUBBAND;

///
/// EFI_80211_COUNTRY_TRIPLET_OPERATE
///
typedef struct {
  ///
  /// Indicates the operating extension identifier. It has a positive integer value of
  /// 201 or greater.
  ///
  UINT8                              OperatingExtId;
  ///
  /// Index into a set of values for radio equipment set of rules.
  ///
  UINT8                              OperatingClass;
  ///
  /// Specifies aAirPropagationTime characteristics used in BSS operation. Refer the
  /// definition of aAirPropagationTime in IEEE 802.11 specification.
  ///
  UINT8                              CoverageClass;
} EFI_80211_COUNTRY_TRIPLET_OPERATE;

///
/// EFI_80211_COUNTRY_TRIPLET
///
typedef union {
  ///
  /// The subband triplet.
  ///
  EFI_80211_COUNTRY_TRIPLET_SUBBAND  Subband;
  ///
  /// The operating triplet.
  ///
  EFI_80211_COUNTRY_TRIPLET_OPERATE  Operating;
} EFI_80211_COUNTRY_TRIPLET;

///
/// EFI_80211_ELEMENT_COUNTRY
///
typedef struct {
  ///
  /// Common header of an element.
  ///
  EFI_80211_ELEMENT_HEADER           Hdr;
  ///
  /// Specifies country strings in 3 octets.
  ///
  UINT8                              CountryStr[3];
  ///
  /// Indicates a triplet that repeated in country element. The number of triplets is
  /// determined by the Hdr.Length field.
  ///
  EFI_80211_COUNTRY_TRIPLET          CountryTriplet[1];
} EFI_80211_ELEMENT_COUNTRY;

///
/// EFI_80211_ELEMENT_DATA_RSN
///
typedef struct {
  ///
  /// Indicates the version number of the RSNA protocol. Value 1 is defined in current
  /// IEEE 802.11 specification.
  ///
  UINT16                             Version;
  ///
  /// Specifies the cipher suite selector used by the BSS to protect group address frames.
  ///
  UINT32                             GroupDataCipherSuite;
  ///
  /// Indicates the number of pairwise cipher suite selectors that are contained in
  /// PairwiseCipherSuiteList.
  ///
//UINT16                             PairwiseCipherSuiteCount;
  ///
  /// Contains a series of cipher suite selectors that indicate the pairwise cipher
  /// suites contained in this element.
  ///
//UINT32                             PairwiseCipherSuiteList[PairwiseCipherSuiteCount];
  ///
  /// Indicates the number of AKM suite selectors that are contained in AKMSuiteList.
  ///
//UINT16                             AKMSuiteCount;
  ///
  /// Contains a series of AKM suite selectors that indicate the AKM suites contained in
  /// this element.
  ///
//UINT32                             AKMSuiteList[AKMSuiteCount];
  ///
  /// Indicates requested or advertised capabilities.
  ///
//UINT16                             RSNCapabilities;
  ///
  /// Indicates the number of PKMIDs in the PMKIDList.
  ///
//UINT16                             PMKIDCount;
  ///
  /// Contains zero or more PKMIDs that the STA believes to be valid for the destination
  /// AP.
//UINT8                              PMKIDList[PMKIDCount][16];
  ///
  /// Specifies the cipher suite selector used by the BSS to protect group addressed
  /// robust management frames.
  ///
//UINT32                             GroupManagementCipherSuite;
} EFI_80211_ELEMENT_DATA_RSN;

///
/// EFI_80211_ELEMENT_RSN
///
typedef struct {
  ///
  /// Common header of an element.
  ///
  EFI_80211_ELEMENT_HEADER           Hdr;
  ///
  /// Points to RSN element. The size of a RSN element is limited to 255 octets.
  ///
  EFI_80211_ELEMENT_DATA_RSN         *Data;
} EFI_80211_ELEMENT_RSN;

///
/// EFI_80211_ELEMENT_EXT_CAP
///
typedef struct {
  ///
  /// Common header of an element.
  ///
  EFI_80211_ELEMENT_HEADER           Hdr;
  ///
  /// Indicates the capabilities being advertised by the STA transmitting the element.
  /// This is a bit field with variable length. Refer to IEEE 802.11 specification for
  /// bit value.
  ///
  UINT8                              Capabilities[1];
} EFI_80211_ELEMENT_EXT_CAP;

///
/// EFI_80211_BSS_DESCRIPTION
///
typedef struct {
  ///
  /// Indicates a specific BSSID of the found BSS.
  ///
  EFI_80211_MAC_ADDRESS              BSSId;
  ///
  /// Specifies the SSID of the found BSS. If NULL, ignore SSIdLen field.
  ///
  UINT8                              *SSId;
  ///
  /// Specifies the SSID of the found BSS. If NULL, ignore SSIdLen field.
  ///
  UINT8                              SSIdLen;
  ///
  /// Specifies the type of the found BSS.
  ///
  EFI_80211_BSS_TYPE                 BSSType;
  ///
  /// The beacon period in TU of the found BSS.
  ///
  UINT16                             BeaconPeriod;
  ///
  /// The timestamp of the received frame from the found BSS.
  ///
  UINT64                             Timestamp;
  ///
  /// The advertised capabilities of the BSS.
  ///
  UINT16                             CapabilityInfo;
  ///
  /// The set of data rates that shall be supported by all STAs that desire to join this
  /// BSS.
  ///
  UINT8                              *BSSBasicRateSet;
  ///
  /// The set of data rates that the peer STA desires to use for communication within
  /// the BSS.
  ///
  UINT8                              *OperationalRateSet;
  ///
  /// The information required to identify the regulatory domain in which the peer STA
  /// is located.
  ///
  EFI_80211_ELEMENT_COUNTRY          *Country;
  ///
  /// The cipher suites and AKM suites supported in the BSS.
  ///
  EFI_80211_ELEMENT_RSN              RSN;
  ///
  /// Specifies the RSSI of the received frame.
  ///
  UINT8                              RSSI;
  ///
  /// Specifies the RCPI of the received frame.
  ///
  UINT8                              RCPIMeasurement;
  ///
  /// Specifies the RSNI of the received frame.
  ///
  UINT8                              RSNIMeasurement;
  ///
  /// Specifies the elements requested by the request element of the Probe Request frame.
  /// This is an optional parameter and may be NULL.
  ///
  UINT8                              *RequestedElements;
  ///
  /// Specifies the BSS membership selectors that represent the set of features that
  /// shall be supported by all STAs to join this BSS.
  ///
  UINT8                              *BSSMembershipSelectorSet;
  ///
  /// Specifies the parameters within the Extended Capabilities element that are
  /// supported by the MAC entity. This is an optional parameter and may be NULL.
  ///
  EFI_80211_ELEMENT_EXT_CAP          *ExtCapElement;
} EFI_80211_BSS_DESCRIPTION;

///
/// EFI_80211_SUBELEMENT_INFO
///
typedef struct {
  ///
  /// Indicates the unique identifier within the containing element or sub-element.
  ///
  UINT8                              SubElementID;
  ///
  /// Specifies the number of octets in the Data field.
  ///
  UINT8                              Length;
  ///
  /// A variable length data buffer.
  ///
  UINT8                              Data[1];
} EFI_80211_SUBELEMENT_INFO;

///
/// EFI_80211_MULTIPLE_BSSID
///
typedef struct {
  ///
  /// Common header of an element.
  ///
  EFI_80211_ELEMENT_HEADER           Hdr;
  ///
  /// Indicates the maximum number of BSSIDs in the multiple BSSID set. When Indicator
  /// is set to n, 2n is the maximum number.
  ///
  UINT8                              Indicator;
  ///
  /// Contains zero or more sub-elements.
  ///
  EFI_80211_SUBELEMENT_INFO          SubElement[1];
} EFI_80211_MULTIPLE_BSSID;

///
/// EFI_80211_BSS_DESP_PILOT
///
typedef struct {
  ///
  /// Indicates a specific BSSID of the found BSS.
  ///
  EFI_80211_MAC_ADDRESS              BSSId;
  ///
  /// Specifies the type of the found BSS.
  ///
  EFI_80211_BSS_TYPE                 BSSType;
  ///
  /// One octet field to report condensed capability information.
  ///
  UINT8                              ConCapInfo;
  ///
  /// Two octet's field to report condensed country string.
  ///
  UINT8                              ConCountryStr[2];
  ///
  /// Indicates the operating class value for the operating channel.
  ///
  UINT8                              OperatingClass;
  ///
  /// Indicates the operating channel.
  ///
  UINT8                              Channel;
  ///
  /// Indicates the measurement pilot interval in TU.
  ///
  UINT8                              Interval;
  ///
  /// Indicates that the BSS is within a multiple BSSID set.
  ///
  EFI_80211_MULTIPLE_BSSID           *MultipleBSSID;
  ///
  /// Specifies the RCPI of the received frame.
  ///
  UINT8                              RCPIMeasurement;
  ///
  /// Specifies the RSNI of the received frame.
  ///
  UINT8                              RSNIMeasurement;
} EFI_80211_BSS_DESP_PILOT;

///
/// EFI_80211_SCAN_RESULT
///
typedef struct {
  ///
  /// The number of EFI_80211_BSS_DESCRIPTION in BSSDespSet. If zero, BSSDespSet should
  /// be ignored.
  ///
  UINTN                              NumOfBSSDesp;
  ///
  /// Points to zero or more instances of EFI_80211_BSS_DESCRIPTION.
  ///
  EFI_80211_BSS_DESCRIPTION          **BSSDespSet;
  ///
  /// The number of EFI_80211_BSS_DESP_PILOT in BSSDespFromPilotSet. If zero,
  /// BSSDespFromPilotSet should be ignored.
  ///
  UINTN                              NumofBSSDespFromPilot;
  ///
  /// Points to zero or more instances of EFI_80211_BSS_DESP_PILOT.
  ///
  EFI_80211_BSS_DESP_PILOT           **BSSDespFromPilotSet;
  ///
  /// Specifies zero or more elements. This is an optional parameter and may be NULL.
  ///
  UINT8                              *VendorSpecificInfo;
} EFI_80211_SCAN_RESULT;

///
/// EFI_80211_SCAN_DATA_TOKEN
///
typedef struct {
  ///
  /// This Event will be signaled after the Status field is updated by the EFI Wireless
  /// MAC Connection Protocol driver. The type of Event must be EFI_NOTIFY_SIGNAL.
  ///
  EFI_EVENT                          Event;
  ///
  /// Will be set to one of the following values:
  ///   EFI_SUCCESS:       Scan operation completed successfully.
  ///   EFI_NOT_FOUND:     Failed to find available BSS.
  ///   EFI_DEVICE_ERROR:  An unexpected network or system error occurred.
  ///   EFI_ACCESS_DENIED: The scan operation is not completed due to some underlying
  ///                      hardware or software state.
  ///   EFI_NOT_READY:     The scan operation is started but not yet completed.
  EFI_STATUS                         Status;
  ///
  /// Pointer to the scan data.
  ///
  EFI_80211_SCAN_DATA                *Data;
  ///
  /// Indicates the scan state.
  ///
  EFI_80211_SCAN_RESULT_CODE         ResultCode;
  ///
  /// Indicates the scan result. It is caller's responsibility to free this buffer.
  ///
  EFI_80211_SCAN_RESULT              *Result;
} EFI_80211_SCAN_DATA_TOKEN;

///
/// EFI_80211_ELEMENT_SUPP_CHANNEL_TUPLE
///
typedef struct {
  ///
  /// The first channel number in a subband of supported channels.
  ///
  UINT8                              FirstChannelNumber;
  ///
  /// The number of channels in a subband of supported channels.
  ///
  UINT8                              NumberOfChannels;
} EFI_80211_ELEMENT_SUPP_CHANNEL_TUPLE;

///
/// EFI_80211_ELEMENT_SUPP_CHANNEL
///
typedef struct {
  ///
  /// Common header of an element.
  ///
  EFI_80211_ELEMENT_HEADER                Hdr;
  ///
  /// Indicates one or more tuples of (first channel, number of channels).
  ///
  EFI_80211_ELEMENT_SUPP_CHANNEL_TUPLE    Subband[1];
} EFI_80211_ELEMENT_SUPP_CHANNEL;

///
/// EFI_80211_ASSOCIATE_DATA
///
typedef struct {
  ///
  /// Specifies the address of the peer MAC entity to associate with.
  ///
  EFI_80211_MAC_ADDRESS              BSSId;
  ///
  /// Specifies the requested operational capabilities to the AP in 2 octets.
  ///
  UINT16                             CapabilityInfo;
  ///
  /// Specifies a time limit in TU, after which the associate procedure is terminated.
  ///
  UINT32                             FailureTimeout;
  ///
  /// Specifies if in power save mode, how often the STA awakes and listens for the next
  /// beacon frame in TU.
  ///
  UINT32                             ListenInterval;
  ///
  /// Indicates a list of channels in which the STA is capable of operating.
  ///
  EFI_80211_ELEMENT_SUPP_CHANNEL     *Channels;
  ///
  /// The cipher suites and AKM suites selected by the STA.
  ///
  EFI_80211_ELEMENT_RSN              RSN;
  ///
  /// Specifies the parameters within the Extended Capabilities element that are
  /// supported by the MAC entity.  This is an optional parameter and may be NULL.
  ///
  EFI_80211_ELEMENT_EXT_CAP          *ExtCapElement;
  ///
  /// Specifies zero or more elements. This is an optional parameter and may be NULL.
  ///
  UINT8                              *VendorSpecificInfo;
} EFI_80211_ASSOCIATE_DATA;

///
/// EFI_80211_ELEMENT_TIMEOUT_VAL
///
typedef struct {
  ///
  /// Common header of an element.
  ///
  EFI_80211_ELEMENT_HEADER           Hdr;
  ///
  /// Specifies the timeout interval type.
  ///
  UINT8                              Type;
  ///
  /// Specifies the timeout interval value.
  ///
  UINT32                             Value;
} EFI_80211_ELEMENT_TIMEOUT_VAL;

///
/// EFI_80211_ASSOCIATE_RESULT
///
typedef struct {
  ///
  /// Specifies the address of the peer MAC entity from which the association request
  /// was received.
  ///
  EFI_80211_MAC_ADDRESS              BSSId;
  ///
  /// Specifies the operational capabilities advertised by the AP.
  ///
  UINT16                             CapabilityInfo;
  ///
  /// Specifies the association ID value assigned by the AP.
  ///
  UINT16                             AssociationID;
  ///
  /// Indicates the measured RCPI of the corresponding association request frame. It is
  /// an optional parameter and is set to zero if unavailable.
  ///
  UINT8                              RCPIValue;
  ///
  /// Indicates the measured RSNI at the time the corresponding association request
  /// frame was received. It is an optional parameter and is set to zero if unavailable.
  ///
  UINT8                              RSNIValue;
  ///
  /// Specifies the parameters within the Extended Capabilities element that are
  /// supported by the MAC entity.  This is an optional parameter and may be NULL.
  ///
  EFI_80211_ELEMENT_EXT_CAP          *ExtCapElement;
  ///
  /// Specifies the timeout interval when the result code is AssociateRefusedTemporarily.
  ///
  EFI_80211_ELEMENT_TIMEOUT_VAL      TimeoutInterval;
  ///
  /// Specifies zero or more elements. This is an optional parameter and may be NULL.
  ///
  UINT8                              *VendorSpecificInfo;
} EFI_80211_ASSOCIATE_RESULT;

///
/// EFI_80211_ASSOCIATE_DATA_TOKEN
///
typedef struct {
  ///
  /// This Event will be signaled after the Status field is updated by the EFI Wireless
  /// MAC Connection Protocol driver. The type of Event must be EFI_NOTIFY_SIGNAL.
  ///
  EFI_EVENT                          Event;
  ///
  /// Will be set to one of the following values:
  ///   EFI_SUCCESS:      Association operation completed successfully.
  ///   EFI_DEVICE_ERROR: An unexpected network or system error occurred.
  ///
  EFI_STATUS                         Status;
  ///
  /// Pointer to the association data.
  ///
  EFI_80211_ASSOCIATE_DATA           *Data;
  ///
  /// Indicates the association state.
  ///
  EFI_80211_ASSOCIATE_RESULT_CODE    ResultCode;
  ///
  /// Indicates the association result. It is caller's responsibility to free this
  /// buffer.
  ///
  EFI_80211_ASSOCIATE_RESULT         *Result;
} EFI_80211_ASSOCIATE_DATA_TOKEN;

///
/// EFI_80211_DISASSOCIATE_DATA
///
typedef struct {
  ///
  /// Specifies the address of the peer MAC entity with which to perform the
  /// disassociation process.
  ///
  EFI_80211_MAC_ADDRESS              BSSId;
  ///
  /// Specifies the reason for initiating the disassociation process.
  ///
  EFI_80211_REASON_CODE              ReasonCode;
  ///
  /// Zero or more elements, may be NULL.
  ///
  UINT8                              *VendorSpecificInfo;
} EFI_80211_DISASSOCIATE_DATA;

///
/// EFI_80211_DISASSOCIATE_DATA_TOKEN
///
typedef struct {
  ///
  /// This Event will be signaled after the Status field is updated by the EFI Wireless
  /// MAC Connection Protocol driver. The type of Event must be EFI_NOTIFY_SIGNAL.
  ///
  EFI_EVENT                          Event;
  ///
  /// Will be set to one of the following values:
  ///   EFI_SUCCESS:       Disassociation operation completed successfully.
  ///   EFI_DEVICE_ERROR:  An unexpected network or system error occurred.
  ///   EFI_ACCESS_DENIED: The disassociation operation is not completed due to some
  ///                      underlying hardware or software state.
  ///   EFI_NOT_READY:     The disassociation operation is started but not yet completed.
  ///
  EFI_STATUS                         Status;
  ///
  /// Pointer to the disassociation data.
  ///
  EFI_80211_DISASSOCIATE_DATA        *Data;
  ///
  /// Indicates the disassociation state.
  ///
  EFI_80211_DISASSOCIATE_RESULT_CODE ResultCode;
} EFI_80211_DISASSOCIATE_DATA_TOKEN;

///
/// EFI_80211_AUTHENTICATION_DATA
///
typedef struct {
  ///
  /// Specifies the address of the peer MAC entity with which to perform the
  /// authentication process.
  ///
  EFI_80211_MAC_ADDRESS              BSSId;
  ///
  /// Specifies the type of authentication algorithm to use during the authentication
  /// process.
  ///
  EFI_80211_AUTHENTICATION_TYPE      AuthType;
  ///
  /// Specifies a time limit in TU after which the authentication procedure is
  /// terminated.
  ///
  UINT32                             FailureTimeout;
  ///
  /// Specifies the set of elements to be included in the first message of the FT
  /// authentication sequence, may be NULL.
  ///
  UINT8                              *FTContent;
  ///
  /// Specifies the set of elements to be included in the SAE Commit Message or SAE
  /// Confirm Message, may be NULL.
  ///
  UINT8                              *SAEContent;
  ///
  /// Zero or more elements, may be NULL.
  ///
  UINT8                              *VendorSpecificInfo;
} EFI_80211_AUTHENTICATE_DATA;

///
/// EFI_80211_AUTHENTICATION_RESULT
///
typedef struct {
  ///
  /// Specifies the address of the peer MAC entity from which the authentication request
  /// was received.
  ///
  EFI_80211_MAC_ADDRESS              BSSId;
  ///
  /// Specifies the set of elements to be included in the second message of the FT
  /// authentication sequence, may be NULL.
  ///
  UINT8                              *FTContent;
  ///
  /// Specifies the set of elements to be included in the SAE Commit Message or SAE
  /// Confirm Message, may be NULL.
  ///
  UINT8                              *SAEContent;
  ///
  /// Zero or more elements, may be NULL.
  ///
  UINT8                              *VendorSpecificInfo;
} EFI_80211_AUTHENTICATE_RESULT;

///
/// EFI_80211_AUTHENTICATE_DATA_TOKEN
///
typedef struct {
  ///
  /// This Event will be signaled after the Status field is updated by the EFI Wireless
  /// MAC Connection Protocol driver. The type of Event must be EFI_NOTIFY_SIGNAL.
  ///
  EFI_EVENT                          Event;
  ///
  /// Will be set to one of the following values:
  ///   EFI_SUCCESS: Authentication operation completed successfully.
  ///   EFI_PROTOCOL_ERROR: Peer MAC entity rejects the authentication.
  ///   EFI_NO_RESPONSE:    Peer MAC entity does not response the authentication request.
  ///   EFI_DEVICE_ERROR:   An unexpected network or system error occurred.
  ///   EFI_ACCESS_DENIED:  The authentication operation is not completed due to some
  ///                       underlying hardware or software state.
  ///   EFI_NOT_READY:      The authentication operation is started but not yet completed.
  ///
  EFI_STATUS                         Status;
  ///
  /// Pointer to the authentication data.
  ///
  EFI_80211_AUTHENTICATE_DATA        *Data;
  ///
  /// Indicates the association state.
  ///
  EFI_80211_AUTHENTICATE_RESULT_CODE ResultCode;
  ///
  /// Indicates the association result. It is caller's responsibility to free this
  /// buffer.
  ///
  EFI_80211_AUTHENTICATE_RESULT      *Result;
} EFI_80211_AUTHENTICATE_DATA_TOKEN;

///
/// EFI_80211_DEAUTHENTICATE_DATA
///
typedef struct {
  ///
  /// Specifies the address of the peer MAC entity with which to perform the
  /// deauthentication process.
  ///
  EFI_80211_MAC_ADDRESS              BSSId;
  ///
  /// Specifies the reason for initiating the deauthentication process.
  ///
  EFI_80211_REASON_CODE              ReasonCode;
  ///
  /// Zero or more elements, may be NULL.
  ///
  UINT8                              *VendorSpecificInfo;
} EFI_80211_DEAUTHENTICATE_DATA;

///
/// EFI_80211_DEAUTHENTICATE_DATA_TOKEN
///
typedef struct {
  ///
  /// This Event will be signaled after the Status field is updated by the EFI Wireless
  /// MAC Connection Protocol driver. The type of Event must be EFI_NOTIFY_SIGNAL.
  ///
  EFI_EVENT                          Event;
  ///
  /// Will be set to one of the following values:
  ///   EFI_SUCCESS:       Deauthentication operation completed successfully.
  ///   EFI_DEVICE_ERROR:  An unexpected network or system error occurred.
  ///   EFI_ACCESS_DENIED: The deauthentication operation is not completed due to some
  ///                      underlying hardware or software state.
  ///   EFI_NOT_READY:     The deauthentication operation is started but not yet
  ///                      completed.
  ///
  EFI_STATUS                         Status;
  ///
  /// Pointer to the deauthentication data.
  ///
  EFI_80211_DEAUTHENTICATE_DATA      *Data;
} EFI_80211_DEAUTHENTICATE_DATA_TOKEN;

/**
  Request a survey of potential BSSs that administrator can later elect to try to join.

  The Scan() function returns the description of the set of BSSs detected by the scan
  process. Passive scan operation is performed by default.

  @param[in]  This                Pointer to the EFI_WIRELESS_MAC_CONNECTION_PROTOCOL
                                  instance.
  @param[in]  Data                Pointer to the scan token.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  Data is NULL.
                                  Data->Data is NULL.
  @retval EFI_UNSUPPORTED         One or more of the input parameters are not supported
                                  by this implementation.
  @retval EFI_ALREADY_STARTED     The scan operation is already started.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_WIRELESS_MAC_CONNECTION_SCAN)(
  IN EFI_WIRELESS_MAC_CONNECTION_PROTOCOL        *This,
  IN EFI_80211_SCAN_DATA_TOKEN                   *Data
  );

/**
  Request an association with a specified peer MAC entity that is within an AP.

  The Associate() function provides the capability for MAC layer to become associated
  with an AP.

  @param[in]  This                Pointer to the EFI_WIRELESS_MAC_CONNECTION_PROTOCOL
                                  instance.
  @param[in]  Data                Pointer to the association token.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  Data is NULL.
                                  Data->Data is NULL.
  @retval EFI_UNSUPPORTED         One or more of the input parameters are not supported
                                  by this implementation.
  @retval EFI_ALREADY_STARTED     The association process is already started.
  @retval EFI_NOT_READY           Authentication is not performed before this association
                                  process.
  @retval EFI_NOT_FOUND           The specified peer MAC entity is not found.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_WIRELESS_MAC_CONNECTION_ASSOCIATE)(
  IN EFI_WIRELESS_MAC_CONNECTION_PROTOCOL        *This,
  IN EFI_80211_ASSOCIATE_DATA_TOKEN              *Data
  );

/**
  Request a disassociation with a specified peer MAC entity.

  The Disassociate() function is invoked to terminate an existing association.
  Disassociation is a notification and cannot be refused by the receiving peer except
  when management frame protection is negotiated and the message integrity check fails.

  @param[in]  This                Pointer to the EFI_WIRELESS_MAC_CONNECTION_PROTOCOL
                                  instance.
  @param[in]  Data                Pointer to the disassociation token.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  Data is NULL.
  @retval EFI_ALREADY_STARTED     The disassociation process is already started.
  @retval EFI_NOT_READY           The disassociation service is invoked to a
                                  nonexistent association relationship.
  @retval EFI_NOT_FOUND           The specified peer MAC entity is not found.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_WIRELESS_MAC_CONNECTION_DISASSOCIATE)(
  IN EFI_WIRELESS_MAC_CONNECTION_PROTOCOL        *This,
  IN EFI_80211_DISASSOCIATE_DATA_TOKEN           *Data
  );

/**
  Request the process of establishing an authentication relationship with a peer MAC
  entity.

  The Authenticate() function requests authentication with a specified peer MAC entity.
  This service might be time-consuming thus is designed to be invoked independently of
  the association service.

  @param[in]  This                Pointer to the EFI_WIRELESS_MAC_CONNECTION_PROTOCOL
                                  instance.
  @param[in]  Data                Pointer to the authentication token.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  Data is NULL.
                                  Data.Data is NULL.
  @retval EFI_UNSUPPORTED         One or more of the input parameters are not supported
                                  by this implementation.
  @retval EFI_ALREADY_STARTED     The authentication process is already started.
  @retval EFI_NOT_FOUND           The specified peer MAC entity is not found.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_WIRELESS_MAC_CONNECTION_AUTHENTICATE)(
  IN EFI_WIRELESS_MAC_CONNECTION_PROTOCOL        *This,
  IN EFI_80211_AUTHENTICATE_DATA_TOKEN           *Data
  );

/**
  Invalidate the authentication relationship with a peer MAC entity.

  The Deauthenticate() function requests that the authentication relationship with a
  specified peer MAC entity be invalidated. Deauthentication is a notification and when
  it is sent out the association at the transmitting station is terminated.

  @param[in]  This                Pointer to the EFI_WIRELESS_MAC_CONNECTION_PROTOCOL
                                  instance.
  @param[in]  Data                Pointer to the deauthentication token.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  Data is NULL.
                                  Data.Data is NULL.
  @retval EFI_ALREADY_STARTED     The deauthentication process is already started.
  @retval EFI_NOT_READY           The deauthentication service is invoked to a
                                  nonexistent association or authentication relationship.
  @retval EFI_NOT_FOUND           The specified peer MAC entity is not found.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_WIRELESS_MAC_CONNECTION_DEAUTHENTICATE)(
  IN EFI_WIRELESS_MAC_CONNECTION_PROTOCOL        *This,
  IN EFI_80211_DEAUTHENTICATE_DATA_TOKEN         *Data
  );

///
/// The EFI_WIRELESS_MAC_CONNECTION_PROTOCOL is designed to provide management service
/// interfaces for the EFI wireless network stack to establish wireless connection with
/// AP. An EFI Wireless MAC Connection Protocol instance will be installed on each
/// communication device that the EFI wireless network stack runs on.
///
struct _EFI_WIRELESS_MAC_CONNECTION_PROTOCOL {
  EFI_WIRELESS_MAC_CONNECTION_SCAN               Scan;
  EFI_WIRELESS_MAC_CONNECTION_ASSOCIATE          Associate;
  EFI_WIRELESS_MAC_CONNECTION_DISASSOCIATE       Disassociate;
  EFI_WIRELESS_MAC_CONNECTION_AUTHENTICATE       Authenticate;
  EFI_WIRELESS_MAC_CONNECTION_DEAUTHENTICATE     Deauthenticate;
};

extern EFI_GUID gEfiWiFiProtocolGuid;

#endif
