\ Copyright (c) 2000 Daniel C. Sobral <dcs@freebsd.org>
\ All rights reserved.
\
\ Redistribution and use in source and binary forms, with or without
\ modification, are permitted provided that the following conditions
\ are met:
\ 1. Redistributions of source code must retain the above copyright
\    notice, this list of conditions and the following disclaimer.
\ 2. Redistributions in binary form must reproduce the above copyright
\    notice, this list of conditions and the following disclaimer in the
\    documentation and/or other materials provided with the distribution.
\
\ THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
\ ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
\ IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
\ ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
\ FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
\ DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
\ OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
\ HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
\ LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
\ OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
\ SUCH DAMAGE.
\
\ $FreeBSD: src/sys/boot/forth/pnp.4th,v 1.2.32.1 2008/11/25 02:59:29 kensmith Exp $

pnpdevices drop

: enumerate
  pnphandlers begin
    dup @
  while
    ." Probing " dup @ pnph.name @ dup strlen type ." ..." cr
    0 over @ pnph.enumerate @ ccall drop
    cell+
  repeat
;

: summary
  ." PNP scan summary:" cr
  pnpdevices stqh_first @
  begin
    dup
  while
    dup pnpi.ident stqh_first @ pnpid.ident @ dup strlen type
    dup pnpi.desc @ ?dup if
      ."  : "
      dup strlen type
    then
    cr
    pnpi.link stqe_next @
  repeat
  drop
;

: compare-pnpid ( addr addr' -- flag )
  begin
    over c@ over c@ <> if drop drop false exit then
    over c@ over c@ and
  while
    char+ swap char+ swap
  repeat
  c@ swap c@ or 0=
;

: search-pnpid  ( id -- flag )
  >r
  pnpdevices stqh_first @
  begin ( pnpinfo )
    dup
  while
    dup pnpi.ident stqh_first @
    begin ( pnpinfo pnpident )
      dup pnpid.ident @ r@ compare-pnpid
      if
	r> drop
	\ XXX Temporary debugging message
	." Found " pnpid.ident @ dup strlen type
	pnpi.desc @ ?dup if
	  ." : " dup strlen type
	then cr
	\ drop drop
	true
	exit
      then
      pnpid.link stqe_next @
      ?dup 0=
    until
    pnpi.link stqe_next @
  repeat
  r> drop
  drop
  false
;

: skip-space  ( addr -- addr' )
  begin
    dup c@ bl =
    over c@ 9 = or
  while
    char+
  repeat
;

: skip-to-space  ( addr -- addr' )
  begin
    dup c@ bl <>
    over c@ 9 <> and
    over c@ and
  while
    char+
  repeat
;

: premature-end?  ( addr -- addr flag )
  postpone dup postpone c@ postpone 0=
  postpone if postpone exit postpone then
; immediate

0 value filename
0 value timestamp
0 value id

only forth also support-functions

: (load) load ;

: check-pnpid  ( -- )
  line_buffer .addr @ 
  \ Search for filename
  skip-space premature-end?
  dup to filename
  \ Search for end of filename
  skip-to-space premature-end?
  0 over c!  char+
  \ Search for timestamp
  skip-space premature-end?
  dup to timestamp
  skip-to-space premature-end?
  0 over c!  char+
  \ Search for ids
  begin
    skip-space premature-end?
    dup to id
    skip-to-space dup c@ >r
    0 over c!  char+
    id search-pnpid if
      filename dup strlen 1 ['] (load) catch if
	drop drop drop
	." Error loading " filename dup strlen type cr
      then
      r> drop exit
    then
    r> 0=
  until
;

: load-pnp
  0 to end_of_file?
  reset_line_reading
  s" /boot/pnpid.conf" O_RDONLY fopen fd !
  fd @ -1 <> if
    begin
      end_of_file? 0=
    while
      read_line
      check-pnpid
    repeat
    fd @ fclose
  then
;

