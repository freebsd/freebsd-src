;; Test all possible combinations of the endmask in bits 5-9.
;; The mask that is used is computed as 2**bits-1 where bits
;; are the bits 5-9 from the instruction.  Note that 0 and 32
;; are treated identically, and disassembled as 0.

	sl.iz	5,0,r7,r9
	sl.iz	5,1,r7,r9
	sl.iz	5,2,r7,r9
	sl.iz	5,3,r7,r9
	sl.iz	5,4,r7,r9
	sl.iz	5,5,r7,r9
	sl.iz	5,6,r7,r9
	sl.iz	5,7,r7,r9
	sl.iz	5,8,r7,r9
	sl.iz	5,9,r7,r9
	sl.iz	5,10,r7,r9
	sl.iz	5,11,r7,r9
	sl.iz	5,12,r7,r9
	sl.iz	5,13,r7,r9
	sl.iz	5,14,r7,r9
	sl.iz	5,15,r7,r9
	sl.iz	5,16,r7,r9
	sl.iz	5,17,r7,r9
	sl.iz	5,18,r7,r9
	sl.iz	5,19,r7,r9
	sl.iz	5,20,r7,r9
	sl.iz	5,21,r7,r9
	sl.iz	5,22,r7,r9
	sl.iz	5,23,r7,r9
	sl.iz	5,24,r7,r9
	sl.iz	5,25,r7,r9
	sl.iz	5,26,r7,r9
	sl.iz	5,27,r7,r9
	sl.iz	5,28,r7,r9
	sl.iz	5,29,r7,r9
	sl.iz	5,30,r7,r9
	sl.iz	5,31,r7,r9
	sl.iz	5,32,r7,r9
