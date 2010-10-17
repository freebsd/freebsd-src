	.text
	.global _start
_start:	
	mov 0, # external
	mov 0, # global
	mov 0, # local
	
	mov 0, # external - .
	mov 0, # global - .
	mov 0, # local - .

	bc            external
	bc            global
	bc            local
	
	bc rx, #0,    external
	bc rx, #0,    global
	bc rx, #0,    local

	bc r0, #0,    external
	bc r0, #0,    global
	bc r0, #0,    local
	
	bc  r0, r1,   external
	bc  r0, r1,   global
	bc  r0, r1,   local
	.global global
global:
	nop
local:
	nop
