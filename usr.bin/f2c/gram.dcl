spec:	  dcl
	| common
	| external
	| intrinsic
	| equivalence
	| data
	| implicit
	| namelist
	| SSAVE
		{ NO66("SAVE statement");
		  saveall = YES; }
	| SSAVE savelist
		{ NO66("SAVE statement"); }
	| SFORMAT
		{ fmtstmt(thislabel); setfmt(thislabel); }
	| SPARAM in_dcl SLPAR paramlist SRPAR
		{ NO66("PARAMETER statement"); }
	;

dcl:	  type opt_comma name in_dcl new_dcl dims lengspec
		{ settype($3, $1, $7);
		  if(ndim>0) setbound($3,ndim,dims);
		}
	| dcl SCOMMA name dims lengspec
		{ settype($3, $1, $5);
		  if(ndim>0) setbound($3,ndim,dims);
		}
	| dcl SSLASHD datainit vallist SSLASHD
		{ if (new_dcl == 2) {
			err("attempt to give DATA in type-declaration");
			new_dcl = 1;
			}
		}
	;

new_dcl:	{ new_dcl = 2; } ;

type:	  typespec lengspec
		{ varleng = $2; }
	;

typespec:  typename
		{ varleng = ($1<0 || ONEOF($1,M(TYLOGICAL)|M(TYLONG))
				? 0 : typesize[$1]);
		  vartype = $1; }
	;

typename:    SINTEGER	{ $$ = TYLONG; }
	| SREAL		{ $$ = tyreal; }
	| SCOMPLEX	{ ++complex_seen; $$ = tycomplex; }
	| SDOUBLE	{ $$ = TYDREAL; }
	| SDCOMPLEX	{ ++dcomplex_seen; NOEXT("DOUBLE COMPLEX statement"); $$ = TYDCOMPLEX; }
	| SLOGICAL	{ $$ = TYLOGICAL; }
	| SCHARACTER	{ NO66("CHARACTER statement"); $$ = TYCHAR; }
	| SUNDEFINED	{ $$ = TYUNKNOWN; }
	| SDIMENSION	{ $$ = TYUNKNOWN; }
	| SAUTOMATIC	{ NOEXT("AUTOMATIC statement"); $$ = - STGAUTO; }
	| SSTATIC	{ NOEXT("STATIC statement"); $$ = - STGBSS; }
	;

lengspec:
		{ $$ = varleng; }
	| SSTAR intonlyon expr intonlyoff
		{
		expptr p;
		p = $3;
		NO66("length specification *n");
		if( ! ISICON(p) || p->constblock.Const.ci <= 0 )
			{
			$$ = 0;
			dclerr("length must be a positive integer constant",
				NPNULL);
			}
		else {
			if (vartype == TYCHAR)
				$$ = p->constblock.Const.ci;
			else switch((int)p->constblock.Const.ci) {
				case 1:	$$ = 1; break;
				case 2: $$ = typesize[TYSHORT];	break;
				case 4: $$ = typesize[TYLONG];	break;
				case 8: $$ = typesize[TYDREAL];	break;
				case 16: $$ = typesize[TYDCOMPLEX]; break;
				default:
					dclerr("invalid length",NPNULL);
					$$ = varleng;
				}
			}
		}
	| SSTAR intonlyon SLPAR SSTAR SRPAR intonlyoff
		{ NO66("length specification *(*)"); $$ = -1; }
	;

common:	  SCOMMON in_dcl var
		{ incomm( $$ = comblock("") , $3 ); }
	| SCOMMON in_dcl comblock var
		{ $$ = $3;  incomm($3, $4); }
	| common opt_comma comblock opt_comma var
		{ $$ = $3;  incomm($3, $5); }
	| common SCOMMA var
		{ incomm($1, $3); }
	;

comblock:  SCONCAT
		{ $$ = comblock(""); }
	| SSLASH SNAME SSLASH
		{ $$ = comblock(token); }
	;

external: SEXTERNAL in_dcl name
		{ setext($3); }
	| external SCOMMA name
		{ setext($3); }
	;

intrinsic:  SINTRINSIC in_dcl name
		{ NO66("INTRINSIC statement"); setintr($3); }
	| intrinsic SCOMMA name
		{ setintr($3); }
	;

equivalence:  SEQUIV in_dcl equivset
	| equivalence SCOMMA equivset
	;

equivset:  SLPAR equivlist SRPAR
		{
		struct Equivblock *p;
		if(nequiv >= maxequiv)
			many("equivalences", 'q', maxequiv);
		p  =  & eqvclass[nequiv++];
		p->eqvinit = NO;
		p->eqvbottom = 0;
		p->eqvtop = 0;
		p->equivs = $2;
		}
	;

equivlist:  lhs
		{ $$=ALLOC(Eqvchain);
		  $$->eqvitem.eqvlhs = (struct Primblock *)$1;
		}
	| equivlist SCOMMA lhs
		{ $$=ALLOC(Eqvchain);
		  $$->eqvitem.eqvlhs = (struct Primblock *) $3;
		  $$->eqvnextp = $1;
		}
	;

data:	  SDATA in_data datalist
	| data opt_comma datalist
	;

in_data:
		{ if(parstate == OUTSIDE)
			{
			newproc();
			startproc(ESNULL, CLMAIN);
			}
		  if(parstate < INDATA)
			{
			enddcl();
			parstate = INDATA;
			datagripe = 1;
			}
		}
	;

datalist:  datainit datavarlist SSLASH datapop vallist SSLASH
		{ ftnint junk;
		  if(nextdata(&junk) != NULL)
			err("too few initializers");
		  frdata($2);
		  frrpl();
		}
	;

datainit: /* nothing */ { frchain(&datastack); curdtp = 0; } ;

datapop: /* nothing */ { pop_datastack(); } ;

vallist:  { toomanyinit = NO; }  val
	| vallist SCOMMA val
	;

val:	  value
		{ dataval(ENULL, $1); }
	| simple SSTAR value
		{ dataval($1, $3); }
	;

value:	  simple
	| addop simple
		{ if( $1==OPMINUS && ISCONST($2) )
			consnegop((Constp)$2);
		  $$ = $2;
		}
	| complex_const
	;

savelist: saveitem
	| savelist SCOMMA saveitem
	;

saveitem: name
		{ int k;
		  $1->vsave = YES;
		  k = $1->vstg;
		if( ! ONEOF(k, M(STGUNKNOWN)|M(STGBSS)|M(STGINIT)) )
			dclerr("can only save static variables", $1);
		}
	| comblock
	;

paramlist:  paramitem
	| paramlist SCOMMA paramitem
	;

paramitem:  name SEQUALS expr
		{ if($1->vclass == CLUNKNOWN)
			make_param((struct Paramblock *)$1, $3);
		  else dclerr("cannot make into parameter", $1);
		}
	;

var:	  name dims
		{ if(ndim>0) setbound($1, ndim, dims); }
	;

datavar:	  lhs
		{ Namep np;
		  np = ( (struct Primblock *) $1) -> namep;
		  vardcl(np);
		  if(np->vstg == STGCOMMON)
			extsymtab[np->vardesc.varno].extinit = YES;
		  else if(np->vstg==STGEQUIV)
			eqvclass[np->vardesc.varno].eqvinit = YES;
		  else if(np->vstg!=STGINIT && np->vstg!=STGBSS)
			dclerr("inconsistent storage classes", np);
		  $$ = mkchain((char *)$1, CHNULL);
		}
	| SLPAR datavarlist SCOMMA dospec SRPAR
		{ chainp p; struct Impldoblock *q;
		pop_datastack();
		q = ALLOC(Impldoblock);
		q->tag = TIMPLDO;
		(q->varnp = (Namep) ($4->datap))->vimpldovar = 1;
		p = $4->nextp;
		if(p)  { q->implb = (expptr)(p->datap); p = p->nextp; }
		if(p)  { q->impub = (expptr)(p->datap); p = p->nextp; }
		if(p)  { q->impstep = (expptr)(p->datap); }
		frchain( & ($4) );
		$$ = mkchain((char *)q, CHNULL);
		q->datalist = hookup($2, $$);
		}
	;

datavarlist: datavar
		{ if (!datastack)
			curdtp = 0;
		  datastack = mkchain((char *)curdtp, datastack);
		  curdtp = $1; curdtelt = 0;
		  }
	| datavarlist SCOMMA datavar
		{ $$ = hookup($1, $3); }
	;

dims:
		{ ndim = 0; }
	| SLPAR dimlist SRPAR
	;

dimlist:   { ndim = 0; }   dim
	| dimlist SCOMMA dim
	;

dim:	  ubound
		{
		  if(ndim == maxdim)
			err("too many dimensions");
		  else if(ndim < maxdim)
			{ dims[ndim].lb = 0;
			  dims[ndim].ub = $1;
			}
		  ++ndim;
		}
	| expr SCOLON ubound
		{
		  if(ndim == maxdim)
			err("too many dimensions");
		  else if(ndim < maxdim)
			{ dims[ndim].lb = $1;
			  dims[ndim].ub = $3;
			}
		  ++ndim;
		}
	;

ubound:	  SSTAR
		{ $$ = 0; }
	| expr
	;

labellist: label
		{ nstars = 1; labarray[0] = $1; }
	| labellist SCOMMA label
		{ if(nstars < maxlablist)  labarray[nstars++] = $3; }
	;

label:	  SICON
		{ $$ = execlab( convci(toklen, token) ); }
	;

implicit:  SIMPLICIT in_dcl implist
		{ NO66("IMPLICIT statement"); }
	| implicit SCOMMA implist
	;

implist:  imptype SLPAR letgroups SRPAR
	| imptype
		{ if (vartype != TYUNKNOWN)
			dclerr("-- expected letter range",NPNULL);
		  setimpl(vartype, varleng, 'a', 'z'); }
	;

imptype:   { needkwd = 1; } type
		/* { vartype = $2; } */
	;

letgroups: letgroup
	| letgroups SCOMMA letgroup
	;

letgroup:  letter
		{ setimpl(vartype, varleng, $1, $1); }
	| letter SMINUS letter
		{ setimpl(vartype, varleng, $1, $3); }
	;

letter:  SNAME
		{ if(toklen!=1 || token[0]<'a' || token[0]>'z')
			{
			dclerr("implicit item must be single letter", NPNULL);
			$$ = 0;
			}
		  else $$ = token[0];
		}
	;

namelist:	SNAMELIST
	| namelist namelistentry
	;

namelistentry:  SSLASH name SSLASH namelistlist
		{
		if($2->vclass == CLUNKNOWN)
			{
			$2->vclass = CLNAMELIST;
			$2->vtype = TYINT;
			$2->vstg = STGBSS;
			$2->varxptr.namelist = $4;
			$2->vardesc.varno = ++lastvarno;
			}
		else dclerr("cannot be a namelist name", $2);
		}
	;

namelistlist:  name
		{ $$ = mkchain((char *)$1, CHNULL); }
	| namelistlist SCOMMA name
		{ $$ = hookup($1, mkchain((char *)$3, CHNULL)); }
	;

in_dcl:
		{ switch(parstate)
			{
			case OUTSIDE:	newproc();
					startproc(ESNULL, CLMAIN);
			case INSIDE:	parstate = INDCL;
			case INDCL:	break;

			case INDATA:
				if (datagripe) {
					errstr(
				"Statement order error: declaration after DATA",
						CNULL);
					datagripe = 0;
					}
				break;

			default:
				dclerr("declaration among executables", NPNULL);
			}
		}
	;
