# -*- Perl -*-

%reg_ctry = ();
%ctry_files = ();
%file_descrs = ();

while(<>) {
    next if(!/^\# ZONE-DESCR/);
    chop;
    split;
    
    shift(@_); shift(@_);	# get rid of # ZONE-DESCR

    # Now $_[0] is region, $_[1] is filename, $_[2] is country,
    # and @_[3 .. $#_] is the description
    $reg = $_[0];
    $file = $_[1];
    $ctry = $_[2];
    $descr = join(' ', @_[3 .. $#_]);

    if($reg_ctry{$reg} =~ /$ctry/) {
	# do nothing
    } else {
	$reg_ctry{$reg} = $ctry . "," . $reg_ctry{$reg};
    }

    $ctry_files{$ctry} .= ",$reg/$file";
    $file_descrs{"$reg/$file"} = $descr;
}

print "/* This file automatically generated. */\n";
print "#include \"tzsetup.h\"\n";

foreach $ctry (sort keys %ctry_files) {
    print "const char *files_$ctry\[\] = {\n";
    $ctry_files{$ctry} =~ s/^,//;
    foreach $file (sort {$file_descrs{$a} cmp $file_descrs{$b}} 
		   split(/,/, $ctry_files{$ctry})) {
	print "\t\"$file\",\n";
    }
    print "\t0 };\n";
    print "const char *menu_$ctry\[\] = {\n";
    $i = 0;

    foreach $file (sort {$file_descrs{$a} cmp $file_descrs{$b}} 
		   split(/,/, $ctry_files{$ctry})) {
	$i++;
	print "\t\"$i\", \"$file_descrs{$file}\",\n";
    }
    print "\t0, 0 };\n";

    print "struct country $ctry = { files_$ctry, menu_$ctry, $i };\n";
}

foreach $reg (sort keys %reg_ctry) {
    print "\nstruct country *menu_$reg\[\] = {\n";
    $reg_ctry{$reg} =~ s/,$//;
    foreach $ctry (sort split(/,/, $reg_ctry{$reg})) {
	print "\t&$ctry,\n";
    }

    print "\t0 };\n";
    print "const char *name_$reg\[\] = {\n";
    $i = 0;
    foreach $ctry (sort split(/,/, $reg_ctry{$reg})) {
	$i++;
	$ctry =~ s/_/ /g;
	print "\t\"$i\", \"$ctry\",\n";
    }
    print "\t0, 0 };\n";

    print "struct region $reg = { menu_$reg, name_$reg, $i };\n";
}

exit 0;

