#!/usr/local/bin/perl

use CGI;
$query = new CGI;

# We generate a regular HTML file containing a very long list
# and a popup menu that does nothing except to show that we
# don't lose the state information.
print $query->header;
print $query->start_html("Internal Links Example");
print "<H1>Internal Links Example</H1>\n";
print "Click <cite>Submit Query</cite> to create a state.  Then scroll down and",
    " click on any of the <cite>Jump to top</cite> links.  This is not very exciting.";

print "<A NAME=\"start\"></A>\n"; # an anchor point at the top

# pick a default starting value;
$query->param('amenu','FOO1') unless $query->param('amenu');

print $query->startform;
print $query->popup_menu('amenu',[('FOO1'..'FOO9')]);
print $query->submit,$query->endform;

# We create a long boring list for the purposes of illustration.
$myself = $query->self_url;
print "<OL>\n";
for (1..100) {
    print qq{<LI>List item #$_ <A HREF="$myself#start">Jump to top</A>\n};
}
print "</OL>\n";

print $query->end_html;

