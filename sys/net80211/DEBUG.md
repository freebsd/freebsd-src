# Debugging in net80211

This document describes how debugging is implemented in net80211.

## Overview

net80211 has run-time configurable debugging available. It is configured
per-VAP.  It is implemented as a bitmask which can be controlled via a
sysctl at runtime.

Debugging is compiled in when IEEE80211_DEBUG is defined.

There is currently no global debugging API available; top-level net80211
code is typically using printf() or some wrapper around it (eg
net80211_printf).

The debug API is defined in (ieee80211_var.h).  This includes the
debug field definitions and exported debugging API.  The actual implementation
of the debugging routines is currently in (ieee80211_input.c) - see
(ieee80211_note) for an example.

The bitmap of available debugging sections is in (ieee80211_var.h), prefixed
with IEEE80211_MSG .  See (IEEE80211_MSG_DEBUG) for an example.

## Usage

Calls to the debugging APIs should not include a terminating '\n' character.
This will be added by the debug call.

The simplest example is a call to IEEE80211_DPRINTF().  This takes a vap
pointer, which debug option to log to, then the format string and optional
arguments.  For example:

```
IEEE80211_DPRINTF(vap, IEEE80211_MSG_11N, "%s: called!", __func__);
```

The debug flags can be combined together using bitwise OR so they are
emitted if one or more debug options are set, for example:

```
IEEE80211_DPRINTF(vap, IEEE80211_MSG_11N | IEEE80211_MSG_ASSOC,
    "%s: called!", __func__);
```

There are a number of different debugging calls that are designed to be
used in different contexts.  Although they all currently end up printing
to the same debug output, keeping them separate allows for future
behavioural changes whilst minimising rototilling the whole codebase (eg
allowing non-DPRINTF to turn into event tracing.)

 * Straight up debugging should be done through IEEE80211_DPRINTF() .
 * Debugging that's related to a specific ieee80211_node (eg a state
   change for a specific node) should be done via a call to IEEE80211_NOTE() .
 * Debugging that's related to a specific ethernet MAC address (eg
   scan results) should be done via a call to IEEE80211_NOTE_MAC() .
 * Debugging that should include a frame header should be done via
   a call to IEEE80211_NOTE_FRAME().  Note this takes a (struct ieee80211_frame \*)
   pointer.
 * Debugging involving discarding frames (eg invalid frames) should be
   done via a call to IEEE80211_DISCARD() .
 * Debugging involving discarding frames due to an invalid / bad IE should
   be done via a call to IEEE80211_DISCARD_IE().
 * Debugging involving discarding frames due to a MAC address (eg ACL failure)
   should be done via a call to IEEE80211_DISCARD_MAC().

## Usage Notes

 * It is required that the debugging be compiled in/out purely by defining or not
   defining IEEE80211_DEBUG.  This can often trip up unused variable warnings
   when debugging is disabled, so just double-check both configurations.

 * It is important to ensure calls to the debugging (and any other logging API)
   do not change any state/variables.  For example, do not call a function that
   updates some counter or some state variable inside a call to IEEE80211_DPRINTF().
   It won't be called at best and it will just be compiled out entirely at worst.

## Configuration

 * The 'vap->iv_debug' field is controlled by the OS specific module.
 * In FreeBSD (ieee80211_freebsd.c) it is assigned a sysctl (net.wlan.X.debug)
   during (ieee80211_sysctl_vattach).
 * FreeBSD ships the wlandebug(8) tool to query and set this at runtime.

## Implementation Details

* The debug API goes out of its way to do the debug flag check before evaluating
  function parameters and potentially assembling the logging output. See
  (IEEE80211_DPRINTF) for an example.

## Future work

 * Top-level net80211 debugging APIs and control would be nice (for things
   that are not specific to a VAP.)
 * Drivers end up having to implement their own debugging API; it may be nice
   to provide drivers a net80211 API to do their own driver specific logging.
 * The debug macros should likely be refactored out to a new header file,
   separate from ieee80211_var.h, so they can be more easily referenced.
 * The debug fields should likely be refactored out into a new separate header
   file that is designed to be consumed both by the kernel and by userland
   utilities wishing to query/set the debug bitmask.
