.explicit
_start:
{.mib
	br.cloop.sptk	start
}	;;
{.mib
	nop		0
	br.cloop.sptk	start
}	;;
{.mbb
	br.cloop.sptk	start
	nop		0
}	;;
{.mbb
	nop		0
	br.cloop.sptk	start
	nop		0
}	;;
