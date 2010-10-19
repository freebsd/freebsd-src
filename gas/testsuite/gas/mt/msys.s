;; This file is a set of tests for the MorphoySys instructions.

; Make sure that each mnemonic gives the proper opcode.  Use R0 and #0
; for all operands so that everything but the opcode will be 0 in the
; assembled instructions.

	ldctxt R0,R0,#0,#0,#0
	ldfb R0,R0,#0
	stfb R0, R0, #0
	fbcb R0,#0,#0,#0,#0,#0,#0,#0,#0
	mfbcb R0,#0,R0,#0,#0,#0,#0,#0
	fbcci R0,#0,#0,#0,#0,#0,#0,#0
	fbrci R0,#0,#0,#0,#0,#0,#0,#0
	fbcri R0,#0,#0,#0,#0,#0,#0,#0
	fbrri R0,#0,#0,#0,#0,#0,#0,#0
	mfbcci R0,#0,R0,#0,#0,#0,#0
	mfbrci R0,#0,R0,#0,#0,#0,#0
	mfbcri R0,#0,R0,#0,#0,#0,#0
	mfbrri R0,#0,R0,#0,#0,#0,#0
	fbcbdr R0,#0,R0,#0,#0,#0,#0,#0,#0,#0
	rcfbcb #0,#0,#0,#0,#0,#0,#0,#0,#0,#0
	mrcfbcb R0,#0,#0,#0,#0,#0,#0,#0,#0
	cbcast #0,#0,#0
	dupcbcast #0,#0,#0,#0
	wfbi #0,#0,#0,#0,#0
	wfb R0,R0,#0,#0,#0
	rcrisc R0,#0,R0,#0,#0,#0,#0,#0,#0
	fbcbinc R0, #0, #0, #0, #0, #0, #0, #0
	rcxmode R0, #0, #0, #0, #0, #0, #0, #0, #0

; Check to make sure that the parse routines that allow predifined 
; symbols (uppaer and lower case) to be used for some of the operands.

; dup operand: dup, xx
	si R14
	fbcbdr R0,#0,R0,#0,#0,#0,#0,#0,#dup,#0  ; dup = 1
	fbcbdr R0,#0,R0,#0,#0,#0,#0,#0,#xx,#0   ; xx = 0
	fbcbdr R0,#0,R0,#0,#0,#0,#0,#0,#DUP,#0 
	fbcbdr R0,#0,R0,#0,#0,#0,#0,#0,#XX,#0   

; ball operand: all, one
	si R14
	rcfbcb #0,#0,#all,#0,#0,#0,#0,#0,#0,#0  ; all = 1
	rcfbcb #0,#0,#one,#0,#0,#0,#0,#0,#0,#0  ; one = 0
	rcfbcb #0,#0,#ALL,#0,#0,#0,#0,#0,#0,#0  
	rcfbcb #0,#0,#ONE,#0,#0,#0,#0,#0,#0,#0  

; type operand: odd, even, oe 
	si R14
	mrcfbcb R0,#0,#oe,#0,#0,#0,#0,#0,#0     ; oe = 2
	mrcfbcb R0,#0,#even,#0,#0,#0,#0,#0,#0   ; even = 1
	mrcfbcb R0,#0,#odd,#0,#0,#0,#0,#0,#0    ; odd = 0
	mrcfbcb R0,#0,#OE,#0,#0,#0,#0,#0,#0    
	mrcfbcb R0,#0,#EVEN,#0,#0,#0,#0,#0,#0   
	mrcfbcb R0,#0,#ODD,#0,#0,#0,#0,#0,#0    

; xmode operand: pm, xm
	si R14
	rcxmode R0, #0, #0, #pm, #0, #0, #0, #0, #0  ; pm = 1
	rcxmode R0, #0, #0, #xm, #0, #0, #0, #0, #0  ; xm = 0
	rcxmode R0, #0, #0, #PM, #0, #0, #0, #0, #0  
	rcxmode R0, #0, #0, #XM, #0, #0, #0, #0, #0 

; rc, rc1, rc2 operands: r,c
	si R14
        ldctxt R0,R0,#r,#0,#0            ; rc operand.  r = 1 
        ldctxt R0,R0,#c,#0,#0            ; rc operand.  c = 0 
        ldctxt R0,R0,#R,#0,#0            
        ldctxt R0,R0,#C,#0,#0           
      
	fbcb R0,#0,#0,#0,#r,#0,#0,#0,#0  ; rc1 operand.  r = 1
	fbcb R0,#0,#0,#0,#c,#0,#0,#0,#0  ; rc1 operand.  c = 0

	cbcast #0,#r,#0                  ; rc2 operand.  r = 1
	cbcast #0,#c,#0                  ; rc2 opearnd.  c = 0

; cbrb operand: cb, rb
	si R14
	fbcb R0,#0,#0,#0,#0,#rb,#0,#0,#0  ; rb = 1
	fbcb R0,#0,#0,#0,#0,#cb,#0,#0,#0  ; cb = 0
	fbcb R0,#0,#0,#0,#0,#RB,#0,#0,#0 
	fbcb R0,#0,#0,#0,#0,#CB,#0,#0,#0  

; rbbc operand: rt, br1, br2, cs
	si R14
	fbcb R0,#cs,#0,#0,#0,#0,#0,#0,#0   ; cs = 3
	fbcb R0,#br2,#0,#0,#0,#0,#0,#0,#0  ; br2 = 2
	fbcb R0,#br1,#0,#0,#0,#0,#0,#0,#0  ; br1 = 1
	fbcb R0,#rt,#0,#0,#0,#cb,#0,#0,#0  ; rt = 0
	fbcb R0,#CS,#0,#0,#0,#0,#0,#0,#0  
	fbcb R0,#BR2,#0,#0,#0,#0,#0,#0,#0  
	fbcb R0,#BR1,#0,#0,#0,#0,#0,#0,#0  
	fbcb R0,#RT,#0,#0,#0,#cb,#0,#0,#0  

	intlvr R0, #0, R0, #0, #0
