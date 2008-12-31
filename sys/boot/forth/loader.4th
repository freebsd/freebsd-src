\ Copyright (c) 1999 Daniel C. Sobral <dcs@freebsd.org>
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
\ $FreeBSD: src/sys/boot/forth/loader.4th,v 1.25.6.1 2008/11/25 02:59:29 kensmith Exp $

s" arch-i386" environment? [if] [if]
	s" loader_version" environment?  [if]
		11 < [if]
			.( Loader version 1.1+ required) cr
			abort
		[then]
	[else]
		.( Could not get loader version!) cr
		abort
	[then]
[then] [then]

256 dictthreshold !  \ 256 cells minimum free space
2048 dictincrease !  \ 2048 additional cells each time

include /boot/support.4th

\ ***** boot-conf
\
\	Prepares to boot as specified by loaded configuration files.

only forth also support-functions also builtins definitions

: boot
  0= if ( interpreted ) get_arguments then

  \ Unload only if a path was passed
  dup if
    >r over r> swap
    c@ [char] - <> if
      0 1 unload drop
    else
      s" kernelname" getenv? if ( a kernel has been loaded )
        1 boot exit
      then
      load_kernel_and_modules
      ?dup if exit then
      0 1 boot exit
    then
  else
    s" kernelname" getenv? if ( a kernel has been loaded )
      1 boot exit
    then
    load_kernel_and_modules
    ?dup if exit then
    0 1 boot exit
  then
  load_kernel_and_modules
  ?dup 0= if 0 1 boot then
;

: boot-conf
  0= if ( interpreted ) get_arguments then
  0 1 unload drop
  load_kernel_and_modules
  ?dup 0= if 0 1 autoboot then
;

also forth definitions also builtins

builtin: boot
builtin: boot-conf

only forth definitions also support-functions

\ ***** check-password
\
\	If a password was defined, execute autoboot and ask for
\	password if autoboot returns.

: check-password
  password .addr @ if
    0 autoboot
    false >r
    begin
      bell emit bell emit
      ." Password: "
      password .len @ read-password
      dup password .len @ = if
        2dup password .addr @ password .len @
        compare 0= if r> drop true >r then
      then
      drop free drop
      r@
    until
    r> drop
  then
;

\ ***** start
\
\       Initializes support.4th global variables, sets loader_conf_files,
\       process conf files, and, if any one such file was succesfully
\       read to the end, load kernel and modules.

: start  ( -- ) ( throws: abort & user-defined )
  s" /boot/defaults/loader.conf" initialize
  include_conf_files
  include_nextboot_file
  \ Will *NOT* try to load kernel and modules if no configuration file
  \ was succesfully loaded!
  any_conf_read? if
    load_kernel
    load_modules
  then
;

\ ***** initialize
\
\	Overrides support.4th initialization word with one that does
\	everything start one does, short of loading the kernel and
\	modules. Returns a flag

: initialize ( -- flag )
  s" /boot/defaults/loader.conf" initialize
  include_conf_files
  include_nextboot_file
  any_conf_read?
;

\ ***** read-conf
\
\	Read a configuration file, whose name was specified on the command
\	line, if interpreted, or given on the stack, if compiled in.

: (read-conf)  ( addr len -- )
  conf_files .addr @ ?dup if free abort" Fatal error freeing memory" then
  strdup conf_files .len ! conf_files .addr !
  include_conf_files \ Will recurse on new loader_conf_files definitions
;

: read-conf  ( <filename> | addr len -- ) ( throws: abort & user-defined )
  state @ if
    \ Compiling
    postpone (read-conf)
  else
    \ Interpreting
    bl parse (read-conf)
  then
; immediate

\ ***** enable-module
\
\       Turn a module loading on.

: enable-module ( <module> -- )
  bl parse module_options @ >r
  begin
    r@
  while
    2dup
    r@ module.name dup .addr @ swap .len @
    compare 0= if
      2drop
      r@ module.name dup .addr @ swap .len @ type
      true r> module.flag !
      ."  will be loaded." cr
      exit
    then
    r> module.next @ >r
  repeat
  r> drop
  type ."  wasn't found." cr
;

\ ***** disable-module
\
\       Turn a module loading off.

: disable-module ( <module> -- )
  bl parse module_options @ >r
  begin
    r@
  while
    2dup
    r@ module.name dup .addr @ swap .len @
    compare 0= if
      2drop
      r@ module.name dup .addr @ swap .len @ type
      false r> module.flag !
      ."  will not be loaded." cr
      exit
    then
    r> module.next @ >r
  repeat
  r> drop
  type ."  wasn't found." cr
;

\ ***** toggle-module
\
\       Turn a module loading on/off.

: toggle-module ( <module> -- )
  bl parse module_options @ >r
  begin
    r@
  while
    2dup
    r@ module.name dup .addr @ swap .len @
    compare 0= if
      2drop
      r@ module.name dup .addr @ swap .len @ type
      r@ module.flag @ 0= dup r> module.flag !
      if
        ."  will be loaded." cr
      else
        ."  will not be loaded." cr
      then
      exit
    then
    r> module.next @ >r
  repeat
  r> drop
  type ."  wasn't found." cr
;

\ ***** show-module
\
\	Show loading information about a module.

: show-module ( <module> -- )
  bl parse module_options @ >r
  begin
    r@
  while
    2dup
    r@ module.name dup .addr @ swap .len @
    compare 0= if
      2drop
      ." Name: " r@ module.name dup .addr @ swap .len @ type cr
      ." Path: " r@ module.loadname dup .addr @ swap .len @ type cr
      ." Type: " r@ module.type dup .addr @ swap .len @ type cr
      ." Flags: " r@ module.args dup .addr @ swap .len @ type cr
      ." Before load: " r@ module.beforeload dup .addr @ swap .len @ type cr
      ." After load: " r@ module.afterload dup .addr @ swap .len @ type cr
      ." Error: " r@ module.loaderror dup .addr @ swap .len @ type cr
      ." Status: " r> module.flag @ if ." Load" else ." Don't load" then cr
      exit
    then
    r> module.next @ >r
  repeat
  r> drop
  type ."  wasn't found." cr
;

\ Words to be used inside configuration files

: retry false ;         \ For use in load error commands
: ignore true ;         \ For use in load error commands

\ Return to strict forth vocabulary

: #type
  over - >r
  type
  r> spaces
;

: .? 2 spaces 2swap 15 #type 2 spaces type cr ;

: ?
  ['] ? execute
  s" boot-conf" s" load kernel and modules, then autoboot" .?
  s" read-conf" s" read a configuration file" .?
  s" enable-module" s" enable loading of a module" .?
  s" disable-module" s" disable loading of a module" .?
  s" toggle-module" s" toggle loading of a module" .?
  s" show-module" s" show module load data" .?
;

only forth also

