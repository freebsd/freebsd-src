# Registering a net80211 driver

This document covers the top level driver registration and
'struct ieee80211com' which represents a net80211 device
to the rest of the kernel.

## Overview

Each net80211 driver needs to register with net80211.  Drivers
typically do this by putting 'struct ieee80211com' at the
top of their driver softc structure and defining a macro
that lets the driver author convert between the driver softc
struct and the ieee80211com struct.

There currently isn't a call to allocate or initialise
a net80211com structure.

Drivers will go through and initialise fields in the ieee80211com
struct which describes the hardware capabilities, crypto
support, operating characteristics in various modes (11abg,
11n, 11ac), configure the list of available / supported
channels, and then finally call ieee80211_ifattach().

There is a specific order with which to do the initialisation,
as various parts of the net80211 setup process will set
defaults which can then be overridden by the driver.

## Top level driver behaviour

There are a few top level driver methods that need implementing
by all FreeBSD drivers.  They are, in order here:

 * probe()
 * attach()
 * detach()
 * suspend()
 * resume()
 * shutdown()

### Driver Probe

There is typically nothing 802.11 specific about the driver
probe phase.  This is normally just matching on the bus
and device information and stating whether this driver
supports the given hardware.

### Driver Attach

The attach path involves listing out the NIC hardware, firmware
and feature support, initialising net80211, overriding its
defaults and then completing the net80211 registration process.

Drivers will typically do as much work up front as needed to
ensure the device is available and gather configuration before
the net80211 registration process can take place.  This includes
information such as the available channels, NIC operating modes
and MAC address.  Devices which require firmware may need to
initialise the hardware and load the firmware in order to
fetch this configuration which may require interrupts to be
available.

The order of operations is typically:

* Do the device specific initialisation - busdma, memory,
  interrupt allocation, check firmware, etc - so it can fail
  before net80211 registration happens.

* Initialise the ieee80211com struct with a few things:

  + ic->ic_softc - point to the driver softc.
  + ic->ic_name - the driver name, typically device_get_nameunit(dev).
  + ic->ic_phytype = just set this to IEEE80211_T_OFDM for now,
    its no longer needed.
  + ic->ic_opmode = IEEE80211_M_STA - default to STA mode, but again
    its not needed these days.
  + ic->ic_caps - device capabilities (STA, AP, MONITOR, WPA/WPA2, etc.).
  + ic->ic_flags_ext - extended capabilities/flags (HT, VHT, etc.).
  + 802.11n and later NICs need to set ic->ic_rxstream and ic->ic_txstream
    to indicate the number of TX/RX spatial streams (not antennas,
    that is NOT 1:1 spatial streams).
  + 802.11n and later devices need to initialise ic->ic_htcaps with
    the HT capabilities.
  + 802.11ac devices need to also initialise ic->ic_vht_cap with
    the VHT capabilities.

* The driver next needs to initialise the set of supported channels.
  This typically involves calls to ieee80211_add_channels_default_2ghz()
  and ieee80211_add_channel_list_5ghz().

* The driver then calls ieee80211_ifattach() to initialise the
  net80211 state for this driver and set the driver defaults.

* Next, the driver will initialise the 'struct ieee80211com' level
  function methods, most of which are covered later in this document.

* Next, if supported, the driver will attach radiotap state
  via a call to ieee80211_radiotap_attach().

* Next, the driver can override the default set of software and hardware
  supported ciphers via calls to ieee80211_set_hardware_ciphers()
  and ieee80211_set_software_ciphers().

* Finally, if required, the driver can call ieee80211_announce() to
  verbosely announce the net80211 attach state.

### Driver Detach

The detach pass requires some care.  It's done as part of newbus rather
than net80211.

This typically involves a few steps:

 * First, the driver needs to stop any of its current work.
   For most drivers this will involve setting some state to inform
   the rest of the driver it is shutting down, then shutting down the chipset
   activity, DMA and firmware interfaces so the hardware is quiet.

 * Next the driver will stop its own activity - callouts, taskqueues, etc.

 * The driver will then wait to make sure activity has stopped.

 * Next the driver will shut off the hardware as appropriate.

 * The driver will then call ieee80211_ifdetach() to detach the net80211
   state and shut down any left over resources.  Note that this will
   loop through and destroy virtual interfaces and other state - so those
   paths need to know that the driver is shutting down, rather than just
   deleting the virtual interfaces.

 * Finally the driver will release its hardware, driver, kernel, etc
   resources and return.

The driver author must assume that a detach/shutdown call means the
hardware is no longer available.

### Driver Suspend

The suspend driver call happens from newbus, not from net80211.

The driver is required to call ieee80211_suspend_all() to suspend
all activity on the virtual interfaces and then do any work it
needs to do in order to suspend the hardware.

Note that ieee80211_suspend_all() will go through the normal
virtual interface transition to idle, and then will call the ic->ic_parent()
method to eventually shut down the parent interface.

### Driver Resume

The resume driver call happens from newbus, not from net80211.

The driver is required to do any required work to bring the hardware
back from suspend, and then call call ieee80211_resume_all() to
resume the virtual interfaces.

Note that ieee80211_resume_all() will go through the normal
virtual interface transition to active, and will call the ic->ic_parent()
method to notify the driver that one or more virtual interfaces
is starting.  The driver will need to do the relevant work to
re-initialise the hardware and firmware state.

### Driver Shutdown

The shutdown driver call happens from newbus, not from net80211.

This is invocated when the hardware is shutting down.  Typically
this will be a much shorter version of the detach method where
the driver and hardware are stopped, but resources do not need
to be cleaned up.

The driver author must assume that a detach/shutdown call means the
hardware is no longer available.

## Top level control methods

There are a few top level functions that need to be implemented
for general net80211 driver work to happen.  This is not currently
a comprehensive list; see the 'struct ieee80211com' list and other
documentation sections to further understand what drivers need
to implement.

 * ic_parent() - called to notify the driver that the parent
   interface needs starting or stopping.  This will be called
   when at least one VAP is active, or all VAPs are inactive.
 * ic_ioctl() - the ioctl() interface to the driver; this is accessible
   via ioctl() calls on a socket with a reference to the VAP.
 * ic_vap_create() / ic_vap_delete() - create and delete VAPs.
 * ic_getradiocaps() - get the radio capabilities and channel list.
 * ic_setregdomain() - get/set the regulatory domain state/configuration.
 * ic_set_quiet() - set the quiet time IE configuration.
 * ic_transmit(), ic_send_mgmt(), ic_raw_xmit() - the transmit path.
 * ic_update_slot() - NIC level slot timing update (for 11b/11g).
 * ic_update_mcast() - update NIC level multicast state changes.
 * ic_update_promisc() - update NIC level promiscuous settings.
 * ic_newassoc() - station association / update.
 * ic_tdma_tdma_update() - TDMA configuration for ath(4) NICs.
 * ic_node_alloc() / ic_node_free() / ic_node_init() / ic_node_cleanup() -
   'struct ieee80211_node' node allocation, free and management.
 * ic_node_age() / ic_node_drain() - handle node aging and timeout.
 * ic_node_getrssi() /ic_node_getsignal() / ic_node_getmimoinfo() -
   get node signal strength and noise floor information.
 * ic_scan_start() / ic_scan_end() / ic_scan_curchan() / ic_scan_mindwell() -
   configure and manage scanning.
 * ic_set_channel() - set the channel (for NICs that need it
   handled by net80211 rather than by firmware, etc.)
 * ic_recv_action() / ic_send_action() - 802.11 action frame send/receive
   override methods.

There are also quite a few methods controlling 802.11n ADDBA, AMPDU, BAR,
channel width and other processing.  These only need to be implemented
for 802.11n and later devices.

## Concurrency Notes

The top level driver methods (besides probe and attach, of course)
may and are called during existing and overlapping operations.
Although newbus will not call e.g., suspend and resume together,
newbus may call detach (e.g., due to a hotplug detach event for USB)
whilst the driver is actively doing USB transfers.

It is thus up to the driver to ensure that suitable ordering / locking
is performed between other driver entry points (transmit, receive,
ioctl handling, callouts/taskqueues, etc.) and the newbus driver
methods mentioned here (suspend, resume, shutdown, detach).

The detach/shutdown methods are notably difficult to get right.
The detach method may be called during active work - VAP state
changes, ioctls, transmit/receive data, callouts/tasks - and
as it is not an asynchronous method, it needs to block until its
done, it needs to make sure existing work is completed AND
no new work is scheduled.

## Future work

 * There is currently no methods to initialise 'struct ieee80211com' before
   the driver does its work.  It is just assumed to be zero-filled during
   allocation.  It would be nice to have a function that drivers call
   to explicitly initialise it to some set of defaults, even if those
   defaults are currently zero'ed memory.

 * The device is available once net80211_ifattach() is called.  There likely
   needs to be a method to mark the driver as actually ready once all of
   the function pointers and state are completely initialised.

 * Similarly to shutdown, it may be nice to provide a net80211 method to
   tell net80211 the whole device is going down, so stop trying to schedule
   further work (drop packets, don't call timers, ioctls will fail, etc.)

 * There are almost no default methods for the above function pointers -
   net80211 will just panic if they aren't provided.  It may be nice
   to provide default methods so a very simple driver can probe/attach
   fail VAP creation without panicing, do suspend, resume and detach/shutdown.
