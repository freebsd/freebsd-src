package Pod::Html;

use Pod::Functions;
use Getopt::Long;	# package for handling command-line parameters
require Exporter;
use vars qw($VERSION);
$VERSION = 1.01;
@ISA = Exporter;
@EXPORT = qw(pod2html htmlify);
use Cwd;

use Carp;

use locale;	# make \w work right in non-ASCII lands

use strict;

use Config;

=head1 NAME

Pod::Html - module to convert pod files to HTML

=head1 SYNOPSIS

    use Pod::Html;
    pod2html([options]);

=head1 DESCRIPTION

Converts files from pod format (see L<perlpod>) to HTML format.  It
can automatically generate indexes and cross-references, and it keeps
a cache of things it knows how to cross-reference.

=head1 ARGUMENTS

Pod::Html takes the following arguments:

=over 4

=item help

    --help

Displays the usage message.

=item htmlroot

    --htmlroot=name

Sets the base URL for the HTML files.  When cross-references are made,
the HTML root is prepended to the URL.

=item infile

    --infile=name

Specify the pod file to convert.  Input is taken from STDIN if no
infile is specified.

=item outfile

    --outfile=name

Specify the HTML file to create.  Output goes to STDOUT if no outfile
is specified.

=item podroot

    --podroot=name

Specify the base directory for finding library pods.

=item podpath

    --podpath=name:...:name

Specify which subdirectories of the podroot contain pod files whose
HTML converted forms can be linked-to in cross-references.

=item libpods

    --libpods=name:...:name

List of page names (eg, "perlfunc") which contain linkable C<=item>s.

=item netscape

    --netscape

Use Netscape HTML directives when applicable.

=item nonetscape

    --nonetscape

Do not use Netscape HTML directives (default).

=item index

    --index

Generate an index at the top of the HTML file (default behaviour).

=item noindex

    --noindex

Do not generate an index at the top of the HTML file.


=item recurse

    --recurse

Recurse into subdirectories specified in podpath (default behaviour).

=item norecurse

    --norecurse

Do not recurse into subdirectories specified in podpath.

=item title

    --title=title

Specify the title of the resulting HTML file.

=item verbose

    --verbose

Display progress messages.

=back

=head1 EXAMPLE

    pod2html("pod2html",
	     "--podpath=lib:ext:pod:vms", 
	     "--podroot=/usr/src/perl",
	     "--htmlroot=/perl/nmanual",
	     "--libpods=perlfunc:perlguts:perlvar:perlrun:perlop",
	     "--recurse",
	     "--infile=foo.pod",
	     "--outfile=/perl/nmanual/foo.html");

=head1 AUTHOR

Tom Christiansen, E<lt>tchrist@perl.comE<gt>.

=head1 BUGS

Has trouble with C<> etc in = commands.

=head1 SEE ALSO

L<perlpod>

=head1 COPYRIGHT

This program is distributed under the Artistic License.

=cut

my $dircache = "pod2html-dircache";
my $itemcache = "pod2html-itemcache";

my @begin_stack = ();		# begin/end stack

my @libpods = ();	    	# files to search for links from C<> directives
my $htmlroot = "/";	    	# http-server base directory from which all
				#   relative paths in $podpath stem.
my $htmlfile = "";		# write to stdout by default
my $podfile = "";		# read from stdin by default
my @podpath = ();		# list of directories containing library pods.
my $podroot = ".";		# filesystem base directory from which all
				#   relative paths in $podpath stem.
my $recurse = 1;		# recurse on subdirectories in $podpath.
my $verbose = 0;		# not verbose by default
my $doindex = 1;   	    	# non-zero if we should generate an index
my $listlevel = 0;		# current list depth
my @listitem = ();		# stack of HTML commands to use when a =item is
				#   encountered.  the top of the stack is the
				#   current list.
my @listdata = ();		# similar to @listitem, but for the text after
				#   an =item
my @listend = ();		# similar to @listitem, but the text to use to
				#   end the list.
my $ignore = 1;			# whether or not to format text.  we don't
				#   format text until we hit our first pod
				#   directive.

my %items_named = ();		# for the multiples of the same item in perlfunc
my @items_seen = ();
my $netscape = 0;		# whether or not to use netscape directives.
my $title;			# title to give the pod(s)
my $top = 1;			# true if we are at the top of the doc.  used
				#   to prevent the first <HR> directive.
my $paragraph;			# which paragraph we're processing (used
				#   for error messages)
my %pages = ();			# associative array used to find the location
				#   of pages referenced by L<> links.
my %sections = ();		# sections within this page
my %items = ();			# associative array used to find the location
				#   of =item directives referenced by C<> links
my $Is83;                       # is dos with short filenames (8.3)

sub init_globals {
$dircache = "pod2html-dircache";
$itemcache = "pod2html-itemcache";

@begin_stack = ();		# begin/end stack

@libpods = ();	    	# files to search for links from C<> directives
$htmlroot = "/";	    	# http-server base directory from which all
				#   relative paths in $podpath stem.
$htmlfile = "";		# write to stdout by default
$podfile = "";		# read from stdin by default
@podpath = ();		# list of directories containing library pods.
$podroot = ".";		# filesystem base directory from which all
				#   relative paths in $podpath stem.
$recurse = 1;		# recurse on subdirectories in $podpath.
$verbose = 0;		# not verbose by default
$doindex = 1;   	    	# non-zero if we should generate an index
$listlevel = 0;		# current list depth
@listitem = ();		# stack of HTML commands to use when a =item is
				#   encountered.  the top of the stack is the
				#   current list.
@listdata = ();		# similar to @listitem, but for the text after
				#   an =item
@listend = ();		# similar to @listitem, but the text to use to
				#   end the list.
$ignore = 1;			# whether or not to format text.  we don't
				#   format text until we hit our first pod
				#   directive.

@items_seen = ();
%items_named = ();
$netscape = 0;		# whether or not to use netscape directives.
$title = '';			# title to give the pod(s)
$top = 1;			# true if we are at the top of the doc.  used
				#   to prevent the first <HR> directive.
$paragraph = '';			# which paragraph we're processing (used
				#   for error messages)
%sections = ();		# sections within this page

# These are not reinitialised here but are kept as a cache.
# See get_cache and related cache management code.
#%pages = ();			# associative array used to find the location
				#   of pages referenced by L<> links.
#%items = ();			# associative array used to find the location
				#   of =item directives referenced by C<> links
$Is83=$^O eq 'dos';
}

sub pod2html {
    local(@ARGV) = @_;
    local($/);
    local $_;

    init_globals();

    $Is83 = 0 if (defined (&Dos::UseLFN) && Dos::UseLFN());

    # cache of %pages and %items from last time we ran pod2html

    #undef $opt_help if defined $opt_help;

    # parse the command-line parameters
    parse_command_line();

    # set some variables to their default values if necessary
    local *POD;
    unless (@ARGV && $ARGV[0]) { 
	$podfile  = "-" unless $podfile;	# stdin
	open(POD, "<$podfile")
		|| die "$0: cannot open $podfile file for input: $!\n";
    } else {
	$podfile = $ARGV[0];  # XXX: might be more filenames
	*POD = *ARGV;
    } 
    $htmlfile = "-" unless $htmlfile;	# stdout
    $htmlroot = "" if $htmlroot eq "/";	# so we don't get a //

    # read the pod a paragraph at a time
    warn "Scanning for sections in input file(s)\n" if $verbose;
    $/ = "";
    my @poddata  = <POD>;
    close(POD);

    # scan the pod for =head[1-6] directives and build an index
    my $index = scan_headings(\%sections, @poddata);

    unless($index) {
	warn "No pod in $podfile\n" if $verbose;
	return;
    }

    # open the output file
    open(HTML, ">$htmlfile")
	    || die "$0: cannot open $htmlfile file for output: $!\n";

    # put a title in the HTML file if one wasn't specified
    if ($title eq '') {
	TITLE_SEARCH: {
	    for (my $i = 0; $i < @poddata; $i++) { 
		if ($poddata[$i] =~ /^=head1\s*NAME\b/m) {
		    for my $para ( @poddata[$i, $i+1] ) { 
			last TITLE_SEARCH
			    if ($title) = $para =~ /(\S+\s+-+.*\S)/s;
		    }
		} 

	    } 
	}
    }
    if (!$title and $podfile =~ /\.pod$/) {
	# probably a split pod so take first =head[12] as title
	for (my $i = 0; $i < @poddata; $i++) { 
	    last if ($title) = $poddata[$i] =~ /^=head[12]\s*(.*)/;
	} 
	warn "adopted '$title' as title for $podfile\n"
	    if $verbose and $title;
    } 
    if ($title) {
	$title =~ s/\s*\(.*\)//;
    } else {
	warn "$0: no title for $podfile";
	$podfile =~ /^(.*)(\.[^.\/]+)?$/;
	$title = ($podfile eq "-" ? 'No Title' : $1);
	warn "using $title" if $verbose;
    }
    print HTML <<END_OF_HEAD;
<HTML>
<HEAD>
<TITLE>$title</TITLE>
<LINK REV="made" HREF="mailto:$Config{perladmin}">
</HEAD>

<BODY>

END_OF_HEAD

    # load/reload/validate/cache %pages and %items
    get_cache($dircache, $itemcache, \@podpath, $podroot, $recurse);

    # scan the pod for =item directives
    scan_items("", \%items, @poddata);

    # put an index at the top of the file.  note, if $doindex is 0 we
    # still generate an index, but surround it with an html comment.
    # that way some other program can extract it if desired.
    $index =~ s/--+/-/g;
    print HTML "<!-- INDEX BEGIN -->\n";
    print HTML "<!--\n" unless $doindex;
    print HTML $index;
    print HTML "-->\n" unless $doindex;
    print HTML "<!-- INDEX END -->\n\n";
    print HTML "<HR>\n" if $doindex;

    # now convert this file
    warn "Converting input file\n" if $verbose;
    foreach my $i (0..$#poddata) {
	$_ = $poddata[$i];
	$paragraph = $i+1;
	if (/^(=.*)/s) {	# is it a pod directive?
	    $ignore = 0;
	    $_ = $1;
	    if (/^=begin\s+(\S+)\s*(.*)/si) {# =begin
		process_begin($1, $2);
	    } elsif (/^=end\s+(\S+)\s*(.*)/si) {# =end
		process_end($1, $2);
	    } elsif (/^=cut/) {			# =cut
		process_cut();
	    } elsif (/^=pod/) {			# =pod
		process_pod();
	    } else {
		next if @begin_stack && $begin_stack[-1] ne 'html';

		if (/^=(head[1-6])\s+(.*\S)/s) {	# =head[1-6] heading
		    process_head($1, $2);
		} elsif (/^=item\s*(.*\S)/sm) {	# =item text
		    process_item($1);
		} elsif (/^=over\s*(.*)/) {		# =over N
		    process_over();
		} elsif (/^=back/) {		# =back
		    process_back();
		} elsif (/^=for\s+(\S+)\s+(.*)/si) {# =for
		    process_for($1,$2);
		} else {
		    /^=(\S*)\s*/;
		    warn "$0: $podfile: unknown pod directive '$1' in "
		       . "paragraph $paragraph.  ignoring.\n";
		}
	    }
	    $top = 0;
	}
	else {
	    next if $ignore;
	    next if @begin_stack && $begin_stack[-1] ne 'html';
	    my $text = $_;
	    process_text(\$text, 1);
	    print HTML "<P>\n$text";
	}
    }

    # finish off any pending directives
    finish_list();
    print HTML <<END_OF_TAIL;
</BODY>

</HTML>
END_OF_TAIL

    # close the html file
    close(HTML);

    warn "Finished\n" if $verbose;
}

##############################################################################

my $usage;			# see below
sub usage {
    my $podfile = shift;
    warn "$0: $podfile: @_\n" if @_;
    die $usage;
}

$usage =<<END_OF_USAGE;
Usage:  $0 --help --htmlroot=<name> --infile=<name> --outfile=<name>
           --podpath=<name>:...:<name> --podroot=<name>
           --libpods=<name>:...:<name> --recurse --verbose --index
           --netscape --norecurse --noindex

  --flush      - flushes the item and directory caches.
  --help       - prints this message.
  --htmlroot   - http-server base directory from which all relative paths
                 in podpath stem (default is /).
  --index      - generate an index at the top of the resulting html
                 (default).
  --infile     - filename for the pod to convert (input taken from stdin
                 by default).
  --libpods    - colon-separated list of pages to search for =item pod
                 directives in as targets of C<> and implicit links (empty
                 by default).  note, these are not filenames, but rather
                 page names like those that appear in L<> links.
  --netscape   - will use netscape html directives when applicable.
  --nonetscape - will not use netscape directives (default).
  --outfile    - filename for the resulting html file (output sent to
                 stdout by default).
  --podpath    - colon-separated list of directories containing library
                 pods.  empty by default.
  --podroot    - filesystem base directory from which all relative paths
                 in podpath stem (default is .).
  --noindex    - don't generate an index at the top of the resulting html.
  --norecurse  - don't recurse on those subdirectories listed in podpath.
  --recurse    - recurse on those subdirectories listed in podpath
                 (default behavior).
  --title      - title that will appear in resulting html file.
  --verbose    - self-explanatory

END_OF_USAGE

sub parse_command_line {
    my ($opt_flush,$opt_help,$opt_htmlroot,$opt_index,$opt_infile,$opt_libpods,$opt_netscape,$opt_outfile,$opt_podpath,$opt_podroot,$opt_norecurse,$opt_recurse,$opt_title,$opt_verbose);
    my $result = GetOptions(
			    'flush'      => \$opt_flush,
			    'help'       => \$opt_help,
			    'htmlroot=s' => \$opt_htmlroot,
			    'index!'     => \$opt_index,
			    'infile=s'   => \$opt_infile,
			    'libpods=s'  => \$opt_libpods,
			    'netscape!'  => \$opt_netscape,
			    'outfile=s'  => \$opt_outfile,
			    'podpath=s'  => \$opt_podpath,
			    'podroot=s'  => \$opt_podroot,
			    'norecurse'  => \$opt_norecurse,
			    'recurse!'   => \$opt_recurse,
			    'title=s'    => \$opt_title,
			    'verbose'    => \$opt_verbose,
			   );
    usage("-", "invalid parameters") if not $result;

    usage("-") if defined $opt_help;	# see if the user asked for help
    $opt_help = "";			# just to make -w shut-up.

    $podfile  = $opt_infile if defined $opt_infile;
    $htmlfile = $opt_outfile if defined $opt_outfile;

    @podpath  = split(":", $opt_podpath) if defined $opt_podpath;
    @libpods  = split(":", $opt_libpods) if defined $opt_libpods;

    warn "Flushing item and directory caches\n"
	if $opt_verbose && defined $opt_flush;
    unlink($dircache, $itemcache) if defined $opt_flush;

    $htmlroot = $opt_htmlroot if defined $opt_htmlroot;
    $podroot  = $opt_podroot if defined $opt_podroot;

    $doindex  = $opt_index if defined $opt_index;
    $recurse  = $opt_recurse if defined $opt_recurse;
    $title    = $opt_title if defined $opt_title;
    $verbose  = defined $opt_verbose ? 1 : 0;
    $netscape = $opt_netscape if defined $opt_netscape;
}


my $saved_cache_key;

sub get_cache {
    my($dircache, $itemcache, $podpath, $podroot, $recurse) = @_;
    my @cache_key_args = @_;

    # A first-level cache:
    # Don't bother reading the cache files if they still apply
    # and haven't changed since we last read them.

    my $this_cache_key = cache_key(@cache_key_args);

    return if $saved_cache_key and $this_cache_key eq $saved_cache_key;

    # load the cache of %pages and %items if possible.  $tests will be
    # non-zero if successful.
    my $tests = 0;
    if (-f $dircache && -f $itemcache) {
	warn "scanning for item cache\n" if $verbose;
	$tests = load_cache($dircache, $itemcache, $podpath, $podroot);
    }

    # if we didn't succeed in loading the cache then we must (re)build
    #  %pages and %items.
    if (!$tests) {
	warn "scanning directories in pod-path\n" if $verbose;
	scan_podpath($podroot, $recurse, 0);
    }
    $saved_cache_key = cache_key(@cache_key_args);
}

sub cache_key {
    my($dircache, $itemcache, $podpath, $podroot, $recurse) = @_;
    return join('!', $dircache, $itemcache, $recurse,
		@$podpath, $podroot, stat($dircache), stat($itemcache));
}

#
# load_cache - tries to find if the caches stored in $dircache and $itemcache
#  are valid caches of %pages and %items.  if they are valid then it loads
#  them and returns a non-zero value.
#

sub load_cache {
    my($dircache, $itemcache, $podpath, $podroot) = @_;
    my($tests);
    local $_;

    $tests = 0;

    open(CACHE, "<$itemcache") ||
	die "$0: error opening $itemcache for reading: $!\n";
    $/ = "\n";

    # is it the same podpath?
    $_ = <CACHE>;
    chomp($_);
    $tests++ if (join(":", @$podpath) eq $_);

    # is it the same podroot?
    $_ = <CACHE>;
    chomp($_);
    $tests++ if ($podroot eq $_);

    # load the cache if its good
    if ($tests != 2) {
	close(CACHE);
	return 0;
    }

    warn "loading item cache\n" if $verbose;
    while (<CACHE>) {
	/(.*?) (.*)$/;
	$items{$1} = $2;
    }
    close(CACHE);

    warn "scanning for directory cache\n" if $verbose;
    open(CACHE, "<$dircache") ||
	die "$0: error opening $dircache for reading: $!\n";
    $/ = "\n";
    $tests = 0;

    # is it the same podpath?
    $_ = <CACHE>;
    chomp($_);
    $tests++ if (join(":", @$podpath) eq $_);

    # is it the same podroot?
    $_ = <CACHE>;
    chomp($_);
    $tests++ if ($podroot eq $_);

    # load the cache if its good
    if ($tests != 2) {
	close(CACHE);
	return 0;
    }

    warn "loading directory cache\n" if $verbose;
    while (<CACHE>) {
	/(.*?) (.*)$/;
	$pages{$1} = $2;
    }

    close(CACHE);

    return 1;
}

#
# scan_podpath - scans the directories specified in @podpath for directories,
#  .pod files, and .pm files.  it also scans the pod files specified in
#  @libpods for =item directives.
#
sub scan_podpath {
    my($podroot, $recurse, $append) = @_;
    my($pwd, $dir);
    my($libpod, $dirname, $pod, @files, @poddata);

    unless($append) {
	%items = ();
	%pages = ();
    }

    # scan each directory listed in @podpath
    $pwd = getcwd();
    chdir($podroot)
	|| die "$0: error changing to directory $podroot: $!\n";
    foreach $dir (@podpath) {
	scan_dir($dir, $recurse);
    }

    # scan the pods listed in @libpods for =item directives
    foreach $libpod (@libpods) {
	# if the page isn't defined then we won't know where to find it
	# on the system.
	next unless defined $pages{$libpod} && $pages{$libpod};

	# if there is a directory then use the .pod and .pm files within it.
	if ($pages{$libpod} =~ /([^:]*[^(\.pod|\.pm)]):/) {
	    #  find all the .pod and .pm files within the directory
	    $dirname = $1;
	    opendir(DIR, $dirname) ||
		die "$0: error opening directory $dirname: $!\n";
	    @files = grep(/(\.pod|\.pm)$/ && ! -d $_, readdir(DIR));
	    closedir(DIR);

	    # scan each .pod and .pm file for =item directives
	    foreach $pod (@files) {
		open(POD, "<$dirname/$pod") ||
		    die "$0: error opening $dirname/$pod for input: $!\n";
		@poddata = <POD>;
		close(POD);

		scan_items("$dirname/$pod", @poddata);
	    }

	    # use the names of files as =item directives too.
	    foreach $pod (@files) {
		$pod =~ /^(.*)(\.pod|\.pm)$/;
		$items{$1} = "$dirname/$1.html" if $1;
	    }
	} elsif ($pages{$libpod} =~ /([^:]*\.pod):/ ||
		 $pages{$libpod} =~ /([^:]*\.pm):/) {
	    # scan the .pod or .pm file for =item directives
	    $pod = $1;
	    open(POD, "<$pod") ||
		die "$0: error opening $pod for input: $!\n";
	    @poddata = <POD>;
	    close(POD);

	    scan_items("$pod", @poddata);
	} else {
	    warn "$0: shouldn't be here (line ".__LINE__."\n";
	}
    }
    @poddata = ();	# clean-up a bit

    chdir($pwd)
	|| die "$0: error changing to directory $pwd: $!\n";

    # cache the item list for later use
    warn "caching items for later use\n" if $verbose;
    open(CACHE, ">$itemcache") ||
	die "$0: error open $itemcache for writing: $!\n";

    print CACHE join(":", @podpath) . "\n$podroot\n";
    foreach my $key (keys %items) {
	print CACHE "$key $items{$key}\n";
    }

    close(CACHE);

    # cache the directory list for later use
    warn "caching directories for later use\n" if $verbose;
    open(CACHE, ">$dircache") ||
	die "$0: error open $dircache for writing: $!\n";

    print CACHE join(":", @podpath) . "\n$podroot\n";
    foreach my $key (keys %pages) {
	print CACHE "$key $pages{$key}\n";
    }

    close(CACHE);
}

#
# scan_dir - scans the directory specified in $dir for subdirectories, .pod
#  files, and .pm files.  notes those that it finds.  this information will
#  be used later in order to figure out where the pages specified in L<>
#  links are on the filesystem.
#
sub scan_dir {
    my($dir, $recurse) = @_;
    my($t, @subdirs, @pods, $pod, $dirname, @dirs);
    local $_;

    @subdirs = ();
    @pods = ();

    opendir(DIR, $dir) ||
	die "$0: error opening directory $dir: $!\n";
    while (defined($_ = readdir(DIR))) {
	if (-d "$dir/$_" && $_ ne "." && $_ ne "..") {	    # directory
	    $pages{$_}  = "" unless defined $pages{$_};
	    $pages{$_} .= "$dir/$_:";
	    push(@subdirs, $_);
	} elsif (/\.pod$/) {	    	    	    	    # .pod
	    s/\.pod$//;
	    $pages{$_}  = "" unless defined $pages{$_};
	    $pages{$_} .= "$dir/$_.pod:";
	    push(@pods, "$dir/$_.pod");
	} elsif (/\.pm$/) { 	    	    	    	    # .pm
	    s/\.pm$//;
	    $pages{$_}  = "" unless defined $pages{$_};
	    $pages{$_} .= "$dir/$_.pm:";
	    push(@pods, "$dir/$_.pm");
	}
    }
    closedir(DIR);

    # recurse on the subdirectories if necessary
    if ($recurse) {
	foreach my $subdir (@subdirs) {
	    scan_dir("$dir/$subdir", $recurse);
	}
    }
}

#
# scan_headings - scan a pod file for head[1-6] tags, note the tags, and
#  build an index.
#
sub scan_headings {
    my($sections, @data) = @_;
    my($tag, $which_head, $title, $listdepth, $index);

    # here we need	local $ignore = 0;
    #  unfortunately, we can't have it, because $ignore is lexical
    $ignore = 0;

    $listdepth = 0;
    $index = "";

    # scan for =head directives, note their name, and build an index
    #  pointing to each of them.
    foreach my $line (@data) {
	if ($line =~ /^=(head)([1-6])\s+(.*)/) {
	    ($tag,$which_head, $title) = ($1,$2,$3);
	    chomp($title);
	    $$sections{htmlify(0,$title)} = 1;

	    while ($which_head != $listdepth) {
		if ($which_head > $listdepth) {
		    $index .= "\n" . ("\t" x $listdepth) . "<UL>\n";
		    $listdepth++;
		} elsif ($which_head < $listdepth) {
		    $listdepth--;
		    $index .= "\n" . ("\t" x $listdepth) . "</UL>\n";
		}
	    }

	    $index .= "\n" . ("\t" x $listdepth) . "<LI>" .
	              "<A HREF=\"#" . htmlify(0,$title) . "\">" .
		      html_escape(process_text(\$title, 0)) . "</A>";
	}
    }

    # finish off the lists
    while ($listdepth--) {
	$index .= "\n" . ("\t" x $listdepth) . "</UL>\n";
    }

    # get rid of bogus lists
    $index =~ s,\t*<UL>\s*</UL>\n,,g;

    $ignore = 1;	# restore old value;

    return $index;
}

#
# scan_items - scans the pod specified by $pod for =item directives.  we
#  will use this information later on in resolving C<> links.
#
sub scan_items {
    my($pod, @poddata) = @_;
    my($i, $item);
    local $_;

    $pod =~ s/\.pod$//;
    $pod .= ".html" if $pod;

    foreach $i (0..$#poddata) {
	$_ = $poddata[$i];

	# remove any formatting instructions
	s,[A-Z]<([^<>]*)>,$1,g;

	# figure out what kind of item it is and get the first word of
	#  it's name.
	if (/^=item\s+(\w*)\s*.*$/s) {
	    if ($1 eq "*") {		# bullet list
		/\A=item\s+\*\s*(.*?)\s*\Z/s;
		$item = $1;
	    } elsif ($1 =~ /^\d+/) {	# numbered list
		/\A=item\s+\d+\.?(.*?)\s*\Z/s;
		$item = $1;
	    } else {
#		/\A=item\s+(.*?)\s*\Z/s;
		/\A=item\s+(\w*)/s;
		$item = $1;
	    }

	    $items{$item} = "$pod" if $item;
	}
    }
}

#
# process_head - convert a pod head[1-6] tag and convert it to HTML format.
#
sub process_head {
    my($tag, $heading) = @_;
    my $firstword;

    # figure out the level of the =head
    $tag =~ /head([1-6])/;
    my $level = $1;

    # can't have a heading full of spaces and speechmarks and so on
    $firstword = $heading; $firstword =~ s/\s*(\w+)\s.*/$1/;

    print HTML "<P>\n" unless $listlevel;
    print HTML "<HR>\n" unless $listlevel || $top;
    print HTML "<H$level>"; # unless $listlevel;
    #print HTML "<H$level>" unless $listlevel;
    my $convert = $heading; process_text(\$convert, 0);
    $convert = html_escape($convert);
    print HTML '<A NAME="' . htmlify(0,$heading) . "\">$convert</A>";
    print HTML "</H$level>"; # unless $listlevel;
    print HTML "\n";
}

#
# process_item - convert a pod item tag and convert it to HTML format.
#
sub process_item {
    my $text = $_[0];
    my($i, $quote, $name);

    my $need_preamble = 0;
    my $this_entry;


    # lots of documents start a list without doing an =over.  this is
    # bad!  but, the proper thing to do seems to be to just assume
    # they did do an =over.  so warn them once and then continue.
    warn "$0: $podfile: unexpected =item directive in paragraph $paragraph.  ignoring.\n"
	unless $listlevel;
    process_over() unless $listlevel;

    return unless $listlevel;

    # remove formatting instructions from the text
    1 while $text =~ s/[A-Z]<([^<>]*)>/$1/g;
    pre_escape(\$text);

    $need_preamble = $items_seen[$listlevel]++ == 0;

    # check if this is the first =item after an =over
    $i = $listlevel - 1;
    my $need_new = $listlevel >= @listitem;

    if ($text =~ /\A\*/) {		# bullet

	if ($need_preamble) {
	    push(@listend,  "</UL>");
	    print HTML "<UL>\n";
	}

	print HTML '<LI>';
	if ($text =~ /\A\*\s*(.+)\Z/s) {
	    print HTML '<STRONG>';
	    if ($items_named{$1}++) {
		print HTML html_escape($1);
	    } else {
		my $name = 'item_' . htmlify(1,$1);
		print HTML qq(<A NAME="$name">), html_escape($1), '</A>';
	    }
	    print HTML '</STRONG>';
	}

    } elsif ($text =~ /\A[\d#]+/) {	# numbered list

	if ($need_preamble) {
	    push(@listend,  "</OL>");
	    print HTML "<OL>\n";
	}

	print HTML '<LI>';
	if ($text =~ /\A\d+\.?\s*(.+)\Z/s) {
	    print HTML '<STRONG>';
	    if ($items_named{$1}++) {
		print HTML html_escape($1);
	    } else {
		my $name = 'item_' . htmlify(0,$1);
		print HTML qq(<A NAME="$name">), html_escape($1), '</A>';
	    }
	    print HTML '</STRONG>';
	}

    } else {			# all others

	if ($need_preamble) {
	    push(@listend,  '</DL>');
	    print HTML "<DL>\n";
	}

	print HTML '<DT>';
	if ($text =~ /(\S+)/) {
	    print HTML '<STRONG>';
	    if ($items_named{$1}++) {
		print HTML html_escape($text);
	    } else {
		my $name = 'item_' . htmlify(1,$text);
		print HTML qq(<A NAME="$name">), html_escape($text), '</A>';
	    }
	    print HTML '</STRONG>';
	}
       print HTML '<DD>';
    }

    print HTML "\n";
}

#
# process_over - process a pod over tag and start a corresponding HTML
# list.
#
sub process_over {
    # start a new list
    $listlevel++;
}

#
# process_back - process a pod back tag and convert it to HTML format.
#
sub process_back {
    warn "$0: $podfile: unexpected =back directive in paragraph $paragraph.  ignoring.\n"
	unless $listlevel;
    return unless $listlevel;

    # close off the list.  note, I check to see if $listend[$listlevel] is
    # defined because an =item directive may have never appeared and thus
    # $listend[$listlevel] may have never been initialized.
    $listlevel--;
    print HTML $listend[$listlevel] if defined $listend[$listlevel];
    print HTML "\n";

    # don't need the corresponding perl code anymore
    pop(@listitem);
    pop(@listdata);
    pop(@listend);

    pop(@items_seen);
}

#
# process_cut - process a pod cut tag, thus stop ignoring pod directives.
#
sub process_cut {
    $ignore = 1;
}

#
# process_pod - process a pod pod tag, thus ignore pod directives until we see a
# corresponding cut.
#
sub process_pod {
    # no need to set $ignore to 0 cause the main loop did it
}

#
# process_for - process a =for pod tag.  if it's for html, split
# it out verbatim, if illustration, center it, otherwise ignore it.
#
sub process_for {
    my($whom, $text) = @_;
    if ( $whom =~ /^(pod2)?html$/i) {
	print HTML $text;
    } elsif ($whom =~ /^illustration$/i) {
        1 while chomp $text;
	for my $ext (qw[.png .gif .jpeg .jpg .tga .pcl .bmp]) {
	  $text .= $ext, last if -r "$text$ext";
	}
        print HTML qq{<p align = "center"><img src = "$text" alt = "$text illustration"></p>};
    }
}

#
# process_begin - process a =begin pod tag.  this pushes
# whom we're beginning on the begin stack.  if there's a
# begin stack, we only print if it us.
#
sub process_begin {
    my($whom, $text) = @_;
    $whom = lc($whom);
    push (@begin_stack, $whom);
    if ( $whom =~ /^(pod2)?html$/) {
	print HTML $text if $text;
    }
}

#
# process_end - process a =end pod tag.  pop the
# begin stack.  die if we're mismatched.
#
sub process_end {
    my($whom, $text) = @_;
    $whom = lc($whom);
    if ($begin_stack[-1] ne $whom ) {
	die "Unmatched begin/end at chunk $paragraph\n"
    } 
    pop @begin_stack;
}

#
# process_text - handles plaintext that appears in the input pod file.
# there may be pod commands embedded within the text so those must be
# converted to html commands.
#
sub process_text {
    my($text, $escapeQuotes) = @_;
    my($result, $rest, $s1, $s2, $s3, $s4, $match, $bf);
    my($podcommand, $params, $tag, $quote);

    return if $ignore;

    $quote  = 0;    	    	# status of double-quote conversion
    $result = "";
    $rest = $$text;

    if ($rest =~ /^\s+/) {	# preformatted text, no pod directives
	$rest =~ s/\n+\Z//;
	$rest =~ s#.*#
	    my $line = $&;
	    1 while $line =~ s/\t+/' ' x (length($&) * 8 - length($`) % 8)/e;
	    $line;
	#eg;

	$rest   =~ s/&/&amp;/g;
	$rest   =~ s/</&lt;/g;
	$rest   =~ s/>/&gt;/g;
	$rest   =~ s/"/&quot;/g;

	# try and create links for all occurrences of perl.* within
	# the preformatted text.
	$rest =~ s{
		    (\s*)(perl\w+)
		  }{
		    if (defined $pages{$2}) {	# is a link
			qq($1<A HREF="$htmlroot/$pages{$2}">$2</A>);
		    } elsif (defined $pages{dosify($2)}) {	# is a link
			qq($1<A HREF="$htmlroot/$pages{dosify($2)}">$2</A>);
		    } else {
			"$1$2";
		    }
		  }xeg;
	$rest =~ s/(<A HREF=)([^>:]*:)?([^>:]*)\.pod:([^>:]*:)?/$1$3.html/g;

  my $urls = '(' . join ('|', qw{
                http
                telnet
		mailto
		news
                gopher
                file
                wais
                ftp
            } ) 
        . ')';
  
  my $ltrs = '\w';
  my $gunk = '/#~:.?+=&%@!\-';
  my $punc = '.:?\-';
  my $any  = "${ltrs}${gunk}${punc}";

  $rest =~ s{
        \b                          # start at word boundary
        (                           # begin $1  {
          $urls     :               # need resource and a colon
          [$any] +?                 # followed by on or more
                                    #  of any valid character, but
                                    #  be conservative and take only
                                    #  what you need to....
        )                           # end   $1  }
        (?=                         # look-ahead non-consumptive assertion
                [$punc]*            # either 0 or more puntuation
                [^$any]             #   followed by a non-url char
            |                       # or else
                $                   #   then end of the string
        )
      }{<A HREF="$1">$1</A>}igox;

	$result =   "<PRE>"	# text should be as it is (verbatim)
		  . "$rest\n"
		  . "</PRE>\n";
    } else {			# formatted text
	# parse through the string, stopping each time we find a
	# pod-escape.  once the string has been throughly processed
	# we can output it.
	while (length $rest) {
	    # check to see if there are any possible pod directives in
	    # the remaining part of the text.
	    if ($rest =~ m/[BCEIFLSZ]</) {
		warn "\$rest\t= $rest\n" unless
		    $rest =~ /\A
			   ([^<]*?)
			   ([BCEIFLSZ]?)
			   <
			   (.*)\Z/xs;

		$s1 = $1;	# pure text
		$s2 = $2;	# the type of pod-escape that follows
		$s3 = '<';	# '<'
		$s4 = $3;	# the rest of the string
	    } else {
		$s1 = $rest;
		$s2 = "";
		$s3 = "";
		$s4 = "";
	    }

	    if ($s3 eq '<' && $s2) {	# a pod-escape
		$result    .= ($escapeQuotes ? process_puretext($s1, \$quote) : $s1);
		$podcommand = "$s2<";
		$rest       = $s4;

		# find the matching '>'
		$match = 1;
		$bf = 0;
		while ($match && !$bf) {
		    $bf = 1;
		    if ($rest =~ /\A([^<>]*[BCEIFLSZ]<)(.*)\Z/s) {
			$bf = 0;
			$match++;
			$podcommand .= $1;
			$rest        = $2;
		    } elsif ($rest =~ /\A([^>]*>)(.*)\Z/s) {
			$bf = 0;
			$match--;
			$podcommand .= $1;
			$rest        = $2;
		    }
		}

		if ($match != 0) {
		    warn <<WARN;
$0: $podfile: cannot find matching > for $s2 in paragraph $paragraph.
WARN
		    $result .= substr $podcommand, 0, 2;
		    $rest = substr($podcommand, 2) . $rest;
		    next;
		}

		# pull out the parameters to the pod-escape
		$podcommand =~ /^([BCFEILSZ]?)<(.*)>$/s;
		$tag    = $1;
		$params = $2;

		# process the text within the pod-escape so that any escapes
		# which must occur do.
		process_text(\$params, 0) unless $tag eq 'L';

		$s1 = $params;
		if (!$tag || $tag eq " ") {	#  <> : no tag
		    $s1 = "&lt;$params&gt;";
		} elsif ($tag eq "L") {		# L<> : link 
		    $s1 = process_L($params);
		} elsif ($tag eq "I" ||		# I<> : italicize text
			 $tag eq "B" ||		# B<> : bold text
			 $tag eq "F") {		# F<> : file specification
		    $s1 = process_BFI($tag, $params);
		} elsif ($tag eq "C") {		# C<> : literal code
		    $s1 = process_C($params, 1);
		} elsif ($tag eq "E") {		# E<> : escape
		    $s1 = process_E($params);
		} elsif ($tag eq "Z") {		# Z<> : zero-width character
		    $s1 = process_Z($params);
		} elsif ($tag eq "S") {		# S<> : non-breaking space
		    $s1 = process_S($params);
		} elsif ($tag eq "X") {		# S<> : non-breaking space
		    $s1 = process_X($params);
		} else {
		    warn "$0: $podfile: unhandled tag '$tag' in paragraph $paragraph\n";
		}

		$result .= "$s1";
	    } else {
		# for pure text we must deal with implicit links and
		# double-quotes among other things.
		$result .= ($escapeQuotes ? process_puretext("$s1$s2$s3", \$quote) : "$s1$s2$s3");
		$rest    = $s4;
	    }
	}
    }
    $$text = $result;
}

sub html_escape {
    my $rest = $_[0];
    $rest   =~ s/&/&amp;/g;
    $rest   =~ s/</&lt;/g;
    $rest   =~ s/>/&gt;/g;
    $rest   =~ s/"/&quot;/g;
    return $rest;
} 

#
# process_puretext - process pure text (without pod-escapes) converting
#  double-quotes and handling implicit C<> links.
#
sub process_puretext {
    my($text, $quote) = @_;
    my(@words, $result, $rest, $lead, $trail);

    # convert double-quotes to single-quotes
    $text =~ s/\A([^"]*)"/$1''/s if $$quote;
    while ($text =~ s/\A([^"]*)["]([^"]*)["]/$1``$2''/sg) {}

    $$quote = ($text =~ m/"/ ? 1 : 0);
    $text =~ s/\A([^"]*)"/$1``/s if $$quote;

    # keep track of leading and trailing white-space
    $lead  = ($text =~ /\A(\s*)/s ? $1 : "");
    $trail = ($text =~ /(\s*)\Z/s ? $1 : "");

    # collapse all white space into a single space
    $text =~ s/\s+/ /g;
    @words = split(" ", $text);

    # process each word individually
    foreach my $word (@words) {
	# see if we can infer a link
	if ($word =~ /^\w+\(/) {
	    # has parenthesis so should have been a C<> ref
	    $word = process_C($word);
#	    $word =~ /^[^()]*]\(/;
#	    if (defined $items{$1} && $items{$1}) {
#		$word =   "\n<CODE><A HREF=\"$htmlroot/$items{$1}#item_"
#			. htmlify(0,$word)
#			. "\">$word</A></CODE>";
#	    } elsif (defined $items{$word} && $items{$word}) {
#		$word =   "\n<CODE><A HREF=\"$htmlroot/$items{$word}#item_"
#			. htmlify(0,$word)
#			. "\">$word</A></CODE>";
#	    } else {
#		$word =   "\n<CODE><A HREF=\"#item_"
#			. htmlify(0,$word)
#			. "\">$word</A></CODE>";
#	    }
	} elsif ($word =~ /^[\$\@%&*]+\w+$/) {
	    # perl variables, should be a C<> ref
	    $word = process_C($word, 1);
	} elsif ($word =~ m,^\w+://\w,) {
	    # looks like a URL
	    $word = qq(<A HREF="$word">$word</A>);
	} elsif ($word =~ /[\w.-]+\@[\w-]+\.\w/) {
	    # looks like an e-mail address
	    my ($w1, $w2, $w3) = ("", $word, "");
	    ($w1, $w2, $w3) = ("(", $1, ")$2") if $word =~ /^\((.*?)\)(,?)/;
	    ($w1, $w2, $w3) = ("&lt;", $1, "&gt;$2") if $word =~ /^<(.*?)>(,?)/;
	    $word = qq($w1<A HREF="mailto:$w2">$w2</A>$w3);
	} elsif ($word !~ /[a-z]/ && $word =~ /[A-Z]/) {  # all uppercase?
	    $word = html_escape($word) if $word =~ /["&<>]/;
	    $word = "\n<FONT SIZE=-1>$word</FONT>" if $netscape;
	} else { 
	    $word = html_escape($word) if $word =~ /["&<>]/;
	}
    }

    # build a new string based upon our conversion
    $result = "";
    $rest   = join(" ", @words);
    while (length($rest) > 75) {
	if ( $rest =~ m/^(.{0,75})\s(.*?)$/o ||
	     $rest =~ m/^(\S*)\s(.*?)$/o) {

	    $result .= "$1\n";
	    $rest    = $2;
	} else {
	    $result .= "$rest\n";
	    $rest    = "";
	}
    }
    $result .= $rest if $rest;

    # restore the leading and trailing white-space
    $result = "$lead$result$trail";

    return $result;
}

#
# pre_escape - convert & in text to $amp;
#
sub pre_escape {
    my($str) = @_;

    $$str =~ s,&,&amp;,g;
}

#
# dosify - convert filenames to 8.3
#
sub dosify {
    my($str) = @_;
    if ($Is83) {
        $str = lc $str;
        $str =~ s/(\.\w+)/substr ($1,0,4)/ge;
        $str =~ s/(\w+)/substr ($1,0,8)/ge;
    }
    return $str;
}

#
# process_L - convert a pod L<> directive to a corresponding HTML link.
#  most of the links made are inferred rather than known about directly
#  (i.e it's not known whether the =head\d section exists in the target file,
#   or whether a .pod file exists in the case of split files).  however, the
#  guessing usually works.
#
# Unlike the other directives, this should be called with an unprocessed
# string, else tags in the link won't be matched.
#
sub process_L {
    my($str) = @_;
    my($s1, $s2, $linktext, $page, $page83, $section, $link);	# work strings

    $str =~ s/\n/ /g;			# undo word-wrapped tags
    $s1 = $str;
    for ($s1) {
	# LREF: a la HREF L<show this text|man/section>
	$linktext = $1 if s:^([^|]+)\|::;

	# make sure sections start with a /
	s,^",/",g;
	s,^,/,g if (!m,/, && / /);

	# check if there's a section specified
	if (m,^(.*?)/"?(.*?)"?$,) {	# yes
	    ($page, $section) = ($1, $2);
	} else {			# no
	    ($page, $section) = ($str, "");
	}

	# check if we know that this is a section in this page
	if (!defined $pages{$page} && defined $sections{$page}) {
	    $section = $page;
	    $page = "";
	}
    }

    $page83=dosify($page);
    $page=$page83 if (defined $pages{$page83});
    if ($page eq "") {
	$link = "#" . htmlify(0,$section);
	$linktext = $section unless defined($linktext);
    } elsif ( $page =~ /::/ ) {
	$linktext  = ($section ? "$section" : "$page");
	$page =~ s,::,/,g;
	$link = "$htmlroot/$page.html";
	$link .= "#" . htmlify(0,$section) if ($section);
    } elsif (!defined $pages{$page}) {
	warn "$0: $podfile: cannot resolve L<$str> in paragraph $paragraph: no such page '$page'\n";
	$link = "";
	$linktext = $page unless defined($linktext);
    } else {
	$linktext  = ($section ? "$section" : "the $page manpage") unless defined($linktext);
	$section = htmlify(0,$section) if $section ne "";

	# if there is a directory by the name of the page, then assume that an
	# appropriate section will exist in the subdirectory
	if ($section ne "" && $pages{$page} =~ /([^:]*[^(\.pod|\.pm)]):/) {
	    $link = "$htmlroot/$1/$section.html";

	# since there is no directory by the name of the page, the section will
	# have to exist within a .html of the same name.  thus, make sure there
	# is a .pod or .pm that might become that .html
	} else {
	    $section = "#$section";
	    # check if there is a .pod with the page name
	    if ($pages{$page} =~ /([^:]*)\.pod:/) {
		$link = "$htmlroot/$1.html$section";
	    } elsif ($pages{$page} =~ /([^:]*)\.pm:/) {
		$link = "$htmlroot/$1.html$section";
	    } else {
		warn "$0: $podfile: cannot resolve L$str in paragraph $paragraph: ".
			     "no .pod or .pm found\n";
		$link = "";
		$linktext = $section unless defined($linktext);
	    }
	}
    }

    process_text(\$linktext, 0);
    if ($link) {
	$s1 = "<A HREF=\"$link\">$linktext</A>";
    } else {
	$s1 = "<EM>$linktext</EM>";
    }
    return $s1;
}

#
# process_BFI - process any of the B<>, F<>, or I<> pod-escapes and
# convert them to corresponding HTML directives.
#
sub process_BFI {
    my($tag, $str) = @_;
    my($s1);			# work string
    my(%repltext) = (	'B' => 'STRONG',
			'F' => 'EM',
			'I' => 'EM');

    # extract the modified text and convert to HTML
    $s1 = "<$repltext{$tag}>$str</$repltext{$tag}>";
    return $s1;
}

#
# process_C - process the C<> pod-escape.
#
sub process_C {
    my($str, $doref) = @_;
    my($s1, $s2);

    $s1 = $str;
    $s1 =~ s/\([^()]*\)//g;	# delete parentheses
    $s2 = $s1;
    $s1 =~ s/\W//g;		# delete bogus characters
    $str = html_escape($str);

    # if there was a pod file that we found earlier with an appropriate
    # =item directive, then create a link to that page.
    if ($doref && defined $items{$s1}) {
	$s1 = ($items{$s1} ?
	       "<A HREF=\"$htmlroot/$items{$s1}#item_" . htmlify(0,$s2) .  "\">$str</A>" :
	       "<A HREF=\"#item_" . htmlify(0,$s2) .  "\">$str</A>");
	$s1 =~ s,(perl\w+/(\S+)\.html)#item_\2\b,$1,; 
	confess "s1 has space: $s1" if $s1 =~ /HREF="[^"]*\s[^"]*"/;
    } else {
	$s1 = "<CODE>$str</CODE>";
	# warn "$0: $podfile: cannot resolve C<$str> in paragraph $paragraph\n" if $verbose
    }


    return $s1;
}

#
# process_E - process the E<> pod directive which seems to escape a character.
#
sub process_E {
    my($str) = @_;

    for ($str) {
	s,([^/].*),\&$1\;,g;
    }

    return $str;
}

#
# process_Z - process the Z<> pod directive which really just amounts to
# ignoring it.  this allows someone to start a paragraph with an =
#
sub process_Z {
    my($str) = @_;

    # there is no equivalent in HTML for this so just ignore it.
    $str = "";
    return $str;
}

#
# process_S - process the S<> pod directive which means to convert all
# spaces in the string to non-breaking spaces (in HTML-eze).
#
sub process_S {
    my($str) = @_;

    # convert all spaces in the text to non-breaking spaces in HTML.
    $str =~ s/ /&nbsp;/g;
    return $str;
}

#
# process_X - this is supposed to make an index entry.  we'll just 
# ignore it.
#
sub process_X {
    return '';
}


#
# finish_list - finish off any pending HTML lists.  this should be called
# after the entire pod file has been read and converted.
#
sub finish_list {
    while ($listlevel > 0) {
	print HTML "</DL>\n";
	$listlevel--;
    }
}

#
# htmlify - converts a pod section specification to a suitable section
# specification for HTML.  if first arg is 1, only takes 1st word.
#
sub htmlify {
    my($compact, $heading) = @_;

    if ($compact) {
      $heading =~ /^(\w+)/;
      $heading = $1;
    } 

  # $heading = lc($heading);
  $heading =~ s/[^\w\s]/_/g;
  $heading =~ s/(\s+)/ /g;
  $heading =~ s/^\s*(.*?)\s*$/$1/s;
  $heading =~ s/ /_/g;
  $heading =~ s/\A(.{32}).*\Z/$1/s;
  $heading =~ s/\s+\Z//;
  $heading =~ s/_{2,}/_/g;

  return $heading;
}

BEGIN {
}

1;
