#!/usr/local/bin/perl

use CGI;
$query = new CGI;
print $query->header;
print $query->start_html("A Clickable Image");
print <<END;
<H1>A Clickable Image</H1>
</A>
END
print "Sorry, this isn't very exciting!\n";

print $query->startform;
print $query->image_button('picture',"./wilogo.gif");
print "Give me a: ",$query->popup_menu('letter',['A','B','C','D','E','W']),"\n"; # 
print "<P>Magnification: ",$query->radio_group('magnification',['1X','2X','4X','20X']),"\n";
print "<HR>\n";

if ($query->param) {
    print "<P>Magnification, <EM>",$query->param('magnification'),"</EM>\n";
    print "<P>Selected Letter, <EM>",$query->param('letter'),"</EM>\n";
    ($x,$y) = ($query->param('picture.x'),$query->param('picture.y'));
    print "<P>Selected Position <EM>($x,$y)</EM>\n";
}

print $query->end_html;
