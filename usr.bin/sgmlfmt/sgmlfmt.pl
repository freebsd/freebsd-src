#!/usr/bin/perl
# $Id: sgmlfmt.pl,v 1.9 1996/03/26 13:26:53 jfieber Exp $

# Format an sgml document tagged according to the linuxdoc DTD.
# by John Fieber <jfieber@freebsd.org> for the FreeBSD documentation
# project.  
#
# Bugs:
#
# Text lines that start with a period (.) confuse the conversions that
# use groff.  The workaround is to make sure the SGML source doesn't
# have any periods at the beginning of a line.
#
# Although legal by the DTD, it the ascii formatting gets botched if
# the <heading></heading> tags are omitted following a <sect?>.
#
# Beginning and end tags for the <tag> element must occur on the same line.
#
# The whole approach of using sgmlsasp and passing a few things
# through for processing by this script is doomed.  This whole thing
# needs to be re-thought and a better DTD should be used anyway.
#
#######################################################################

# Look in a couple places for the SGML DTD and replacement files
#

require 'newgetopt.pl';

if (-d "$ENV{'HOME'}/lib/sgml/FreeBSD") {
    $sgmldir = "$ENV{'HOME'}/lib/sgml";
}
elsif (-d "$ENV{'HOME'}/sgml/FreeBSD") {
    $sgmldir = "$ENV{'HOME'}/sgml";
}
elsif (-d "/usr/share/sgml/FreeBSD" ) {
    $sgmldir = "/usr/share/sgml";
}
else {
    die "Cannot locate sgml files!\n";
}

#
# Locate the DTD, an SGML declaration, and the replacement files
#

$doctype = "<!DOCTYPE html PUBLIC \"-//IETF//DTD HTML 2.0//EN\">";
$dtdbase = "$sgmldir/FreeBSD";
$dtd = "$dtdbase/dtd/linuxdoc";
if (-f "$dtd.dec") {
    $decl = "$dtd.dec";
}
else {
    $decl = "";
}
$replbase = "$dtdbase/rep";

if (! $ENV{"SGML_PATH"}) {
    $ENV{"SGML_PATH"} = "$sgmldir/%O/%C/%T";
}

sub usage {
    print "Usage:\n";
    print "sgmlfmt -f <format> [-i <namea> ...] [-links] [-ssi] file\n";
    print "where <format> is one of: html, latex, ascii, nroff\n";
}

#
# Look for the file specified on the command line
#

sub getfile {
    local($filearg) = @_;
    if (-f "$filearg.sgml") {
	$file = "$filearg.sgml";
    }
    elsif (-f $filearg) {
	$file = $filearg;
    }
    else {
	return 0;
    }
    $fileroot = $file;
    $fileroot =~ s/.*\///;	# drop the path
    $fileroot =~ s/\.sgml$//;	# drop the .sgml
    $filepath = $file;
    $filepath =~ s/\/*[^\/]*$//;	
    if ($filepath eq "") {
	$ENV{"SGML_PATH"} .= ":.";
    }
    else {
	$ENV{"SGML_PATH"} .= ":$filepath/%S:.";
    }
    return 1;
}

#
# A function to run sgmls and sgmlsasp on the input file.
#
# Arguments:
#   1. A file handle for the output
#   2. A replacement file (directory actually)
#

sub sgmlparse {
    local($fhandle, $replacement) = @_;
    $defines = join(" -i ", @opt_i);
    if ($defines ne "") {
	$defines = "-i $defines";
    }
    $ENV{'SGML_PATH'} = "$replbase/$replacement.%N:$ENV{'SGML_PATH'}";
    open($fhandle, "sgmls $defines $decl $file | sgmlsasp $replbase/$replacement.mapping |");
}

#
# Generate nroff output
#

sub gen_nroff {
    open (outfile, ">$fileroot.nroff");
    &sgmlparse(infile, "nroff");
    $\ = "\n";              # automatically add newline on print
    while (<infile>) {
	chop;
	# This is supposed to ensure that no text line starts
	# with a dot (.), thus confusing groff, but it doesn't
	# appear to work.
	unless (/^\.DS/.../^\.DE/) {
	    s/^[ \t]{1,}(.*)/$1/g;
	}
	s/^\.[ \t].*/\\\&$&/g;
	print outfile;
    }
    $\ = "";
    close(infile);
    close(outfile);
}

#
# Generate ASCII output using groff
#

sub gen_ascii {
    &sgmlparse(infile, "nroff");
    open(outfile, "| groff -T ascii -t -ms | col -b > $fileroot.ascii");
    while (<infile>) {
	print outfile;
    }
    close(infile);
    close(outfile);
}

#
# Generate Postscript output using groff (this is suboptimal
# for printed output!)
#

sub gen_ps {
    &sgmlparse(infile, "grops");
    open(outfile, "| groff -T ps -t -ms > $fileroot.ps");
    while (<infile>) {
	print outfile;
    }
    close(infile);
    close(outfile);
}

#
# Generate LaTeX output
#

sub gen_latex {
    open(outfile, ">$fileroot.tex");
    &sgmlparse(infile, "latex");
    while (<infile>) {
	print outfile;
    }
    close(infile);
    close(outfile);
}


#
# Generate HTML output.
#
# HTML is generated in two passes.  
#
# The first pass takes the output from sgmlsasp and gathers information
# about the structure of the document that is used in the sceond pass
# for splitting the file into separate files.  Targets for cross
# references are also stored in this pass.
#
# Based on the information from the first pass, the second pass
# generates a numbered series of HTML files, a "toplevel" file
# containing the title, author, abstract and a brief table of
# contents.  A detailed table of contents is also generated.  The
# second pass generates links for cross references and URLs.

#
# Tunable parameters
#
$maxlevel = 3;			# max section depth
$num_depth = 4;			# depth of numbering
$m_depth = 2;			# depth of menus


$sc = 0;			# section counter
$filecount = 0;			# file counter

# Other variables:
#
#  st_xxxx  - Section Table.  Arrays containing information about a
#             given section.  To be accesssed via the section counter $sc.
#             
#  st_ol    - The output level of the given section.  I.E. how many
#             levels from the table of contents does it lie in terms
#             of HTML files which is distinct from <sect1>, <sect2> etc.
#             levels. 
#
#  st_sl    - The absolute depth of a section.  Contrast st_ol.
# 
#  st_num   - The section number in the form X.Y.Z....
#
#  st_file  - The HTML file the section belongs to.
#
#  st_header - The text of the section title.
# 
#  st_parent - The section number of the given sections parent.

sub gen_html {
    local($i, $sl);
    $tmpfile = "/tmp/sgmlf.$$";

    open(bar, ">$tmpfile");
#    print STDERR "(Pass 1...";
    &sgmlparse(foo, "html");
    while (<foo>) {
	print bar;
	# count up the number of files to be generated
	# and gather assorted info about the document structure
	if (/^<\@\@sect>/) {
	    $sl++;		# current section level
	    $sc++;		# current section number
	    $st_sl[$sc] = $sl;

	    # In case this section has subsections, set the parent
	    # pointer for this level to point at this section.
	    $parent_pointer[$sl] = $sc;

	    # Figure out who is the parent if this section.
	    $st_parent[$sc] = $parent_pointer[$sl - 1];

	    # Per level counters
	    $counter[$sl]++;
	    $counter[$sl + 1] = 0;

	    # calculate the section number in the form x.y.z.
	    if ($sl <= $num_depth) {
		$st_num[$sc] = $st_num[$st_parent[$sc]] . "$counter[$sl].";
	    }

	    # calculate the file number and output level
	    if ($sl <= $maxlevel) {
		$filecount++;
		$st_ol[$sc] = $sl;
	    }
	    else {
		$st_ol[$sc] = $maxlevel;
	    }

	    $st_file[$sc] = $filecount;

	    # Calculate the highest level node in which this
	    # node should appear as a menu item.  
	    $st_pl[$sc] = $sl - $m_depth;
	    if ($st_pl[$sc] < 0) {
		$st_pl[$sc] = 0;
	    }
	    if ($st_pl[$sc] > $maxlevel) { 
		$st_pl[$sc] = $maxlevel;
	    } 
	}
	if (/^<\@\@endsect>/) {
	    $sl--;
	}

	# record section titles
	if (/^<\@\@head>/) {
	    chop;
	    s/^<\@\@head>//;
	    $st_header[$sc] = $_;
	}

	# record the section number that a label occurs in
	if (/^<\@\@label>/) {
	    chop;
	    s/^<\@\@label>//;
	    if ($references{$_} eq "") {
		$references{$_} = "$filecount";
		if ($opt_links) {
		    &extlink($_, "${fileroot}${filecount}.html");
		}
	    }
	    else {
		print STDERR "Warning: the label `$_' is multiply-defined.\n";
	    }
	}
    }
    close(bar);

    open(foofile, $tmpfile);
    &html2html(foofile, "boo");

    unlink($tmpfile);
}

#
# HTML conversion, pass number 2
#

sub html2html {
    local($infile, $outfile) = @_;
    local($i);

    $sc = 0;
    push(@scs, $sc);

    open(tocfile, ">${fileroot}_toc.html");
    print tocfile "$doctype\n<HTML>\n";

    while (<$infile>) {
	# change `<' and `>' to `&lt;' and `&gt;' in <pre></pre>
	if (/<pre>/.../<\/pre>/) {
	    s/</\&lt;/g;
	    s/\&lt;([\/]*)pre>/<\1pre>/g;
	    s/>/\&gt;/g;
	    s/<([\/]*)pre\&gt;/<\1pre>/g;
	}

	# remove extraneous empty paragraphs (it is arguable that this
 	# is really a bug with the DTD, but changing it would break
 	# almost every document written to this DTD.)
	s/<p><\/p>//;

      tagsw: {
	  # titles and headings
	  if (s/^<\@\@title>//) {
	      chop;
	      $st_header[0] = $_;
	      $st_parent[0] = -1;
	      print tocfile "<HEAD>\n<TITLE>$st_header[0]</TITLE>\n</HEAD>\n";
	      print tocfile "<H1>$st_header[0]</H1>\n";

	      $header[$st_ol[$sc]] = 
		  "$doctype\n<HTML>\n<HEAD>\n<TITLE>$st_header[0]</TITLE>\n" . 
		      "</HEAD>\n<BODY>\n";
	      if ($opt_ssi) {	# Server Side Include hook
		  $header[$st_ol[$sc]] .=
		      "<!--#include virtual=\"./$fileroot.hdr\" -->";
	      }
	      $header[$st_ol[$sc]] .= "\n<H1>$st_header[0]</H1>\n"; 

	      $footer[$st_ol[$sc]] = "\n";
	      if ($opt_ssi) {	# Server Side Include hook
		  $footer[$st_ol[$sc]] .= 
		      "<!--#include virtual=\"./$fileroot.ftr\" -->";
	      }
	      $footer[$st_ol[$sc]] .= "\n</BODY>\n</HTML>\n";
	      last tagsw;
	  }

	  #
	  # HEADER begin
	  #
	  if (s/^<\@\@head>//) {
	      chop;

	      if ($part == 1) {
		  $text[0] .= "<H1>Part $partnum:<BR>$_";
		  last tagsw;
	      }

	      $href = "\"${fileroot}$st_file[$sc].html#$sc\"";

	      # set up headers and footers
	      if ($st_sl[$sc] > 0 && $st_sl[$sc] <= $maxlevel) {
		  $header[$st_ol[$sc]] = 
		      "$doctype\n<HTML>\n<HEAD>\n<TITLE>$_</TITLE>\n</HEAD>\n<BODY>\n";
		  if ($opt_ssi) { # Server Side Include hook
		      $header[$st_ol[$sc]] .=
			  "<!--#include virtual=\"./$fileroot.hdr$st_ol[$sc]\" -->";
		  }
		  $header[$st_ol[$sc]] .= "\n$navbar[$st_ol[$sc]]\n<HR>\n";
		  $footer[$st_ol[$sc]] = "<HR>\n$navbar[$st_ol[$sc]]\n";
		  if ($opt_ssi) { # Server Side Include hook
		      $footer[$st_ol[$sc]] .=
			  "<!--#include virtual=\"./$fileroot.ftr$st_ol[$sc]\" -->";
		  }
                  $footer[$st_ol[$sc]] .= "\n</BODY>\n</HTML>\n";
	      }

	      # Add this to the master table of contents
	      print tocfile "<DD>$st_num[$sc] " . 
		  "<A HREF=$href>$_";

	      # Calculate the <H?> level to use in the HTML file
	      $hlevel = $st_sl[$sc] - $st_ol[$sc] + 2;
	      $shlevel = $st_sl[$sc] - $st_ol[$sc] + 3;

	      $i = $st_ol[$sc];

	      # Add the section header
	      $text[$i] .= "<H$hlevel><A NAME=\"$sc\"></A>$st_num[$sc] $_";
	      $i--;
	      
	      # And also to the parent 
	      if ($st_sl[$sc] == $st_ol[$sc] && $i >= 0) {
		  $text[$i] .= "<H$shlevel>$st_num[$sc] " . 
			  "<A HREF=$href>$_";
		  $i--;
	      }

	      # and to the grandparents
	      for (; $i >= $st_pl[$sc];  $i--) {
		  $text[$i] .= "<DD>$st_num[$sc] " .
		      "<A HREF=$href>$_";
	      }

	      last tagsw;
	  }

	  #
	  # HEADER end
	  #
	  if (s/^<\@\@endhead>//) {
	      if ($part == 1) {
		  $text[0] .= "</H1>\n";
		  $part = 0;
		  last tagsw;
	      }
	      print tocfile "</A>\n";

	      $i = $st_ol[$sc];

	      # Close the section header
	      $text[$i] .= "</H$hlevel>\n";
	      $i--;

	      # in the parent...
	      if ($st_sl[$sc] == $st_ol[$sc] && $i >= 0) {
		  $text[$i] .= "</A></H$shlevel>\n";
		  $i--;
	      }

	      # in the grandparent...
	      for (; $i >= $st_pl[$sc];  $i--) {
		  $text[$i] .= "</A></DD>\n";
	      }
	      last tagsw;
	  }
	  
	  # sectioning
	  if (s/^<\@\@part>//) {
	      $part = 1;
	      $partnum++;
	      last tagsw;
	  }

	  #
	  # BEGINNING of a section
	  #
	  if (s/^<\@\@sect>//) {
	      # Increment the section counter and save it on a stack
	      # for future reference.
	      $sc++;
	      push(@scs, $sc);

	      # Set up the navigation bar
	      if ($st_file[$sc] > $st_file[$sc - 1]) {
		  &navbar($st_file[$sc], $filecount, $sc);
	      }

	      # Prepare for menu entries in the table of contents and
	      # parent file(s).
	      if ($st_sl[$sc - 1] < $st_sl[$sc]) {
		  print tocfile "<DL>\n";
		  $i = $st_ol[$sc] - 1 - ($st_sl[$sc] == $st_ol[$sc]);
		  for (; $i >= $st_pl[$sc];  $i--) {
		      $text[$i] .= "<DL>\n";
		  }
	      }
	      last tagsw;
	  }

	  #
	  # END of a section
	  #
	  if (s/^<\@\@endsect>//) {
	      
	      # Remember the section number! Subsections may have
	      # altered the global $sc variable.
	      local ($lsc) = pop(@scs);

	      # Close off subsection menus we may have created in
	      # parent file(s).
	      if ($st_sl[$lsc] > $st_sl[$sc + 1]) {
		  print tocfile "</DL>\n";
		  if ($st_sl[$lsc] > 1) {
		       print tocfile "</DD>\n";
		  }
		  $i = $st_ol[$lsc] - 1 - ($st_sl[$lsc] == $st_ol[$lsc]);
		  for (; $i >= $st_pl[$lsc];  $i--) {
		      $text[$i] .= "</DL>\n";
		  }
	      }

	      # If this section is below $maxlevel, write it now.
	      if ($st_sl[$lsc] <= $maxlevel) {
		  open(SECOUT, ">${fileroot}$st_file[$lsc].html");
		  print SECOUT "$header[$st_ol[$lsc]]  $text[$st_ol[$lsc]] " . 
		      "$footer[$st_ol[$lsc]]";
		  $text[$st_ol[$lsc]] = "";
		  close(SECOUT);
	      }
	      last tagsw;
	  }		

	  # cross references
	  if (s/^<\@\@label>//) {
	      chop;
	      $text[$st_ol[$sc]] .= "<A NAME=\"$_\"></A>";
	      last tagsw;
	  }
	  if (s/^<\@\@ref>//) {
	      chop;
	      $refname = $_;
	      if ($references{$_} eq "") {
		  print "Warning: Reference to $_ has no defined target\n";
	      }
	      else {
		  $text[$st_ol[$sc]] .= 
		      "<A HREF=\"${fileroot}$references{$_}.html#$_\">";
	      }
	      last tagsw;
	  }
	  if (s/^<\@\@endref>//) {
#	      $text[$st_ol[$sc]] .= "</A>";
	      last tagsw;
	  }
	  if (s/^<\@\@refnam>//) {
	      $text[$st_ol[$sc]] .= "$refname</A>";
	      last tagsw;
	  }
	  # URLs
	  if (s/^<\@\@url>//) {
	      chop;
	      $urlname = $_;
	      $text[$st_ol[$sc]] .= "<A HREF=\"$urlname\">";
	      last tagsw;
	  }
	  if (s/^<\@\@urlnam>//) {
	      $text[$st_ol[$sc]] .= "$urlname</A>";
	      last tagsw;
	  }
	  if (s/^<\@\@endurl>//) {
#	      $text[$st_ol[$sc]] .= "</A>";
	      last tagsw;
	  }
	  

	  # If nothing else did anything with this line, just print it.
	  $text[$st_ol[$sc]] .= "$_";
      }
    }

    print tocfile "</HTML>\n";
    open(SECOUT, ">$fileroot.html");
    print SECOUT "$header[0] $text[0] $footer[0]";
    close(SECOUT);
    close tocfile;
}

# navbar
#
# Generate a navigation bar to go on the top and bottom of the page.

sub navbar {
    local ($fnum, $fmax, $sc) = @_;
    local ($i, $itext, $prv, $nxt, $colon);

    $colon = "<b>:</b>";

    # Generate the section hierarchy

    $navbar[$st_ol[$sc]] =
	"<A HREF=\"${fileroot}.html\"><EM>$st_header[0]</EM></A>\n";
    $i = $st_parent[$sc];
    while ($i > 0) {
	$itext = " $colon <A HREF=\"${fileroot}$st_file[$i].html\"><EM>$st_header[$i]</EM></A>\n$itext";
	$i = $st_parent[$i];
    }
    $navbar[$st_ol[$sc]] .= "$itext $colon <EM>$st_header[$sc]</EM><BR>\n";

    # Generate previous and next pointers

    # Previous pointer must be in a different file AND must be at the
    # same or higher section level.  If the current node is the
    # beginning of a chapter, then previous will go to the beginning
    # of the previous chapter, not the end of the previous chapter.

    $prv = $sc;
    while ($prv >= 0 && $st_file[$prv] >= $st_file[$sc] - 1) { 
	$prv--; 
    }
    $prv++;
    $navbar[$st_ol[$sc]] .=
	"<b>Previous:</b> <A HREF=\"${fileroot}$st_file[$prv].html\"><EM>$st_header[$prv]</EM></A><BR>\n";

    # Then next pointer must be in a higher numbered file OR the home
    # page of the document.

    $nxt = $sc;
    if ($st_file[$nxt] == $filecount) { 
	$nxt = 0; 
    }
    else {
	while ($st_file[$nxt] == $st_file[$sc]) {
	    $nxt++;
	}
    }

    $navbar[$st_ol[$sc]] .=
	"<b>Next:</b> <A HREF=\"${fileroot}$st_file[$nxt].html\"><EM>$st_header[$nxt]</EM></A>\n";

    $navbar[$st_ol[$sc]] .= "\n";

}


# extlink
#
# creates a symbolic link from the name in a reference to the numbered
# html file.  Since the file number that any given section has is 
# subject to change as the document goes through revisions, this allows
# for a fixed target that separate documents can hook into.
#
# Slashes (/) in the reference are converted to percents (%) while
# spaces ( ) are converted to underscores (_);

sub extlink {
    local ($ref, $fn) = @_;

    $ref =~ s/\//%/g;
    $ref =~ s/ /_/g;

    $file = "$ref.html";

    if (-e $file) {
	if (-l $file) {
	    unlink($file);
	    symlink($fn, $file);
	}
	else {
	    print "Warning: $file exists and is not a symbolic link\n";
	}
    }
    else {
	symlink($fn, $file);
    }
}

# Now, read the command line and take appropriate action

sub main {
    # Check arguments
    if (!&NGetOpt('f=s', 'links', 'ssi', 'i:s@')) {
	&usage;
	exit 1;
    }
    if (@ARGV == 0) {
	print "An input file must be specified.\n";
	&usage;
	exit 1;
    }
    if (&getfile($ARGV[0]) == 0) {
	print "Cannot locate specified file: $ARGV[0]\n";
	&usage;
	exit 1;
    }

    # Generate output
    if ($opt_f eq 'html') {
	print "generating $fileroot.html";
	if ($opt_links == 1) {
	    print " with external links";
	}
	print "...\n"; &gen_html(); 
    }
    elsif ($opt_f eq 'tex' || $opt_f eq 'latex') {
	print "generating $fileroot.tex...\n"; &gen_latex(); 
    }
    elsif ($opt_f eq 'nroff') { 
	print "generating $fileroot.nroff...\n"; &gen_nroff();
    }
    elsif ($opt_f eq 'ascii') {
	print "generating $fileroot.ascii...\n"; &gen_ascii(); 
    }
    elsif ($opt_f eq 'ps') { 
	print "generating $fileroot.ps...\n"; &gen_ps(); 
    }
    else {
	if ($opt_f eq "") {
	    print "An output format must be specified with the -f option.\n";
	}
	else {
	    print "\"$opt_f\" is an unknown output format.\n";
	}
	&usage;
	exit 1;
    }

}

&main;

exit 0;

