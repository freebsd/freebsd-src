# 802.11 protocol overview

This is a quick overview of the 802.11 protocol and where it intersects with
net80211.  It is not intended as a comprehensive deep dive into all of 802.11.

TODO: link to appropriate sections in 802.11-2016 / 802.11-2020 depending upon
which PDF is freely available.

## 802.11 overview

The 802.11 protocol / specification is a very large document which covers
everything from the raw signals going out over the air up to how devices
need to behave in different operating modes.

The IEEE specification documents and amendments describe what devices should
and must do in order to interoperate.  It's important to note that the
intersection of "what the standard says" and "what devices do" is not always
fully aligned.  The 802.11 specification has evolved over twenty-five years
and for the most part this allows interoperability between the original 802.11b
hardware and modern multi-band 802.11ax devices.

It's also important to note that 802.11 is not just limited to the IEEE
specifications.  802.11 devices are almost exclusively RF devices (if you
read the specification you may find the old infrared / IR protocol definition!)
and so need to operate inside of the radio regulatory rules defined by each
country.  These define a wide variety of RF environmental behaviours
including frequencies can be used, when devices can transmit, what transmit
power is allowed, interoperability with other devices (802.11 and non-802.11)
and radar interoperability.  For the purposes of this document these will
be called "regulatory concerns" and will be covered elsewhere.

The 802.11 specification breaks things up into a handful of top level areas:

 * the PHY layer - how the device interfaces with the RF environment and
   encodes/decodes RF transmissions into data streams.
 * the MAC layer - defines how data is packetized into individual data frames,
   exchanged with the upper layer (ethernet/bridge), deciding when and what
   to transmit via the PHY layer.
 * MLME - (MAC layer management entities) - defines all of the state methods
   and transitions that underpin the 802.11 MAC state machine.
 * Security - the cipher and key management components.
 * PHY specifications - the specific implementations of PHYs - 2GHz DSSS
   (spread spectrum), 2GHZ CCK, OFDM, ERP, 802.11n / HT, 802.11ac / VHT, etc.

Most 802.11 implementations do not implement a 1:1 definition of each of these
layers - notably implementing every single MLME state would be a huge amount
of work.

## 802.11 revisions

There have been many revisions of the 802.11 specification. The specifications
can be found online at https://www.ieee802.org/11/.

The latest specification being implemented in net80211 is 802.11-2020, however
net80211 is far from completely compliant.  Generally new code which implements
802.11 features / protocol handling should identify the specification and
section which it is referencing.

## 802.11 protocol and frame definitions

net80211 keeps most 802.11 frame and protocol definitions in a single location
(ieee80211.h).
This contains descriptions of the 802.11 frame and field definitions, ranging
from the lowest definition of the frame itself up through frame types/subtypes,
individual field definitions, information elements, action frames, and
anything else that can be found in the 802.11 specifications.

The PHY definitions can be found in (ieee80211_phy.c) and (ieee80211_phy.h).
Notably those include the frame timing information useful for rate control
and frame duration calculations.

## 802.11 Revisions

(TBD)

### Legacy 802.11

The earliest 802.11 devices implement 1Mbit/s and 2Mbit/s direct spread spectrum
frames.  These include the earliest Wavelan devices.  These are grandfathered
into 802.11b.  The PHY specification can be found in 802.11-2020 Section 15.)

### 802.11b

802.11b devices implement Section 15 (1Mbit/2Mbit) PHYs as well as the high
rate DSSS specification (802.11-2020 Section 16) to provide 5.5Mbit and 11Mbit
CCK rates.  They interoperate with legacy 802.11 devices by using compatible
PHY encodings and will limit their performance if said legacy devices are
detected.

### 802.11a

802.11a devices implement OFDM rates from 6Mbit/s to 54Mbit/s on the 5GHz
band.  Among other features, it also defines 5MHz and 10MHz wide channel
behaviour.  This is covered in the OFDM PHY specification (802.11-2020
Section 17.)

### 802.11g

802.11g devices implement OFDM rates from 802.11a, the CCK rates from 802.11b
and the DSSS rates from legacy 802.11.  These are covered in the ERP
specification (802.11-2020 Section 18.) There are some MAC extensions for
negotiating 802.11b / 802.11g interoperability and these are documented
throughout the MAC specification.  This also specifies support for 5MHz and
10MHz wide channels.

### 802.11n (HT)

802.11n introduced a variety of high throughput rates and feature support
(hence why it's called HT - high throughput).  It introduces higher density
OFDM rate encodings, 20 and 40MHz wide channels with interoperability for
earlier devices, packet aggregation via A-MPDU and A-MSDU, MIMO (multiple input,
multiple output spatial streams), some initial beamforming support, power
saving extensions and more.

The physical layer support is covered in the HT PHY specification (802.11-2020
Section 19.)  The rest of the MAC extensions are documented throughout the
rest of the specification.

### 802.11ac (VHT)

802.11ac extends the 802.11n specification (hence why it's VHT - Very
High Throughput) and boosts performance by adding higher density OFDM QAM
encoding (256-QAM), wider channels (80MHz, 160MHz), split 80+80MHz channel
support, much larger A-MSDU / A-MPDU frame sizes, support for MU-MIMO
(multi-user MIMO) allowing APs to transmit to multiple STAs at the same time
and various other extensions.

It builds on top of the 802.11n MAC and PHY specification, so a lot of
802.11n feature and MAC negotiation happens as part of 802.11ac negotiation.

The PHY layer is covered in the VHT PHY Specification (802.11-2020 Section
21.)  Again, the rest of the MAC extensions are documented throughout the
rest of the specification.

### Greenfield versus backwards compatibility

The various protocols supported by 802.11 build on top of earlier protocols.
So typically you're not building a single implementation for each protocol -
for example, you can't handle 802.11ac support without implementing a large
amount of 802.11n support.

(As a side note, the 802.11 frame has a protocol version field, and
that actually changed in 802.11ah (900MHz and longer distance bands) -
which changes a lot of what the fields do.  No, net80211 currently does not
support 802.11ah and will drop frames whose 802.11 protocol ID is not
supported.)

At the PHY layer, later model hardware can transmit data encodings which
earlier model hardware just won't recognise.  All they'll see is an increase
in RF power on the channel at best and signals that will confuse the
RX decoder / cause hardware issues at worst.)

So each of the PHY specifications will lay out a few things:

 * How frames should be encoded in the air in a way that earlier
   hardware can decode them enough to know it's not for them;
 * How devices can identify that earlier protocol devices are around and
   change the configuration (eg STA changing its own configuration,
   AP changing the configuration of the network it controls, etc)
   to provide backwards compatibility.

These come at a performance cost.  For example, an 802.11g AP which
supports 802.11b and 802.11 devices needs to notice that an 802.11b
device wishes to associate, and when it sees this, change some of
its configuration (notably "long preamble" so 802.11b devices can
decode frames that are being transmitted, whether destined to it or not.)

Various devices allow backwards compatbility to be configured.
For example, an 802.11n AP may be configured to deny non-802.11n clients.
This may improve performance but then earlier clients can't connect.

In 802.11n deployments this was known as a "greenfield deployment".
This typically disables any and all pre-11n interoperability at both
a MAC and PHY layer.  net80211 has some flags for this to specifically
inform devices that they can configure the hardware for such a setup.
Not all drivers implement it however, and in a lot of cases they will
still handle pre-11n framing, even if the net80211 code will deny
association.

There are other components to backwards compatibility which are worth
keeping in mind when reading through the 802.11 specification and
net80211 stack / driver code.  These include:

 * short/long preamble - (vap_update_preamble)
 * short/long slot time configuration - (vap_update_slot)
 * 802.11g protection mode (vap_update_erp_protmode) -
   whether to use CTS-to-self around each transmission
 * 802.11n protection mode (vap_update_ht_protmode) -
   whether to use RTS/CTS around each transmission
 * 802.11n 20/40MHz BSS operation (whether an 802.11n AP sees other APs that
   overlap its frequency range and need to reconfigure how to protect
   transmissions)

## How 802.11 (very briefly) works over the air

This is a very brief and not at all comprehensive overview of how 802.11
works over the air.  The goal of this section is to provide enough background
information to help de-mystify reading the net80211 stack and wireless
driver source.

### Why there's timing requirements in the first place

Each of the PHY sections in the 802.11 specification describe what
the PHY needs to do in order to transmit and receive data. It's not
anywhere as easy as "toggle some bits on a wire".

An important thing to understand is that hardware isn't immediate.
All the state machines in your 802.11 devices take non-zero time
to make decisions about when to transmit, when to receive, locking
onto a signal, deciding it can be decoded, getting reset for the next
frame, etc.

So a lot of what you'll see in 802.11 negotiation and feature support
is linked to the underlying hardware implementations and limitations
of the time.  For example the 802.11b specification defines the slot time
as 20uS, but the 802.11g specification lowers it to 9uS.  The "slot time"
value defines the unit of time used for contention management / backoff, and
it's defined partly by what the speed of light dictates (ie how big
of a physical area you want to be able to "hear" in determining if the
area is busy) and how quickly the hardware can guarantee to respond.
It dropped to 9uS because hardware got better, but to interoperate
with older devices without starting to transmit before they're
ready to react, 802.11g devices will fall back to 20uS slot time when
they detect an 802.11b device.

This carries through everywhere in odd places that you're not necessarily
aware of.  For example, the 802.11n A-MPDU definition includes negotiated
padding between frames and limits encryption ciphers (typically CCMP or
GCMP.)  This is due to hardware support - the MAC may be able to support
much less padding when no encryption is used, but setting up / resetting
the encryption / decryption blocks may take more time and thus larger
A-MPDU padding values are negotiated.

### Wait, the speed of light?

Yes.  The speed of light is roughly 300 metres for each microsecond of
travel time.

### Preambles, SIGs, PLCP, sending actual data and waiting / slot times

There are a few things that are worth understanding at a high level.

 * The first thing that a device needs to do is determine
   whether the air is busy or free.  There'll be some hardware
   to determine the signal level versus noise floor and provide
   a signal to the transmit hardware that the air is free,
   and to the receiver that it may want to try start decoding
   something.

 * The receiver needs to get in synchronisation with the transmitter.
   This is a one way operation - the transmitter needs to transmit
   enough of a signal that the receiver can "lock onto" and get itself
   ready for further data.  This is called the "preamble" - it's
   typically a low bitrate repeating pattern of data that gives
   the receiver hardware time to lock onto, figure out the signal
   level and be ready for the next phase.

 * Note that the receiver may pick up the preamble at any point in its
   transmission so it can't guarantee it will see exactly "x" bits of some
   repeating pattern.

 * Then there's other bits and pieces - eg look for L-SIG, HT-SIG
   in the PHY documentation - which is used to further synchronise
   what's about to happen.

 * Finally it will start transmitting the PHY framing bits needed to
   identify what the upcoming transmit rate and configuration is
   (all the stuff leading up to the PLCP header, then the PLCP header.)

Things get more complicated with MIMO, MU-MIMO, 802.11ax OFDM-A, etc but
don't worry about those for now - they build on top of all of these
ideas.

Once the data is transmitted, there's some quiet time between frames
before the receiver can ACK (and then a period of time where an ACK
is expected.)  The transmitter needs to finish transmitting, then
reset its internal state back to idle to be ready to receive - and
there's the pesky speed of light speed of 300m per microsecond -
so there's some MAC (interframe spacing) and PHY (slot time) enforcing
quiet so everyone has a chance to receive the frame and the reciver
gets ready to receive.  Then if there's an ACK, the ACK happens.

### PLCP header

Once all of the preamble, SIG/training stuff is done, the transmitter
will send a PLCP header with information about the transmitter
type and rate (and that's very handwaving it.)  net80211 has definitions
for the plcp header (ieee80211_plcp_hdr) but it's highly unlikely it will
be relevant or available in modern devices.

### How data is encoded - encoding rates, symbols, guard intervals

Now, once the transmitter has sent all of that, it will start to send
actual data encoded at the desired transmit rate.  The data bits
that you're transmitting go through a variety of encoding schemes
before they're turned into bits that are clocked out at the 802.11
physical layer (think "forward error correction" as an example),
but they're turned into what are known as "symbols".

A symbol can be thought of as a group of bits encoded in one specific
RF representation.  Explaining all the details isn't in scope of this
document (and I encourage interested parties to do a quick dive
into information theory!) but there are a couple of important higher
level concepts to understand here that influence what happens
later on in packet delivery.

For OFDM encoding:

  * Each symbol is preceded by a quiet time called a guard interval, to make
    sure any reflections don't interfere with the upcoming symbol;
  * Each symbol is then transmitted for a specific length of time to make sure
    it's received by everyone inside the desired area (again light = 300m/sec
    per microsecond);
  * All symbols for a given 802.11 MPDU are sent at the exact same rate;
  * This is repeated until all the symbols are transmitted.

The higher the data rate used, the higher the signal level needs to be
and lower the tolerance it has to interference.  Forward error correction
can only do so much, and the higher throughput rate encodings sacrifice
FEC for throughput.

Once an uncorrectable error occurs and the frame fails CRC, the whole MPDU is
dropped by the receiver.

Part of why A-MPDU is so important for high throughput is that the
errors are limited to a single MPDU in the burst of MPDUs. Ie, if
the transmitter sends ten MPDUs in a single A-MPDU, and five of them
have uncorrectable errors, then five .. well, didn't.  This means
the receiver can ACK some but not all of the MPDUs, and the transmitter
can re-send those with new MPDUs.

The default guard interval is 800ns.  802.11n allows for negotiating
shorter guard interval (400ns) which can be done per device in a BSS.
An 800ns guard interval is a little short of 300 metres, and 400ns is
a little short of 150 metres - so using short guard interval means
you trade increased performance for potential decreased performance
if you have reflections or stations more than 150 metres away.

802.11ax adds support for 1.6us and 3.2us guard intervals for physically
larger deployments.

### MAC layer framing

The MAC layer handles data that is encapsulated in the given transmit
rate that was established in the PHY (PLCP) header.  This includes
the 802.11 MAC header, CRC trailer, any of the cipher processing that
happens in between.  In the case of 802.11n, it can encapsulate
multiple frames being sent back to back in a single transmission.

Devices which do partial / no offload will typically produce and
consume 802.11 MAC layer frames to the driver and net80211.
It's thus important to understand MAC framing and frame types.

### MPDU versus MSDU

An MSDU (MAC service data unit) is an individual frame (think "802.2/802.3
ethernet") passed from the network stack into net80211.

An MPDU (MAC protocol data unit) is one or more MSDU frames wrapped by an
(ieee80211_frame) header and CRC trailer.  It is what is eventually
encapsulated inside the PHY framing (preambles, training symbols, PLCP
header, etc) and sent over the air.

Notably an 802.11 MPDU isn't just an IPv4/IPv6 frame with an 802.11
header/trailer - it is a full ethernet frame that is being wrapped
by 802.11 framing.

### Tracking airtime with NAV

802.11 devices have to interoperate in a shared medium.  Earlier protocol
definitions require one transmitter at a time.  Later specification
devices (MU-MIMO with 802.11ac, OFDM-A with 802.11ax, etc) introduce the
ability for devices to transmit and receive simultaneously.

The simplest way to track this is with NAV (network access vector.)
The NAV implementation in pre-11ax devices is a single counter which
counts down to zero.  Once it is zero, the air is considered "available"
to attempt to check to transmit on.  The transmitter will also check
whether the air is busy (ie can it detect any signals present) before
it transmits - this is called CCA (clear channel access) and is
typically implemented in hardware.

The duration field in (ieee80211_frame) is a microsecond field which
covers the whole duration of the frame being transmitted.  Receivers
that decode the frame - even if it's not destined to them! - will listen
to the NAV and add it to their own NAV.

All 802.11 frames have a duration field.

### Fragmented frames

(TBD)

### Sequence counters and duplication detection

(TBD)

### EDCA and QoS

(TBD)

### Inter-frame spacing (IFS)

(TBD)

## 802.11 frame layout

An individual 802.11 frame contains frame control (version, type, subtype),
duration, addressing, sequence number and optional QoS information.
The basic definition is available at (ieee80211_frame) but other definitions
are also possible - (ieee80211_qosframe), (ieee80211_frame_addr4),
(ieee80211_qosframe_addr4).

It then has a 4 byte CRC32 trailer appended at the end.

### Addressing types and traffic direction

(TBD - 3addr, 4addr, each of the fields, etc)

### QoS versus non-QoS frames

(TBD)

### RTS/CTS exchange and airtime

(TBD)

### CTS-to-self / OFDM protection

CTS to self is a concept introduced in 802.11g.  The general idea is that a
transmitter can send a CTS to its own MAC address for the duration that it
wishes to transmit for.  Since the CTS frame is transmitted at a slower
legacy rate, it both reserves airtime in any receiver in earshot, and
it is understood by older 11b only devices which do not understand 11g.

This ends up also being useful for 11n, 11ac etc to interoperate with
earlier devices, but they typically rely on a normal RTS exchange.

### Data frames

(TBD)

### Management frames

(TBD)

### Control frames

(TBD)

Notably, control frames do not have a sequence number and so can't be
de-duplicated.

### Action frames

(TBD)

## Frame combinations

There are various ways that 802.11 frames are combined together to improve
performance.

### ACK, Delayed Block-ACK, Immediate Block-ACK

(TBD)

### Atheros Fast Frames

(TBD)

### A-MSDU

(TBD)

### A-MPDU

(TBD)

## Security / Encryption

This is a much larger topic, however it's worth touching on the basics here to
understand how frames are redirected into the security/encryption paths in
net80211 and what devices may do with said frames.

### WEP, IV header and keys

WEP is an obsolete encryption method dating back to the earliest 802.11b
specifications.  It involves a 4 byte header which includes

 * a 24 bit IV (initialisation vector);
 * a 2 bit field indication which of four keys to use;
 * A CRC at the end.

It's relevant today because later cipher frame formats still use the IV
header - they're just extended to include more information.  Notably, the
four key indexes are typically implemented and used in hardware, and have
different meanings depending upon the kind of traffic being handled.

### WPA/WPA2 management

This is handled in userland.  The 802.11 specification covers everything
involved in key exchange and management but it's out of scope for this 802.11
overview documentation.

### CCMP, GCMP, TKIP frames

These later ciphers still use the WEP header, but they then add extra bytes
to it to include the larger sequence number space, other options needed
for said ciphers, and a larger trailer for CRC and TKIP MIC.

### IV duplication / tracking

net80211 tracks the received IV / sequence number for each station indexed
by QoS TID.  Anything with an earlier IV is discarded as a stale packet or
potential replay attack.  See the ni_txseqs[] and ni_rxseqs[] field in
(ieee80211_node).

Note that the 802.11 layer sequence number field will apply /first/.  Traffic
which the 802.11 input layer thinks is old or retransmits will be discarded
before handed to the net80211 crypto routines.

### Unicast vs Group Keys

WEP has four global keys which are shared between all devices wishing to
communicate.  The keys are provided in the WEP header.

However for later ciphers the four key indexes start taking on new meanings.
Notably key index 0 is the "unicast key" which handles traffic for a given
station and is unique for that station, and keys 2 and 3 are used for
group keys - shared keys for broadcast traffic that all stations need to be
able to decrypt.

(key 1 is also used for unicast station traffic for seamless station key
updating, but net80211 currently doesn't support this extension/feature.)

There's also upcoming work for encrypted management traffic and encrypted
beacons which reuse the key indexes for their traffic, but then don't treat
them as "global keys" - they start being treated as "global keys but only
for this traffic type."

It's important to understand the difference between global keys (WEP) versus
group and unicast keys (everything else) when looking through the net80211
data and encryption handling paths.

## 802.11 Operating Modes

(TBD)

### Station

(TBD)

### Access Point

(TBD)

### IBSS / Ad-Hoc

(TBD)

### Mesh / 802.11s

(TBD)
