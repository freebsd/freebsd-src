.arm
.syntax unified
	fconsts s17, #4
	fconsts s18, #0xa5
	fconsts s19, #0x40
	fconstd d17, #4
	fconstd d18, #0xa5
	fconstd d19, #0x40
	fshtos s17, 9
	fshtod d17, 9
	fsltos s17, 9
	fsltod d17, 9
	fuhtos s17, 9
	fuhtod d17, 9
	fultos s17, 9
	fultod d17, 9

	ftoshs s19, 7
	ftoshd d19, 7
	ftosls s19, 7
	ftosld d19, 7
	ftouhs s19, 7
	ftouhd d19, 7
	ftouls s19, 7
	ftould d19, 7
