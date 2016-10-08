/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 source code published at the 9fans list by Rob Pike,
 * <http://lists.cse.psu.edu/archives/9fans/2002-February/015773.html>
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)slug.h	1.5 (gritter) 10/31/05	*/
enum slugtypes {
	NONE,		// can't happen
	VBOX,		// Vertical Box -- printable stuff
	SP,		// paddable SPace
	BS,		// start Breakable Stream
	US,		// start Unbreakable Stream
	BF,		// start Breakable Float
	UF,		// start Unbreakable Float
	PT,		// start Page Top material (header)
	BT,		// start page BoTtom material (footer)
	END,		// ENDs of groups
	NEUTRAL,	// NEUTRALized slugs can do no harm (cf. CIA)
	PAGE,		// beginning of PAGE in troff input
	TM,		// Terminal Message to appear during output
	COORD,		// output page COORDinates
	NE,		// NEed command
	MC,		// Multiple-Column command
	CMD,		// misc CoMmanDs:  FC, FL, BP
	PARM,		// misc PARaMeters:  NP, FO
	LASTTYPE	// can't happen either
};

enum cmdtypes {
	FC,	// Freeze 2-Column material
	FL,	// FLush all floats before reading more stream
	BP	// Break Page
};

enum parmtypes {
	NP,	// distance of top margin from page top (New Page)
	FO,	// distance of bottom margin from page top (FOoter)
	PL,	// distance of physical page bottom from page top (Page Length)
	MF,	// minimum fullness required for padding
	CT,	// tolerance for division into two columns
	WARN,	// warnings to stderr?	
	DBG	// debugging flag
};

class slug {
	int	serialnum;
	int	dp;		// offset of data for this slug in inbuf
	int	linenum;	// input line number (approx) for this slug
	int	font;		// font in effect at slug beginning
	float	size;		// size in effect at slug beginning
	int	seen;		// 0 until output
	int	ncol;		// number of columns (1 or 2)
	int	offset;		// horizontal offset for 2 columns
  public:
	int	type;		// VBOX, PP, etc.
	int	parm;		// parameter
	int	base;		// "depth" of this slug (from n command)
	int	hpos;		// abs horizontal position
	int	dv;		// height of this slug above its input Vpos
	union {
		int	ht;	// "height" of this slug (from n command)
		int	parm2;	// second parameter, since only VBOXes have ht
	};
	friend	slug getslug(FILE *);
	friend	void checkout();
	friend	slug eofslug();
	void	coalesce();	// with next slug in array slugs[]
	void	neutralize();	// render this one a no-op
	void	dump();		// dump its contents for debugging
	char	*headstr();	// string value of text
	void	slugout(int);	// add the slug to the output
	char	*typname();	// printable slug type
	int	serialno()	{ return serialnum; }
	int	numcol()	{ return ncol; }
	int	lineno()	{ return linenum; }
};

// functions in slug.c
slug	eofslug();
slug	getslug(FILE *);
