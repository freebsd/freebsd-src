# net80211 Datapath - Transmit

## Overview

This document provides an overview for the transmit data path in
net80211, between the interface to the operating system, through net80211 and
into the driver.

The details about underlying implementations (eg how A-MPDU RX aggregation
is handled) will be covered in dedicated documents.

## Concurrency Notes

The transmit path(s), receive path and control / ioctl paths all run
in parallel and can be scheduled on multiple concurrently running
kernel threads.  It's important to keep this in mind.

## Transmit Path

There are two paths from the operating system layer into the net80211 transmit
path - the normal data path and the BPF / radiotap raw frame path.

It is important to note that both paths have no serialisation between
them, and multiple sending paths in the OS can and will queue frames
simultaneously across multiple concurrently executing threads/CPUs.
Please keep this in mind when reading the transmit handling and
how it interacts with 802.11 sequence numbering and encryption IV.

### Data Path - net80211

This is configured at the ifnet setup in ieee80211_vap_setup() -
the output path is ieee80211_vap_transmit().  This input path
takes 802.3 ethernet frames with no attached metadata (such as
rate control, transmit power, etc) - it is left up to the stack.

This hands the packet off to ieee80211_start_pkt() which will
perform the initial 802.11 destination lookup, query the node
state (eg whether it's in power save) and the VAP state (eg
is the vap itself in power state, or in a non-RUN state)
and drop or queue the frame appropriately.

It is then handed over to ieee80211_vap_pkt_send_dest() with
a destination ieee80211_node reference.

ieee80211_vap_pkt_send_dest() performs the bulk of the
net80211 transmit handling.  Packets will be queued here if the
destination node is in a power saving mode.

This includes:

 * Firstly - checking if the packet needs to be queued for
   power saving operation and will pass it via ieee80211_pwrsave()
   if needed;
 * QoS classification via a call to ieee80211_classify();
 * BPF TX tap via a call to BPF_MTAP();
 * handling 802.11 encapsulation via ieee80211_encap() if required;
 * A-MPDU TX decisions, AMSDU and Atheros Fast-Frames decisions.

At this point the packet has been 802.11 encapsulated if required,
marked as needing encryption if required, and has been optionally
fragmented into a list of 802.11 fragments.

Finally, the packet / fragment packet chain is sent up to the driver via a call
to ieee80211_parent_xmitpkt().   The driver is expected to queue the
packet / fragment list or discard the packet / fragment list.  The specific
format of the mbuf chain and how ieee80211_node references are kept
is documented in ieee80211_parent_xmitpkt().

#### Notes on transmit path serialisation

Note that by default the IEEE80211_TX_LOCK() is held across the call to
ieee80211_encap() and ieee80211_parent_xmitpkt().  Drivers can register
that they properly handle 802.11 sequence number offloading via
IEEE80211_FEXT_SEQNO_OFFLOAD.  The lock is to ensure that packets
queued to the driver layer are added to the driver transmit queue
in the same order that they are 802.11 encapsulated - which sets the
802.11 sequence number.  Drivers which set IEEE80211_FEXT_SEQNO_OFFLOAD
indicate that they will assign the sequence number themselves - likely
at the same time that the transmit encryption IV number is assigned,
or simply offloaded in firmware - and thus this lock is not
required.

### Data path - Driver

The call ieee80211_parent_xmit() will call the driver ic->ic_transmit()
method.  At this point the driver can choose to queue / send the frame
(and take ownership of it), or return an error, and return it back
to net80211.  Currently net80211 will just free the mbuf and node reference
and return, but drivers should not assume that.

The mbuf passed in will be either a single 802.11/802.3 frame in an mbuf,
or a list of 802.11 fragments chained by m->m_nextpkt.  If the driver
has not set IEEE80211_FEXT_SEQNO_OFFLOAD then the packet will have
a sequence number assigned which the driver can fetch via M_SEQNO_GET().
The mbuf also holds an ieee80211_node reference.

(Note that fragments do not have sequence numbers assigned nor node
references.)

The driver needs to do a few things with this frame.  Notably if it's
an 802.3 offload device, it will be handed an 802.3 frame with no
802.11 information.  In that case, the driver just needs to queue
it for send to the hardware/firmware.

For devices which accept 802.11 frames, a few things are needed:

 * It needs to queue them for send, in the order they're given.
 * If there are any reasons the frames need to be buffered in the
   driver - eg node power state, asynchronous node/key/state updates -
   then they'll be buffered here until needed.
 * It needs to do any local hardware/firmware setup - rate control,
   transmit configuration, destination queue decisions, etc.
 * Hardware/firmware typically has some way to mark a frame as a type
   (control, data, management), whether RTS/CTS is needed, 
 * If IEEE80211_FEXT_SEQNO_OFFLOAD is set in the driver, it may need to
   allocate 802.11 sequence numbers via a call to ieee80211_output_seqno_assign().
 * If the frame is part of an MPDU (m->m_flags & M_AMPDU_MPDU) then
   the frame may need to be handled differently.  (For example rtwn(4)
   leaves sequence number assignment up to the firmware when A-MPDU is
   enabled.)
 * If the mbuf is marked as needing encryption (IEEE80211_FC1_PROTECTED
   is set in the 802.11 header) then the frame needs to be encrypted
   with the current encryption state via a call to ieee80211_crypto_encap().
 * Finally, the frame is queued to the hardware/firmware.

Again it is critical that the 802.11 sequence number and encryption be
called together in the same order.  This is typically done by the TX work
being done in a lock, or all frames being pushed into a single software
TX queue.

### Data path vs control path and the need to buffer frames

net80211 currently treats encryption key programming, VAP state
and other updates as synchronous calls.  For example, the
transmit path will call the driver to add a node, then
set the encryption keys and then queue a frame to be transmitted.

For devices which are programmed directly with no queued operations
(such as the ath(4) devices) the encryption key and node programming
is immediate.  However, for many other devices - firmware and
USB are two examples - these operations are asynchronous.
And these code paths tend to be in the transmit paths from
upper layers that may have locks held, so sleeping is not an option.

So for now this needs to be implemented in the driver itself.
It will need to maintain a per-node queue of transmit frames;
it will need to track asynchronous node creation/updates and
encryption key updates and buffer transmit frames for a node
until the node add/update and encryption key add/update is
completed.

### Transmit Completion Notifications

The net80211 stack may request a completion notification
to be called when a transmit frame has completed.
This will be done via a call to ieee80211_add_callback().
It is used in various parts of the net80211 stack to
drive the MAC state machines - for example, being notified
once an BAR (Block-ACK request) frame has completed so
the retry timer can be cancelled.

This requires that mbufs that are transmitted with a requested
completion callback be checked and handled appropriately.
This is covered in the next section.

### Completing and freeing transmit path mbufs

There are two paths to freeing mbufs - ieee80211_free_mbuf() and
ieee80211_tx_complete().

#### Before transmit - ieee80211_free_mbuf()

ieee80211_free_mbuf() is used in drivers and net80211 to free
a list of mbufs as part of the transmit path setup so it can
properly account for and free an 802.11 MPDU / 802.3 frame,
or a list of mbufs representing 802.11 fragments.  It doesn't
handle the ieee80211_node reference as at the early stage
of transmit there is a single ieee80211_node reference
covering all of the fragments being passed to the driver
for transmit.

If you're not supporting 802.11 fragment transmit (and you have
to register your driver with the IEEE80211_C_TXFRAG capability
to even support this) then you can ignore all of the above
and just not call ieee80211_free_mbuf() for now.

This must not be used for receive mbufs.  Yes, this is not
well named and should likely just be renamed.

#### After transmit queueing / attempts - ieee80211_tx_complete().

In the general case of an transmit mbuf being completed (either
successfully or unsuccessfully) net80211 provides a call
to handle everything - ieee80211_tx_complete().  This takes
the relevant destination node (struct ieee80211_node),
the mbuf, and a status indiciating success or failure.

A call to ieee80211_tx_complete() handles a variety of
common functions:

 * It increments the ifnet counters as appropriate;
 * If the frame has a TX completion notification callback attached
   it will process said callback;
 * If a node is supplied then the node reference is freed

In the past some drivers implemented the mbuf TX callback
handling themselves, resulting in some drivers supporting
callback and some drivers not supporting callbacks. The goal
here is to implement a single way for completions to be
handled.

Note that some hardware / firmware do not support per-frame
completion / status notification.  For example, USB devices
tend to not send individual notifications for frames - you
may be able to request it for specific frames, but the
status notifications are expensive.  In these cases, drivers
may just call ieee80211_tx_complete() with a status based
on whether the frame was queued to the USB endpoint successfully
or not.

#### Atheros Fast Frames / 802.11n A-MSDU transmit

(Note this is purposely short - a larger write-up for this will be
done on a separate page.)

The transmit path above will call ieee80211_ff_check() and
ieee80211_amsdu_check() to see if the given node/frame should be
queued for an Atheros Fast Frames MPDU or an A-MSDU.

If the frame should be queued it will be queued locally and NULL
will be returned; if there's already a frame queued it may be
paired with a queued frame and both returned as a single mbuf / MPDU
to send.

As far as the driver is concerned, it will be handed a single
802.11 MPDU to send.

#### 802.11n A-MPDU transmit

net80211 implements the A-MPDU negotiation and block-ack request/response
handling.  However currently the driver must implement A-MPDU packet
queuing, buffering, submission and retransmission.

There are some methods that the driver can override to control the
A-MPDU transmit negotiation flow (ic->ic_addba_request, ic->ic_addba_response,
ic->ic_addba_response_timeout, ic->ic_addba_stop) and the Block-Ack
response completion or error/timeout (ic->ic_bar_response).

#### Driver queue completion

Currently there are two things a driver should do when its own queues
are (mostly) empty:

 * When the transmit queue is empty or mostly empty, call ieee80211_ff_flush()
   to flush out any pending A-MSDU / Atheros Fast Frames to be transmitted;
 * When the receive queue is being handled, call ieee80211_ff_age_all() to
   flush out any frames that are older than a provided time interval.

These calls ensure that any queued frames in Fast Frames / A-MSDU queue
don't stay in there permanently.

### Non data frame transmission (management, control, action, beacon, etc)

Non data frames are sent via ieee80211_raw_output().  The main exception to
this is beacon frames, which are separately initialised and pulled from
net80211 into the driver by the driver specific beacon handling routines.

Raw frames differ from data frames in a couple of ways:

 * Transmit parameters are typically sent from userland or the caller
   (struct ieee80211_bpf_params \*), and
 * The input path into the driver is via ic->ic_raw_xmit(), not ic->ic_transmit().

The driver can combine the data and non-data paths into a single path.
The main reason for keeping these separate is to cleanly support drivers
and firmware which allow 802.3 frames to be sent and received, but still
need a side channel to send and receive management frames for various other
functions.

The raw frame output path is used by:

 * The BPF output path - ieee80211_output() ;
 * The management frame output path - ieee80211_mgmt_output() ;
 * The NULL data output path - ieee80211_send_nulldata() ;
 * Sending probe requests - ieee80211_send_probereq() ;
 * Sending probe responses - ieee80211_send_proberesp() ;
 * Sending 802.11n BAR frames - ieee80211_send_bar() ;
 * .. and anywhere where the individual protocol (eg 802.11s) wishes to send raw
   non-data frames.

This path is not REALLY designed for high speed data - for example,
it should work for basic packet injection, but it does not pass through
the normal functions for encryption, power save, TX aggregation and other
data specific operations.  It expects to be handed a raw, already encapsulated
802.11 frame.

Note this is not an 802.11 MPDU - this is an 802.11 frame.  For example,
non-data frames may not have sequence numbers.  NULL data frames have a sequence
number but that sequence number must be 0.

Once the driver ic->ic_raw_xmit() call is made, the driver can handle the
802.11 frame in any way it sees fit.  Again, it can't assume it's an 802.11
data frame.

### BPF path

Control frames are injected from userland and net80211 via a raw transmit path,
separate from the data path.  This dates back to the earliest Orinoco/WaveLAN
cards, where the earlier firmware only allowed 802.3 frames to be sent/received,
but later firmware introduced raw packet transmit to allow wpa_supplicant
operation.

Packet injection begins via the BPF/radiotap input path.  The code in
ieee80211_radiotap.c attaches a BPF operator to the VAP during the
call to ieee80211_radiotap_vattach().

Raw frames start in BPF and are queued via bpf_ieee80211_write(), which will
send the frame into the driver via a call to the VAP ifp->if_output() and then
if provided, a copy of the feedback mbuf via the VAP ifp->if_input().

The ifp->if_output() method by default is ieee80211_output().  The driver
can override this.  This takes care of validating that it is an 802.11
frame, extracts the (struct ieee80211_bpf_params \*) header from the
destination sockaddr passed in via BPF, finds the relevant
struct ieee80211_node \*) tx node, grabs a reference, some further sanity
checks and then calls ieee80211_raw_output().  The rest of the raw output
path is the same as net80211 sourced raw frames.

### Power Save Management

By default, net80211 will track legacy power-save state between IBSS nodes
and STA <-> AP nodes (ie, full node buffering via the power management bit
in the 802.11 header; TIM/ATIM bitmaps in beacons, NULL data frames to wake up)
and PS-POLL frames being sent by stations to request individual frames.

The transmit path will pass frames destined to asleep stations to the power
save queue via a call to ieee80211_pwrsave().

There are a number of VAP methods for the driver to tie into if it needs to be
informed about this state (vap->iv_set_tim, vap->iv_recv_pspoll, vap->iv_node_ps).
These allow the driver to keep its own internal state in sync with net80211
and allows it to better maintain its own transmit queue state.

See the ath(4) driver for a comprehensive example of how these methods are used
to correctly transmit and buffer frames from an AP to STA device without packet
loss.

### Transmit path encryption

The net80211 stack needs to handle a variety of transmit encryption schemes
based on all the combinations that driver and firmware interfaces may require.

In general, the transmit encryption is done in two phases:

 * In ieee80211_encap(), the transmit key is chosen via a call
   to ieee80211_crypto_getucastkey() or ieee80211_crypto_getmcastkey() - the
   key index is added to the 802.11 header and space is reserved between
   the 802.11 header/payload and at the end for the encryption key data to be
   added;
 * Then when the driver transmits the frame, it calls ieee80211_crypto_encap()
   to actually do the encryption.

Some hardware will completely offload encryption, so although the key choice
is made, various driver configuration options are set to inform net80211 not
to add all the padding.  Others will offload encryption but require the
space to be provided in the frame for the hardware/firmware to add the
encryption information into.

### What is IEEE80211_F_DATAPAD ?

This is actually to support hardware such as the Atheros 802.11abgn chips,
which have a 4 byte alignment requirement between the 802.11 header and
the data payload (including the encryption parts.)

Yes, it likely should be a more generic option.

### Future work

  * It would be nice to more formally define and enforce what drivers should be
    doing with mbufs during the whole transmit lifecycle of an mbuf.
  * Perhaps add a function or two for the drivers to use to
    query whether a given mbuf has a TX notification attached (rather
    than drivers querying M_TXCB) so they can individually
    register for explicit notifications so they can provide more
    accurate completion information.
  * The fast frames age / flush routines should really be expanded to
    be required functionality in net80211 drivers rather than optional
    when IEEE80211_SUPPORT_SUPERG is enabled, so further software transmit
    queue management is possible in net80211.

