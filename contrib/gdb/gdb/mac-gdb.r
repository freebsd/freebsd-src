/* Resource file for MacGDB. */

#include "SysTypes.r"
#include "Types.r"

#include "mac-defs.h"

/* Version resources. */

resource 'vers' (1) {
	0,
	0,
	0,
	0,
	verUS,
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
	"GDB " VERSION_STRING " for MPW"
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

#ifdef Macgdb /* Exclude the following from SIOWgdb which uses SIOW.r.  */

resource 'MBAR' (128) {
	{ mApple, mFile, mEdit, mDebug };
};

resource 'MENU' (mApple, preload) {
	mApple,
	textMenuProc,
	0x7FFFFFFD,
	enabled,
	apple,
	{
		"About...", noIcon, noKey, noMark, plain,
		"-", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (mFile, preload) {
	mFile,
	textMenuProc,
	allEnabled,
	enabled,
	"File",
	{
		"New", noIcon, noKey, noMark, plain,
		"Open...", noIcon, "O", noMark, plain,
		"-", noIcon, noKey, noMark, plain,
		"Quit", noIcon, "Q", noMark, plain
	}
};

resource 'MENU' (mEdit, preload) {
	mEdit,
	textMenuProc,
	allEnabled,
	enabled,
	"Edit",
	{
		"Undo", noIcon, "Z", noMark, plain,
		"-", noIcon, noKey, noMark, plain,
		"Cut", noIcon, "X", noMark, plain,
		"Copy", noIcon, "C", noMark, plain,
		"Paste", noIcon, "V", noMark, plain,
		"Clear", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (mDebug, preload) {
	mDebug,
	textMenuProc,
	allEnabled,
	enabled,
	"Debug",
	{
		"Target", noIcon, "T", noMark, plain,
		"-", noIcon, noKey, noMark, plain,
		"Run", noIcon, "R", noMark, plain,
		"Continue", noIcon, "K", noMark, plain,
		"Step", noIcon, "S", noMark, plain,
		"Next", noIcon, "N", noMark, plain
	}
};

resource 'ALRT' (128) {
	{40, 40, 180, 420},
	128,
	{	/* array: 4 elements */
		/* [1] */
		OK, visible, sound1,
		/* [2] */
		OK, visible, sound1,
		/* [3] */
		OK, visible, sound1,
		/* [4] */
		OK, visible, sound1
	}
};

resource 'DITL' (128) {
	{	/* array DITLarray: 2 elements */
		/* [1] */
		{110, 150, 128, 200},
		Button {
			enabled,
			"OK"
		},
		/* [2] */
		{10, 10, 100, 370},
		StaticText {
			disabled,
			"GDB 4.12.3\n"
			"Copyright ©  1994 Free Software Foundation, Inc.\n"
		},
	}
};

resource 'WIND' (wConsole, preload, purgeable) {
	{40, 40, 310, 572},
	zoomDocProc,
	visible,
	goAway,
	0x0,
	"GDB Console"
};

resource 'SIZE' (-1) {
	reserved,
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
	multiFinderAware,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	not32BitCompatible,
	isHighLevelEventAware,
	localAndRemoteHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
	5000*1024,
	2000*1024
};

resource 'DLOG' (128) {
	{40, 40, 240, 280},
	documentProc,
	visible,
	goAway,
	0x0,
	128,
	""
};

#endif
