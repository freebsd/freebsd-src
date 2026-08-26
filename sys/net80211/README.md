# net80211

This is the 802.11 wireless stack for FreeBSD.

## Introduction

The net80211 subsystem implements the 802.11 protocol and support infrastructure.
It supports a variety of device types, 802.11 protocols, operating modes and
security extensions.

net80211 handles the 802.11 state machine, interface management, node management,
virtual interfaces, packet encapsulation and de-encapsulation and basic
security key management.

The userland ioctl() API provides control mechanisms for the above and is how
management tooling (ifconfig, libifconfig) and management services (hostapd,
wpa_supplicant) interfaces with the net80211 stack.

The security state machine and key management (802.1x, WPA, etc) are handled
by management services.

Drivers can implement as much or as little of the 802.11 infrastructure as
needed.  net80211 support drivers from full-offload (ie, supplying ethernet
encapsulated/de-encapsulated frames with management control via driver
methods) down to fully software controlled devices (ie, the hardware
is minimal and all 802.11 packet handling, state machine, reordering, security,
etc is handled by net80211.)

## Overview

net80211 consists of a few top level design modules:

 * The 802.11 device representation and functions (ieee80211com), used
   in conjunction with an 802.11 device driver to represent the physical device.

 * The 802.11 virtual interface representation and functions (ieee80211vap),
   used to represent instances of virtual interfaces.

 * A representation of 802.11 stations/nodes (ieee80211_node), which
   keep the state of each 802.11 station/node that the stack knows about.

 * Encryption handling (ieee80211_crypto), handling 802.11 frame encryption,
   decryption and session/state tracking.

 * Regulatory domain (ieee80211_regdomain), which implements the 802.11
   regulatory domains, allowed frequencies, operating modes and transmit power.

 * Radar detection (ieee80211_dfs.c), tracking the state of radar detection and
   interoperability in the 5GHz frequency range.

 * Transmit rate control (ieee80211_ratectl.c) implements software and
   firmware based rate control for devices that don't implement full rate control
   offload.

 * Power save support (ieee82011_power.c) implements various power saving
   mode features and support for devices which do not fully implement offloaded
   power management.

 * Operating system specific interfaces (ieee80211_freebsd.c) which implement
   the bulk of the operating system specific glue (logging, memory allocation,
   network interfaces, etc.)

 * The configuration interface (ieee80211_ioctl.c) which implements the ioctl
   API used by userland to configure and monitor the state of the 802.11 stack
   and devices.

In addition, each operating mode (adhoc, station, AP, WDS, mesh) have their own
modules that implement the state machines and functionality required for 802.11.

## Portability

Although net80211 attempts to keep most OS specific components in a single file
(ieee80211_freebsd.c), it is not currently perfect.

Notably:

 * There are still plenty of FreeBSD-isms located throughout the source code,
   including BSD specific includes, network APIs, etc.

 * The interface and networking model is still very BSD, including using the
   system implementation of mbufs.

When developing for net80211 please keep in mind that other operating systems
(such as DragonflyBSD) and third parties do leverage this codebase.
Try to keep all FreeBSD specific components in ieee80211_freebsd.[ch].

## Protocol Overview

A basic protocol overview is available at (@ref md_net80211_PROTOCOL).

The most comprehensive overview is the 802.11 protocol document itself,
but it is very large and implementations do not always correspond 1:1
with the protocol definitions.

## Functional Overview

(TODO)

 * Module layout
 * Logging
 * Debugging - (@ref md_net80211_DEBUG)
 * Top-level device layout (ieee80211com)
 * Data / Control Path Overview (@ref md_net80211_DATAPATH_TRANSMIT), (@ref md_net80211_DATAPATH_RECEIVE)
 * Deferred work (@ref md_net80211_DEFERRED_WORK)
 * Regulatory
 * Virtual interfaces
 * Operating Modes
 * Nodes
 * Node tables, node table iteration
 * Device and VAP states
 * Node states
 * Operating modes
 * Cipher management
 * Radar detection
 * ioctl interface
 * ACL support
 * Scanning, Scan Modules
 * Power Management
 * Transmit Path
 * Receive Path
 * A-MSDU Fast Frames
 * A-MPDU
 * Radiotap
 * Monitor Mode

## Driver Overview

(TODO)

 * Introduction
 * Driver Structure
 * Setup and Attach
 * Virtual Interfaces
 * Control Path
 * Data Path
 * VAP state
 * Device State
 * Suspend and Resume

