  /*  Input/Output Statements */

io:	  io1
		{ endio(); }
	;

io1:	  iofmove ioctl
	| iofmove unpar_fexpr
		{ ioclause(IOSUNIT, $2); endioctl(); }
	| iofmove SSTAR
		{ ioclause(IOSUNIT, ENULL); endioctl(); }
	| iofmove SPOWER
		{ ioclause(IOSUNIT, IOSTDERR); endioctl(); }
	| iofctl ioctl
	| read ioctl
		{ doio(CHNULL); }
	| read infmt
		{ doio(CHNULL); }
	| read ioctl inlist
		{ doio(revchain($3)); }
	| read infmt SCOMMA inlist
		{ doio(revchain($4)); }
	| read ioctl SCOMMA inlist
		{ doio(revchain($4)); }
	| write ioctl
		{ doio(CHNULL); }
	| write ioctl outlist
		{ doio(revchain($3)); }
	| print
		{ doio(CHNULL); }
	| print SCOMMA outlist
		{ doio(revchain($3)); }
	;

iofmove:   fmkwd end_spec in_ioctl
	;

fmkwd:	  SBACKSPACE
		{ iostmt = IOBACKSPACE; }
	| SREWIND
		{ iostmt = IOREWIND; }
	| SENDFILE
		{ iostmt = IOENDFILE; }
	;

iofctl:  ctlkwd end_spec in_ioctl
	;

ctlkwd:	  SINQUIRE
		{ iostmt = IOINQUIRE; }
	| SOPEN
		{ iostmt = IOOPEN; }
	| SCLOSE
		{ iostmt = IOCLOSE; }
	;

infmt:	  unpar_fexpr
		{
		ioclause(IOSUNIT, ENULL);
		ioclause(IOSFMT, $1);
		endioctl();
		}
	| SSTAR
		{
		ioclause(IOSUNIT, ENULL);
		ioclause(IOSFMT, ENULL);
		endioctl();
		}
	;

ioctl:	  SLPAR fexpr SRPAR
		{
		  ioclause(IOSUNIT, $2);
		  endioctl();
		}
	| SLPAR ctllist SRPAR
		{ endioctl(); }
	;

ctllist:  ioclause
	| ctllist SCOMMA ioclause
	;

ioclause:  fexpr
		{ ioclause(IOSPOSITIONAL, $1); }
	| SSTAR
		{ ioclause(IOSPOSITIONAL, ENULL); }
	| SPOWER
		{ ioclause(IOSPOSITIONAL, IOSTDERR); }
	| nameeq expr
		{ ioclause($1, $2); }
	| nameeq SSTAR
		{ ioclause($1, ENULL); }
	| nameeq SPOWER
		{ ioclause($1, IOSTDERR); }
	;

nameeq:  SNAMEEQ
		{ $$ = iocname(); }
	;

read:	  SREAD end_spec in_ioctl
		{ iostmt = IOREAD; }
	;

write:	  SWRITE end_spec in_ioctl
		{ iostmt = IOWRITE; }
	;

print:	  SPRINT end_spec fexpr in_ioctl
		{
		iostmt = IOWRITE;
		ioclause(IOSUNIT, ENULL);
		ioclause(IOSFMT, $3);
		endioctl();
		}
	| SPRINT end_spec SSTAR in_ioctl
		{
		iostmt = IOWRITE;
		ioclause(IOSUNIT, ENULL);
		ioclause(IOSFMT, ENULL);
		endioctl();
		}
	;

inlist:	  inelt
		{ $$ = mkchain((char *)$1, CHNULL); }
	| inlist SCOMMA inelt
		{ $$ = mkchain((char *)$3, $1); }
	;

inelt:	  lhs
		{ $$ = (tagptr) $1; }
	| SLPAR inlist SCOMMA dospec SRPAR
		{ $$ = (tagptr) mkiodo($4,revchain($2)); }
	;

outlist:  uexpr
		{ $$ = mkchain((char *)$1, CHNULL); }
	| other
		{ $$ = mkchain((char *)$1, CHNULL); }
	| out2
	;

out2:	  uexpr SCOMMA uexpr
		{ $$ = mkchain((char *)$3, mkchain((char *)$1, CHNULL) ); }
	| uexpr SCOMMA other
		{ $$ = mkchain((char *)$3, mkchain((char *)$1, CHNULL) ); }
	| other SCOMMA uexpr
		{ $$ = mkchain((char *)$3, mkchain((char *)$1, CHNULL) ); }
	| other SCOMMA other
		{ $$ = mkchain((char *)$3, mkchain((char *)$1, CHNULL) ); }
	| out2  SCOMMA uexpr
		{ $$ = mkchain((char *)$3, $1); }
	| out2  SCOMMA other
		{ $$ = mkchain((char *)$3, $1); }
	;

other:	  complex_const
		{ $$ = (tagptr) $1; }
	| SLPAR expr SRPAR
		{ $$ = (tagptr) $2; }
	| SLPAR uexpr SCOMMA dospec SRPAR
		{ $$ = (tagptr) mkiodo($4, mkchain((char *)$2, CHNULL) ); }
	| SLPAR other SCOMMA dospec SRPAR
		{ $$ = (tagptr) mkiodo($4, mkchain((char *)$2, CHNULL) ); }
	| SLPAR out2  SCOMMA dospec SRPAR
		{ $$ = (tagptr) mkiodo($4, revchain($2)); }
	;

in_ioctl:
		{ startioctl(); }
	;
