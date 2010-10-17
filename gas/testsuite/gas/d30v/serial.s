# serial.s
#
# In the following examples, the right-subinstructions
# will never be executed.  GAS should detect this.
	
	trap r21 -> add r2, r0, r0 ; right instruction will never be executed.
	dbt     -> add r2, r0, r0               ; ditto
	rtd     -> add r2, r0, r0               ; ditto
	reit    -> add r2, r0, r0               ; ditto
	mvtsys psw,  r1 -> add r2, r0, r0       ; OK
	mvtsys pswh, r1 -> add r2, r0, r0       ; OK
	mvtsys pswl, r1 -> add r2, r0, r0       ; OK
	mvtsys f0, r1 -> add r2, r0, r0         ; OK
	mvtsys mod_s, r1 -> add r2, r0, r0      ; OK
