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
\ $FreeBSD$

s" arch-alpha" environment? [if] [if]
	s" loader_version" environment?  [if]
		3 < [if]
			.( Loader version 0.3+ required) cr
			abort
		[then]
	[else]
		.( Could not get loader version!) cr
		abort
	[then]
[then] [then]

s" arch-i386" environment? [if] [if]
	s" loader_version" environment?  [if]
		8 < [if]
			.( Loader version 0.8+ required) cr
			abort
		[then]
	[else]
		.( Could not get loader version!) cr
		abort
	[then]
[then] [then]

include /boot/support.4th

only forth definitions also support-functions

\ ***** boot-conf
\
\	Prepares to boot as specified by loaded configuration files.

also support-functions definitions

: set-tempoptions  ( addrN lenN ... addr1 len1 N -- addr len 1 | 0 )
  \ No options, set the default ones
  dup 0= if
    s" kernel_options" getenv dup -1 = if
      drop
    else
      s" temp_options" setenv
    then
    exit
  then

  \ Skip filename
  2 pick
  c@
  [char] - <> if
    swap >r swap >r
    1 >r  \ Filename present
    1 -   \ One less argument
  else
    0 >r  \ Filename not present
  then

  \ If no other arguments exist, use default options
  ?dup 0= if
    s" kernel_options" getenv dup -1 = if
      drop
    else
      s" temp_options" setenv
    then
    \ Put filename back on the stack, if necessary
    r> if r> r> 1 else 0 then
    exit
  then

  \ Concatenate remaining arguments into a single string
  >r strdup r>
  1 ?do
    \ Allocate new buffer
    2over nip over + 1+
    allocate if out_of_memory throw then
    \ Copy old buffer over
    0 2swap over >r strcat
    \ Free old buffer
    r> free if free_error throw then
    \ Copy a space
    s"  " strcat
    \ Copy next string (do not free)
    2swap strcat
  loop

  \ Set temp_options variable, free whatever memory that needs freeing
  over >r
  s" temp_options" setenv
  r> free if free_error throw then

  \ Put filename back on the stack, if necessary
  r> if r> r> 1 else 0 then
;

: get-arguments ( -- addrN lenN ... addr1 len1 N )
  0
  begin
    \ Get next word on the command line
    parse-word
  ?dup while
    2>r ( push to the rstack, so we can retrieve in the correct order )
    1+
  repeat
  drop ( empty string )
  dup
  begin
    dup
  while
    2r> rot
    >r rot r>
    1 -
  repeat
  drop
;

also builtins

: load-conf  ( args 1 | 0 "args" -- flag )
  0= if ( interpreted ) get-arguments then
  set-tempoptions
  s" temp_options" getenv -1 <> if 2swap 2 else 1 then
  load_kernel_and_modules
;

only forth also support-functions also builtins definitions

: boot
  \ Unload only if a path was passed
  >in @ parse-word rot >in !
  if
    c@ [char] - <> if
      0 1 unload drop
    else
      get-arguments 1 boot exit
    then
  else
    0 1 boot exit
  then
  load-conf
  ?dup 0= if 0 1 boot then
;

: boot-conf
  0 1 unload drop
  load-conf
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

only forth also

