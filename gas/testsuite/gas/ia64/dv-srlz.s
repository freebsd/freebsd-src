//
// Auto-insertion of instruction and data serialization
//			
.text
start:		
// Requires data serialization	
	ptc.e	r1
	ld8	r1 = [r2]
	rfi
// Requires instruction serialization
	ptc.e	r1
	epc
	rfi
