# emulate a C preprocessor (well, sort of)
:TOP
y/	/ /
s/  */ /g
s%/\*.*\*/%%
/\/\*/{
	:COMMENT
	/\*\//!{
		s/.*//
		N
		bCOMMENT
	}
	s%^.*\*/%%
	bTOP
}
/^ *# *ifdef/{
	s/^ *# *ifdef //
	b
}
/^ *# *ifndef/{
	s/^ *# *ifndef //
	b
}
/^ *# *if.*defined/{
	s/^ *# *if //
	:IF
	/^defined/!{
		:NUKE
		s/^.//
		/^defined/!bNUKE
	}
	h
	/^defined/s/^defined *( *\([A-Za-z0-9_]*\) *).*/\1/p
	g
	/^defined/s/^defined *( *\([[A-Za-z0-9_]*\) *)//
	/defined/!{
		d
		b
	}
	bIF
}
d
