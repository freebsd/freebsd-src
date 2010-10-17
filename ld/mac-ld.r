/* Resources for GNU LD. */

#include "SysTypes.r"

/* Version resources. */

resource 'vers' (1)  {
	0,
	0,
	0,
	0,
	verUs,
	VERSION_STRING,
	VERSION_STRING  " (C) 1986-95 FSF, Inc."
};

resource 'vers' (2, purgeable)  {
	0,
	0,
	0,
	0,
	verUs,
	VERSION_STRING,
	"GLD " VERSION_STRING " for MPW"
};

#ifdef WANT_CFRG

#include "CodeFragmentTypes.r"

resource 'cfrg' (0) {
	{
		kPowerPC,
		kFullLib,
		kNoVersionNum, kNoVersionNum,
		0, 0,
		kIsApp, kOnDiskFlat, kZeroOffset, kWholeFork,
		PROG_NAME
	}
};

#endif /* WANT_CFRG */
