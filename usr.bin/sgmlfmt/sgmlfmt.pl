#!/usr/bin/perl
# $Id:$

# Format an sgml document tagged according to the linuxdoc DTD.
# by John Fieber <jfieber@freebsd.org> for the FreeBSD documentation
# project.  
#
# Usage: sgmlformat -format [-format ...] inputfile [inputfile ...]
#
#  -format              outputfile         format
#  -------------------------------------------------------------
#   -html               inputfile.html     HTML
#   -txt | -ascii       inputfile.txt      ascii text
#   -tex | -latex       inputfile.tex      LaTeX
#   -nroff              inputfile.nroff    groff for ms macros
#   -ps                 inputfile.txt      postscript
#
# Bugs:
#
# Text lines that start with a period (.) confuse the conversions that
# use groff.  The workaround is to make sure the SGML source doesn't
# have any periods at the beginning of a line.

#######################################################################

# Look in a couple places for the SGML DTD and replacement files
#

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
    $ENV{'SGML_PATH'} = "$replbase/$replacement.%N:$ENV{'SGML_PATH'}";
    open($fhandle, "sgmls $decl $file | sgmlsasp $replbase/$replacement.mapping |");
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
    open(outfile, "| groff -T ascii -t -ms | col -b > $fileroot.txt");
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


$sc = 0;			# number of sections
$filecount = 0;			# number of files

sub gen_html {
    local($i, $sl);
    $tmpfile = "/tmp/sgmlf.$$";

    open(bar, ">$tmpfile");
#    print STDERR "(Pass 1...";
    &sgmlparse(foo, "html");
    while (<foo>) {
	print bar;
	# count up the number of files to be generated
	if (/^<@@sect>/) {
	    $sl++;
	    $sc++;
	    $st_sl[$sc] = $sl;

	    # Per level counters
	    $counter[$sl]++;
	    $counter[$sl + 1] = 0;

	    # calculate the section number in the form x.y.z.
	    $st_num[$sc] = "";
	    if ($sl <= $num_depth) {
		for ($i = 1; $i <= $sl; $i++) {
		    $st_num[$sc] .= "$counter[$i].";
		}
		$st_num[$sc] .= " ";
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
	if (/^<@@endsect>/) {
	    $sl--;
	}

	# record the section number that a label occurs in
	if (/^<@@label>/) {
	    chop;
	    s/^<@@label>//;
	    if ($references{$_} eq "") {
		$references{$_} = "$filecount";
	    }
	    else {
		print STDERR "Warning: the label `$_' is multiply-defined.\n";
	    }
	}
    }
    close(bar);

#    print STDERR " Pass 2...";
    open(foofile, $tmpfile);
    &html2html(foofile, "boo");
#    print STDERR ")\n";

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
    print tocfile "<HTML>\n";

    while (<$infile>) {
	# change `<' and `>' to `&lt;' and `&gt;' in <pre></pre>
	if (/<pre>/.../<\/pre>/) {
	    s/</\&lt;/g;
	    s/\&lt;([\/]*)pre>/<\1pre>/g;
	    s/>/\&gt;/g;
	    s/<([\/]*)pre\&gt;/<\1pre>/g;
	}

      tagsw: {
	  # titles and headings
	  if (s/^<@@title>//) {
	      chop;
	      print tocfile "<HEAD>\n<TITLE>$_</TITLE>\n</HEAD>\n";
	      print tocfile "<H1>$_</H1>\n";
	      $header[$st_ol[$sc]] = 
		  "<HTML>\n<HEAD>\n<TITLE>$_</TITLE>\n" . 
		      "</HEAD>\n<BODY>\n<H1>$_</H1>\n"; 
	      $footer[$st_ol[$sc]] = "</BODY>\n</HTML>\n";
	      last tagsw;
	  }

	  #
	  # HEADER begin
	  #
	  if (s/^<@@head>//) {
	      chop;

	      if ($part == 1) {
		  $text[0] .= "<H1>Part $partnum:<BR>$_";
		  last tagsw;
	      }

	      $href = "\"$fileroot-$st_file[$sc].html#$sc\"";

	      # set up headers and footers
	      if ($st_sl[$sc] > 0 && $st_sl[$sc] <= $maxlevel) {
		  $header[$st_ol[$sc]] = 
		      "<HTML>\n<HEAD>\n<TITLE>$_</TITLE>\n</HEAD>\n" .
			  "<BODY>\n$navbar[$st_ol[$sc]]\n<HR>\n";
		  $footer[$st_ol[$sc]] =
		      "<HR>\n$navbar[$st_ol[$sc]]\n</BODY>\n</HTML>";
	      }

	      # Add this to the master table of contents
	      print tocfile "<DD>$st_num[$sc]" . 
		  "<A HREF=$href>$_";

	      # Calculate the <H?> level to use in the HTML file
	      $hlevel = $st_sl[$sc] - $st_ol[$sc] + 2;
	      $shlevel = $st_sl[$sc] - $st_ol[$sc] + 3;

	      $i = $st_ol[$sc];

	      # Add the section header
	      $text[$i] .= "<H$hlevel><A NAME=\"$sc\"></A>$st_num[$sc]$_";
	      $i--;
	      
	      # And also to the parent 
	      if ($st_sl[$sc] == $st_ol[$sc] && $i >= 0) {
		  $text[$i] .= "<H$shlevel>$st_num[$sc]" . 
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
	  if (s/^<@@endhead>//) {
	      if ($part == 1) {
		  $text[0] .= "</H1>\n";
		  $part = 0;
		  last tagsw;
	      }
	      print tocfile "</A></DD>\n";

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
	  if (s/^<@@part>//) {
	      $part = 1;
	      $partnum++;
	      # not yet implemented in the DTD
	      last tagsw;
	  }

	  #
	  # BEGINNING of a section
	  #
	  if (s/^<@@sect>//) {
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
	  if (s/^<@@endsect>//) {
	      
	      # Remember the section number! Subsections may have
	      # altered the global $sc variable.
	      local ($lsc) = pop(@scs);

	      # Close off subsection menus we may have created in
	      # parent file(s).
	      if ($st_sl[$lsc] > $st_sl[$sc + 1]) {
		  print tocfile "</DL>\n";
		  $i = $st_ol[$lsc] - 1 - ($st_sl[$lsc] == $st_ol[$lsc]);
		  for (; $i >= $st_pl[$lsc];  $i--) {
		      $text[$i] .= "</DL>\n";
		  }
	      }

	      # If this section is below $maxlevel, write it now.
	      if ($st_sl[$lsc] <= $maxlevel) {
		  open(SECOUT, ">${fileroot}-$st_file[$lsc].html");
		  print SECOUT "$header[$st_ol[$lsc]]  $text[$st_ol[$lsc]] " . 
		      "$footer[$st_ol[$lsc]]";
		  $text[$st_ol[$lsc]] = "";
		  close(SECOUT);
	      }
	      last tagsw;
	  }		

	  # cross references
	  if (s/^<@@label>//) {
	      chop;
	      $text[$st_ol[$sc]] .= "<A NAME=\"$_\"></A>";
	      last tagsw;
	  }
	  if (s/^<@@ref>//) {
	      chop;
	      $refname = $_;
	      $text[$st_ol[$sc]] .= 
		  "<A HREF=\"${fileroot}-$references{$_}.html#$refname\">";
	      last tagsw;
	  }
	  if (s/^<@@endref>//) {
#	      $text[$st_ol[$sc]] .= "</A>";
	      last tagsw;
	  }
	  if (s/^<@@refnam>//) {
	      $text[$st_ol[$sc]] .= "$refname</A>";
	      last tagsw;
	  }
	  # URLs
	  if (s/^<@@url>//) {
	      chop;
	      $urlname = $_;
	      $text[$st_ol[$sc]] .= "<A HREF=\"$urlname\">";
	      last tagsw;
	  }
	  if (s/^<@@urlnam>//) {
	      $text[$st_ol[$sc]] .= "$urlname</A>";
	      last tagsw;
	  }
	  if (s/^<@@endurl>//) {
#	      $text[$st_ol[$sc]] .= "</A>";
	      last tagsw;
	  }
	  

	  # If nothing else did anything with this line, just print it.
	  $text[$st_ol[$sc]] .= "$_";
      }
    }

    print tocfile "</HTML>";
    open(SECOUT, ">$fileroot.html");
    print SECOUT "$header[0] $text[0] $footer[0]";
    close(SECOUT);
    close tocfile;
}

sub navbar {
    local ($fnum, $fmax, $sc) = @_;

    $prevf = $fnum - 1;
    $nextf = $fnum + 1;

    $navbar[$st_ol[$sc]] = "<B>\n";
    $navbar[$st_ol[$sc]] .=
	"<A HREF=\"${fileroot}.html\">Table of Contents</A>\n";
    if ($prevf <= 0) {
	$navbar[$st_ol[$sc]] .=
	    "| <A HREF=\"${fileroot}.html\">Previous</A>\n";
    }
    else {
	$navbar[$st_ol[$sc]] .=
	    "| <A HREF=\"${fileroot}-${prevf}.html\">Previous</A>\n";
    }
    if ($nextf <= $fmax) {
	$navbar[$st_ol[$sc]] .=
	    "| <A HREF=\"${fileroot}-${nextf}.html\">Next</A>\n";
    }
    else {
	$navbar[$st_ol[$sc]] .=
	    "| <A HREF=\"${fileroot}.html\">Next</A>\n";
    }
    $navbar[$st_ol[$sc]] .= "</B>\n";
}





# Now, read the command line and take appropriate action

$fcount = 0;
for (@ARGV) {
    if (/^-.*/) {
	s/^-//;
	$gen{$_} = 1;
    }
    else {
	@infiles[$fcount] = $_;
	$fcount++;
    }
}

for ($i = 0; $i < $fcount; $i++) {
    if (&getfile($infiles[$i])) {
	if ($gen{'html'}) { 
	    print "generating $fileroot.html...\n"; &gen_html(); }
	if ($gen{'tex'} || $gen{'latex'}) { 
	    print "generating $fileroot.tex...\n"; &gen_latex(); }
	if ($gen{'nroff'}) { 
	    print "generating $fileroot.nroff...\n"; &gen_nroff(); }
	if ($gen{'txt'} || $gen{'ascii'}) { 
	    print "generating $fileroot.txt...\n"; &gen_ascii(); }
	if ($gen{'ps'}) { 
	    print "generating $fileroot.ps...\n"; &gen_ps(); }
    }
    else {
	print "Input file $infiles[$i] not found\n";
    }
}

exit 0;

