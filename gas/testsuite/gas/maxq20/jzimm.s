.text

_main:   
 	        JUMP C, #03h
                JUMP S, #03h
                JUMP Z, #fffh
                JUMP NZ, #03h

		SJUMP C, #03h
                SJUMP S, #03h
                SJUMP Z, #fffh
                SJUMP NZ, #03h

		LJUMP C, #03h
                LJUMP S, #03h
                LJUMP Z, #fffh
                LJUMP NZ, #03h
