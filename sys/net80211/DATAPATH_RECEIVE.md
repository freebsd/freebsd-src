# net80211 Datapath - Receive

## Overview

This document provides an overview for receive data paths in
net80211, between the interface to the operating system, through net80211 and
into the driver.

The details about underlying implementations (eg how A-MPDU RX aggregation
is handled) will be covered in dedicated documents.

## Concurrency Notes

The transmit path(s), receive path and control / ioctl paths all run
in parallel and can be scheduled on multiple concurrently running
kernel threads.  It's important to keep this in mind.

## Receive Path

### Concurrency

There must only be one packet receive path into net80211.  The net80211 stack
has not yet been fully validated to ensure that state changes all occur under
sufficient locking.

### Data Path

The receive path is split into three broad categories:

 * The normal 802.11/802.3 packet receive path from drivers;
 * The input path for reinjected frames (eg WDS, 802.11s, BPF);
 * Various side channels for offloaded non-data path (eg explicitly
   scan results, management frames, etc.)

#### Data Path - Initial Input

The driver receive path begins in ieee80211_input.c . The four
entry points are:

 * ieee80211_input() / ieee80211_input_mimo() and
 * ieee80211_input_all() / ieee80211_input_mimo_all().

The first two are called when the destination MAC address is a known
(struct ieee80211_node) node.  These are passed up to the
VAP via a call to ni->ni_vap->iv_input().

The second two are called when the destination MAC address is NOT
a known node.  In this instance, the frames are treated as broadcast
and routed to each VAP BSS node via a call to ieee80211_input_mimo().

Each VAP vap->iv_input() method handles the behavioural specific
needs of the interface.

#### Data Path - VAP type / behaviour

Each VAP type will do roughly the same thing - for example see
sta_input() in ieee80211_sta.c .

 * Check the frame size and protocol ID;
 * Check if the frame has been decrypted in hardware;
 * Grab A-MPDU session frames and put them in the reorder queue;
 * Handle control frames sent to the node, or general scan frames;
 * Get the frame QoS information / TID information if present;
 * If appropriate, check the 802.11 receive sequence number;
 * Break the handling up into data, management and control;
 * Reinject into a radiotap/BPF session via a call to
   ieee80211_radiotap_rx().

The data paths will typically do the following:

 * Do decryption if needed;
 * Do 802.11 decap if needed;
 * Enforce security requirements if needed;
 * Eventually deliver the frame up to the higher level network
   stack via a call to ieee80211_deliver_data() which will
   strip away any last bits of 802.11 / net80211,
   call ieee80211_vap_deliver_data(), which will call the
   network stack input interface.

The control and management paths will call vap->iv_recv_mgmt()
and vap->iv_recv_ctl() which implement the per VAP type behaviours.
These will include participating in driving the scan engine,
the per-node state machines and the VAP state machine.

#### Reinjected Path

#### Side Channels

Drivers may need a specific side channel for management/control
frames, MAC layer events (eg A-MPDU aggregation session state);
some power state communication, scan information and other
things that would normally show up as 802.11 frames.

These will be covered in more detail in other documents.

### Receive Status and Parameters

Received 802.11 / 802.3 frames can come with a variety of information
that isn't strictly the data payload.  These include receive timestamps
(at beginning or end of frame), receive noise floor / signal strength,
channel / frequency, channel width, received rate, aggregation frame
boundaries, decryption state, etc.

The original paths - ieee80211_input() and ieee80211_input_all() -
took a noise floor and rssi parameter.  Later drivers provide
information about all of the above by attaching a (struct ieee80211_rx_stats)
to the receive mbuf via a call to ieee80211_add_rx_params() bafore
calling ieee80211_input_mimo() and ieee80211_input_mimo_all() .

Existing drivers should be migrated to the mimo versions of these
APIs and the existing API should eventually be deprecated and
replace the mimo versions.

All new drivers must use the ieee80211_input_mimo() and
ieee80211_input_mimo_all() API calls.

### Driver Receive Path Requirements

The driver receive path has a few top level requirements:

 * Driver / stack locks must not be held during receive.  This means that
   drivers should dequeue their frames first into a local list, release
   whatever locks are needed and then pass the frames up to net80211.

 * Drivers are responsible for doing the node lookup before
   calling ieee80211_input() / ieee80211_input_mimo() or
   calling ieee80211_input_all() / ieee80211_input_mimo_all().

 * Drivers are also responsible for creating and attaching the
   ieee80211_rx_stats information via a call to ieee80211_add_rx_params().

 * Drivers are responsible for tagging a frame as a potential
   A_MPDU by tagging the received mbuf with the M_AMPDU flag.
   They should do this by just tagging all mbufs to a node
   with ni->ni_flags & IEEE80211_NODE_HT set w/ the M_AMPDU flag.
   This is a holdover from the 802.11n code which enforces that
   only potential AMPDU frames can be added to an A-MPDU receive
   aggregation session and may be relaxed / removed in the future.

### Driver Receive Path Methods

Drivers can hook into the receive path processing in a variety of ways.
There are a number of vap methods that a driver can hook into
processing.  The details will be covered in the driver document.

These include:

 * vap->iv_input - the driver can replace the iv_input method
   with its own method to first handle frames before they are passed
   to the VAP type receive path.
 * vap->iv_recv_mgmt - the driver can hook here to handle
   management frames before the VAP type management receive path.
 * vap->iv_recv_ctl - the driver can hook here to handle
   control frames before the VAP type control receive path.
 * vap->iv_bmiss - the driver can hook here to be informed of
   beacon miss frames.

These may be called at any time and overlapping with others (eg
the beacon miss event - which may be triggered by a timer -
can be called in parallel with the various receive path methods.)
