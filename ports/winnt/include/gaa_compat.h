/*
 * ports/winnt/include/gaa_compat.h
 *
 * This header allows systems without a recent-enough SDK to build NTP
 * which can use GetAdaptersAddresses(), related functions and macros.
 */
#ifndef GAA_COMPAT_H
#define GAA_COMPAT_H

#ifdef _W64
# include <iphlpapi.h>
#else	/* !_W64 follows */

#pragma warning(push)
/* warning C4201: nonstandard extension used : nameless struct/union */
#pragma warning(disable:4201)
/* warning C4214: nonstandard extension used : bit field types other than int */
#pragma warning(disable:4214)

/* +++++++++++++++++++++++ from nldef.h */
typedef enum {
    //
    // These values are from iptypes.h.
    // They need to fit in a 4 bit field.
    //
    IpPrefixOriginOther = 0,
    IpPrefixOriginManual,
    IpPrefixOriginWellKnown,
    IpPrefixOriginDhcp,
    IpPrefixOriginRouterAdvertisement,
    IpPrefixOriginUnchanged = 1 << 4
} NL_PREFIX_ORIGIN;

typedef enum {
    //
    // These values are from in iptypes.h.
    // They need to fit in a 4 bit field.
    //
    IpSuffixOriginOther = 0,
    IpSuffixOriginManual,
    IpSuffixOriginWellKnown,
    IpSuffixOriginDhcp,
    IpSuffixOriginLinkLayerAddress,
    IpSuffixOriginRandom,
    IpSuffixOriginUnchanged = 1 << 4
} NL_SUFFIX_ORIGIN;

typedef enum {
    //
    // These values are from in iptypes.h.
    //
    IpDadStateInvalid    = 0,
    IpDadStateTentative,
    IpDadStateDuplicate,
    IpDadStateDeprecated,
    IpDadStatePreferred,
} NL_DAD_STATE;
/* +++++++++++++++++++++++ from nldef.h */


/* +++++++++++++++++++++++ from ifdef.h */
typedef ULONG32 NET_IF_OBJECT_ID, *PNET_IF_OBJECT_ID;


typedef enum _NET_IF_ADMIN_STATUS   // ifAdminStatus
{
    NET_IF_ADMIN_STATUS_UP = 1,
    NET_IF_ADMIN_STATUS_DOWN = 2,
    NET_IF_ADMIN_STATUS_TESTING = 3
} NET_IF_ADMIN_STATUS, *PNET_IF_ADMIN_STATUS;

typedef enum _NET_IF_OPER_STATUS   // ifOperStatus
{
    NET_IF_OPER_STATUS_UP = 1,
    NET_IF_OPER_STATUS_DOWN = 2,
    NET_IF_OPER_STATUS_TESTING = 3,
    NET_IF_OPER_STATUS_UNKNOWN = 4,
    NET_IF_OPER_STATUS_DORMANT = 5,
    NET_IF_OPER_STATUS_NOT_PRESENT = 6,
    NET_IF_OPER_STATUS_LOWER_LAYER_DOWN = 7
} NET_IF_OPER_STATUS, *PNET_IF_OPER_STATUS;

//
// Flags to extend operational status
//
#define NET_IF_OPER_STATUS_DOWN_NOT_AUTHENTICATED        0x00000001
#define NET_IF_OPER_STATUS_DOWN_NOT_MEDIA_CONNECTED      0x00000002
#define NET_IF_OPER_STATUS_DORMANT_PAUSED                0x00000004
#define NET_IF_OPER_STATUS_DORMANT_LOW_POWER             0x00000008

typedef UINT32 NET_IF_COMPARTMENT_ID, *PNET_IF_COMPARTMENT_ID;

//
// Define compartment ID type:
//
#define NET_IF_COMPARTMENT_ID_UNSPECIFIED   (NET_IF_COMPARTMENT_ID)0
#define NET_IF_COMPARTMENT_ID_PRIMARY       (NET_IF_COMPARTMENT_ID)1

#define NET_IF_OID_IF_ALIAS             0x00000001  // identifies the ifAlias string for an interface
#define NET_IF_OID_COMPARTMENT_ID       0x00000002  // identifies the compartment ID for an interface.
#define NET_IF_OID_NETWORK_GUID         0x00000003  // identifies the NetworkGuid for an interface.
#define NET_IF_OID_IF_ENTRY             0x00000004  // identifies statistics for an interface.

//
// Define NetworkGUID type:
//
typedef GUID NET_IF_NETWORK_GUID, *PNET_IF_NETWORK_GUID;

//
// Define macros for an "unspecified" NetworkGUID value to be used in structures
// that haven't had the NET_LUID field filled in yet.
//
#define NET_SET_UNSPECIFIED_NETWORK_GUID(_pNetworkGuid)
#define NET_IS_UNSPECIFIED_NETWORK_GUID(_NetworkGuidValue)

//
// To prevent collisions between user-assigned and system-assigend site-ids,
// we partition the site-id space into two:
// 1. User-Assigned: NET_SITEID_UNSPECIFIED < SiteId < NET_SITEID_MAXUSER
// 2. System-Assigned: NET_SITEID_MAXUSER < SiteId < NET_SITEID_MAXSYSTEM
//
// Note: A network's SiteId doesn't really need to be settable.
// 1. The network profile manager creates a network per network profile.
// 2. NDIS/IF assigns a unique SiteId to each network.
//
#define NET_SITEID_UNSPECIFIED (0)
#define NET_SITEID_MAXUSER (0x07ffffff)
#define NET_SITEID_MAXSYSTEM (0x0fffffff)
C_ASSERT(NET_SITEID_MAXUSER < NET_SITEID_MAXSYSTEM);

typedef enum _NET_IF_RCV_ADDRESS_TYPE // ifRcvAddressType
{
    NET_IF_RCV_ADDRESS_TYPE_OTHER = 1,
    NET_IF_RCV_ADDRESS_TYPE_VOLATILE = 2,
    NET_IF_RCV_ADDRESS_TYPE_NON_VOLATILE = 3
} NET_IF_RCV_ADDRESS_TYPE, *PNET_IF_RCV_ADDRESS_TYPE;

typedef struct _NET_IF_RCV_ADDRESS_LH
{
    NET_IF_RCV_ADDRESS_TYPE ifRcvAddressType;
    USHORT                  ifRcvAddressLength;
    USHORT                  ifRcvAddressOffset; // from beginning of this struct
} NET_IF_RCV_ADDRESS_LH, *PNET_IF_RCV_ADDRESS_LH;

typedef struct _NET_IF_ALIAS_LH
{
    USHORT  ifAliasLength;  // in bytes, of ifAlias string
    USHORT  ifAliasOffset;  // in bytes, from beginning of this struct
} NET_IF_ALIAS_LH, *PNET_IF_ALIAS_LH;

#pragma warning(push)
#pragma warning(disable:4214) // bit field types other than int
typedef union _NET_LUID_LH
{
    ULONG64     Value;
    struct
    {
        ULONG64     Reserved:24;
        ULONG64     NetLuidIndex:24;
        ULONG64     IfType:16;                  // equal to IANA IF type
    }Info;
} NET_LUID_LH, *PNET_LUID_LH;
#pragma warning(pop)

#if (NTDDI_VERSION >= NTDDI_LONGHORN)
typedef NET_IF_RCV_ADDRESS_LH NET_IF_RCV_ADDRESS;
typedef NET_IF_RCV_ADDRESS* PNET_IF_RCV_ADDRESS;

typedef NET_IF_ALIAS_LH NET_IF_ALIAS;
typedef NET_IF_ALIAS* PNET_IF_ALIAS;
#endif //NTDDI_LONGHORN

//
// Need to make this visible on all platforms (for the purpose of IF_LUID).
//
typedef NET_LUID_LH NET_LUID;
typedef NET_LUID* PNET_LUID;

//
// IF_LUID
//
// Define the locally unique datalink interface identifier type.
// This type is persistable.
//
typedef NET_LUID IF_LUID, *PIF_LUID;

typedef ULONG NET_IFINDEX, *PNET_IFINDEX;       // Interface Index (ifIndex)
typedef UINT16 NET_IFTYPE, *PNET_IFTYPE;        // Interface Type (IANA ifType)

#define NET_IFINDEX_UNSPECIFIED (NET_IFINDEX)(0)    // Not a valid ifIndex
#define NET_IFLUID_UNSPECIFIED (0)    // Not a valid if Luid

//
// Definitions for NET_IF_INFORMATION.Flags
//
#define NIIF_HARDWARE_INTERFACE         0x00000001  // Set iff hardware
#define NIIF_FILTER_INTERFACE           0x00000002
#define NIIF_NDIS_RESERVED1             0x00000004
#define NIIF_NDIS_RESERVED2             0x00000008
#define NIIF_NDIS_RESERVED3             0x00000010
#define NIIF_NDIS_WDM_INTERFACE         0x00000020
#define NIIF_NDIS_ENDPOINT_INTERFACE    0x00000040

#define NIIF_WAN_TUNNEL_TYPE_UNKNOWN    ((ULONG)(-1))

//
// Define datalink interface access types.
//
typedef enum _NET_IF_ACCESS_TYPE
{
    NET_IF_ACCESS_LOOPBACK = 1,
    NET_IF_ACCESS_BROADCAST = 2,
    NET_IF_ACCESS_POINT_TO_POINT = 3,
    NET_IF_ACCESS_POINT_TO_MULTI_POINT = 4,
    NET_IF_ACCESS_MAXIMUM = 5
} NET_IF_ACCESS_TYPE, *PNET_IF_ACCESS_TYPE;


//
// Define datalink interface direction types.
//
typedef enum _NET_IF_DIRECTION_TYPE
{
    NET_IF_DIRECTION_SENDRECEIVE,
    NET_IF_DIRECTION_SENDONLY,
    NET_IF_DIRECTION_RECEIVEONLY,
    NET_IF_DIRECTION_MAXIMUM
} NET_IF_DIRECTION_TYPE, *PNET_IF_DIRECTION_TYPE;


typedef enum _NET_IF_CONNECTION_TYPE
{
   NET_IF_CONNECTION_DEDICATED = 1,
   NET_IF_CONNECTION_PASSIVE = 2,
   NET_IF_CONNECTION_DEMAND = 3,
   NET_IF_CONNECTION_MAXIMUM = 4
} NET_IF_CONNECTION_TYPE, *PNET_IF_CONNECTION_TYPE;


typedef enum _NET_IF_MEDIA_CONNECT_STATE
{
    MediaConnectStateUnknown,
    MediaConnectStateConnected,
    MediaConnectStateDisconnected
} NET_IF_MEDIA_CONNECT_STATE, *PNET_IF_MEDIA_CONNECT_STATE;

#define NET_IF_LINK_SPEED_UNKNOWN   ((ULONG64)(-1))

//
// Defines the duplex state of media
//
typedef enum _NET_IF_MEDIA_DUPLEX_STATE
{
    MediaDuplexStateUnknown,
    MediaDuplexStateHalf,
    MediaDuplexStateFull
} NET_IF_MEDIA_DUPLEX_STATE, *PNET_IF_MEDIA_DUPLEX_STATE;


// Special values for fields in NET_PHYSICAL_LOCATION
#define NIIF_BUS_NUMBER_UNKNOWN         ((ULONG)(-1))
#define NIIF_SLOT_NUMBER_UNKNOWN        ((ULONG)(-1))
#define NIIF_FUNCTION_NUMBER_UNKNOWN    ((ULONG)(-1))

typedef struct _NET_PHYSICAL_LOCATION_LH
{
    ULONG                   BusNumber;          // Physical location
    ULONG                   SlotNumber;         // ... for hardware
    ULONG                   FunctionNumber;     // ... devices.
} NET_PHYSICAL_LOCATION_LH, *PNET_PHYSICAL_LOCATION_LH;

//
// maximum string size in -wchar- units
//
#define IF_MAX_STRING_SIZE 256

typedef struct _IF_COUNTED_STRING_LH
{
    USHORT      Length; // in -Bytes-
    WCHAR       String[IF_MAX_STRING_SIZE + 1];
} IF_COUNTED_STRING_LH, *PIF_COUNTED_STRING_LH;

#define IF_MAX_PHYS_ADDRESS_LENGTH 32

typedef struct _IF_PHYSICAL_ADDRESS_LH
{
    USHORT      Length;
    UCHAR       Address[IF_MAX_PHYS_ADDRESS_LENGTH];
} IF_PHYSICAL_ADDRESS_LH, *PIF_PHYSICAL_ADDRESS_LH;

#if (NTDDI_VERSION >= NTDDI_LONGHORN)
typedef NET_PHYSICAL_LOCATION_LH NET_PHYSICAL_LOCATION;
typedef NET_PHYSICAL_LOCATION* PNET_PHYSICAL_LOCATION;

typedef IF_COUNTED_STRING_LH IF_COUNTED_STRING;
typedef IF_COUNTED_STRING* PIF_COUNTED_STRING;

typedef IF_PHYSICAL_ADDRESS_LH IF_PHYSICAL_ADDRESS;
typedef IF_PHYSICAL_ADDRESS* PIF_PHYSICAL_ADDRESS;
#endif


//
// IF_INDEX
//
// Define the interface index type.
// This type is not persistable.
// This must be unsigned (not an enum) to replace previous uses of
// an index that used a DWORD type.
//

typedef NET_IFINDEX IF_INDEX, *PIF_INDEX;
#define IFI_UNSPECIFIED NET_IFINDEX_UNSPECIFIED


//
// Get definitions for IFTYPE and IF_ACCESS_TYPE.
//
#include <ipifcons.h>


//
// Types of tunnels (sub-type of IF_TYPE when IF_TYPE is IF_TYPE_TUNNEL).
// See http://www.iana.org/assignments/ianaiftype-mib.
//
typedef enum {
    TUNNEL_TYPE_NONE = 0,
    TUNNEL_TYPE_OTHER = 1,
    TUNNEL_TYPE_DIRECT = 2,
    TUNNEL_TYPE_6TO4 = 11,
    TUNNEL_TYPE_ISATAP = 13,
    TUNNEL_TYPE_TEREDO = 14,
} TUNNEL_TYPE, *PTUNNEL_TYPE;

//
// IF_ADMINISTRATIVE_STATE
//
// Datalink Interface Administrative State.
// Indicates whether the interface has been administratively enabled.
//

typedef enum _IF_ADMINISTRATIVE_STATE {
    IF_ADMINISTRATIVE_DISABLED,
    IF_ADMINISTRATIVE_ENABLED,
    IF_ADMINISTRATIVE_DEMANDDIAL,
} IF_ADMINISTRATIVE_STATE, *PIF_ADMINISTRATIVE_STATE;


//
// Note: Interface is Operational iff
// ((MediaSense is Connected) and (AdministrativeState is Enabled))
// or
// ((MediaSense is Connected) and (AdministrativeState is OnDemand))
//
// !Operational iff
// ((MediaSense != Connected) or (AdministrativeState is Disabled))
//

//
// OperStatus values from RFC 2863
//
typedef enum {
    IfOperStatusUp = 1,
    IfOperStatusDown,
    IfOperStatusTesting,
    IfOperStatusUnknown,
    IfOperStatusDormant,
    IfOperStatusNotPresent,
    IfOperStatusLowerLayerDown
} IF_OPER_STATUS;
/* +++++++++++++++++++++++ from ifdef.h */


/* +++++++++++++++++++++++ from iptypes.h */
// Definitions and structures used by getnetworkparams and getadaptersinfo apis

#define MAX_ADAPTER_DESCRIPTION_LENGTH  128 // arb.
#define MAX_ADAPTER_NAME_LENGTH         256 // arb.
#define MAX_ADAPTER_ADDRESS_LENGTH      8   // arb.
#define DEFAULT_MINIMUM_ENTITIES        32  // arb.
#define MAX_HOSTNAME_LEN                128 // arb.
#define MAX_DOMAIN_NAME_LEN             128 // arb.
#define MAX_SCOPE_ID_LEN                256 // arb.
#define MAX_DHCPV6_DUID_LENGTH          130 // RFC 3315.

//
// types
//

// Node Type

#define BROADCAST_NODETYPE              1
#define PEER_TO_PEER_NODETYPE           2
#define MIXED_NODETYPE                  4
#define HYBRID_NODETYPE                 8

//
// IP_ADDRESS_STRING - store an IP address as a dotted decimal string
//

typedef struct {
    char String[4 * 4];
} IP_ADDRESS_STRING, *PIP_ADDRESS_STRING, IP_MASK_STRING, *PIP_MASK_STRING;

//
// IP_ADDR_STRING - store an IP address with its corresponding subnet mask,
// both as dotted decimal strings
//

typedef struct _IP_ADDR_STRING {
    struct _IP_ADDR_STRING* Next;
    IP_ADDRESS_STRING IpAddress;
    IP_MASK_STRING IpMask;
    DWORD Context;
} IP_ADDR_STRING, *PIP_ADDR_STRING;

//
// ADAPTER_INFO - per-adapter information. All IP addresses are stored as
// strings
//

typedef struct _IP_ADAPTER_INFO {
    struct _IP_ADAPTER_INFO* Next;
    DWORD ComboIndex;
    char AdapterName[MAX_ADAPTER_NAME_LENGTH + 4];
    char Description[MAX_ADAPTER_DESCRIPTION_LENGTH + 4];
    UINT AddressLength;
    BYTE Address[MAX_ADAPTER_ADDRESS_LENGTH];
    DWORD Index;
    UINT Type;
    UINT DhcpEnabled;
    PIP_ADDR_STRING CurrentIpAddress;
    IP_ADDR_STRING IpAddressList;
    IP_ADDR_STRING GatewayList;
    IP_ADDR_STRING DhcpServer;
    BOOL HaveWins;
    IP_ADDR_STRING PrimaryWinsServer;
    IP_ADDR_STRING SecondaryWinsServer;
    time_t LeaseObtained;
    time_t LeaseExpires;
} IP_ADAPTER_INFO, *PIP_ADAPTER_INFO;

#ifdef _WINSOCK2API_

//
// The following types require Winsock2.
//

typedef NL_PREFIX_ORIGIN IP_PREFIX_ORIGIN;
typedef NL_SUFFIX_ORIGIN IP_SUFFIX_ORIGIN;
typedef NL_DAD_STATE IP_DAD_STATE;

typedef struct _IP_ADAPTER_UNICAST_ADDRESS_LH {
    union {
        ULONGLONG Alignment;
        struct { 
            ULONG Length;
            DWORD Flags;
        };
    };
    struct _IP_ADAPTER_UNICAST_ADDRESS_LH *Next;
    SOCKET_ADDRESS Address;

    IP_PREFIX_ORIGIN PrefixOrigin;
    IP_SUFFIX_ORIGIN SuffixOrigin;
    IP_DAD_STATE DadState;

    ULONG ValidLifetime;
    ULONG PreferredLifetime;
    ULONG LeaseLifetime;
    UINT8 OnLinkPrefixLength;
} IP_ADAPTER_UNICAST_ADDRESS_LH,
 *PIP_ADAPTER_UNICAST_ADDRESS_LH;

typedef struct _IP_ADAPTER_UNICAST_ADDRESS_XP {
    union {
        ULONGLONG Alignment;
        struct { 
            ULONG Length;
            DWORD Flags;
        };
    };
    struct _IP_ADAPTER_UNICAST_ADDRESS_XP *Next;
    SOCKET_ADDRESS Address;

    IP_PREFIX_ORIGIN PrefixOrigin;
    IP_SUFFIX_ORIGIN SuffixOrigin;
    IP_DAD_STATE DadState;

    ULONG ValidLifetime;
    ULONG PreferredLifetime;
    ULONG LeaseLifetime;
} IP_ADAPTER_UNICAST_ADDRESS_XP, *PIP_ADAPTER_UNICAST_ADDRESS_XP;

#if (NTDDI_VERSION >= NTDDI_LONGHORN)
typedef  IP_ADAPTER_UNICAST_ADDRESS_LH IP_ADAPTER_UNICAST_ADDRESS;
typedef  IP_ADAPTER_UNICAST_ADDRESS_LH *PIP_ADAPTER_UNICAST_ADDRESS;
#elif (NTDDI_VERSION >= NTDDI_WINXP)
typedef  IP_ADAPTER_UNICAST_ADDRESS_XP IP_ADAPTER_UNICAST_ADDRESS;
typedef  IP_ADAPTER_UNICAST_ADDRESS_XP *PIP_ADAPTER_UNICAST_ADDRESS;
#endif

//
// Bit values of IP_ADAPTER_UNICAST_ADDRESS Flags field.
//
#define IP_ADAPTER_ADDRESS_DNS_ELIGIBLE 0x01
#define IP_ADAPTER_ADDRESS_TRANSIENT    0x02

typedef struct _IP_ADAPTER_ANYCAST_ADDRESS_XP {
    union {
        ULONGLONG Alignment;
        struct { 
            ULONG Length;
            DWORD Flags;
        };
    };
    struct _IP_ADAPTER_ANYCAST_ADDRESS_XP *Next;
    SOCKET_ADDRESS Address;
} IP_ADAPTER_ANYCAST_ADDRESS_XP, *PIP_ADAPTER_ANYCAST_ADDRESS_XP;
#if (NTDDI_VERSION >= NTDDI_WINXP)
typedef IP_ADAPTER_ANYCAST_ADDRESS_XP IP_ADAPTER_ANYCAST_ADDRESS;
typedef IP_ADAPTER_ANYCAST_ADDRESS_XP *PIP_ADAPTER_ANYCAST_ADDRESS;
#endif

typedef struct _IP_ADAPTER_MULTICAST_ADDRESS_XP {
    union {
        ULONGLONG Alignment;
        struct {
            ULONG Length;
            DWORD Flags;
        };
    };
    struct _IP_ADAPTER_MULTICAST_ADDRESS_XP *Next;
    SOCKET_ADDRESS Address;
} IP_ADAPTER_MULTICAST_ADDRESS_XP, *PIP_ADAPTER_MULTICAST_ADDRESS_XP;
#if (NTDDI_VERSION >= NTDDI_WINXP)
typedef IP_ADAPTER_MULTICAST_ADDRESS_XP IP_ADAPTER_MULTICAST_ADDRESS;
typedef IP_ADAPTER_MULTICAST_ADDRESS_XP *PIP_ADAPTER_MULTICAST_ADDRESS;
#endif

typedef struct _IP_ADAPTER_DNS_SERVER_ADDRESS_XP {
    union {
        ULONGLONG Alignment;
        struct {
            ULONG Length;
            DWORD Reserved;
        };
    };
    struct _IP_ADAPTER_DNS_SERVER_ADDRESS_XP *Next;
    SOCKET_ADDRESS Address;
} IP_ADAPTER_DNS_SERVER_ADDRESS_XP, *PIP_ADAPTER_DNS_SERVER_ADDRESS_XP;
#if (NTDDI_VERSION >= NTDDI_WINXP)
typedef IP_ADAPTER_DNS_SERVER_ADDRESS_XP IP_ADAPTER_DNS_SERVER_ADDRESS;
typedef IP_ADAPTER_DNS_SERVER_ADDRESS_XP *PIP_ADAPTER_DNS_SERVER_ADDRESS;
#endif

typedef struct _IP_ADAPTER_WINS_SERVER_ADDRESS_LH {
    union {
        ULONGLONG Alignment;
        struct {
            ULONG Length;
            DWORD Reserved;
        };
    };
    struct _IP_ADAPTER_WINS_SERVER_ADDRESS_LH *Next;
    SOCKET_ADDRESS Address;
} IP_ADAPTER_WINS_SERVER_ADDRESS_LH, *PIP_ADAPTER_WINS_SERVER_ADDRESS_LH;
#if (NTDDI_VERSION >= NTDDI_LONGHORN)
typedef IP_ADAPTER_WINS_SERVER_ADDRESS_LH IP_ADAPTER_WINS_SERVER_ADDRESS;
typedef IP_ADAPTER_WINS_SERVER_ADDRESS_LH *PIP_ADAPTER_WINS_SERVER_ADDRESS;
#endif


typedef struct _IP_ADAPTER_GATEWAY_ADDRESS_LH {
    union {
        ULONGLONG Alignment;
        struct {
            ULONG Length;
            DWORD Reserved;
        };
    };
    struct _IP_ADAPTER_GATEWAY_ADDRESS_LH *Next;
    SOCKET_ADDRESS Address;
} IP_ADAPTER_GATEWAY_ADDRESS_LH, *PIP_ADAPTER_GATEWAY_ADDRESS_LH;
#if (NTDDI_VERSION >= NTDDI_LONGHORN)
typedef IP_ADAPTER_GATEWAY_ADDRESS_LH IP_ADAPTER_GATEWAY_ADDRESS;
typedef IP_ADAPTER_GATEWAY_ADDRESS_LH *PIP_ADAPTER_GATEWAY_ADDRESS;
#endif

typedef struct _IP_ADAPTER_PREFIX_XP {
    union {
        ULONGLONG Alignment;
        struct {
            ULONG Length;
            DWORD Flags;
        };
    };
    struct _IP_ADAPTER_PREFIX_XP *Next;
    SOCKET_ADDRESS Address;
    ULONG PrefixLength;
} IP_ADAPTER_PREFIX_XP, *PIP_ADAPTER_PREFIX_XP;
#if (NTDDI_VERSION >= NTDDI_WINXP)
typedef IP_ADAPTER_PREFIX_XP IP_ADAPTER_PREFIX;
typedef IP_ADAPTER_PREFIX_XP *PIP_ADAPTER_PREFIX;
#endif

//
// Bit values of IP_ADAPTER_ADDRESSES Flags field.
//
#define IP_ADAPTER_DDNS_ENABLED               0x00000001
#define IP_ADAPTER_REGISTER_ADAPTER_SUFFIX    0x00000002
#define IP_ADAPTER_DHCP_ENABLED               0x00000004
#define IP_ADAPTER_RECEIVE_ONLY               0x00000008
#define IP_ADAPTER_NO_MULTICAST               0x00000010
#define IP_ADAPTER_IPV6_OTHER_STATEFUL_CONFIG 0x00000020
#define IP_ADAPTER_NETBIOS_OVER_TCPIP_ENABLED 0x00000040
#define IP_ADAPTER_IPV4_ENABLED               0x00000080
#define IP_ADAPTER_IPV6_ENABLED               0x00000100

typedef struct _IP_ADAPTER_ADDRESSES_LH {
    union {
        ULONGLONG Alignment;
        struct {
            ULONG Length;
            IF_INDEX IfIndex;
        };
    };
    struct _IP_ADAPTER_ADDRESSES_LH *Next;
    PCHAR AdapterName;
    PIP_ADAPTER_UNICAST_ADDRESS_LH FirstUnicastAddress;
    PIP_ADAPTER_ANYCAST_ADDRESS_XP FirstAnycastAddress;
    PIP_ADAPTER_MULTICAST_ADDRESS_XP FirstMulticastAddress;
    PIP_ADAPTER_DNS_SERVER_ADDRESS_XP FirstDnsServerAddress;
    PWCHAR DnsSuffix;
    PWCHAR Description;
    PWCHAR FriendlyName;
    BYTE PhysicalAddress[MAX_ADAPTER_ADDRESS_LENGTH];
    ULONG PhysicalAddressLength;
    union {
        ULONG Flags;
        struct {
            ULONG DdnsEnabled : 1;
            ULONG RegisterAdapterSuffix : 1;
            ULONG Dhcpv4Enabled : 1;
            ULONG ReceiveOnly : 1;
            ULONG NoMulticast : 1;
            ULONG Ipv6OtherStatefulConfig : 1;
            ULONG NetbiosOverTcpipEnabled : 1;
            ULONG Ipv4Enabled : 1;
            ULONG Ipv6Enabled : 1;
            ULONG Ipv6ManagedAddressConfigurationSupported : 1;
        };
    };
    ULONG Mtu;
    IFTYPE IfType;
    IF_OPER_STATUS OperStatus;
    IF_INDEX Ipv6IfIndex;
    ULONG ZoneIndices[16];
    PIP_ADAPTER_PREFIX_XP FirstPrefix;

    ULONG64 TransmitLinkSpeed;
    ULONG64 ReceiveLinkSpeed;
    PIP_ADAPTER_WINS_SERVER_ADDRESS_LH FirstWinsServerAddress;
    PIP_ADAPTER_GATEWAY_ADDRESS_LH FirstGatewayAddress;
    ULONG Ipv4Metric;
    ULONG Ipv6Metric;
    IF_LUID Luid;
    SOCKET_ADDRESS Dhcpv4Server;
    NET_IF_COMPARTMENT_ID CompartmentId;
    NET_IF_NETWORK_GUID NetworkGuid;
    NET_IF_CONNECTION_TYPE ConnectionType;    
    TUNNEL_TYPE TunnelType;
    //
    // DHCP v6 Info.
    //
    SOCKET_ADDRESS Dhcpv6Server;
    BYTE Dhcpv6ClientDuid[MAX_DHCPV6_DUID_LENGTH];
    ULONG Dhcpv6ClientDuidLength;
    ULONG Dhcpv6Iaid;
} IP_ADAPTER_ADDRESSES_LH, 
 *PIP_ADAPTER_ADDRESSES_LH;

typedef struct _IP_ADAPTER_ADDRESSES_XP {
    union {
        ULONGLONG Alignment;
        struct {
            ULONG Length;
            DWORD IfIndex;
        };
    };
    struct _IP_ADAPTER_ADDRESSES_XP *Next;
    PCHAR AdapterName;
    PIP_ADAPTER_UNICAST_ADDRESS_XP FirstUnicastAddress;
    PIP_ADAPTER_ANYCAST_ADDRESS_XP FirstAnycastAddress;
    PIP_ADAPTER_MULTICAST_ADDRESS_XP FirstMulticastAddress;
    PIP_ADAPTER_DNS_SERVER_ADDRESS_XP FirstDnsServerAddress;
    PWCHAR DnsSuffix;
    PWCHAR Description;
    PWCHAR FriendlyName;
    BYTE PhysicalAddress[MAX_ADAPTER_ADDRESS_LENGTH];
    DWORD PhysicalAddressLength;
    DWORD Flags;
    DWORD Mtu;
    DWORD IfType;
    IF_OPER_STATUS OperStatus;
    DWORD Ipv6IfIndex;
    DWORD ZoneIndices[16];
    PIP_ADAPTER_PREFIX_XP FirstPrefix;
} IP_ADAPTER_ADDRESSES_XP,
 *PIP_ADAPTER_ADDRESSES_XP;

#if (NTDDI_VERSION >= NTDDI_LONGHORN)
typedef  IP_ADAPTER_ADDRESSES_LH IP_ADAPTER_ADDRESSES;
typedef  IP_ADAPTER_ADDRESSES_LH *PIP_ADAPTER_ADDRESSES;
#elif (NTDDI_VERSION >= NTDDI_WINXP)
typedef  IP_ADAPTER_ADDRESSES_XP IP_ADAPTER_ADDRESSES;
typedef  IP_ADAPTER_ADDRESSES_XP *PIP_ADAPTER_ADDRESSES;
#else
//
// For platforms other platforms that are including
// the file but not using the types.
//
typedef  IP_ADAPTER_ADDRESSES_XP IP_ADAPTER_ADDRESSES;
typedef  IP_ADAPTER_ADDRESSES_XP *PIP_ADAPTER_ADDRESSES;
#endif


//
// Flags used as argument to GetAdaptersAddresses().
// "SKIP" flags are added when the default is to include the information.
// "INCLUDE" flags are added when the default is to skip the information.
//
#define GAA_FLAG_SKIP_UNICAST                   0x0001
#define GAA_FLAG_SKIP_ANYCAST                   0x0002
#define GAA_FLAG_SKIP_MULTICAST                 0x0004
#define GAA_FLAG_SKIP_DNS_SERVER                0x0008
#define GAA_FLAG_INCLUDE_PREFIX                 0x0010
#define GAA_FLAG_SKIP_FRIENDLY_NAME             0x0020
#define GAA_FLAG_INCLUDE_WINS_INFO              0x0040
#define GAA_FLAG_INCLUDE_GATEWAYS               0x0080
#define GAA_FLAG_INCLUDE_ALL_INTERFACES         0x0100
#define GAA_FLAG_INCLUDE_ALL_COMPARTMENTS       0x0200
#define GAA_FLAG_INCLUDE_TUNNEL_BINDINGORDER    0x0400

#endif /* _WINSOCK2API_ */
/* +++++++++++++++++++++++ from iptypes.h */


/* +++++++++++++++++++++++ from iphlpapi.h */
#ifdef _WINSOCK2API_

//
// The following functions require Winsock2.
//

ULONG
WINAPI
GetAdaptersAddresses(
    IN ULONG Family,
    IN ULONG Flags,
    IN PVOID Reserved,
    __out_bcount_opt(*SizePointer) PIP_ADAPTER_ADDRESSES AdapterAddresses, 
    IN OUT PULONG SizePointer
    );

#endif
/* +++++++++++++++++++++++ from iphlpapi.h */

#pragma warning(pop)
#endif	/* !_W64 */
#endif	/* GAA_COMPAT_H */
