; Collection of macros, for GNU Binutils .cpu files. -*- Scheme -*-
;
; Copyright 2000 Free Software Foundation, Inc.
;
; Contributed by Red Hat Inc.
;
; This file is part of the GNU Binutils.
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

; Enums.

; Define a normal enum without using name/value pairs.
; This is currently the same as define-full-enum but it needn't remain
; that way (it's define-full-enum that would change).

(define-pmacro (define-normal-enum name comment attrs prefix vals)
  "\
Define a normal enum, fixed number of arguments.
"
  (define-full-enum name comment attrs prefix vals)
)

; Define a normal insn enum.

(define-pmacro (define-normal-insn-enum name comment attrs prefix fld vals)
  "\
Define a normal instruction opcode enum.
"
  (define-full-insn-enum name comment attrs prefix fld vals)
)

; Instruction fields.

; Normally, fields are unsigned have no encode/decode needs.

(define-pmacro (define-normal-ifield name comment attrs start length)
  "Define a normal instruction field.\n"
  (define-full-ifield name comment attrs start length UINT #f #f)
)

; For those who don't like typing.

(define-pmacro df
  "Shorthand form of define-full-ifield.\n"
  define-full-ifield
)
(define-pmacro dnf
  "Shorthand form of define-normal-ifield.\n"
  define-normal-ifield
)

; Define a normal multi-ifield.
; FIXME: The define-normal version for ifields doesn't include the mode.

(define-pmacro (define-normal-multi-ifield name comment attrs
		 mode subflds insert extract)
  "Define a normal multi-part instruction field.\n"
  (define-full-multi-ifield name comment attrs mode subflds insert extract)
)

; For those who don't like typing.

(define-pmacro dnmf
  "Shorthand form of define-normal-multi-ifield.\n"
  define-normal-multi-ifield
)

; Simple multi-ifields: mode is UINT, default insert/extract support.

(define-pmacro (dsmf name comment attrs subflds)
  "Define a simple multi-part instruction field.\n"
  (define-full-multi-ifield name comment attrs UINT subflds #f #f)
)

; Hardware.

; Simpler version for most hardware elements.
; Allow special assembler support specification but no semantic-name or
; get/set specs.

(define-pmacro (define-normal-hardware name comment attrs type
		 indices values handlers)
  "\
Define a normal hardware element.
"
  (define-full-hardware name comment attrs name type
    indices values handlers () () ())
)

; For those who don't like typing.

(define-pmacro dnh
  "Shorthand form of define-normal-hardware.\n"
  define-normal-hardware
)

; Simpler version of dnh that leaves out the indices, values, handlers,
; get, set, and layout specs.
; This is useful for 1 bit registers.
; ??? While dsh and dnh aren't that distinguishable when perusing a .cpu file,
; they both take a fixed number of positional arguments, and dsh is a proper
; subset of dnh with all arguments in the same positions, so methinks things
; are ok.

(define-pmacro (define-simple-hardware name comment attrs type)
  "\
Define a simple hardware element (usually a scalar register).
"
  (define-full-hardware name comment attrs name type () () () () () ())
)

(define-pmacro dsh
  "Shorthand form of define-simple-hardware.\n"
  define-simple-hardware
)

; Operands.

(define-pmacro (define-normal-operand name comment attrs type index)
  "Define a normal operand.\n"
  (define-full-operand name comment attrs type DFLT index () () ())
)

; For those who don't like typing.
; FIXME: dno?

(define-pmacro dnop
  "Shorthand form of define-normal-operand.\n"
  define-normal-operand
)

(define-pmacro (dndo x-name x-mode x-args
		     x-syntax x-base-ifield x-encoding x-ifield-assertion
		     x-getter x-setter)
  "Define a normal derived operand."
  (define-derived-operand
    (name x-name)
    (mode x-mode)
    (args x-args)
    (syntax x-syntax)
    (base-ifield x-base-ifield)
    (encoding x-encoding)
    (ifield-assertion x-ifield-assertion)
    (getter x-getter)
    (setter x-setter)
    )
)

; Instructions.

; Define an instruction object, normal version.
; At present all fields must be specified.
; Fields ifield-assertion is absent.

(define-pmacro (define-normal-insn name comment attrs syntax fmt semantics timing)
  "Define a normal instruction.\n"
  (define-full-insn name comment attrs syntax fmt () semantics timing)
)

; To reduce the amount of typing.
; Note that this is the same name as the D'ni in MYST.  Oooohhhh.....
; this must be the right way to go. :-)

(define-pmacro dni
  "Shorthand form of define-normal-insn.\n"
  define-normal-insn
)

; Macro instructions.

; Define a macro-insn object, normal version.
; This only supports expanding to one real insn.

(define-pmacro (define-normal-macro-insn name comment attrs syntax expansion)
  "Define a normal macro instruction.\n"
  (define-full-minsn name comment attrs syntax expansion)
)

; To reduce the amount of typing.

(define-pmacro dnmi
  "Shorthand form of define-normal-macro-insn.\n"
  define-normal-macro-insn
)

; Modes.
; ??? Not currently available for use.
;
; Define Normal Mode
;
;(define-pmacro (define-normal-mode name comment attrs bits bytes
;		 non-mode-c-type printf-type sem-mode ptr-to host?)
;  "Define a normal mode.\n"
;  (define-full-mode name comment attrs bits bytes
;    non-mode-c-type printf-type sem-mode ptr-to host?)
;)
;
; For those who don't like typing.
;(define-pmacro dnm
;  "Shorthand form of define-normal-mode.\n"
;  define-normal-mode
;)
