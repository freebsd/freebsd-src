#!/usr/local/bin/perl

$DIFF = "/usr/bin/diff";
$PERL = "/usr/bin/perl";

use CGI qw(:standard);
use CGI::Carp;

print header;
print start_html("File Diff Example");
print "<strong>Version </strong>$CGI::VERSION<p>";

print <<EOF;
<H1>File Diff Example</H1>
Enter two files.  When you press "submit" their diff will be
produced.
EOF
    ;

# Start a multipart form.
print start_multipart_form;
print "File #1:",filefield(-name=>'file1',-size=>45),"<BR>\n";
print "File #2:",filefield(-name=>'file2',-size=>45),"<BR>\n";
print "Diff type: ",radio_group(-name=>'type',
					-value=>['context','normal']),"<br>\n";
print reset,submit(-name=>'submit',-value=>'Do Diff');
print endform;

# Process the form if there is a file name entered
$file1 = param('file1');
$file2 = param('file2');

$|=1;				# for buffering
if ($file1 && $file2) {
    $realfile1 = tmpFileName($file1);
    $realfile2 = tmpFileName($file2);
    print "<HR>\n";
    print "<H2>$file1 vs $file2</H2>\n";

    print "<PRE>\n";
    $options = "-c" if param('type') eq 'context';
    system "$DIFF $options $realfile1 $realfile2 | $PERL -pe 's/>/&gt;/g; s/</&lt;/g;'";
    close $file1;
    close $file2;
    print "</PRE>\n";
}

print <<EOF;
<HR>
<A HREF="../cgi_docs.html">CGI documentation</A>
<HR>
<ADDRESS>
<A HREF="/~lstein">Lincoln D. Stein</A>
</ADDRESS><BR>
Last modified 17 July 1996
EOF
    ;
print end_html;

sub sanitize {
    my $name = shift;
    my($safe) = $name=~/([a-zA-Z0-9._~#,]+)/;
    unless ($safe) {
	print "<strong>$name is not a valid Unix filename -- sorry</strong>";
	exit 0;
    }
    return $safe;
}
