#!/usr/local/bin/perl

use CGI qw(:standard);

@ANIMALS=sort qw/lion tiger bear pig porcupine ferret zebra gnu ostrich
    emu moa goat weasel yak chicken sheep hyena dodo lounge-lizard
    squirrel rat mouse hedgehog racoon baboon kangaroo hippopotamus
    giraffe/;

# Recover the previous animals from the magic cookie.
# The cookie has been formatted as an associative array
# mapping animal name to the number of animals.
%zoo = cookie('animals');

# Recover the new animal(s) from the parameter 'new_animal'
@new = param('new_animals');

# If the action is 'add', then add new animals to the zoo.  Otherwise
# delete them.
foreach (@new) {
    if (param('action') eq 'Add') {
	$zoo{$_}++;
    } elsif (param('action') eq 'Delete') {
	$zoo{$_}-- if $zoo{$_};
	delete $zoo{$_} unless $zoo{$_};
    }
}

# Add new animals to old, and put them in a cookie
$the_cookie = cookie(-name=>'animals',
		     -value=>\%zoo,
		     -expires=>'+1h');

# Print the header, incorporating the cookie and the expiration date...
print header(-cookie=>$the_cookie);

# Now we're ready to create our HTML page.
print start_html('Animal crackers');

print <<EOF;
<h1>Animal Crackers</h1>
Choose the animals you want to add to the zoo, and click "add".
Come back to this page any time within the next hour and the list of 
animals in the zoo will be resurrected.  You can even quit Netscape
completely!
<p>
Try adding the same animal several times to the list.  Does this
remind you vaguely of a shopping cart?
<p>
<em>This script only works with Netscape browsers</em>
<p>
<center>
<table border>
<tr><th>Add/Delete<th>Current Contents
EOF
    ;

print "<tr><td>",start_form;
print scrolling_list(-name=>'new_animals',
		     -values=>[@ANIMALS],
		     -multiple=>1,
		     -override=>1,
		     -size=>10),"<br>";
print submit(-name=>'action',-value=>'Delete'),
    submit(-name=>'action',-value=>'Add');
print end_form;

print "<td>";
if (%zoo) {			# make a table
    print "<ul>\n";
    foreach (sort keys %zoo) {
	print "<li>$zoo{$_} $_\n";
    }
    print "</ul>\n";
} else {
    print "<strong>The zoo is empty.</strong>\n";
}
print "</table></center>";

print <<EOF;
<hr>
<ADDRESS>Lincoln D. Stein</ADDRESS><BR>
<A HREF="./">More Examples</A>
EOF
    ;
print end_html;


