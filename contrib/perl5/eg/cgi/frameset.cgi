#!/usr/local/bin/perl

use CGI;
$query = new CGI;
print $query->header;
$TITLE="Frameset Example";

# We use the path information to distinguish between calls
# to the script to:
# (1) create the frameset
# (2) create the query form
# (3) create the query response

$path_info = $query->path_info;

# If no path information is provided, then we create 
# a side-by-side frame set
if (!$path_info) {
    &print_frameset;
    exit 0;
}

# If we get here, then we either create the query form
# or we create the response.
&print_html_header;
&print_query if $path_info=~/query/;
&print_response if $path_info=~/response/;
&print_end;


# Create the frameset
sub print_frameset {
    $script_name = $query->script_name;
    print <<EOF;
<html><head><title>$TITLE</title></head>
<frameset cols="50,50">
<frame src="$script_name/query" name="query">
<frame src="$script_name/response" name="response">
</frameset>
EOF
    ;
    exit 0;
}

sub print_html_header {
    print $query->start_html($TITLE);
}

sub print_end {
    print qq{<P><hr><A HREF="../index.html" TARGET="_top">More Examples</A>};
    print $query->end_html;
}

sub print_query {
    $script_name = $query->script_name;
    print "<H1>Frameset Query</H1>\n";
    print $query->startform(-action=>"$script_name/response",-TARGET=>"response");
    print "What's your name? ",$query->textfield('name');
    print "<P>What's the combination?<P>",
    $query->checkbox_group(-name=>'words',
			       -values=>['eenie','meenie','minie','moe']);

    print "<P>What's your favorite color? ",
    $query->popup_menu(-name=>'color',
		       -values=>['red','green','blue','chartreuse']),
    "<P>";
    print $query->submit;
    print $query->endform;
}

sub print_response {
    print "<H1>Frameset Result</H1>\n";
    unless ($query->param) {
	print "<b>No query submitted yet.</b>";
	return;
    }
    print "Your name is <EM>",$query->param(name),"</EM>\n";
    print "<P>The keywords are: <EM>",join(", ",$query->param(words)),"</EM>\n";
    print "<P>Your favorite color is <EM>",$query->param(color),"</EM>\n";
}

