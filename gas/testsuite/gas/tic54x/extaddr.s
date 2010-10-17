*
* Extended addressing support 
*
        .version 548
	.far_mode
	.global F1, start, end
	; LDX pseudo-op
	ldx	#F1,16,a	; load upper 8 bits of extended address
	or	#F1,a,a		; load remaining bits
	bacc	a
	; extended addressing functions
start:		
	fb	end	
	
	fbd	end		
        nop
        nop

	fbacc	a
	fbaccd	a
        nop
        nop
	fcala	a
	fcalad	b
        nop
        nop
	fcall	end
	
	fcalld	end
        nop
        nop

	fret	
	fretd
        nop
        nop
	frete
	freted
        nop
        nop
	.space	16*0xFFFF
	.align	0x80
end:	
	fb	end
	.end
