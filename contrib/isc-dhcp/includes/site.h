/* Site-specific definitions.

   For supported systems, you shouldn't need to make any changes here.
   However, you may want to, in order to deal with site-specific
   differences. */

/* Add any site-specific definitions and inclusions here... */

/* #include <site-foo-bar.h> */
/* #define SITE_FOOBAR */

/* Define this if you don't want dhcpd to run as a daemon and do want
   to see all its output printed to stdout instead of being logged via
   syslog().   This also makes dhcpd use the dhcpd.conf in its working
   directory and write the dhcpd.leases file there. */

/* #define DEBUG */

/* Define this to see what the parser is parsing.   You probably don't
   want to see this. */

/* #define DEBUG_TOKENS */

/* Define this to see dumps of incoming and outgoing packets.    This
   slows things down quite a bit... */

/* #define DEBUG_PACKET */

/* Define this if you want to see dumps of expression evaluation. */

/* #define DEBUG_EXPRESSIONS */

/* Define this if you want to see dumps of find_lease() in action. */

/* #define DEBUG_FIND_LEASE */

/* Define this if you want to see dumps of parsed expressions. */

/* #define DEBUG_EXPRESSION_PARSE */

/* Define this if you want to watch the class matching process. */

/* #define DEBUG_CLASS_MATCHING */

/* Define this if you want to track memory usage for the purpose of
   noticing memory leaks quickly. */

/* #define DEBUG_MEMORY_LEAKAGE */

/* Define this if you want exhaustive (and very slow) checking of the
   malloc pool for corruption. */

/* #define DEBUG_MALLOC_POOL */

/* Define this if you want to see a message every time a lease's state
   changes. */
/* #define DEBUG_LEASE_STATE_TRANSITIONS */

/* Define this if you want to maintain a history of the last N operations
   that changed reference counts on objects.   This can be used to debug
   cases where an object is dereferenced too often, or not often enough. */

/* #define DEBUG_RC_HISTORY */

/* Define this if you want to see the history every cycle. */

/* #define DEBUG_RC_HISTORY_EXHAUSTIVELY */

/* This is the number of history entries to maintain - by default, 256. */

/* #define RC_HISTORY_MAX 10240 */

/* Define this if you want dhcpd to dump core when a non-fatal memory
   allocation error is detected (i.e., something that would cause a
   memory leak rather than a memory smash). */

/* #define POINTER_DEBUG */

/* Define this if you want debugging output for DHCP failover protocol
   messages. */

/* #define DEBUG_FAILOVER_MESSAGES */

/* Define this if you want debugging output for DHCP failover protocol
   lease assignment timing. */

/* #define DEBUG_FAILOVER_TIMING */

/* Define this if you want all leases written to the lease file, even if
   they are free leases that have never been used. */

/* #define DEBUG_DUMP_ALL_LEASES */

/* Define this if you want DHCP failover protocol support in the DHCP
   server. */

#define FAILOVER_PROTOCOL

/* Define this if you want DNS update functionality to be available. */

#define NSUPDATE

/* Define this if you want the dhcpd.pid file to go somewhere other than
   the default (which varies from system to system, but is usually either
   /etc or /var/run. */

/* #define _PATH_DHCPD_PID	"/var/run/dhcpd.pid" */

/* Define this if you want the dhcpd.leases file (the dynamic lease database)
   to go somewhere other than the default location, which is normally
   /etc/dhcpd.leases. */

/* #define _PATH_DHCPD_DB	"/etc/dhcpd.leases" */

/* Define this if you want the dhcpd.conf file to go somewhere other than
   the default location.   By default, it goes in /etc/dhcpd.conf. */

/* #define _PATH_DHCPD_CONF	"/etc/dhcpd.conf" */

/* Network API definitions.   You do not need to choose one of these - if
   you don't choose, one will be chosen for you in your system's config
   header.    DON'T MESS WITH THIS UNLESS YOU KNOW WHAT YOU'RE DOING!!! */

/* Define this to use the standard BSD socket API.

   On many systems, the BSD socket API does not provide the ability to
   send packets to the 255.255.255.255 broadcast address, which can
   prevent some clients (e.g., Win95) from seeing replies.   This is
   not a problem on Solaris.

   In addition, the BSD socket API will not work when more than one
   network interface is configured on the server.

   However, the BSD socket API is about as efficient as you can get, so if
   the aforementioned problems do not matter to you, or if no other
   API is supported for your system, you may want to go with it. */

/* #define USE_SOCKETS */

/* Define this to use the Sun Streams NIT API.

   The Sun Streams NIT API is only supported on SunOS 4.x releases. */

/* #define USE_NIT */

/* Define this to use the Berkeley Packet Filter API.

   The BPF API is available on all 4.4-BSD derivatives, including
   NetBSD, FreeBSD and BSDI's BSD/OS.   It's also available on
   DEC Alpha OSF/1 in a compatibility mode supported by the Alpha OSF/1
   packetfilter interface. */

/* #define USE_BPF */

/* Define this to use the raw socket API.

   The raw socket API is provided on many BSD derivatives, and provides
   a way to send out raw IP packets.   It is only supported for sending
   packets - packets must be received with the regular socket API.
   This code is experimental - I've never gotten it to actually transmit
   a packet to the 255.255.255.255 broadcast address - so use it at your
   own risk. */

/* #define USE_RAW_SOCKETS */

/* Define this to change the logging facility used by dhcpd. */

/* #define DHCPD_LOG_FACILITY LOG_DAEMON */

/* Define this if you aren't debugging and you want to save memory
   (potentially a _lot_ of memory) by allocating leases in chunks rather
   than one at a time. */

#define COMPACT_LEASES

/* Define this if you want to be able to save and playback server operational
   traces. */

#define TRACING
