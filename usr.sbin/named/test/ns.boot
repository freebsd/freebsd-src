;
;	boot file for name server
; Note that there should be one primary entry for each SOA record.
;
; type		domain		source file or host
;
primary		ucb.arpa	ns.db
cache		.		ns.ca		; initial cache data
;secondary	ucb.arpa	128.32.0.7	; ucbvax
