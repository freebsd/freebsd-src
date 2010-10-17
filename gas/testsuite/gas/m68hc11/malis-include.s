;;
;; This file verifies the compliance with the Motorola specification:
;; 
;; MOTOROLA STANDARDS
;; Document #1001, Version 1.0
;; SPECIFICATION FOR Motorola 8- and 16-Bit ASSEMBLY LANGUAGE INPUT STANDARD
;; 26, October 1999
;;
;; Available at:
;; 
;; http://www.mcu.motsps.com/dev_tools/hc12/eabi/m8-16alis.pdf
;;
;; Lines starting with '#' represent instructions that fail in GAS.
;;
;;
;;	Section 8.2.12.6 Include File - include
	section .text
value:	set	1
	ldy	#value
