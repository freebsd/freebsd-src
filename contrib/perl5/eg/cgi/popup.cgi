#!/usr/local/bin/perl

use CGI;
$query = new CGI;
print $query->header;
print $query->start_html('Popup Window');


if (!$query->param) {
    print "<H1>Ask your Question</H1>\n";
    print $query->startform(-target=>'_new');
    print "What's your name? ",$query->textfield('name');
    print "<P>What's the combination?<P>",
    $query->checkbox_group(-name=>'words',
			   -values=>['eenie','meenie','minie','moe'],
			   -defaults=>['eenie','moe']);

    print "<P>What's your favorite color? ",
    $query->popup_menu(-name=>'color',
		       -values=>['red','green','blue','chartreuse']),
    "<P>";
    print $query->submit;
    print $query->endform;

} else {
    print "<H1>And the Answer is...</H1>\n";
    print "Your name is <EM>",$query->param(name),"</EM>\n";
    print "<P>The keywords are: <EM>",join(", ",$query->param(words)),"</EM>\n";
    print "<P>Your favorite color is <EM>",$query->param(color),"</EM>\n";
}
print qq{<P><A HREF="cgi_docs.html">Go to the documentation</A>};
print $query->end_html;
