hostapd, wpa_supplicant and the Multi-AP Specification
======================================================

This document describes how hostapd and wpa_supplicant can be configured to
support the Multi-AP Specification.

Introduction to Multi-AP
------------------------

The Wi-Fi Alliance Multi-AP Specification is the technical specification for
Wi-Fi CERTIFIED EasyMesh(TM) [1], the Wi-Fi Alliance® certification program for
Multi-AP. It defines control protocols between Wi-Fi® access points (APs) to
join them into a network with centralized control and operation. It is targeted
only at routers (repeaters, gateways, ...), not at clients. Clients are not
involved at all in the protocols.

Most of the Multi-AP specification falls outside of the scope of
hostapd/wpa_supplicant. hostapd/wpa_supplicant is only involved for the items
summarized below. The rest of the protocol must be implemented by a separate
daemon, e.g., prplMesh [2]. That daemon also needs to communicate with hostapd,
e.g., to get a list of associated clients, but this can be done using the normal
hostapd interfaces.

hostapd/wpa_supplicant needs to be configured specifically to support:
- the WPS onboarding process;
- configuring backhaul links.

The text below refers to "Multi-AP Specification v1.0" [3].


Fronthaul and backhaul links
----------------------------

In a Multi-AP network, the central controller can configure the BSSs on the
devices that are joined into the network. These are called fronthaul BSSs.
From the point of view of hostapd, there is nothing special about these
fronthaul BSSs.

In addition to fronthaul BSSs, the controller can also configure backhaul
links. A backhaul link is a link between two access point devices, giving
internet access to access point devices that don't have a wired link. The
Multi-AP specification doesn't dictate this, but typically the backhaul link
will be bridged into a LAN together with (one of) the fronthaul BSS(s) and the
wired Ethernet ports.

A backhaul link must be treated specially by hostapd and wpa_supplicant. One
side of the backhaul link is configured through the Multi-AP protocol as the
"backhaul STA", i.e., the client side of the link. A backhaul STA is like any
station and is handled appropriately by wpa_supplicant, but two additional
features are required. It must send an additional information element in each
(Re)Association Request frame ([3], section 5.2, paragraph 4). In addition, it
must use 4-address mode for all frames sent over this link ([3], section 14).
Therefore, wpa_supplicant must be configured explicitly as the backhaul STA
role, by setting 'multi_ap_backhaul_sta=1' in the network configuration block
or when configuring the network profile through the control interface. When
'multi_ap_backhaul_sta=1', wpa_supplicant includes the Multi-AP IE in
(Re)Association Request frame and verifies that it is included in the
(Re)Association Response frame. If it is not, association fails. If it is,
wpa_supplicant sets 4-address mode for this interface through a driver
callback.

The AP side of the backhaul link is called a "backhaul BSS". Such a BSS must
be handled specially by hostapd, because it must add an additional information
element in each (Re)Association Response frame, but only to stations that have
identified themselves as backhaul stations ([3], section 5.2, paragraph 5-6).
This is important because it is possible to use the same BSS and SSID for
fronthaul and backhaul at the same time. The additional information element must
only be used for frames sent to a backhaul STA, not to a normal STA. Also,
frames sent to a backhaul STA must use 4-address mode, while frames sent to a
normal STA (fronthaul, when it's a fronthaul and backhaul BSS) must use
3-address mode.

A BSS is configured in Multi-AP mode in hostapd by setting the 'multi_ap'
configuration option to 1 (backhaul BSS), 2 (fronthaul BSS), or 3
(simultaneous backhaul and fronthaul BSS). If this option is set, hostapd
parses the Multi-AP information element in the Association Request frame. If the
station is a backhaul STA and the BSS is configured as a backhaul BSS,
hostapd sets up 4-address mode. Since there may be multiple stations connected
simultaneously, and each of them has a different RA (receiver address), a VLAN
is created for each backhaul STA and it is automatically added to a bridge.
This is the same behavior as for WDS, and the relevant option ('bridge' or
'wds_bridge') applies here as well.

If 'multi_ap' is 1 (backhaul BSS only), any station that tries to associate
without the Multi-AP information element will be denied.

If 'multi_ap' is 2 (fronthaul BSS only), any station that tries to associate
with the Multi-AP information element will be denied. That is also the only
difference with 'multi_ap' set to 0: in the latter case, the Multi-AP
information element is simply ignored.

In summary, this is the end-to-end behavior for a backhaul BSS (i.e.,
multi_ap_backhaul_sta=1 in wpa_supplicant on STA, and multi_ap=1 or 3 in
hostapd on AP). Note that point 1 means that hostapd must not be configured
with WPS support on the backhaul BSS (multi_ap=1). hostapd does not check for
that.

1. Backhaul BSS beacons do not advertise WPS support (other than that, nothing
   Multi-AP specific).
2. STA sends Authentication frame (nothing Multi-AP specific).
3. AP sends Authentication frame (nothing Multi-AP specific).
4. STA sends Association Request frame with Multi-AP IE.
5. AP sends Association Response frame with Multi-AP IE.
6. STA and AP both use 4-address mode for Data frames.


WPS support
-----------

WPS requires more special handling. WPS must only be advertised on fronthaul
BSSs, not on backhaul BSSs, so WPS should not be enabled on a backhaul-only
BSS in hostapd.conf. The WPS configuration purely works on the fronthaul BSS.
When a WPS M1 message has an additional subelement that indicates a request for
a Multi-AP backhaul link, hostapd must not respond with the normal fronthaul
BSS credentials; instead, it should respond with the (potentially different)
backhaul BSS credentials.

To support this, hostapd has the 'multi_ap_backhaul_ssid',
'multi_ap_backhaul_wpa_psk' and 'multi_ap_backhaul_wpa_passphrase' options.
When these are set on an BSS with WPS, they are used instead of the normal
credentials when hostapd receives a WPS M1 message with the Multi-AP IE. Only
WPA2-Personal is supported in the Multi-AP specification, so there is no need
to specify authentication or encryption options. For the backhaul credentials,
per-device PSK is not supported.

If the BSS is a simultaneous backhaul and fronthaul BSS, there is no need to
specify the backhaul credentials, since the backhaul and fronthaul credentials
are identical.

To enable the Multi-AP backhaul STA feature when it performs WPS, a new
parameter has been introduced to the WPS_PBC control interface call. When this
"multi_ap=1" option is set, it adds the Multi-AP backhaul subelement to the
Association Request frame and the M1 message. It then configures the new network
profile with 'multi_ap_backhaul_sta=1'. Note that this means that if the AP does
not follow the Multi-AP specification, wpa_supplicant will fail to associate.

In summary, this is the end-to-end behavior for WPS of a backhaul link (i.e.,
multi_ap=1 option is given in the wps_pbc call on the STA side, and multi_ap=2
and multi_ap_backhaul_ssid and either multi_ap_backhaul_wpa_psk or
multi_ap_backhaul_wpa_passphrase are set to the credentials of a backhaul BSS
in hostapd on Registrar AP).

1. Fronthaul BSS Beacon frames advertise WPS support (nothing Multi-AP
   specific).
2. Enrollee sends Authentication frame (nothing Multi-AP specific).
3. AP sends Authentication frame (nothing Multi-AP specific).
4. Enrollee sends Association Request frame with Multi-AP IE.
5. AP sends Association Response frame with Multi-AP IE.
6. Enrollee sends M1 with additional Multi-AP subelement.
7. AP sends M8 with backhaul instead of fronthaul credentials.
8. Enrollee sends Deauthentication frame.


References
----------

[1] https://www.wi-fi.org/discover-wi-fi/wi-fi-easymesh
[2] https://github.com/prplfoundation/prplMesh
[3] https://www.wi-fi.org/file/multi-ap-specification-v10
    (requires registration)
