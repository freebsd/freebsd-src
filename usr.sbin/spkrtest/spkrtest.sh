:
#
# Test script for the speaker driver
#
# v1.0 by Eric S. Raymond (Feb 1990)
# v1.1 rightstuff contributed by Eric S. Tiedemann (est@snark.thyrsus.com)
#
reveille="t255l8c.f.afc~c.f.afc~c.f.afc.f.a..f.~c.f.afc~c.f.afc~c.f.afc~c.f.."
contact="<cd<a#~<a#>f"
dance="t240<cfcfgagaa#b#>dc<a#a.~fg.gaa#.agagegc.~cfcfgagaa#b#>dc<a#a.~fg.gga.agfgfgf."
loony="t255cf8f8edc<a>~cf8f8edd#e~ce8cdce8cd.<a>c8c8c#def8af8"
sinister="mst200o2ola.l8bc.~a.~>l2d#"
rightstuff="olcega.a8f>cd2bgc.c8dee2"
toccata="msl16oldcd4mll8pcb-agf+4.g4p4<msl16dcd4mll8pa.a+f+4p16g4"
startrek="l2b.f+.p16a.c+.p l4mn<b.>e8a2mspg+e8c+f+8b2"

case $1 in
reveille) echo  $reveille >/dev/speaker;;
contact)  echo  $contact >/dev/speaker;;
dance)  echo  $dance >/dev/speaker;;
loony)  echo  $loony >/dev/speaker;;
sinister)  echo  $sinister >/dev/speaker;;
rightstuff) echo  $rightstuff >/dev/speaker;;
toccata) echo  $toccata >/dev/speaker;;
startrek) echo  $startrek >/dev/speaker;;
*)
	echo "No such tune. Available tunes are:"
	echo
	echo "reveille -- Reveille"
	echo "contact -- Contact theme from Close Encounters"
	echo "dance -- Lord of the Dance (aka Simple Gifts)"
	echo "loony -- Loony Toons theme"
	echo "sinister -- standard villain's entrance music"
	echo "rightstuff -- a trope from \"The Right Stuff\" score by Bill Conti"
	echo "toccata -- opening bars of Bach's Toccata and Fugue in D Minor"
	echo "startrek -- opening bars of the theme from Star Trek Classic"
	;;
esac
