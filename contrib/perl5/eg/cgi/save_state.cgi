#!/usr/local/bin/perl

use CGI;
$query = new CGI;

print $query->header;
print $query->start_html("Save and Restore Example");
print "<H1>Save and Restore Example</H1>\n";

# Here's where we take action on the previous request
&save_parameters($query)              if $query->param('action') eq 'SAVE';
$query = &restore_parameters($query)  if $query->param('action') eq 'RESTORE';

# Here's where we create the form
print $query->start_multipart_form;
print "Popup 1: ",$query->popup_menu('popup1',[qw{red green purple magenta orange chartreuse brown}]),"\n";
print "Popup 2: ",$query->popup_menu('popup2',[qw{lion tiger bear zebra potto wildebeest frog emu gazelle}]),"\n";
print "<P>";
$default_name = $query->remote_addr . '.sav';
print "Save/restore state from file: ",$query->textfield('savefile',$default_name),"\n";
print "<P>";
print $query->submit('action','SAVE'),$query->submit('action','RESTORE');
print "<P>",$query->defaults;
print $query->endform;

# Here we print out a bit at the end
print $query->end_html;

sub save_parameters {
    local($query) = @_;
    local($filename) = &clean_name($query->param('savefile'));
    if (open(FILE,">$filename")) {
	$query->save(FILE);
	close FILE;
	print "<STRONG>State has been saved to file $filename</STRONG>\n";
	print "<P>If you remember this name you can restore the state later.\n";
    } else {
	print "<STRONG>Error:</STRONG> couldn't write to file $filename: $!\n";
    }
}

sub restore_parameters {
    local($query) = @_;
    local($filename) = &clean_name($query->param('savefile'));
    if (open(FILE,$filename)) {
	$query = new CGI(FILE);  # Throw out the old query, replace it with a new one
	close FILE;
	print "<STRONG>State has been restored from file $filename</STRONG>\n";
    } else {
	print "<STRONG>Error:</STRONG> couldn't restore file $filename: $!\n";
    }
    return $query;
}


# Very important subroutine -- get rid of all the naughty
# metacharacters from the file name. If there are, we
# complain bitterly and die.
sub clean_name {
   local($name) = @_;
   unless ($name=~/^[\w\._\-]+$/) {
      print "<STRONG>$name has naughty characters.  Only ";
      print "alphanumerics are allowed.  You can't use absolute names.</STRONG>";
      die "Attempt to use naughty characters";
   }
   return "WORLD_WRITABLE/$name";
}
