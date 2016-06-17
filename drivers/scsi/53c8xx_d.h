/* DO NOT EDIT - Generated automatically by script_asm.pl */
static u32 SCRIPT[] = {
/*


; NCR 53c810 driver, main script
; Sponsored by 
;	iX Multiuser Multitasking Magazine
;	hm@ix.de
;
; Copyright 1993, 1994, 1995 Drew Eckhardt
;      Visionary Computing 
;      (Unix and Linux consulting and custom programming)
;      drew@PoohSticks.ORG
;      +1 (303) 786-7975
;
; TolerANT and SCSI SCRIPTS are registered trademarks of NCR Corporation.
;
; PRE-ALPHA
;
; For more information, please consult 
;
; NCR 53C810
; PCI-SCSI I/O Processor
; Data Manual
;
; NCR 53C710 
; SCSI I/O Processor
; Programmers Guide
;
; NCR Microelectronics
; 1635 Aeroplaza Drive
; Colorado Springs, CO 80916
; 1+ (719) 578-3400
;
; Toll free literature number
; +1 (800) 334-5454
;
; IMPORTANT : This code is self modifying due to the limitations of 
;	the NCR53c7,8xx series chips.  Persons debugging this code with
;	the remote debugger should take this into account, and NOT set
;	breakpoints in modified instructions.
;
; Design:
; The NCR53c7,8xx family of SCSI chips are busmasters with an onboard 
; microcontroller using a simple instruction set.   
;
; So, to minimize the effects of interrupt latency, and to maximize 
; throughput, this driver offloads the practical maximum amount 
; of processing to the SCSI chip while still maintaining a common
; structure.
;
; Where tradeoffs were needed between efficiency on the older
; chips and the newer NCR53c800 series, the NCR53c800 series 
; was chosen.
;
; While the NCR53c700 and NCR53c700-66 lacked the facilities to fully
; automate SCSI transfers without host processor intervention, this 
; isn't the case with the NCR53c710 and newer chips which allow 
;
; - reads and writes to the internal registers from within the SCSI
; 	scripts, allowing the SCSI SCRIPTS(tm) code to save processor
; 	state so that multiple threads of execution are possible, and also
; 	provide an ALU for loop control, etc.
; 
; - table indirect addressing for some instructions. This allows 
;	pointers to be located relative to the DSA ((Data Structure
;	Address) register.
;
; These features make it possible to implement a mailbox style interface,
; where the same piece of code is run to handle I/O for multiple threads
; at once minimizing our need to relocate code.  Since the NCR53c700/
; NCR53c800 series have a unique combination of features, making a 
; a standard ingoing/outgoing mailbox system, costly, I've modified it.
;
; - Mailboxes are a mixture of code and data.  This lets us greatly
; 	simplify the NCR53c810 code and do things that would otherwise
;	not be possible.
;
; The saved data pointer is now implemented as follows :
;
; 	Control flow has been architected such that if control reaches
;	munge_save_data_pointer, on a restore pointers message or 
;	reconnection, a jump to the address formerly in the TEMP register
;	will allow the SCSI command to resume execution.
;

;
; Note : the DSA structures must be aligned on 32 bit boundaries,
; since the source and destination of MOVE MEMORY instructions 
; must share the same alignment and this is the alignment of the
; NCR registers.
;

ABSOLUTE dsa_temp_lun = 0		; Patch to lun for current dsa
ABSOLUTE dsa_temp_next = 0		; Patch to dsa next for current dsa
ABSOLUTE dsa_temp_addr_next = 0		; Patch to address of dsa next address 
					; 	for current dsa
ABSOLUTE dsa_temp_sync = 0		; Patch to address of per-target
					;	sync routine
ABSOLUTE dsa_temp_target = 0		; Patch to id for current dsa
ABSOLUTE dsa_temp_addr_saved_pointer = 0; Patch to address of per-command
					; 	saved data pointer
ABSOLUTE dsa_temp_addr_residual = 0	; Patch to address of per-command
					;	current residual code
ABSOLUTE dsa_temp_addr_saved_residual = 0; Patch to address of per-command
					; saved residual code
ABSOLUTE dsa_temp_addr_new_value = 0	; Address of value for JUMP operand
ABSOLUTE dsa_temp_addr_array_value = 0 	; Address to copy to
ABSOLUTE dsa_temp_addr_dsa_value = 0	; Address of this DSA value

;
; Once a device has initiated reselection, we need to compare it 
; against the singly linked list of commands which have disconnected
; and are pending reselection.  These commands are maintained in 
; an unordered singly linked list of DSA structures, through the
; DSA pointers at their 'centers' headed by the reconnect_dsa_head
; pointer.
; 
; To avoid complications in removing commands from the list,
; I minimize the amount of expensive (at eight operations per
; addition @ 500-600ns each) pointer operations which must
; be done in the NCR driver by precomputing them on the 
; host processor during dsa structure generation.
;
; The fixed-up per DSA code knows how to recognize the nexus
; associated with the corresponding SCSI command, and modifies
; the source and destination pointers for the MOVE MEMORY 
; instruction which is executed when reselected_ok is called
; to remove the command from the list.  Similarly, DSA is 
; loaded with the address of the next DSA structure and
; reselected_check_next is called if a failure occurs.
;
; Perhaps more concisely, the net effect of the mess is 
;
; for (dsa = reconnect_dsa_head, dest = &reconnect_dsa_head, 
;     src = NULL; dsa; dest = &dsa->next, dsa = dsa->next) {
; 	src = &dsa->next;
; 	if (target_id == dsa->id && target_lun == dsa->lun) {
; 		*dest = *src;
; 		break;
;         }	
; }
;
; if (!dsa)
;           error (int_err_unexpected_reselect);
; else  
;     longjmp (dsa->jump_resume, 0);
;
; 	


; Define DSA structure used for mailboxes
ENTRY dsa_code_template
dsa_code_template:
ENTRY dsa_code_begin
dsa_code_begin:
	MOVE dmode_memory_to_ncr TO DMODE

at 0x00000000 : */	0x78380000,0x00000000,
/*
	MOVE MEMORY 4, dsa_temp_addr_dsa_value, addr_scratch

at 0x00000002 : */	0xc0000004,0x00000000,0x00000000,
/*
	MOVE dmode_memory_to_memory TO DMODE

at 0x00000005 : */	0x78380000,0x00000000,
/*
	CALL scratch_to_dsa

at 0x00000007 : */	0x88080000,0x00000980,
/*
	CALL select

at 0x00000009 : */	0x88080000,0x000001fc,
/*
; Handle the phase mismatch which may have resulted from the 
; MOVE FROM dsa_msgout if we returned here.  The CLEAR ATN 
; may or may not be necessary, and we should update script_asm.pl
; to handle multiple pieces.
    CLEAR ATN

at 0x0000000b : */	0x60000008,0x00000000,
/*
    CLEAR ACK

at 0x0000000d : */	0x60000040,0x00000000,
/*

; Replace second operand with address of JUMP instruction dest operand
; in schedule table for this DSA.  Becomes dsa_jump_dest in 53c7,8xx.c.
ENTRY dsa_code_fix_jump
dsa_code_fix_jump:
	MOVE MEMORY 4, NOP_insn, 0

at 0x0000000f : */	0xc0000004,0x00000000,0x00000000,
/*
	JUMP select_done

at 0x00000012 : */	0x80080000,0x00000224,
/*

; wrong_dsa loads the DSA register with the value of the dsa_next
; field.
;
wrong_dsa:
;		Patch the MOVE MEMORY INSTRUCTION such that 
;		the destination address is the address of the OLD 
;		next pointer.
;
	MOVE MEMORY 4, dsa_temp_addr_next, reselected_ok + 8

at 0x00000014 : */	0xc0000004,0x00000000,0x00000758,
/*
	MOVE dmode_memory_to_ncr TO DMODE	

at 0x00000017 : */	0x78380000,0x00000000,
/*
;
; 	Move the _contents_ of the next pointer into the DSA register as 
;	the next I_T_L or I_T_L_Q tupple to check against the established
;	nexus.
;
	MOVE MEMORY 4, dsa_temp_next, addr_scratch

at 0x00000019 : */	0xc0000004,0x00000000,0x00000000,
/*
	MOVE dmode_memory_to_memory TO DMODE

at 0x0000001c : */	0x78380000,0x00000000,
/*
	CALL scratch_to_dsa

at 0x0000001e : */	0x88080000,0x00000980,
/*
	JUMP reselected_check_next

at 0x00000020 : */	0x80080000,0x000006a4,
/*

ABSOLUTE dsa_save_data_pointer = 0
ENTRY dsa_code_save_data_pointer
dsa_code_save_data_pointer:
    	MOVE dmode_ncr_to_memory TO DMODE

at 0x00000022 : */	0x78380000,0x00000000,
/*
    	MOVE MEMORY 4, addr_temp, dsa_temp_addr_saved_pointer

at 0x00000024 : */	0xc0000004,0x00000000,0x00000000,
/*
    	MOVE dmode_memory_to_memory TO DMODE

at 0x00000027 : */	0x78380000,0x00000000,
/*
; HARD CODED : 24 bytes needs to agree with 53c7,8xx.h
    	MOVE MEMORY 24, dsa_temp_addr_residual, dsa_temp_addr_saved_residual

at 0x00000029 : */	0xc0000018,0x00000000,0x00000000,
/*
        CLEAR ACK

at 0x0000002c : */	0x60000040,0x00000000,
/*



    	RETURN

at 0x0000002e : */	0x90080000,0x00000000,
/*
ABSOLUTE dsa_restore_pointers = 0
ENTRY dsa_code_restore_pointers
dsa_code_restore_pointers:
    	MOVE dmode_memory_to_ncr TO DMODE

at 0x00000030 : */	0x78380000,0x00000000,
/*
    	MOVE MEMORY 4, dsa_temp_addr_saved_pointer, addr_temp

at 0x00000032 : */	0xc0000004,0x00000000,0x00000000,
/*
    	MOVE dmode_memory_to_memory TO DMODE

at 0x00000035 : */	0x78380000,0x00000000,
/*
; HARD CODED : 24 bytes needs to agree with 53c7,8xx.h
    	MOVE MEMORY 24, dsa_temp_addr_saved_residual, dsa_temp_addr_residual

at 0x00000037 : */	0xc0000018,0x00000000,0x00000000,
/*
        CLEAR ACK

at 0x0000003a : */	0x60000040,0x00000000,
/*



    	RETURN

at 0x0000003c : */	0x90080000,0x00000000,
/*

ABSOLUTE dsa_check_reselect = 0
; dsa_check_reselect determines whether or not the current target and
; lun match the current DSA
ENTRY dsa_code_check_reselect
dsa_code_check_reselect:
	MOVE SSID TO SFBR		; SSID contains 3 bit target ID

at 0x0000003e : */	0x720a0000,0x00000000,
/*
; FIXME : we need to accommodate bit fielded and binary here for '7xx/'8xx chips
	JUMP REL (wrong_dsa), IF NOT dsa_temp_target, AND MASK 0xf8

at 0x00000040 : */	0x8084f800,0x00ffff48,
/*
;
; Hack - move to scratch first, since SFBR is not writeable
; 	via the CPU and hence a MOVE MEMORY instruction.
;
	MOVE dmode_memory_to_ncr TO DMODE

at 0x00000042 : */	0x78380000,0x00000000,
/*
	MOVE MEMORY 1, reselected_identify, addr_scratch

at 0x00000044 : */	0xc0000001,0x00000000,0x00000000,
/*
	MOVE dmode_memory_to_memory TO DMODE

at 0x00000047 : */	0x78380000,0x00000000,
/*
	MOVE SCRATCH0 TO SFBR

at 0x00000049 : */	0x72340000,0x00000000,
/*
; FIXME : we need to accommodate bit fielded and binary here for '7xx/'8xx chips
	JUMP REL (wrong_dsa), IF NOT dsa_temp_lun, AND MASK 0xf8

at 0x0000004b : */	0x8084f800,0x00ffff1c,
/*
;		Patch the MOVE MEMORY INSTRUCTION such that
;		the source address is the address of this dsa's
;		next pointer.
	MOVE MEMORY 4, dsa_temp_addr_next, reselected_ok + 4

at 0x0000004d : */	0xc0000004,0x00000000,0x00000754,
/*
	CALL reselected_ok

at 0x00000050 : */	0x88080000,0x00000750,
/*
	CALL dsa_temp_sync	

at 0x00000052 : */	0x88080000,0x00000000,
/*
; Release ACK on the IDENTIFY message _after_ we've set the synchronous 
; transfer parameters! 
	CLEAR ACK

at 0x00000054 : */	0x60000040,0x00000000,
/*
; Implicitly restore pointers on reselection, so a RETURN
; will transfer control back to the right spot.
    	CALL REL (dsa_code_restore_pointers)

at 0x00000056 : */	0x88880000,0x00ffff60,
/*
    	RETURN

at 0x00000058 : */	0x90080000,0x00000000,
/*
ENTRY dsa_zero
dsa_zero:
ENTRY dsa_code_template_end
dsa_code_template_end:

; Perform sanity check for dsa_fields_start == dsa_code_template_end - 
; dsa_zero, puke.

ABSOLUTE dsa_fields_start =  0	; Sanity marker
				; 	pad 48 bytes (fix this RSN)
ABSOLUTE dsa_next = 48		; len 4 Next DSA
 				; del 4 Previous DSA address
ABSOLUTE dsa_cmnd = 56		; len 4 Scsi_Cmnd * for this thread.
ABSOLUTE dsa_select = 60	; len 4 Device ID, Period, Offset for 
			 	;	table indirect select
ABSOLUTE dsa_msgout = 64	; len 8 table indirect move parameter for 
				;       select message
ABSOLUTE dsa_cmdout = 72	; len 8 table indirect move parameter for 
				;	command
ABSOLUTE dsa_dataout = 80	; len 4 code pointer for dataout
ABSOLUTE dsa_datain = 84	; len 4 code pointer for datain
ABSOLUTE dsa_msgin = 88		; len 8 table indirect move for msgin
ABSOLUTE dsa_status = 96 	; len 8 table indirect move for status byte
ABSOLUTE dsa_msgout_other = 104	; len 8 table indirect for normal message out
				; (Synchronous transfer negotiation, etc).
ABSOLUTE dsa_end = 112

ABSOLUTE schedule = 0 		; Array of JUMP dsa_begin or JUMP (next),
				; terminated by a call to JUMP wait_reselect

; Linked lists of DSA structures
ABSOLUTE reconnect_dsa_head = 0	; Link list of DSAs which can reconnect
ABSOLUTE addr_reconnect_dsa_head = 0 ; Address of variable containing
				; address of reconnect_dsa_head

; These select the source and destination of a MOVE MEMORY instruction
ABSOLUTE dmode_memory_to_memory = 0x0
ABSOLUTE dmode_memory_to_ncr = 0x0
ABSOLUTE dmode_ncr_to_memory = 0x0

ABSOLUTE addr_scratch = 0x0
ABSOLUTE addr_temp = 0x0


; Interrupts - 
; MSB indicates type
; 0	handle error condition
; 1 	handle message 
; 2 	handle normal condition
; 3	debugging interrupt
; 4 	testing interrupt 
; Next byte indicates specific error

; XXX not yet implemented, I'm not sure if I want to - 
; Next byte indicates the routine the error occurred in
; The LSB indicates the specific place the error occurred
 
ABSOLUTE int_err_unexpected_phase = 0x00000000	; Unexpected phase encountered
ABSOLUTE int_err_selected = 0x00010000		; SELECTED (nee RESELECTED)
ABSOLUTE int_err_unexpected_reselect = 0x00020000 
ABSOLUTE int_err_check_condition = 0x00030000	
ABSOLUTE int_err_no_phase = 0x00040000
ABSOLUTE int_msg_wdtr = 0x01000000		; WDTR message received
ABSOLUTE int_msg_sdtr = 0x01010000		; SDTR received
ABSOLUTE int_msg_1 = 0x01020000			; single byte special message
						; received

ABSOLUTE int_norm_select_complete = 0x02000000	; Select complete, reprogram
						; registers.
ABSOLUTE int_norm_reselect_complete = 0x02010000	; Nexus established
ABSOLUTE int_norm_command_complete = 0x02020000 ; Command complete
ABSOLUTE int_norm_disconnected = 0x02030000	; Disconnected 
ABSOLUTE int_norm_aborted =0x02040000		; Aborted *dsa
ABSOLUTE int_norm_reset = 0x02050000		; Generated BUS reset.
ABSOLUTE int_debug_break = 0x03000000		; Break point

ABSOLUTE int_debug_panic = 0x030b0000		; Panic driver


ABSOLUTE int_test_1 = 0x04000000		; Test 1 complete
ABSOLUTE int_test_2 = 0x04010000		; Test 2 complete
ABSOLUTE int_test_3 = 0x04020000		; Test 3 complete


; These should start with 0x05000000, with low bits incrementing for 
; each one.


						
ABSOLUTE NCR53c7xx_msg_abort = 0	; Pointer to abort message
ABSOLUTE NCR53c7xx_msg_reject = 0       ; Pointer to reject message
ABSOLUTE NCR53c7xx_zero	= 0		; long with zero in it, use for source
ABSOLUTE NCR53c7xx_sink = 0		; long to dump worthless data in
ABSOLUTE NOP_insn = 0			; NOP instruction

; Pointer to message, potentially multi-byte
ABSOLUTE msg_buf = 0

; Pointer to holding area for reselection information
ABSOLUTE reselected_identify = 0
ABSOLUTE reselected_tag = 0

; Request sense command pointer, it's a 6 byte command, should
; be constant for all commands since we always want 16 bytes of 
; sense and we don't need to change any fields as we did under 
; SCSI-I when we actually cared about the LUN field.
;EXTERNAL NCR53c7xx_sense		; Request sense command


; dsa_schedule  
; PURPOSE : after a DISCONNECT message has been received, and pointers
;	saved, insert the current DSA structure at the head of the 
; 	disconnected queue and fall through to the scheduler.
;
; CALLS : OK
;
; INPUTS : dsa - current DSA structure, reconnect_dsa_head - list
;	of disconnected commands
;
; MODIFIES : SCRATCH, reconnect_dsa_head
; 
; EXITS : always passes control to schedule

ENTRY dsa_schedule
dsa_schedule:




;
; Calculate the address of the next pointer within the DSA 
; structure of the command that is currently disconnecting
;
    CALL dsa_to_scratch

at 0x0000005a : */	0x88080000,0x00000938,
/*
    MOVE SCRATCH0 + dsa_next TO SCRATCH0

at 0x0000005c : */	0x7e343000,0x00000000,
/*
    MOVE SCRATCH1 + 0 TO SCRATCH1 WITH CARRY

at 0x0000005e : */	0x7f350000,0x00000000,
/*
    MOVE SCRATCH2 + 0 TO SCRATCH2 WITH CARRY

at 0x00000060 : */	0x7f360000,0x00000000,
/*
    MOVE SCRATCH3 + 0 TO SCRATCH3 WITH CARRY

at 0x00000062 : */	0x7f370000,0x00000000,
/*

; Point the next field of this DSA structure at the current disconnected 
; list
    MOVE dmode_ncr_to_memory TO DMODE

at 0x00000064 : */	0x78380000,0x00000000,
/*
    MOVE MEMORY 4, addr_scratch, dsa_schedule_insert + 8

at 0x00000066 : */	0xc0000004,0x00000000,0x000001b4,
/*
    MOVE dmode_memory_to_memory TO DMODE

at 0x00000069 : */	0x78380000,0x00000000,
/*
dsa_schedule_insert:
    MOVE MEMORY 4, reconnect_dsa_head, 0 

at 0x0000006b : */	0xc0000004,0x00000000,0x00000000,
/*

; And update the head pointer.
    CALL dsa_to_scratch

at 0x0000006e : */	0x88080000,0x00000938,
/*
    MOVE dmode_ncr_to_memory TO DMODE	

at 0x00000070 : */	0x78380000,0x00000000,
/*
    MOVE MEMORY 4, addr_scratch, reconnect_dsa_head

at 0x00000072 : */	0xc0000004,0x00000000,0x00000000,
/*
    MOVE dmode_memory_to_memory TO DMODE

at 0x00000075 : */	0x78380000,0x00000000,
/*


    MOVE SCNTL2 & 0x7f TO SCNTL2

at 0x00000077 : */	0x7c027f00,0x00000000,
/*
    CLEAR ACK

at 0x00000079 : */	0x60000040,0x00000000,
/*

    WAIT DISCONNECT

at 0x0000007b : */	0x48000000,0x00000000,
/*






    JUMP schedule

at 0x0000007d : */	0x80080000,0x00000000,
/*


;
; select
;
; PURPOSE : establish a nexus for the SCSI command referenced by DSA.
;	On success, the current DSA structure is removed from the issue 
;	queue.  Usually, this is entered as a fall-through from schedule,
;	although the contingent allegiance handling code will write
;	the select entry address to the DSP to restart a command as a 
;	REQUEST SENSE.  A message is sent (usually IDENTIFY, although
;	additional SDTR or WDTR messages may be sent).  COMMAND OUT
;	is handled.
;
; INPUTS : DSA - SCSI command, issue_dsa_head
;
; CALLS : NOT OK
;
; MODIFIES : SCRATCH, issue_dsa_head
;
; EXITS : on reselection or selection, go to select_failed
;	otherwise, RETURN so control is passed back to 
;	dsa_begin.
;

ENTRY select
select:












    CLEAR TARGET

at 0x0000007f : */	0x60000200,0x00000000,
/*

; XXX
;
; In effect, SELECTION operations are backgrounded, with execution
; continuing until code which waits for REQ or a fatal interrupt is 
; encountered.
;
; So, for more performance, we could overlap the code which removes 
; the command from the NCRs issue queue with the selection, but 
; at this point I don't want to deal with the error recovery.
;


    SELECT ATN FROM dsa_select, select_failed

at 0x00000081 : */	0x4300003c,0x000007a4,
/*
    JUMP select_msgout, WHEN MSG_OUT

at 0x00000083 : */	0x860b0000,0x00000214,
/*
ENTRY select_msgout
select_msgout:
    MOVE FROM dsa_msgout, WHEN MSG_OUT

at 0x00000085 : */	0x1e000000,0x00000040,
/*










   RETURN

at 0x00000087 : */	0x90080000,0x00000000,
/*

; 
; select_done
; 
; PURPOSE: continue on to normal data transfer; called as the exit 
;	point from dsa_begin.
;
; INPUTS: dsa
;
; CALLS: OK
;
;

select_done:







; After a successful selection, we should get either a CMD phase or 
; some transfer request negotiation message.

    JUMP cmdout, WHEN CMD

at 0x00000089 : */	0x820b0000,0x00000244,
/*
    INT int_err_unexpected_phase, WHEN NOT MSG_IN 

at 0x0000008b : */	0x9f030000,0x00000000,
/*

select_msg_in:
    CALL msg_in, WHEN MSG_IN

at 0x0000008d : */	0x8f0b0000,0x00000404,
/*
    JUMP select_msg_in, WHEN MSG_IN

at 0x0000008f : */	0x870b0000,0x00000234,
/*

cmdout:
    INT int_err_unexpected_phase, WHEN NOT CMD

at 0x00000091 : */	0x9a030000,0x00000000,
/*



ENTRY cmdout_cmdout
cmdout_cmdout:

    MOVE FROM dsa_cmdout, WHEN CMD

at 0x00000093 : */	0x1a000000,0x00000048,
/*




;
; data_transfer  
; other_out
; other_in
; other_transfer
;
; PURPOSE : handle the main data transfer for a SCSI command in 
;	several parts.  In the first part, data_transfer, DATA_IN
;	and DATA_OUT phases are allowed, with the user provided
;	code (usually dynamically generated based on the scatter/gather
;	list associated with a SCSI command) called to handle these 
;	phases.
;
;	After control has passed to one of the user provided 
;	DATA_IN or DATA_OUT routines, back calls are made to 
;	other_transfer_in or other_transfer_out to handle non-DATA IN
;	and DATA OUT phases respectively, with the state of the active
;	data pointer being preserved in TEMP.
;
;	On completion, the user code passes control to other_transfer
;	which causes DATA_IN and DATA_OUT to result in unexpected_phase
;	interrupts so that data overruns may be trapped.
;
; INPUTS : DSA - SCSI command
;
; CALLS : OK in data_transfer_start, not ok in other_out and other_in, ok in
;	other_transfer
;
; MODIFIES : SCRATCH
;
; EXITS : if STATUS IN is detected, signifying command completion,
;	the NCR jumps to command_complete.  If MSG IN occurs, a 
;	CALL is made to msg_in.  Otherwise, other_transfer runs in 
;	an infinite loop.
;	

ENTRY data_transfer
data_transfer:
    JUMP cmdout_cmdout, WHEN CMD

at 0x00000095 : */	0x820b0000,0x0000024c,
/*
    CALL msg_in, WHEN MSG_IN

at 0x00000097 : */	0x8f0b0000,0x00000404,
/*
    INT int_err_unexpected_phase, WHEN MSG_OUT

at 0x00000099 : */	0x9e0b0000,0x00000000,
/*
    JUMP do_dataout, WHEN DATA_OUT

at 0x0000009b : */	0x800b0000,0x0000028c,
/*
    JUMP do_datain, WHEN DATA_IN

at 0x0000009d : */	0x810b0000,0x000002e4,
/*
    JUMP command_complete, WHEN STATUS

at 0x0000009f : */	0x830b0000,0x0000060c,
/*
    JUMP data_transfer

at 0x000000a1 : */	0x80080000,0x00000254,
/*
ENTRY end_data_transfer
end_data_transfer:

;
; FIXME: On NCR53c700 and NCR53c700-66 chips, do_dataout/do_datain 
; should be fixed up whenever the nexus changes so it can point to the 
; correct routine for that command.
;


; Nasty jump to dsa->dataout
do_dataout:
    CALL dsa_to_scratch

at 0x000000a3 : */	0x88080000,0x00000938,
/*
    MOVE SCRATCH0 + dsa_dataout TO SCRATCH0	

at 0x000000a5 : */	0x7e345000,0x00000000,
/*
    MOVE SCRATCH1 + 0 TO SCRATCH1 WITH CARRY 

at 0x000000a7 : */	0x7f350000,0x00000000,
/*
    MOVE SCRATCH2 + 0 TO SCRATCH2 WITH CARRY 

at 0x000000a9 : */	0x7f360000,0x00000000,
/*
    MOVE SCRATCH3 + 0 TO SCRATCH3 WITH CARRY 

at 0x000000ab : */	0x7f370000,0x00000000,
/*
    MOVE dmode_ncr_to_memory TO DMODE

at 0x000000ad : */	0x78380000,0x00000000,
/*
    MOVE MEMORY 4, addr_scratch, dataout_to_jump + 4

at 0x000000af : */	0xc0000004,0x00000000,0x000002d4,
/*
    MOVE dmode_memory_to_memory TO DMODE

at 0x000000b2 : */	0x78380000,0x00000000,
/*
dataout_to_jump:
    MOVE MEMORY 4, 0, dataout_jump + 4 

at 0x000000b4 : */	0xc0000004,0x00000000,0x000002e0,
/*
dataout_jump:
    JUMP 0

at 0x000000b7 : */	0x80080000,0x00000000,
/*

; Nasty jump to dsa->dsain
do_datain:
    CALL dsa_to_scratch

at 0x000000b9 : */	0x88080000,0x00000938,
/*
    MOVE SCRATCH0 + dsa_datain TO SCRATCH0	

at 0x000000bb : */	0x7e345400,0x00000000,
/*
    MOVE SCRATCH1 + 0 TO SCRATCH1 WITH CARRY 

at 0x000000bd : */	0x7f350000,0x00000000,
/*
    MOVE SCRATCH2 + 0 TO SCRATCH2 WITH CARRY 

at 0x000000bf : */	0x7f360000,0x00000000,
/*
    MOVE SCRATCH3 + 0 TO SCRATCH3 WITH CARRY 

at 0x000000c1 : */	0x7f370000,0x00000000,
/*
    MOVE dmode_ncr_to_memory TO DMODE

at 0x000000c3 : */	0x78380000,0x00000000,
/*
    MOVE MEMORY 4, addr_scratch, datain_to_jump + 4

at 0x000000c5 : */	0xc0000004,0x00000000,0x0000032c,
/*
    MOVE dmode_memory_to_memory TO DMODE		

at 0x000000c8 : */	0x78380000,0x00000000,
/*
ENTRY datain_to_jump
datain_to_jump:
    MOVE MEMORY 4, 0, datain_jump + 4

at 0x000000ca : */	0xc0000004,0x00000000,0x00000338,
/*



datain_jump:
    JUMP 0

at 0x000000cd : */	0x80080000,0x00000000,
/*



; Note that other_out and other_in loop until a non-data phase
; is discovered, so we only execute return statements when we
; can go on to the next data phase block move statement.

ENTRY other_out
other_out:



    INT int_err_unexpected_phase, WHEN CMD

at 0x000000cf : */	0x9a0b0000,0x00000000,
/*
    JUMP msg_in_restart, WHEN MSG_IN 

at 0x000000d1 : */	0x870b0000,0x000003e4,
/*
    INT int_err_unexpected_phase, WHEN MSG_OUT

at 0x000000d3 : */	0x9e0b0000,0x00000000,
/*
    INT int_err_unexpected_phase, WHEN DATA_IN

at 0x000000d5 : */	0x990b0000,0x00000000,
/*
    JUMP command_complete, WHEN STATUS

at 0x000000d7 : */	0x830b0000,0x0000060c,
/*
    JUMP other_out, WHEN NOT DATA_OUT

at 0x000000d9 : */	0x80030000,0x0000033c,
/*
    RETURN

at 0x000000db : */	0x90080000,0x00000000,
/*

ENTRY other_in
other_in:



    INT int_err_unexpected_phase, WHEN CMD

at 0x000000dd : */	0x9a0b0000,0x00000000,
/*
    JUMP msg_in_restart, WHEN MSG_IN 

at 0x000000df : */	0x870b0000,0x000003e4,
/*
    INT int_err_unexpected_phase, WHEN MSG_OUT

at 0x000000e1 : */	0x9e0b0000,0x00000000,
/*
    INT int_err_unexpected_phase, WHEN DATA_OUT

at 0x000000e3 : */	0x980b0000,0x00000000,
/*
    JUMP command_complete, WHEN STATUS

at 0x000000e5 : */	0x830b0000,0x0000060c,
/*
    JUMP other_in, WHEN NOT DATA_IN

at 0x000000e7 : */	0x81030000,0x00000374,
/*
    RETURN

at 0x000000e9 : */	0x90080000,0x00000000,
/*


ENTRY other_transfer
other_transfer:
    INT int_err_unexpected_phase, WHEN CMD

at 0x000000eb : */	0x9a0b0000,0x00000000,
/*
    CALL msg_in, WHEN MSG_IN

at 0x000000ed : */	0x8f0b0000,0x00000404,
/*
    INT int_err_unexpected_phase, WHEN MSG_OUT

at 0x000000ef : */	0x9e0b0000,0x00000000,
/*
    INT int_err_unexpected_phase, WHEN DATA_OUT

at 0x000000f1 : */	0x980b0000,0x00000000,
/*
    INT int_err_unexpected_phase, WHEN DATA_IN

at 0x000000f3 : */	0x990b0000,0x00000000,
/*
    JUMP command_complete, WHEN STATUS

at 0x000000f5 : */	0x830b0000,0x0000060c,
/*
    JUMP other_transfer

at 0x000000f7 : */	0x80080000,0x000003ac,
/*

;
; msg_in_restart
; msg_in
; munge_msg
;
; PURPOSE : process messages from a target.  msg_in is called when the 
;	caller hasn't read the first byte of the message.  munge_message
;	is called when the caller has read the first byte of the message,
;	and left it in SFBR.  msg_in_restart is called when the caller 
;	hasn't read the first byte of the message, and wishes RETURN
;	to transfer control back to the address of the conditional
;	CALL instruction rather than to the instruction after it.
;
;	Various int_* interrupts are generated when the host system
;	needs to intervene, as is the case with SDTR, WDTR, and
;	INITIATE RECOVERY messages.
;
;	When the host system handles one of these interrupts,
;	it can respond by reentering at reject_message, 
;	which rejects the message and returns control to
;	the caller of msg_in or munge_msg, accept_message
;	which clears ACK and returns control, or reply_message
;	which sends the message pointed to by the DSA 
;	msgout_other table indirect field.
;
;	DISCONNECT messages are handled by moving the command
;	to the reconnect_dsa_queue.
;
; INPUTS : DSA - SCSI COMMAND, SFBR - first byte of message (munge_msg
;	only)
;
; CALLS : NO.  The TEMP register isn't backed up to allow nested calls.
;
; MODIFIES : SCRATCH, DSA on DISCONNECT
;
; EXITS : On receipt of SAVE DATA POINTER, RESTORE POINTERS,
;	and normal return from message handlers running under
;	Linux, control is returned to the caller.  Receipt
;	of DISCONNECT messages pass control to dsa_schedule.
;
ENTRY msg_in_restart
msg_in_restart:
; XXX - hackish
;
; Since it's easier to debug changes to the statically 
; compiled code, rather than the dynamically generated 
; stuff, such as
;
; 	MOVE x, y, WHEN data_phase
; 	CALL other_z, WHEN NOT data_phase
; 	MOVE x, y, WHEN data_phase
;
; I'd like to have certain routines (notably the message handler)
; restart on the conditional call rather than the next instruction.
;
; So, subtract 8 from the return address

    MOVE TEMP0 + 0xf8 TO TEMP0

at 0x000000f9 : */	0x7e1cf800,0x00000000,
/*
    MOVE TEMP1 + 0xff TO TEMP1 WITH CARRY

at 0x000000fb : */	0x7f1dff00,0x00000000,
/*
    MOVE TEMP2 + 0xff TO TEMP2 WITH CARRY

at 0x000000fd : */	0x7f1eff00,0x00000000,
/*
    MOVE TEMP3 + 0xff TO TEMP3 WITH CARRY

at 0x000000ff : */	0x7f1fff00,0x00000000,
/*

ENTRY msg_in
msg_in:
    MOVE 1, msg_buf, WHEN MSG_IN

at 0x00000101 : */	0x0f000001,0x00000000,
/*

munge_msg:
    JUMP munge_extended, IF 0x01		; EXTENDED MESSAGE

at 0x00000103 : */	0x800c0001,0x00000524,
/*
    JUMP munge_2, IF 0x20, AND MASK 0xdf	; two byte message

at 0x00000105 : */	0x800cdf20,0x0000044c,
/*
;
; XXX - I've seen a handful of broken SCSI devices which fail to issue
; 	a SAVE POINTERS message before disconnecting in the middle of 
; 	a transfer, assuming that the DATA POINTER will be implicitly 
; 	restored.  
;
; Historically, I've often done an implicit save when the DISCONNECT
; message is processed.  We may want to consider having the option of 
; doing that here. 
;
    JUMP munge_save_data_pointer, IF 0x02	; SAVE DATA POINTER

at 0x00000107 : */	0x800c0002,0x00000454,
/*
    JUMP munge_restore_pointers, IF 0x03	; RESTORE POINTERS 

at 0x00000109 : */	0x800c0003,0x000004b8,
/*
    JUMP munge_disconnect, IF 0x04		; DISCONNECT

at 0x0000010b : */	0x800c0004,0x0000051c,
/*
    INT int_msg_1, IF 0x07			; MESSAGE REJECT

at 0x0000010d : */	0x980c0007,0x01020000,
/*
    INT int_msg_1, IF 0x0f			; INITIATE RECOVERY

at 0x0000010f : */	0x980c000f,0x01020000,
/*



    JUMP reject_message

at 0x00000111 : */	0x80080000,0x000005b4,
/*

munge_2:
    JUMP reject_message

at 0x00000113 : */	0x80080000,0x000005b4,
/*
;
; The SCSI standard allows targets to recover from transient 
; error conditions by backing up the data pointer with a 
; RESTORE POINTERS message.  
;	
; So, we must save and restore the _residual_ code as well as 
; the current instruction pointer.  Because of this messiness,
; it is simpler to put dynamic code in the dsa for this and to
; just do a simple jump down there. 
;

munge_save_data_pointer:
    MOVE DSA0 + dsa_save_data_pointer TO SFBR

at 0x00000115 : */	0x76100000,0x00000000,
/*
    MOVE SFBR TO SCRATCH0

at 0x00000117 : */	0x6a340000,0x00000000,
/*
    MOVE DSA1 + 0xff TO SFBR WITH CARRY

at 0x00000119 : */	0x7711ff00,0x00000000,
/*
    MOVE SFBR TO SCRATCH1

at 0x0000011b : */	0x6a350000,0x00000000,
/*
    MOVE DSA2 + 0xff TO SFBR WITH CARRY 

at 0x0000011d : */	0x7712ff00,0x00000000,
/*
    MOVE SFBR TO SCRATCH2

at 0x0000011f : */	0x6a360000,0x00000000,
/*
    MOVE DSA3 + 0xff TO SFBR WITH CARRY

at 0x00000121 : */	0x7713ff00,0x00000000,
/*
    MOVE SFBR TO SCRATCH3

at 0x00000123 : */	0x6a370000,0x00000000,
/*

    MOVE dmode_ncr_to_memory TO DMODE

at 0x00000125 : */	0x78380000,0x00000000,
/*
    MOVE MEMORY 4, addr_scratch, jump_dsa_save + 4

at 0x00000127 : */	0xc0000004,0x00000000,0x000004b4,
/*
    MOVE dmode_memory_to_memory TO DMODE

at 0x0000012a : */	0x78380000,0x00000000,
/*
jump_dsa_save:
    JUMP 0

at 0x0000012c : */	0x80080000,0x00000000,
/*

munge_restore_pointers:
    MOVE DSA0 + dsa_restore_pointers TO SFBR

at 0x0000012e : */	0x76100000,0x00000000,
/*
    MOVE SFBR TO SCRATCH0

at 0x00000130 : */	0x6a340000,0x00000000,
/*
    MOVE DSA1 + 0xff TO SFBR WITH CARRY

at 0x00000132 : */	0x7711ff00,0x00000000,
/*
    MOVE SFBR TO SCRATCH1

at 0x00000134 : */	0x6a350000,0x00000000,
/*
    MOVE DSA2 + 0xff TO SFBR WITH CARRY

at 0x00000136 : */	0x7712ff00,0x00000000,
/*
    MOVE SFBR TO SCRATCH2

at 0x00000138 : */	0x6a360000,0x00000000,
/*
    MOVE DSA3 + 0xff TO SFBR WITH CARRY

at 0x0000013a : */	0x7713ff00,0x00000000,
/*
    MOVE SFBR TO SCRATCH3

at 0x0000013c : */	0x6a370000,0x00000000,
/*

    MOVE dmode_ncr_to_memory TO DMODE

at 0x0000013e : */	0x78380000,0x00000000,
/*
    MOVE MEMORY 4, addr_scratch, jump_dsa_restore + 4

at 0x00000140 : */	0xc0000004,0x00000000,0x00000518,
/*
    MOVE dmode_memory_to_memory TO DMODE

at 0x00000143 : */	0x78380000,0x00000000,
/*
jump_dsa_restore:
    JUMP 0

at 0x00000145 : */	0x80080000,0x00000000,
/*


munge_disconnect:









 






    JUMP dsa_schedule

at 0x00000147 : */	0x80080000,0x00000168,
/*





munge_extended:
    CLEAR ACK

at 0x00000149 : */	0x60000040,0x00000000,
/*
    INT int_err_unexpected_phase, WHEN NOT MSG_IN

at 0x0000014b : */	0x9f030000,0x00000000,
/*
    MOVE 1, msg_buf + 1, WHEN MSG_IN

at 0x0000014d : */	0x0f000001,0x00000001,
/*
    JUMP munge_extended_2, IF 0x02

at 0x0000014f : */	0x800c0002,0x00000554,
/*
    JUMP munge_extended_3, IF 0x03 

at 0x00000151 : */	0x800c0003,0x00000584,
/*
    JUMP reject_message

at 0x00000153 : */	0x80080000,0x000005b4,
/*

munge_extended_2:
    CLEAR ACK

at 0x00000155 : */	0x60000040,0x00000000,
/*
    MOVE 1, msg_buf + 2, WHEN MSG_IN

at 0x00000157 : */	0x0f000001,0x00000002,
/*
    JUMP reject_message, IF NOT 0x02	; Must be WDTR

at 0x00000159 : */	0x80040002,0x000005b4,
/*
    CLEAR ACK

at 0x0000015b : */	0x60000040,0x00000000,
/*
    MOVE 1, msg_buf + 3, WHEN MSG_IN

at 0x0000015d : */	0x0f000001,0x00000003,
/*
    INT int_msg_wdtr

at 0x0000015f : */	0x98080000,0x01000000,
/*

munge_extended_3:
    CLEAR ACK

at 0x00000161 : */	0x60000040,0x00000000,
/*
    MOVE 1, msg_buf + 2, WHEN MSG_IN

at 0x00000163 : */	0x0f000001,0x00000002,
/*
    JUMP reject_message, IF NOT 0x01	; Must be SDTR

at 0x00000165 : */	0x80040001,0x000005b4,
/*
    CLEAR ACK

at 0x00000167 : */	0x60000040,0x00000000,
/*
    MOVE 2, msg_buf + 3, WHEN MSG_IN

at 0x00000169 : */	0x0f000002,0x00000003,
/*
    INT int_msg_sdtr

at 0x0000016b : */	0x98080000,0x01010000,
/*

ENTRY reject_message
reject_message:
    SET ATN

at 0x0000016d : */	0x58000008,0x00000000,
/*
    CLEAR ACK

at 0x0000016f : */	0x60000040,0x00000000,
/*
    MOVE 1, NCR53c7xx_msg_reject, WHEN MSG_OUT

at 0x00000171 : */	0x0e000001,0x00000000,
/*
    RETURN

at 0x00000173 : */	0x90080000,0x00000000,
/*

ENTRY accept_message
accept_message:
    CLEAR ATN

at 0x00000175 : */	0x60000008,0x00000000,
/*
    CLEAR ACK

at 0x00000177 : */	0x60000040,0x00000000,
/*
    RETURN

at 0x00000179 : */	0x90080000,0x00000000,
/*

ENTRY respond_message
respond_message:
    SET ATN

at 0x0000017b : */	0x58000008,0x00000000,
/*
    CLEAR ACK

at 0x0000017d : */	0x60000040,0x00000000,
/*
    MOVE FROM dsa_msgout_other, WHEN MSG_OUT

at 0x0000017f : */	0x1e000000,0x00000068,
/*
    RETURN

at 0x00000181 : */	0x90080000,0x00000000,
/*

;
; command_complete
;
; PURPOSE : handle command termination when STATUS IN is detected by reading
;	a status byte followed by a command termination message. 
;
;	Normal termination results in an INTFLY instruction, and 
;	the host system can pick out which command terminated by 
;	examining the MESSAGE and STATUS buffers of all currently 
;	executing commands;
;
;	Abnormal (CHECK_CONDITION) termination results in an
;	int_err_check_condition interrupt so that a REQUEST SENSE
;	command can be issued out-of-order so that no other command
;	clears the contingent allegiance condition.
;	
;
; INPUTS : DSA - command	
;
; CALLS : OK
;
; EXITS : On successful termination, control is passed to schedule.
;	On abnormal termination, the user will usually modify the 
;	DSA fields and corresponding buffers and return control
;	to select.
;

ENTRY command_complete
command_complete:
    MOVE FROM dsa_status, WHEN STATUS

at 0x00000183 : */	0x1b000000,0x00000060,
/*

    MOVE SFBR TO SCRATCH0		; Save status

at 0x00000185 : */	0x6a340000,0x00000000,
/*

ENTRY command_complete_msgin
command_complete_msgin:
    MOVE FROM dsa_msgin, WHEN MSG_IN

at 0x00000187 : */	0x1f000000,0x00000058,
/*
; Indicate that we should be expecting a disconnect
    MOVE SCNTL2 & 0x7f TO SCNTL2

at 0x00000189 : */	0x7c027f00,0x00000000,
/*
    CLEAR ACK

at 0x0000018b : */	0x60000040,0x00000000,
/*

    WAIT DISCONNECT

at 0x0000018d : */	0x48000000,0x00000000,
/*

;
; The SCSI specification states that when a UNIT ATTENTION condition
; is pending, as indicated by a CHECK CONDITION status message,
; the target shall revert to asynchronous transfers.  Since
; synchronous transfers parameters are maintained on a per INITIATOR/TARGET 
; basis, and returning control to our scheduler could work on a command
; running on another lun on that target using the old parameters, we must
; interrupt the host processor to get them changed, or change them ourselves.
;
; Once SCSI-II tagged queueing is implemented, things will be even more
; hairy, since contingent allegiance conditions exist on a per-target/lun
; basis, and issuing a new command with a different tag would clear it.
; In these cases, we must interrupt the host processor to get a request 
; added to the HEAD of the queue with the request sense command, or we
; must automatically issue the request sense command.





    INTFLY

at 0x0000018f : */	0x98180000,0x00000000,
/*





    JUMP schedule

at 0x00000191 : */	0x80080000,0x00000000,
/*
command_failed:
    INT int_err_check_condition

at 0x00000193 : */	0x98080000,0x00030000,
/*




;
; wait_reselect
;
; PURPOSE : This is essentially the idle routine, where control lands
;	when there are no new processes to schedule.  wait_reselect
;	waits for reselection, selection, and new commands.
;
;	When a successful reselection occurs, with the aid 
;	of fixed up code in each DSA, wait_reselect walks the 
;	reconnect_dsa_queue, asking each dsa if the target ID
;	and LUN match its.
;
;	If a match is found, a call is made back to reselected_ok,
;	which through the miracles of self modifying code, extracts
;	the found DSA from the reconnect_dsa_queue and then 
;	returns control to the DSAs thread of execution.
;
; INPUTS : NONE
;
; CALLS : OK
;
; MODIFIES : DSA,
;
; EXITS : On successful reselection, control is returned to the 
;	DSA which called reselected_ok.  If the WAIT RESELECT
;	was interrupted by a new commands arrival signaled by 
;	SIG_P, control is passed to schedule.  If the NCR is 
;	selected, the host system is interrupted with an 
;	int_err_selected which is usually responded to by
;	setting DSP to the target_abort address.

ENTRY wait_reselect
wait_reselect:






    WAIT RESELECT wait_reselect_failed

at 0x00000195 : */	0x50000000,0x0000076c,
/*

reselected:



    CLEAR TARGET

at 0x00000197 : */	0x60000200,0x00000000,
/*
    MOVE dmode_memory_to_memory TO DMODE

at 0x00000199 : */	0x78380000,0x00000000,
/*
    ; Read all data needed to reestablish the nexus - 
    MOVE 1, reselected_identify, WHEN MSG_IN

at 0x0000019b : */	0x0f000001,0x00000000,
/*
    ; We used to CLEAR ACK here.





    ; Point DSA at the current head of the disconnected queue.
    MOVE dmode_memory_to_ncr  TO DMODE

at 0x0000019d : */	0x78380000,0x00000000,
/*
    MOVE MEMORY 4, reconnect_dsa_head, addr_scratch

at 0x0000019f : */	0xc0000004,0x00000000,0x00000000,
/*
    MOVE dmode_memory_to_memory TO DMODE

at 0x000001a2 : */	0x78380000,0x00000000,
/*
    CALL scratch_to_dsa

at 0x000001a4 : */	0x88080000,0x00000980,
/*

    ; Fix the update-next pointer so that the reconnect_dsa_head
    ; pointer is the one that will be updated if this DSA is a hit 
    ; and we remove it from the queue.

    MOVE MEMORY 4, addr_reconnect_dsa_head, reselected_ok + 8

at 0x000001a6 : */	0xc0000004,0x00000000,0x00000758,
/*

ENTRY reselected_check_next
reselected_check_next:



    ; Check for a NULL pointer.
    MOVE DSA0 TO SFBR

at 0x000001a9 : */	0x72100000,0x00000000,
/*
    JUMP reselected_not_end, IF NOT 0

at 0x000001ab : */	0x80040000,0x000006ec,
/*
    MOVE DSA1 TO SFBR

at 0x000001ad : */	0x72110000,0x00000000,
/*
    JUMP reselected_not_end, IF NOT 0

at 0x000001af : */	0x80040000,0x000006ec,
/*
    MOVE DSA2 TO SFBR

at 0x000001b1 : */	0x72120000,0x00000000,
/*
    JUMP reselected_not_end, IF NOT 0

at 0x000001b3 : */	0x80040000,0x000006ec,
/*
    MOVE DSA3 TO SFBR

at 0x000001b5 : */	0x72130000,0x00000000,
/*
    JUMP reselected_not_end, IF NOT 0

at 0x000001b7 : */	0x80040000,0x000006ec,
/*
    INT int_err_unexpected_reselect

at 0x000001b9 : */	0x98080000,0x00020000,
/*

reselected_not_end:
    ;
    ; XXX the ALU is only eight bits wide, and the assembler
    ; wont do the dirt work for us.  As long as dsa_check_reselect
    ; is negative, we need to sign extend with 1 bits to the full
    ; 32 bit width of the address.
    ;
    ; A potential work around would be to have a known alignment 
    ; of the DSA structure such that the base address plus 
    ; dsa_check_reselect doesn't require carrying from bytes 
    ; higher than the LSB.
    ;

    MOVE DSA0 TO SFBR

at 0x000001bb : */	0x72100000,0x00000000,
/*
    MOVE SFBR + dsa_check_reselect TO SCRATCH0

at 0x000001bd : */	0x6e340000,0x00000000,
/*
    MOVE DSA1 TO SFBR

at 0x000001bf : */	0x72110000,0x00000000,
/*
    MOVE SFBR + 0xff TO SCRATCH1 WITH CARRY

at 0x000001c1 : */	0x6f35ff00,0x00000000,
/*
    MOVE DSA2 TO SFBR

at 0x000001c3 : */	0x72120000,0x00000000,
/*
    MOVE SFBR + 0xff TO SCRATCH2 WITH CARRY

at 0x000001c5 : */	0x6f36ff00,0x00000000,
/*
    MOVE DSA3 TO SFBR

at 0x000001c7 : */	0x72130000,0x00000000,
/*
    MOVE SFBR + 0xff TO SCRATCH3 WITH CARRY

at 0x000001c9 : */	0x6f37ff00,0x00000000,
/*

    MOVE dmode_ncr_to_memory TO DMODE

at 0x000001cb : */	0x78380000,0x00000000,
/*
    MOVE MEMORY 4, addr_scratch, reselected_check + 4

at 0x000001cd : */	0xc0000004,0x00000000,0x0000074c,
/*
    MOVE dmode_memory_to_memory TO DMODE

at 0x000001d0 : */	0x78380000,0x00000000,
/*
reselected_check:
    JUMP 0

at 0x000001d2 : */	0x80080000,0x00000000,
/*


;
;
ENTRY reselected_ok
reselected_ok:
    MOVE MEMORY 4, 0, 0				; Patched : first word

at 0x000001d4 : */	0xc0000004,0x00000000,0x00000000,
/*
						; 	is address of 
						;       successful dsa_next
						; Second word is last 
						;	unsuccessful dsa_next,
						;	starting with 
						;       dsa_reconnect_head
    ; We used to CLEAR ACK here.






    RETURN					; Return control to where

at 0x000001d7 : */	0x90080000,0x00000000,
/*




selected:
    INT int_err_selected;

at 0x000001d9 : */	0x98080000,0x00010000,
/*

;
; A select or reselect failure can be caused by one of two conditions : 
; 1.  SIG_P was set.  This will be the case if the user has written
;	a new value to a previously NULL head of the issue queue.
;
; 2.  The NCR53c810 was selected or reselected by another device.
;
; 3.  The bus was already busy since we were selected or reselected
;	before starting the command.

wait_reselect_failed:



; Check selected bit.  
    MOVE SIST0 & 0x20 TO SFBR

at 0x000001db : */	0x74422000,0x00000000,
/*
    JUMP selected, IF 0x20

at 0x000001dd : */	0x800c0020,0x00000764,
/*
; Reading CTEST2 clears the SIG_P bit in the ISTAT register.
    MOVE CTEST2 & 0x40 TO SFBR	

at 0x000001df : */	0x741a4000,0x00000000,
/*
    JUMP schedule, IF 0x40

at 0x000001e1 : */	0x800c0040,0x00000000,
/*
; Check connected bit.  
; FIXME: this needs to change if we support target mode
    MOVE ISTAT & 0x08 TO SFBR

at 0x000001e3 : */	0x74140800,0x00000000,
/*
    JUMP reselected, IF 0x08

at 0x000001e5 : */	0x800c0008,0x0000065c,
/*
; FIXME : Something bogus happened, and we shouldn't fail silently.



    INT int_debug_panic

at 0x000001e7 : */	0x98080000,0x030b0000,
/*



select_failed:



; Otherwise, mask the selected and reselected bits off SIST0
    MOVE SIST0 & 0x30 TO SFBR

at 0x000001e9 : */	0x74423000,0x00000000,
/*
    JUMP selected, IF 0x20

at 0x000001eb : */	0x800c0020,0x00000764,
/*
    JUMP reselected, IF 0x10 

at 0x000001ed : */	0x800c0010,0x0000065c,
/*
; If SIGP is set, the user just gave us another command, and
; we should restart or return to the scheduler.
; Reading CTEST2 clears the SIG_P bit in the ISTAT register.
    MOVE CTEST2 & 0x40 TO SFBR	

at 0x000001ef : */	0x741a4000,0x00000000,
/*
    JUMP select, IF 0x40

at 0x000001f1 : */	0x800c0040,0x000001fc,
/*
; Check connected bit.  
; FIXME: this needs to change if we support target mode
; FIXME: is this really necessary? 
    MOVE ISTAT & 0x08 TO SFBR

at 0x000001f3 : */	0x74140800,0x00000000,
/*
    JUMP reselected, IF 0x08

at 0x000001f5 : */	0x800c0008,0x0000065c,
/*
; FIXME : Something bogus happened, and we shouldn't fail silently.



    INT int_debug_panic

at 0x000001f7 : */	0x98080000,0x030b0000,
/*


;
; test_1
; test_2
;
; PURPOSE : run some verification tests on the NCR.  test_1
;	copies test_src to test_dest and interrupts the host
;	processor, testing for cache coherency and interrupt
; 	problems in the processes.
;
;	test_2 runs a command with offsets relative to the 
;	DSA on entry, and is useful for miscellaneous experimentation.
;

; Verify that interrupts are working correctly and that we don't 
; have a cache invalidation problem.

ABSOLUTE test_src = 0, test_dest = 0
ENTRY test_1
test_1:
    MOVE MEMORY 4, test_src, test_dest

at 0x000001f9 : */	0xc0000004,0x00000000,0x00000000,
/*
    INT int_test_1

at 0x000001fc : */	0x98080000,0x04000000,
/*

;
; Run arbitrary commands, with test code establishing a DSA
;
 
ENTRY test_2
test_2:
    CLEAR TARGET

at 0x000001fe : */	0x60000200,0x00000000,
/*
    SELECT ATN FROM 0, test_2_fail

at 0x00000200 : */	0x43000000,0x00000850,
/*
    JUMP test_2_msgout, WHEN MSG_OUT

at 0x00000202 : */	0x860b0000,0x00000810,
/*
ENTRY test_2_msgout
test_2_msgout:
    MOVE FROM 8, WHEN MSG_OUT

at 0x00000204 : */	0x1e000000,0x00000008,
/*
    MOVE FROM 16, WHEN CMD 

at 0x00000206 : */	0x1a000000,0x00000010,
/*
    MOVE FROM 24, WHEN DATA_IN

at 0x00000208 : */	0x19000000,0x00000018,
/*
    MOVE FROM 32, WHEN STATUS

at 0x0000020a : */	0x1b000000,0x00000020,
/*
    MOVE FROM 40, WHEN MSG_IN

at 0x0000020c : */	0x1f000000,0x00000028,
/*
    MOVE SCNTL2 & 0x7f TO SCNTL2

at 0x0000020e : */	0x7c027f00,0x00000000,
/*
    CLEAR ACK

at 0x00000210 : */	0x60000040,0x00000000,
/*
    WAIT DISCONNECT

at 0x00000212 : */	0x48000000,0x00000000,
/*
test_2_fail:
    INT int_test_2

at 0x00000214 : */	0x98080000,0x04010000,
/*

ENTRY debug_break
debug_break:
    INT int_debug_break

at 0x00000216 : */	0x98080000,0x03000000,
/*

;
; initiator_abort
; target_abort
;
; PURPOSE : Abort the currently established nexus from with initiator
;	or target mode.
;
;  

ENTRY target_abort
target_abort:
    SET TARGET

at 0x00000218 : */	0x58000200,0x00000000,
/*
    DISCONNECT

at 0x0000021a : */	0x48000000,0x00000000,
/*
    CLEAR TARGET

at 0x0000021c : */	0x60000200,0x00000000,
/*
    JUMP schedule

at 0x0000021e : */	0x80080000,0x00000000,
/*
    
ENTRY initiator_abort
initiator_abort:
    SET ATN

at 0x00000220 : */	0x58000008,0x00000000,
/*
;
; The SCSI-I specification says that targets may go into MSG out at 
; their leisure upon receipt of the ATN single.  On all versions of the 
; specification, we can't change phases until REQ transitions true->false, 
; so we need to sink/source one byte of data to allow the transition.
;
; For the sake of safety, we'll only source one byte of data in all 
; cases, but to accommodate the SCSI-I dain bramage, we'll sink an  
; arbitrary number of bytes.
    JUMP spew_cmd, WHEN CMD

at 0x00000222 : */	0x820b0000,0x000008b8,
/*
    JUMP eat_msgin, WHEN MSG_IN

at 0x00000224 : */	0x870b0000,0x000008c8,
/*
    JUMP eat_datain, WHEN DATA_IN

at 0x00000226 : */	0x810b0000,0x000008f8,
/*
    JUMP eat_status, WHEN STATUS

at 0x00000228 : */	0x830b0000,0x000008e0,
/*
    JUMP spew_dataout, WHEN DATA_OUT

at 0x0000022a : */	0x800b0000,0x00000910,
/*
    JUMP sated

at 0x0000022c : */	0x80080000,0x00000918,
/*
spew_cmd:
    MOVE 1, NCR53c7xx_zero, WHEN CMD

at 0x0000022e : */	0x0a000001,0x00000000,
/*
    JUMP sated

at 0x00000230 : */	0x80080000,0x00000918,
/*
eat_msgin:
    MOVE 1, NCR53c7xx_sink, WHEN MSG_IN

at 0x00000232 : */	0x0f000001,0x00000000,
/*
    JUMP eat_msgin, WHEN MSG_IN

at 0x00000234 : */	0x870b0000,0x000008c8,
/*
    JUMP sated

at 0x00000236 : */	0x80080000,0x00000918,
/*
eat_status:
    MOVE 1, NCR53c7xx_sink, WHEN STATUS

at 0x00000238 : */	0x0b000001,0x00000000,
/*
    JUMP eat_status, WHEN STATUS

at 0x0000023a : */	0x830b0000,0x000008e0,
/*
    JUMP sated

at 0x0000023c : */	0x80080000,0x00000918,
/*
eat_datain:
    MOVE 1, NCR53c7xx_sink, WHEN DATA_IN

at 0x0000023e : */	0x09000001,0x00000000,
/*
    JUMP eat_datain, WHEN DATA_IN

at 0x00000240 : */	0x810b0000,0x000008f8,
/*
    JUMP sated

at 0x00000242 : */	0x80080000,0x00000918,
/*
spew_dataout:
    MOVE 1, NCR53c7xx_zero, WHEN DATA_OUT

at 0x00000244 : */	0x08000001,0x00000000,
/*
sated:
    MOVE SCNTL2 & 0x7f TO SCNTL2

at 0x00000246 : */	0x7c027f00,0x00000000,
/*
    MOVE 1, NCR53c7xx_msg_abort, WHEN MSG_OUT

at 0x00000248 : */	0x0e000001,0x00000000,
/*
    WAIT DISCONNECT

at 0x0000024a : */	0x48000000,0x00000000,
/*
    INT int_norm_aborted

at 0x0000024c : */	0x98080000,0x02040000,
/*

;
; dsa_to_scratch
; scratch_to_dsa
;
; PURPOSE :
; 	The NCR chips cannot do a move memory instruction with the DSA register 
; 	as the source or destination.  So, we provide a couple of subroutines
; 	that let us switch between the DSA register and scratch register.
;
; 	Memory moves to/from the DSPS  register also don't work, but we 
; 	don't use them.
;
;

 
dsa_to_scratch:
    MOVE DSA0 TO SFBR

at 0x0000024e : */	0x72100000,0x00000000,
/*
    MOVE SFBR TO SCRATCH0

at 0x00000250 : */	0x6a340000,0x00000000,
/*
    MOVE DSA1 TO SFBR

at 0x00000252 : */	0x72110000,0x00000000,
/*
    MOVE SFBR TO SCRATCH1

at 0x00000254 : */	0x6a350000,0x00000000,
/*
    MOVE DSA2 TO SFBR

at 0x00000256 : */	0x72120000,0x00000000,
/*
    MOVE SFBR TO SCRATCH2

at 0x00000258 : */	0x6a360000,0x00000000,
/*
    MOVE DSA3 TO SFBR

at 0x0000025a : */	0x72130000,0x00000000,
/*
    MOVE SFBR TO SCRATCH3

at 0x0000025c : */	0x6a370000,0x00000000,
/*
    RETURN

at 0x0000025e : */	0x90080000,0x00000000,
/*

scratch_to_dsa:
    MOVE SCRATCH0 TO SFBR

at 0x00000260 : */	0x72340000,0x00000000,
/*
    MOVE SFBR TO DSA0

at 0x00000262 : */	0x6a100000,0x00000000,
/*
    MOVE SCRATCH1 TO SFBR

at 0x00000264 : */	0x72350000,0x00000000,
/*
    MOVE SFBR TO DSA1

at 0x00000266 : */	0x6a110000,0x00000000,
/*
    MOVE SCRATCH2 TO SFBR

at 0x00000268 : */	0x72360000,0x00000000,
/*
    MOVE SFBR TO DSA2

at 0x0000026a : */	0x6a120000,0x00000000,
/*
    MOVE SCRATCH3 TO SFBR

at 0x0000026c : */	0x72370000,0x00000000,
/*
    MOVE SFBR TO DSA3

at 0x0000026e : */	0x6a130000,0x00000000,
/*
    RETURN

at 0x00000270 : */	0x90080000,0x00000000,
};

#define A_NCR53c7xx_msg_abort	0x00000000
static u32 A_NCR53c7xx_msg_abort_used[] __attribute((unused)) = {
	0x00000249,
};

#define A_NCR53c7xx_msg_reject	0x00000000
static u32 A_NCR53c7xx_msg_reject_used[] __attribute((unused)) = {
	0x00000172,
};

#define A_NCR53c7xx_sink	0x00000000
static u32 A_NCR53c7xx_sink_used[] __attribute((unused)) = {
	0x00000233,
	0x00000239,
	0x0000023f,
};

#define A_NCR53c7xx_zero	0x00000000
static u32 A_NCR53c7xx_zero_used[] __attribute((unused)) = {
	0x0000022f,
	0x00000245,
};

#define A_NOP_insn	0x00000000
static u32 A_NOP_insn_used[] __attribute((unused)) = {
	0x00000010,
};

#define A_addr_reconnect_dsa_head	0x00000000
static u32 A_addr_reconnect_dsa_head_used[] __attribute((unused)) = {
	0x000001a7,
};

#define A_addr_scratch	0x00000000
static u32 A_addr_scratch_used[] __attribute((unused)) = {
	0x00000004,
	0x0000001b,
	0x00000046,
	0x00000067,
	0x00000073,
	0x000000b0,
	0x000000c6,
	0x00000128,
	0x00000141,
	0x000001a1,
	0x000001ce,
};

#define A_addr_temp	0x00000000
static u32 A_addr_temp_used[] __attribute((unused)) = {
	0x00000025,
	0x00000034,
};

#define A_dmode_memory_to_memory	0x00000000
static u32 A_dmode_memory_to_memory_used[] __attribute((unused)) = {
	0x00000005,
	0x0000001c,
	0x00000027,
	0x00000035,
	0x00000047,
	0x00000069,
	0x00000075,
	0x000000b2,
	0x000000c8,
	0x0000012a,
	0x00000143,
	0x00000199,
	0x000001a2,
	0x000001d0,
};

#define A_dmode_memory_to_ncr	0x00000000
static u32 A_dmode_memory_to_ncr_used[] __attribute((unused)) = {
	0x00000000,
	0x00000017,
	0x00000030,
	0x00000042,
	0x0000019d,
};

#define A_dmode_ncr_to_memory	0x00000000
static u32 A_dmode_ncr_to_memory_used[] __attribute((unused)) = {
	0x00000022,
	0x00000064,
	0x00000070,
	0x000000ad,
	0x000000c3,
	0x00000125,
	0x0000013e,
	0x000001cb,
};

#define A_dsa_check_reselect	0x00000000
static u32 A_dsa_check_reselect_used[] __attribute((unused)) = {
	0x000001bd,
};

#define A_dsa_cmdout	0x00000048
static u32 A_dsa_cmdout_used[] __attribute((unused)) = {
	0x00000094,
};

#define A_dsa_cmnd	0x00000038
static u32 A_dsa_cmnd_used[] __attribute((unused)) = {
};

#define A_dsa_datain	0x00000054
static u32 A_dsa_datain_used[] __attribute((unused)) = {
	0x000000bb,
};

#define A_dsa_dataout	0x00000050
static u32 A_dsa_dataout_used[] __attribute((unused)) = {
	0x000000a5,
};

#define A_dsa_end	0x00000070
static u32 A_dsa_end_used[] __attribute((unused)) = {
};

#define A_dsa_fields_start	0x00000000
static u32 A_dsa_fields_start_used[] __attribute((unused)) = {
};

#define A_dsa_msgin	0x00000058
static u32 A_dsa_msgin_used[] __attribute((unused)) = {
	0x00000188,
};

#define A_dsa_msgout	0x00000040
static u32 A_dsa_msgout_used[] __attribute((unused)) = {
	0x00000086,
};

#define A_dsa_msgout_other	0x00000068
static u32 A_dsa_msgout_other_used[] __attribute((unused)) = {
	0x00000180,
};

#define A_dsa_next	0x00000030
static u32 A_dsa_next_used[] __attribute((unused)) = {
	0x0000005c,
};

#define A_dsa_restore_pointers	0x00000000
static u32 A_dsa_restore_pointers_used[] __attribute((unused)) = {
	0x0000012e,
};

#define A_dsa_save_data_pointer	0x00000000
static u32 A_dsa_save_data_pointer_used[] __attribute((unused)) = {
	0x00000115,
};

#define A_dsa_select	0x0000003c
static u32 A_dsa_select_used[] __attribute((unused)) = {
	0x00000081,
};

#define A_dsa_status	0x00000060
static u32 A_dsa_status_used[] __attribute((unused)) = {
	0x00000184,
};

#define A_dsa_temp_addr_array_value	0x00000000
static u32 A_dsa_temp_addr_array_value_used[] __attribute((unused)) = {
};

#define A_dsa_temp_addr_dsa_value	0x00000000
static u32 A_dsa_temp_addr_dsa_value_used[] __attribute((unused)) = {
	0x00000003,
};

#define A_dsa_temp_addr_new_value	0x00000000
static u32 A_dsa_temp_addr_new_value_used[] __attribute((unused)) = {
};

#define A_dsa_temp_addr_next	0x00000000
static u32 A_dsa_temp_addr_next_used[] __attribute((unused)) = {
	0x00000015,
	0x0000004e,
};

#define A_dsa_temp_addr_residual	0x00000000
static u32 A_dsa_temp_addr_residual_used[] __attribute((unused)) = {
	0x0000002a,
	0x00000039,
};

#define A_dsa_temp_addr_saved_pointer	0x00000000
static u32 A_dsa_temp_addr_saved_pointer_used[] __attribute((unused)) = {
	0x00000026,
	0x00000033,
};

#define A_dsa_temp_addr_saved_residual	0x00000000
static u32 A_dsa_temp_addr_saved_residual_used[] __attribute((unused)) = {
	0x0000002b,
	0x00000038,
};

#define A_dsa_temp_lun	0x00000000
static u32 A_dsa_temp_lun_used[] __attribute((unused)) = {
	0x0000004b,
};

#define A_dsa_temp_next	0x00000000
static u32 A_dsa_temp_next_used[] __attribute((unused)) = {
	0x0000001a,
};

#define A_dsa_temp_sync	0x00000000
static u32 A_dsa_temp_sync_used[] __attribute((unused)) = {
	0x00000053,
};

#define A_dsa_temp_target	0x00000000
static u32 A_dsa_temp_target_used[] __attribute((unused)) = {
	0x00000040,
};

#define A_int_debug_break	0x03000000
static u32 A_int_debug_break_used[] __attribute((unused)) = {
	0x00000217,
};

#define A_int_debug_panic	0x030b0000
static u32 A_int_debug_panic_used[] __attribute((unused)) = {
	0x000001e8,
	0x000001f8,
};

#define A_int_err_check_condition	0x00030000
static u32 A_int_err_check_condition_used[] __attribute((unused)) = {
	0x00000194,
};

#define A_int_err_no_phase	0x00040000
static u32 A_int_err_no_phase_used[] __attribute((unused)) = {
};

#define A_int_err_selected	0x00010000
static u32 A_int_err_selected_used[] __attribute((unused)) = {
	0x000001da,
};

#define A_int_err_unexpected_phase	0x00000000
static u32 A_int_err_unexpected_phase_used[] __attribute((unused)) = {
	0x0000008c,
	0x00000092,
	0x0000009a,
	0x000000d0,
	0x000000d4,
	0x000000d6,
	0x000000de,
	0x000000e2,
	0x000000e4,
	0x000000ec,
	0x000000f0,
	0x000000f2,
	0x000000f4,
	0x0000014c,
};

#define A_int_err_unexpected_reselect	0x00020000
static u32 A_int_err_unexpected_reselect_used[] __attribute((unused)) = {
	0x000001ba,
};

#define A_int_msg_1	0x01020000
static u32 A_int_msg_1_used[] __attribute((unused)) = {
	0x0000010e,
	0x00000110,
};

#define A_int_msg_sdtr	0x01010000
static u32 A_int_msg_sdtr_used[] __attribute((unused)) = {
	0x0000016c,
};

#define A_int_msg_wdtr	0x01000000
static u32 A_int_msg_wdtr_used[] __attribute((unused)) = {
	0x00000160,
};

#define A_int_norm_aborted	0x02040000
static u32 A_int_norm_aborted_used[] __attribute((unused)) = {
	0x0000024d,
};

#define A_int_norm_command_complete	0x02020000
static u32 A_int_norm_command_complete_used[] __attribute((unused)) = {
};

#define A_int_norm_disconnected	0x02030000
static u32 A_int_norm_disconnected_used[] __attribute((unused)) = {
};

#define A_int_norm_reselect_complete	0x02010000
static u32 A_int_norm_reselect_complete_used[] __attribute((unused)) = {
};

#define A_int_norm_reset	0x02050000
static u32 A_int_norm_reset_used[] __attribute((unused)) = {
};

#define A_int_norm_select_complete	0x02000000
static u32 A_int_norm_select_complete_used[] __attribute((unused)) = {
};

#define A_int_test_1	0x04000000
static u32 A_int_test_1_used[] __attribute((unused)) = {
	0x000001fd,
};

#define A_int_test_2	0x04010000
static u32 A_int_test_2_used[] __attribute((unused)) = {
	0x00000215,
};

#define A_int_test_3	0x04020000
static u32 A_int_test_3_used[] __attribute((unused)) = {
};

#define A_msg_buf	0x00000000
static u32 A_msg_buf_used[] __attribute((unused)) = {
	0x00000102,
	0x0000014e,
	0x00000158,
	0x0000015e,
	0x00000164,
	0x0000016a,
};

#define A_reconnect_dsa_head	0x00000000
static u32 A_reconnect_dsa_head_used[] __attribute((unused)) = {
	0x0000006c,
	0x00000074,
	0x000001a0,
};

#define A_reselected_identify	0x00000000
static u32 A_reselected_identify_used[] __attribute((unused)) = {
	0x00000045,
	0x0000019c,
};

#define A_reselected_tag	0x00000000
static u32 A_reselected_tag_used[] __attribute((unused)) = {
};

#define A_schedule	0x00000000
static u32 A_schedule_used[] __attribute((unused)) = {
	0x0000007e,
	0x00000192,
	0x000001e2,
	0x0000021f,
};

#define A_test_dest	0x00000000
static u32 A_test_dest_used[] __attribute((unused)) = {
	0x000001fb,
};

#define A_test_src	0x00000000
static u32 A_test_src_used[] __attribute((unused)) = {
	0x000001fa,
};

#define Ent_accept_message	0x000005d4
#define Ent_cmdout_cmdout	0x0000024c
#define Ent_command_complete	0x0000060c
#define Ent_command_complete_msgin	0x0000061c
#define Ent_data_transfer	0x00000254
#define Ent_datain_to_jump	0x00000328
#define Ent_debug_break	0x00000858
#define Ent_dsa_code_begin	0x00000000
#define Ent_dsa_code_check_reselect	0x000000f8
#define Ent_dsa_code_fix_jump	0x0000003c
#define Ent_dsa_code_restore_pointers	0x000000c0
#define Ent_dsa_code_save_data_pointer	0x00000088
#define Ent_dsa_code_template	0x00000000
#define Ent_dsa_code_template_end	0x00000168
#define Ent_dsa_schedule	0x00000168
#define Ent_dsa_zero	0x00000168
#define Ent_end_data_transfer	0x0000028c
#define Ent_initiator_abort	0x00000880
#define Ent_msg_in	0x00000404
#define Ent_msg_in_restart	0x000003e4
#define Ent_other_in	0x00000374
#define Ent_other_out	0x0000033c
#define Ent_other_transfer	0x000003ac
#define Ent_reject_message	0x000005b4
#define Ent_reselected_check_next	0x000006a4
#define Ent_reselected_ok	0x00000750
#define Ent_respond_message	0x000005ec
#define Ent_select	0x000001fc
#define Ent_select_msgout	0x00000214
#define Ent_target_abort	0x00000860
#define Ent_test_1	0x000007e4
#define Ent_test_2	0x000007f8
#define Ent_test_2_msgout	0x00000810
#define Ent_wait_reselect	0x00000654
static u32 LABELPATCHES[] __attribute((unused)) = {
	0x00000008,
	0x0000000a,
	0x00000013,
	0x00000016,
	0x0000001f,
	0x00000021,
	0x0000004f,
	0x00000051,
	0x0000005b,
	0x00000068,
	0x0000006f,
	0x00000082,
	0x00000084,
	0x0000008a,
	0x0000008e,
	0x00000090,
	0x00000096,
	0x00000098,
	0x0000009c,
	0x0000009e,
	0x000000a0,
	0x000000a2,
	0x000000a4,
	0x000000b1,
	0x000000b6,
	0x000000ba,
	0x000000c7,
	0x000000cc,
	0x000000d2,
	0x000000d8,
	0x000000da,
	0x000000e0,
	0x000000e6,
	0x000000e8,
	0x000000ee,
	0x000000f6,
	0x000000f8,
	0x00000104,
	0x00000106,
	0x00000108,
	0x0000010a,
	0x0000010c,
	0x00000112,
	0x00000114,
	0x00000129,
	0x00000142,
	0x00000148,
	0x00000150,
	0x00000152,
	0x00000154,
	0x0000015a,
	0x00000166,
	0x00000196,
	0x000001a5,
	0x000001a8,
	0x000001ac,
	0x000001b0,
	0x000001b4,
	0x000001b8,
	0x000001cf,
	0x000001de,
	0x000001e6,
	0x000001ec,
	0x000001ee,
	0x000001f2,
	0x000001f6,
	0x00000201,
	0x00000203,
	0x00000223,
	0x00000225,
	0x00000227,
	0x00000229,
	0x0000022b,
	0x0000022d,
	0x00000231,
	0x00000235,
	0x00000237,
	0x0000023b,
	0x0000023d,
	0x00000241,
	0x00000243,
};

static struct {
	u32	offset;
	void		*address;
} EXTERNAL_PATCHES[] __attribute((unused)) = {
};

static u32 INSTRUCTIONS __attribute((unused))	= 301;
static u32 PATCHES __attribute((unused))	= 81;
static u32 EXTERNAL_PATCHES_LEN __attribute((unused))	= 0;
