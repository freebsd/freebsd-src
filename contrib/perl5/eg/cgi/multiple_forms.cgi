#!/usr/local/bin/perl

use CGI;

$query = new CGI;
print $query->header;
print $query->start_html('Multiple Forms');
print "<H1>Multiple Forms</H1>\n";

# Print the first form
print $query->startform;
$name = $query->remote_user || 'anonymous@' . $query->remote_host;

print "What's your name? ",$query->textfield('name',$name,50);
print "<P>What's the combination?<P>",
        $query->checkbox_group('words',['eenie','meenie','minie','moe']);
print "<P>What's your favorite color? ",
        $query->popup_menu('color',['red','green','blue','chartreuse']),
	"<P>";
print $query->submit('form_1','Send Form 1');
print $query->endform;

# Print the second form
print "<HR>\n";
print $query->startform;
print "Some radio buttons: ",$query->radio_group('radio buttons',
						 [qw{one two three four five}],'three'),"\n";
print "<P>What's the password? ",$query->password_field('pass','secret');
print $query->defaults,$query->submit('form_2','Send Form 2'),"\n";
print $query->endform;

print "<HR>\n";

$query->import_names('Q');
if ($Q::form_1) {
    print "<H2>Form 1 Submitted</H2>\n";
    print "Your name is <EM>$Q::name</EM>\n";
    print "<P>The combination is: <EM>{",join(",",@Q::words),"}</EM>\n";
    print "<P>Your favorite color is <EM>$Q::color</EM>\n";
} elsif ($Q::form_2) {
    print <<EOF;
<H2>Form 2 Submitted</H2>
<P>The value of the radio buttons is <EM>$Q::radio_buttons</EM>
<P>The secret password is <EM>$Q::pass</EM>
EOF
    ;
}
print qq{<P><A HREF="./">Other examples</A>};
print qq{<P><A HREF="../cgi_docs.html">Go to the documentation</A>};

print $query->end_html;



