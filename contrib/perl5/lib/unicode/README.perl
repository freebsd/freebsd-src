The *.txt files were copied 30 Aug 2000 from

	http://www.unicode.org/Public/UNIDATA/

and most of them were renamed to better fit 8.3 filename limitations,
by which the Perl distribution tries to live.

	www.unicode.org			Perl distribution

	ArabicShaping.txt		ArabShap.txt
	BidiMirroring.txt		BidiMirr.txt
	Blocks.txt			Blocks.txt
	CaseFolding.txt			CaseFold.txt
	CompositionExclusions.txt	CompExcl.txt
	EastAsianWidth.txt		EAWidth.txt	(0)
	Index.txt			Index.txt
	Jamo.txt			Jamo.txt
	LineBreak.txt			LineBrk.txt	(0)
	NamesList.html			NamesList.html	(0)
	NamesList.txt			Names.txt
	PropList.txt			PropList.txt
	ReadMe.txt			ReadMe.txt
	SpecialCasing.txt		SpecCase.txt
	UnicodeCharacterDatabase.html	UCD301.html
	UnicodeData.html		UCDFF301.html
	UnicodeData.txt			Unicode.301

The two big files, NormalizationTest.txt (1.7MB) and Unihan.txt (15.8MB)
were not copied for space considerations.  The files marked with (0) had
not been updated since Unicode 3.0.0 (10 Sep 1999)

The *.pl files are generated from these files by the 'mktables.PL' script.

While the files have been renamed the links in the html files haven't.

-- 
jhi@iki.fi
