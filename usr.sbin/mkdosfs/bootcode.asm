;;; Hello emacs, this looks like -*- asm -*- code, doesn't it?
;;;
;;; This forms a simple dummy boot program for use with a tool to
;;; format DOS floppies.  All it does is displaying a message, and
;;; recover gracefully by re-initializing the CPU.
;;;
;;; Written by Joerg Wunsch, Dresden.  Placed in the public domain.
;;; This software is provided as is, neither kind of warranty applies.
;;; Use at your own risk.
;;;
;;; (This is written in as86 syntax.  as86 is part of Bruce Evans'
;;; bcc package.)
;;;
;;; $FreeBSD$
;;; 
;;; This code must be linked to address 0x7c00 in order to function
;;; correctly (the BIOS boot address).
;;; 
;;; It's 16-bit code, and we don't care for a data segment.
	use16
	.text

	entry	_begin
_begin:	jmp	init		; jump to boot prog
	nop			; historical baggage ;-)
;;; 
;;; Reserve space for the "BIOS parameter block".
;;; This will be overwritten by the actual formatting routine.
;;; 
bpb:	.ascii	"BSD  4.4"	; "OEM" name
	.word	512		; sector size
	.byte	2		; cluster size
	.word	1		; reserved sectors (just the boot sector)
	.byte	2		; FAT count
	.word	112		; # of entries in root dir
	.word	1440		; total number of sectors, MSDOS 3.3 or below
	.byte	0xf9		; "media descriptor"
	.word	3		; FAT size (sectors)
	.word	9		; sectors per track
	.word	2		; heads per cylinder
	.word	0		; hidden sectors
	;; MSDOS 4.0++ -- only valid iff total number of sectors == 0
	.word	0		; unused
	.long	0		; total number of sectors
	.short	0		; physical drive (0, 1, ..., 0x80) %-)
	.byte	0		; "extented boot signature"
	.long	0		; volume serial number (i.e., garbage :)
	.ascii	"           "	; label -- same as vol label in root dir
	.ascii	"FAT12   "	; file system ID
;;;
;;; Executable code starts here.
;;; 
init:
	;; First, display our message.
	mov	si, *message
lp1:	seg	cs
	movb	al, [si]
	inc	si
	testb	al, al
	jz	lp2		; null-terminated string
	movb	bl, *7		; display with regular attribute
	movb	ah, *0x0e	; int 0x10, fnc 0x0e -- emulate tty
	int	0x10
	jmp	lp1
lp2:	xorb	ah, ah		; int 0x16, fnc 0x00 -- wait for keystroke
	int	0x16
	mov	ax, *0x40	; write 0x1234 to address 0x472 --
	push	ax		; tell the BIOS that this is a warm boot
	pop	dx
	mov	0x72, *0x1234
	jmpf	0xfff0,0xf000	; jump to CPU initialization code

message:
	.byte	7
	.byte	0xc9
	.byte	0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd
	.byte	0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd
	.byte	0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd
	.byte	0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd
	.byte	0xcd,0xcd,0xcd,0xcd,0xcd,0xcd
	.byte	0xbb, 13, 10, 0xba
	.ascii	"  Sorry, this disc does actually not contain  "
	.byte	0xba, 13, 10, 0xba
	.ascii	"  a bootable system.                          "
	.byte	0xba, 13, 10, 0xba
	.ascii	"  Press any key to reboot.                    "
	.byte	0xba, 13, 10, 0xc8
	.byte	0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd
	.byte	0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd
	.byte	0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd
	.byte	0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd
	.byte	0xcd,0xcd,0xcd,0xcd,0xcd,0xcd
	.byte	0xbc, 13,10
	.byte	0

	;; Adjust the value below after changing the length of
	;; the code above!
	.space	0x1fe-0x161	; pad to 512 bytes

	.byte	0x55, 0xaa	; yes, we are bootable (cheating :)
	end
	