#!/usr/local/bin/perl
     
use CGI;
use CGI::Carp qw/fatalsToBrowser/;
 
$query = new CGI;

print $query->header;
print $query->start_html("Example CGI.pm Form");
print "<H1> Example CGI.pm Form</H1>\n";
&print_prompt($query);
&do_work($query);
&print_tail;
print $query->end_html;
 
sub print_prompt {
   my($query) = @_;
 
   print $query->start_form;
   print "<EM>What's your name?</EM><BR>";
   print $query->textfield('name');
   print $query->checkbox('Not my real name');
 
   print "<P><EM>Where can you find English Sparrows?</EM><BR>";
   print $query->checkbox_group(
				-name=>'Sparrow locations',
				-Values=>[England,France,Spain,Asia,Hoboken],
				-linebreak=>'yes',
				-defaults=>[England,Asia]);
 
   print "<P><EM>How far can they fly?</EM><BR>",
   $query->radio_group(
		       -name=>'how far',
		       -Values=>['10 ft','1 mile','10 miles','real far'],
		       -default=>'1 mile');
   
   print "<P><EM>What's your favorite color?</EM>  ";
   print $query->popup_menu(-name=>'Color',
			    -Values=>['black','brown','red','yellow'],
			    -default=>'red');
 
   print $query->hidden('Reference','Monty Python and the Holy Grail');
 
   print "<P><EM>What have you got there?</EM><BR>";
   print $query->scrolling_list(
				-name=>'possessions',
				-Values=>['A Coconut','A Grail','An Icon',
					  'A Sword','A Ticket'],
				-size=>5,
				-multiple=>'true');
 
   print "<P><EM>Any parting comments?</EM><BR>";
   print $query->textarea(-name=>'Comments',
			  -rows=>10,
			  -columns=>50);
   
   print "<P>",$query->reset;
   print $query->submit('Action','Shout');
   print $query->submit('Action','Scream');
   print $query->endform;
   print "<HR>\n";
 	}
 
sub do_work {
    my($query) = @_;
    my(@values,$key);

    print "<H2>Here are the current settings in this form</H2>";

    foreach $key ($query->param) {
	print "<STRONG>$key</STRONG> -> ";
	@values = $query->param($key);
	print join(", ",@values),"<BR>\n";
    }
}
 
sub print_tail {
    print <<END;
<HR>
<ADDRESS>Lincoln D. Stein</ADDRESS><BR>
<A HREF="/">Home Page</A>
END
    ;
}
