package Pod::Html;
use strict;
require Exporter;

use vars qw($VERSION @ISA @EXPORT);
$VERSION = 1.03;
@ISA = qw(Exporter);
@EXPORT = qw(pod2html htmlify);

use Carp;
use Config;
use Cwd;
use File::Spec::Unix;
use Getopt::Long;
use Pod::Functions;

use locale;	# make \w work right in non-ASCII lands

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

=item backlink

    --backlink="Back to Top"

Adds "Back to Top" links in front of every HEAD1 heading (except for
the first).  By default, no backlink are being generated.

=item css

    --css=stylesheet

Specify the URL of a cascading style sheet.

=item flush

    --flush

Flushes the item and directory caches.

=item header

    --header
    --noheader

Creates header and footer blocks containing the text of the NAME
section.  By default, no headers are being generated.

=item help

    --help

Displays the usage message.

=item htmldir

    --htmldir=name

Sets the directory in which the resulting HTML file is placed.  This
is used to generate relative links to other files. Not passing this
causes all links to be absolute, since this is the value that tells
Pod::Html the root of the documentation tree.

=item htmlroot

    --htmlroot=name

Sets the base URL for the HTML files.  When cross-references are made,
the HTML root is prepended to the URL.

=item index

    --index
    --noindex

Generate an index at the top of the HTML file.  This is the default
behaviour.

=item infile

    --infile=name

Specify the pod file to convert.  Input is taken from STDIN if no
infile is specified.

=item libpods

    --libpods=name:...:name

List of page names (eg, "perlfunc") which contain linkable C<=item>s.

=item netscape

    --netscape
    --nonetscape

Use Netscape HTML directives when applicable.  By default, they will
B<not> be used.

=item outfile

    --outfile=name

Specify the HTML file to create.  Output goes to STDOUT if no outfile
is specified.

=item podpath

    --podpath=name:...:name

Specify which subdirectories of the podroot contain pod files whose
HTML converted forms can be linked-to in cross-references.

=item podroot

    --podroot=name

Specify the base directory for finding library pods.

=item quiet

    --quiet
    --noquiet

Don't display I<mostly harmless> warning messages.  These messages
will be displayed by default.  But this is not the same as C<verbose>
mode.

=item recurse

    --recurse
    --norecurse

Recurse into subdirectories specified in podpath (default behaviour).

=item title

    --title=title

Specify the title of the resulting HTML file.

=item verbose

    --verbose
    --noverbose

Display progress messages.  By default, they won't be displayed.

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

=head1 ENVIRONMENT

Uses $Config{pod2html} to setup default options.

=head1 AUTHOR

Tom Christiansen, E<lt>tchrist@perl.comE<gt>.

=head1 SEE ALSO

L<perlpod>

=head1 COPYRIGHT

This program is distributed under the Artistic License.

=cut

my $cache_ext = $^O eq 'VMS' ? ".tmp" : ".x~~";
my $dircache = "pod2htmd$cache_ext";
my $itemcache = "pod2htmi$cache_ext";

my @begin_stack = ();		# begin/end stack

my @libpods = ();	    	# files to search for links from C<> directives
my $htmlroot = "/";	    	# http-server base directory from which all
				#   relative paths in $podpath stem.
my $htmldir = "";		# The directory to which the html pages
				# will (eventually) be written.
my $htmlfile = "";		# write to stdout by default
my $htmlfileurl = "" ;		# The url that other files would use to
				# refer to this file.  This is only used
				# to make relative urls that point to
				# other files.
my $podfile = "";		# read from stdin by default
my @podpath = ();		# list of directories containing library pods.
my $podroot = ".";		# filesystem base directory from which all
				#   relative paths in $podpath stem.
my $css = '';                   # Cascading style sheet
my $recurse = 1;		# recurse on subdirectories in $podpath.
my $quiet = 0;			# not quiet by default
my $verbose = 0;		# not verbose by default
my $doindex = 1;   	    	# non-zero if we should generate an index
my $backlink = '';              # text for "back to top" links
my $listlevel = 0;		# current list depth
my @listend = ();		# the text to use to end the list.
my $after_lpar = 0;             # set to true after a par in an =item
my $ignore = 1;			# whether or not to format text.  we don't
				#   format text until we hit our first pod
				#   directive.

my %items_named = ();		# for the multiples of the same item in perlfunc
my @items_seen = ();
my $netscape = 0;		# whether or not to use netscape directives.
my $title;			# title to give the pod(s)
my $header = 0;			# produce block header/footer
my $top = 1;			# true if we are at the top of the doc.  used
				#   to prevent the first <HR> directive.
my $paragraph;			# which paragraph we're processing (used
				#   for error messages)
my $ptQuote = 0;                # status of double-quote conversion
my %pages = ();			# associative array used to find the location
				#   of pages referenced by L<> links.
my %sections = ();		# sections within this page
my %items = ();			# associative array used to find the location
				#   of =item directives referenced by C<> links
my %local_items = ();           # local items - avoid destruction of %items
my $Is83;                       # is dos with short filenames (8.3)

sub init_globals {
$dircache = "pod2htmd$cache_ext";
$itemcache = "pod2htmi$cache_ext";

@begin_stack = ();		# begin/end stack

@libpods = ();	    	# files to search for links from C<> directives
$htmlroot = "/";	    	# http-server base directory from which all
				#   relative paths in $podpath stem.
$htmldir = "";	    	# The directory to which the html pages
				# will (eventually) be written.
$htmlfile = "";		# write to stdout by default
$podfile = "";		# read from stdin by default
@podpath = ();		# list of directories containing library pods.
$podroot = ".";		# filesystem base directory from which all
				#   relative paths in $podpath stem.
$css = '';                   # Cascading style sheet
$recurse = 1;		# recurse on subdirectories in $podpath.
$quiet = 0;		# not quiet by default
$verbose = 0;		# not verbose by default
$doindex = 1;   	    	# non-zero if we should generate an index
$backlink = '';		# text for "back to top" links
$listlevel = 0;		# current list depth
@listend = ();		# the text to use to end the list.
$after_lpar = 0;        # set to true after a par in an =item
$ignore = 1;			# whether or not to format text.  we don't
				#   format text until we hit our first pod
				#   directive.

@items_seen = ();
%items_named = ();
$netscape = 0;		# whether or not to use netscape directives.
$header = 0;			# produce block header/footer
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
%local_items = ();
$Is83=$^O eq 'dos';
}

#
# clean_data: global clean-up of pod data
#
sub clean_data($){
    my( $dataref ) = @_;
    my $i;
    for( $i = 0; $i <= $#$dataref; $i++ ){
	${$dataref}[$i] =~ s/\s+\Z//;

        # have a look for all-space lines
	if( ${$dataref}[$i] =~ /^\s+$/m ){
	    my @chunks = split( /^\s+$/m, ${$dataref}[$i] );
	    splice( @$dataref, $i, 1, @chunks );
	}
    }
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
    $htmldir =~ s#/\z## ;               # so we don't get a //
    if (  $htmlroot eq ''
       && defined( $htmldir ) 
       && $htmldir ne ''
       && substr( $htmlfile, 0, length( $htmldir ) ) eq $htmldir 
       ) 
    {
	# Set the 'base' url for this file, so that we can use it
	# as the location from which to calculate relative links 
	# to other files. If this is '', then absolute links will
	# be used throughout.
        $htmlfileurl= "$htmldir/" . substr( $htmlfile, length( $htmldir ) + 1);
    }

    # read the pod a paragraph at a time
    warn "Scanning for sections in input file(s)\n" if $verbose;
    $/ = "";
    my @poddata  = <POD>;
    close(POD);
    clean_data( \@poddata );

    # scan the pod for =head[1-6] directives and build an index
    my $index = scan_headings(\%sections, @poddata);

    unless($index) {
	warn "No headings in $podfile\n" if $verbose;
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
    if (!$title and $podfile =~ /\.pod\z/) {
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
	warn "$0: no title for $podfile" unless $quiet;
	$podfile =~ /^(.*)(\.[^.\/]+)?\z/s;
	$title = ($podfile eq "-" ? 'No Title' : $1);
	warn "using $title" if $verbose;
    }
    my $csslink = $css ? qq(\n<LINK REL="stylesheet" HREF="$css" TYPE="text/css">) : '';
    $csslink =~ s,\\,/,g;
    $csslink =~ s,(/.):,$1|,;

    my $block = $header ? <<END_OF_BLOCK : '';
<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 WIDTH=100%>
<TR><TD CLASS=block VALIGN=MIDDLE WIDTH=100% BGCOLOR="#cccccc">
<FONT SIZE=+1><STRONG><P CLASS=block>&nbsp;$title</P></STRONG></FONT>
</TD></TR>
</TABLE>
END_OF_BLOCK

    print HTML <<END_OF_HEAD;
<HTML>
<HEAD>
<TITLE>$title</TITLE>$csslink
<LINK REV="made" HREF="mailto:$Config{perladmin}">
</HEAD>

<BODY>
$block
END_OF_HEAD

    # load/reload/validate/cache %pages and %items
    get_cache($dircache, $itemcache, \@podpath, $podroot, $recurse);

    # scan the pod for =item directives
    scan_items( \%local_items, "", @poddata);

    # put an index at the top of the file.  note, if $doindex is 0 we
    # still generate an index, but surround it with an html comment.
    # that way some other program can extract it if desired.
    $index =~ s/--+/-/g;
    print HTML "<A NAME=\"__index__\"></A>\n";
    print HTML "<!-- INDEX BEGIN -->\n";
    print HTML "<!--\n" unless $doindex;
    print HTML $index;
    print HTML "-->\n" unless $doindex;
    print HTML "<!-- INDEX END -->\n\n";
    print HTML "<HR>\n" if $doindex and $index;

    # now convert this file
    my $after_item;             # set to true after an =item
    warn "Converting input file $podfile\n" if $verbose;
    foreach my $i (0..$#poddata){
        $ptQuote = 0; # status of quote conversion

	$_ = $poddata[$i];
	$paragraph = $i+1;
	if (/^(=.*)/s) {	# is it a pod directive?
	    $ignore = 0;
	    $after_item = 0;
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
		    process_head( $1, $2, $doindex && $index );
		} elsif (/^=item\s*(.*\S)?/sm) {	# =item text
		    warn "$0: $podfile: =item without bullet, number or text"
		       . " in paragraph $paragraph.\n" if !defined($1) or $1 eq '';
		    process_item( $1 );
		    $after_item = 1;
		} elsif (/^=over\s*(.*)/) {		# =over N
		    process_over();
		} elsif (/^=back/) {		# =back
		    process_back();
		} elsif (/^=for\s+(\S+)\s*(.*)/si) {# =for
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
	    if( $text =~ /\A\s+/ ){
		process_pre( \$text );
	        print HTML "<PRE>\n$text</PRE>\n";

	    } else {
		process_text( \$text );

		# experimental: check for a paragraph where all lines
		# have some ...\t...\t...\n pattern
		if( $text =~ /\t/ ){
		    my @lines = split( "\n", $text );
		    if( @lines > 1 ){
			my $all = 2;
			foreach my $line ( @lines ){
			    if( $line =~ /\S/ && $line !~ /\t/ ){
				$all--;
				last if $all == 0;
			    }
			}
			if( $all > 0 ){
			    $text =~ s/\t+/<TD>/g;
			    $text =~ s/^/<TR><TD>/gm;
			    $text = '<TABLE CELLSPACING=0 CELLPADDING=0>' .
                                    $text . '</TABLE>';
			}
		    }
		}
		## end of experimental

		if( $after_item ){
		    print HTML "$text\n";
		    $after_lpar = 1;
		} else {
		    print HTML "<P>$text</P>\n";
		}
	    }
	    $after_item = 0;
	}
    }

    # finish off any pending directives
    finish_list();

    # link to page index
    print HTML "<P><A HREF=\"#__index__\"><SMALL>$backlink</SMALL></A></P>\n"
	if $doindex and $index and $backlink;

    print HTML <<END_OF_TAIL;
$block
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

  --backlink     - set text for "back to top" links (default: none).
  --css          - stylesheet URL
  --flush        - flushes the item and directory caches.
  --[no]header   - produce block header/footer (default is no headers).
  --help         - prints this message.
  --htmldir      - directory for resulting HTML files.
  --htmlroot     - http-server base directory from which all relative paths
                   in podpath stem (default is /).
  --[no]index    - generate an index at the top of the resulting html
                   (default behaviour).
  --infile       - filename for the pod to convert (input taken from stdin
                   by default).
  --libpods      - colon-separated list of pages to search for =item pod
                   directives in as targets of C<> and implicit links (empty
                   by default).  note, these are not filenames, but rather
                   page names like those that appear in L<> links.
  --[no]netscape - will use netscape html directives when applicable.
                   (default is not to use them).
  --outfile      - filename for the resulting html file (output sent to
                   stdout by default).
  --podpath      - colon-separated list of directories containing library
                   pods (empty by default).
  --podroot      - filesystem base directory from which all relative paths
                   in podpath stem (default is .).
  --[no]quiet    - supress some benign warning messages (default is off).
  --[no]recurse  - recurse on those subdirectories listed in podpath
                   (default behaviour).
  --title        - title that will appear in resulting html file.
  --[no]verbose  - self-explanatory (off by default).

END_OF_USAGE

sub parse_command_line {
    my ($opt_backlink,$opt_css,$opt_flush,$opt_header,$opt_help,$opt_htmldir,
	$opt_htmlroot,$opt_index,$opt_infile,$opt_libpods,$opt_netscape,
	$opt_outfile,$opt_podpath,$opt_podroot,$opt_quiet,$opt_recurse,
	$opt_title,$opt_verbose);

    unshift @ARGV, split ' ', $Config{pod2html} if $Config{pod2html};
    my $result = GetOptions(
			    'backlink=s' => \$opt_backlink,
			    'css=s'      => \$opt_css,
			    'flush'      => \$opt_flush,
			    'header!'    => \$opt_header,
			    'help'       => \$opt_help,
			    'htmldir=s'  => \$opt_htmldir,
			    'htmlroot=s' => \$opt_htmlroot,
			    'index!'     => \$opt_index,
			    'infile=s'   => \$opt_infile,
			    'libpods=s'  => \$opt_libpods,
			    'netscape!'  => \$opt_netscape,
			    'outfile=s'  => \$opt_outfile,
			    'podpath=s'  => \$opt_podpath,
			    'podroot=s'  => \$opt_podroot,
			    'quiet!'     => \$opt_quiet,
			    'recurse!'   => \$opt_recurse,
			    'title=s'    => \$opt_title,
			    'verbose!'   => \$opt_verbose,
			   );
    usage("-", "invalid parameters") if not $result;

    usage("-") if defined $opt_help;	# see if the user asked for help
    $opt_help = "";			# just to make -w shut-up.

    @podpath  = split(":", $opt_podpath) if defined $opt_podpath;
    @libpods  = split(":", $opt_libpods) if defined $opt_libpods;

    $backlink = $opt_backlink if defined $opt_backlink;
    $css      = $opt_css      if defined $opt_css;
    $header   = $opt_header   if defined $opt_header;
    $htmldir  = $opt_htmldir  if defined $opt_htmldir;
    $htmlroot = $opt_htmlroot if defined $opt_htmlroot;
    $doindex  = $opt_index    if defined $opt_index;
    $podfile  = $opt_infile   if defined $opt_infile;
    $netscape = $opt_netscape if defined $opt_netscape;
    $htmlfile = $opt_outfile  if defined $opt_outfile;
    $podroot  = $opt_podroot  if defined $opt_podroot;
    $quiet    = $opt_quiet    if defined $opt_quiet;
    $recurse  = $opt_recurse  if defined $opt_recurse;
    $title    = $opt_title    if defined $opt_title;
    $verbose  = $opt_verbose  if defined $opt_verbose;

    warn "Flushing item and directory caches\n"
	if $opt_verbose && defined $opt_flush;
    unlink($dircache, $itemcache) if defined $opt_flush;
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
	# NOTE: Only finds the first so-named directory in the tree.
#	if ($pages{$libpod} =~ /([^:]*[^(\.pod|\.pm)]):/) {
	if ($pages{$libpod} =~ /([^:]*(?<!\.pod)(?<!\.pm)):/) {
	    #  find all the .pod and .pm files within the directory
	    $dirname = $1;
	    opendir(DIR, $dirname) ||
		die "$0: error opening directory $dirname: $!\n";
	    @files = grep(/(\.pod|\.pm)\z/ && ! -d $_, readdir(DIR));
	    closedir(DIR);

	    # scan each .pod and .pm file for =item directives
	    foreach $pod (@files) {
		open(POD, "<$dirname/$pod") ||
		    die "$0: error opening $dirname/$pod for input: $!\n";
		@poddata = <POD>;
		close(POD);
		clean_data( \@poddata );

		scan_items( \%items, "$dirname/$pod", @poddata);
	    }

	    # use the names of files as =item directives too.
### Don't think this should be done this way - confuses issues.(WL)
###	    foreach $pod (@files) {
###		$pod =~ /^(.*)(\.pod|\.pm)$/;
###		$items{$1} = "$dirname/$1.html" if $1;
###	    }
	} elsif ($pages{$libpod} =~ /([^:]*\.pod):/ ||
		 $pages{$libpod} =~ /([^:]*\.pm):/) {
	    # scan the .pod or .pm file for =item directives
	    $pod = $1;
	    open(POD, "<$pod") ||
		die "$0: error opening $pod for input: $!\n";
	    @poddata = <POD>;
	    close(POD);
	    clean_data( \@poddata );

	    scan_items( \%items, "$pod", @poddata);
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
	} elsif (/\.pod\z/) {	    	    	    	    # .pod
	    s/\.pod\z//;
	    $pages{$_}  = "" unless defined $pages{$_};
	    $pages{$_} .= "$dir/$_.pod:";
	    push(@pods, "$dir/$_.pod");
	} elsif (/\.html\z/) { 	    	    	    	    # .html
	    s/\.html\z//;
	    $pages{$_}  = "" unless defined $pages{$_};
	    $pages{$_} .= "$dir/$_.pod:";
	} elsif (/\.pm\z/) { 	    	    	    	    # .pm
	    s/\.pm\z//;
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
    my($tag, $which_head, $otitle, $listdepth, $index);

    # here we need	local $ignore = 0;
    #  unfortunately, we can't have it, because $ignore is lexical
    $ignore = 0;

    $listdepth = 0;
    $index = "";

    # scan for =head directives, note their name, and build an index
    #  pointing to each of them.
    foreach my $line (@data) {
	if ($line =~ /^=(head)([1-6])\s+(.*)/) {
	    ($tag, $which_head, $otitle) = ($1,$2,$3);

            my $title = depod( $otitle );
            my $name = htmlify( $title );
	    $$sections{$name} = 1;
	    $title = process_text( \$otitle );

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
	              "<A HREF=\"#" . $name . "\">" .
		      $title . "</A></LI>";
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
    my( $itemref, $pod, @poddata ) = @_;
    my($i, $item);
    local $_;

    $pod =~ s/\.pod\z//;
    $pod .= ".html" if $pod;

    foreach $i (0..$#poddata) {
	my $txt = depod( $poddata[$i] );

	# figure out what kind of item it is.
	# Build string for referencing this item.
	if ( $txt =~ /\A=item\s+\*\s*(.*)\Z/s ) { # bullet
	    next unless $1;
	    $item = $1;
        } elsif( $txt =~ /\A=item\s+(?>\d+\.?)\s*(.*)\Z/s ) { # numbered list
	    $item = $1;
	} elsif( $txt =~ /\A=item\s+(.*)\Z/s ) { # plain item
	    $item = $1;
	} else {
	    next;
	}
	my $fid = fragment_id( $item );
	$$itemref{$fid} = "$pod" if $fid;
    }
}

#
# process_head - convert a pod head[1-6] tag and convert it to HTML format.
#
sub process_head {
    my($tag, $heading, $hasindex) = @_;

    # figure out the level of the =head
    $tag =~ /head([1-6])/;
    my $level = $1;

    if( $listlevel ){
	warn "$0: $podfile: unterminated list at =head in paragraph $paragraph.  ignoring.\n";
        while( $listlevel ){
            process_back();
        }
    }

    print HTML "<P>\n";
    if( $level == 1 && ! $top ){
	print HTML "<A HREF=\"#__index__\"><SMALL>$backlink</SMALL></A>\n"
	    if $hasindex and $backlink;
	print HTML "<HR>\n"
    }

    my $name = htmlify( depod( $heading ) );
    my $convert = process_text( \$heading );
    print HTML "<H$level><A NAME=\"$name\">$convert</A></H$level>\n";
}


#
# emit_item_tag - print an =item's text
# Note: The global $EmittedItem is used for inhibiting self-references.
#
my $EmittedItem;

sub emit_item_tag($$$){
    my( $otext, $text, $compact ) = @_;
    my $item = fragment_id( $text );

    $EmittedItem = $item;
    ### print STDERR "emit_item_tag=$item ($text)\n";

    print HTML '<STRONG>';
    if ($items_named{$item}++) {
	print HTML process_text( \$otext );
    } else {
	my $name = 'item_' . $item;
	print HTML qq{<A NAME="$name">}, process_text( \$otext ), '</A>';
    }
    print HTML "</STRONG><BR>\n";
    undef( $EmittedItem );
}

sub emit_li {
    my( $tag ) = @_;
    if( $items_seen[$listlevel]++ == 0 ){
	push( @listend, "</$tag>" );
	print HTML "<$tag>\n";
    }
    print HTML $tag eq 'DL' ? '<DT>' : '<LI>';
}

#
# process_item - convert a pod item tag and convert it to HTML format.
#
sub process_item {
    my( $otext ) = @_;

    # lots of documents start a list without doing an =over.  this is
    # bad!  but, the proper thing to do seems to be to just assume
    # they did do an =over.  so warn them once and then continue.
    if( $listlevel == 0 ){
	warn "$0: $podfile: unexpected =item directive in paragraph $paragraph.  ignoring.\n";
	process_over();
    }

    # formatting: insert a paragraph if preceding item has >1 paragraph
    if( $after_lpar ){
	print HTML "<P></P>\n";
	$after_lpar = 0;
    }

    # remove formatting instructions from the text
    my $text = depod( $otext );

    # all the list variants:
    if( $text =~ /\A\*/ ){ # bullet
        emit_li( 'UL' );
	if ($text =~ /\A\*\s+(.+)\Z/s ) { # with additional text
	    my $tag = $1;
	    $otext =~ s/\A\*\s+//;
	    emit_item_tag( $otext, $tag, 1 );
	}

    } elsif( $text =~ /\A\d+/ ){ # numbered list
	emit_li( 'OL' );
	if ($text =~ /\A(?>\d+\.?)\s*(.+)\Z/s ) { # with additional text
	    my $tag = $1;
	    $otext =~ s/\A\d+\.?\s*//;
	    emit_item_tag( $otext, $tag, 1 );
	}

    } else {			# definition list
	emit_li( 'DL' );
	if ($text =~ /\A(.+)\Z/s ){ # should have text
	    emit_item_tag( $otext, $text, 1 );
	}
       print HTML '<DD>';
    }
    print HTML "\n";
}

#
# process_over - process a pod over tag and start a corresponding HTML list.
#
sub process_over {
    # start a new list
    $listlevel++;
    push( @items_seen, 0 );
    $after_lpar = 0;
}

#
# process_back - process a pod back tag and convert it to HTML format.
#
sub process_back {
    if( $listlevel == 0 ){
	warn "$0: $podfile: unexpected =back directive in paragraph $paragraph.  ignoring.\n";
	return;
    }

    # close off the list.  note, I check to see if $listend[$listlevel] is
    # defined because an =item directive may have never appeared and thus
    # $listend[$listlevel] may have never been initialized.
    $listlevel--;
    if( defined $listend[$listlevel] ){
	print HTML '<P></P>' if $after_lpar;
	print HTML $listend[$listlevel];
        print HTML "\n";
        pop( @listend );
    }
    $after_lpar = 0;

    # clean up item count
    pop( @items_seen );
}

#
# process_cut - process a pod cut tag, thus start ignoring pod directives.
#
sub process_cut {
    $ignore = 1;
}

#
# process_pod - process a pod pod tag, thus stop ignoring pod directives
# until we see a corresponding cut.
#
sub process_pod {
    # no need to set $ignore to 0 cause the main loop did it
}

#
# process_for - process a =for pod tag.  if it's for html, spit
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
    pop( @begin_stack );
}

#
# process_pre - indented paragraph, made into <PRE></PRE>
#
sub process_pre {
    my( $text ) = @_;
    my( $rest );
    return if $ignore;

    $rest = $$text;

    # insert spaces in place of tabs
    $rest =~ s#.*#
	    my $line = $&;
	    1 while $line =~ s/\t+/' ' x (length($&) * 8 - length($`) % 8)/e;
	    $line;
	#eg;

    # convert some special chars to HTML escapes
    $rest =~ s/&/&amp;/g;
    $rest =~ s/</&lt;/g;
    $rest =~ s/>/&gt;/g;
    $rest =~ s/"/&quot;/g;

    # try and create links for all occurrences of perl.* within
    # the preformatted text.
    $rest =~ s{
	         (\s*)(perl\w+)
	      }{
		 if ( defined $pages{$2} ){	# is a link
		     qq($1<A HREF="$htmlroot/$pages{$2}">$2</A>);
		 } elsif (defined $pages{dosify($2)}) {	# is a link
		     qq($1<A HREF="$htmlroot/$pages{dosify($2)}">$2</A>);
		 } else {
		     "$1$2";
		 }
	      }xeg;
     $rest =~ s{
		 (<A\ HREF="?) ([^>:]*:)? ([^>:]*) \.pod: ([^>:]*:)?
               }{
                  my $url ;
                  if ( $htmlfileurl ne '' ){
		     # Here, we take advantage of the knowledge 
		     # that $htmlfileurl ne '' implies $htmlroot eq ''.
		     # Since $htmlroot eq '', we need to prepend $htmldir
		     # on the fron of the link to get the absolute path
		     # of the link's target. We check for a leading '/'
		     # to avoid corrupting links that are #, file:, etc.
		     my $old_url = $3 ;
		     $old_url = "$htmldir$old_url" if $old_url =~ m{^\/};
 		     $url = relativize_url( "$old_url.html", $htmlfileurl );
	          } else {
		     $url = "$3.html" ;
		  }
		  "$1$url" ;
	       }xeg;

    # Look for embedded URLs and make them into links.  We don't
    # relativize them since they are best left as the author intended.

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
	  (?!:)                     # Ignore File::, among others.
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

    # text should be as it is (verbatim)
    $$text = $rest;
}


#
# pure text processing
#
# pure_text/inIS_text: differ with respect to automatic C<> recognition.
# we don't want this to happen within IS
#
sub pure_text($){
    my $text = shift();
    process_puretext( $text, \$ptQuote, 1 );
}

sub inIS_text($){
    my $text = shift();
    process_puretext( $text, \$ptQuote, 0 );
}

#
# process_puretext - process pure text (without pod-escapes) converting
#  double-quotes and handling implicit C<> links.
#
sub process_puretext {
    my($text, $quote, $notinIS) = @_;

    ## Guessing at func() or [$@%&]*var references in plain text is destined
    ## to produce some strange looking ref's. uncomment to disable:
    ## $notinIS = 0;

    my(@words, $lead, $trail);

    # convert double-quotes to single-quotes
    if( $$quote && $text =~ s/"/''/s ){
        $$quote = 0;
    }
    while ($text =~ s/"([^"]*)"/``$1''/sg) {};
    $$quote = 1 if $text =~ s/"/``/s;

    # keep track of leading and trailing white-space
    $lead  = ($text =~ s/\A(\s+)//s ? $1 : "");
    $trail = ($text =~ s/(\s+)\Z//s ? $1 : "");

    # split at space/non-space boundaries
    @words = split( /(?<=\s)(?=\S)|(?<=\S)(?=\s)/, $text );

    # process each word individually
    foreach my $word (@words) {
	# skip space runs
 	next if $word =~ /^\s*$/;
	# see if we can infer a link
	if( $notinIS && $word =~ /^(\w+)\((.*)\)$/ ) {
	    # has parenthesis so should have been a C<> ref
            ## try for a pagename (perlXXX(1))?
            my( $func, $args ) = ( $1, $2 );
            if( $args =~ /^\d+$/ ){
                my $url = page_sect( $word, '' );
                if( defined $url ){
                    $word = "<A HREF=\"$url\">the $word manpage</A>";
                    next;
                }
            }
            ## try function name for a link, append tt'ed argument list
            $word = emit_C( $func, '', "($args)");

#### disabled. either all (including $\W, $\w+{.*} etc.) or nothing.
##      } elsif( $notinIS && $word =~ /^[\$\@%&*]+\w+$/) {
##	    # perl variables, should be a C<> ref
##	    $word = emit_C( $word );

	} elsif ($word =~ m,^\w+://\w,) {
	    # looks like a URL
            # Don't relativize it: leave it as the author intended
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

    # put everything back together
    return $lead . join( '', @words ) . $trail;
}


#
# process_text - handles plaintext that appears in the input pod file.
# there may be pod commands embedded within the text so those must be
# converted to html commands.
#

sub process_text1($$;$$);
sub pattern ($) { $_[0] ? '[^\S\n]+'.('>' x ($_[0] + 1)) : '>' }
sub closing ($) { local($_) = shift; (defined && s/\s+$//) ? length : 0 }

sub process_text {
    return if $ignore;
    my( $tref ) = @_;
    my $res = process_text1( 0, $tref );
    $$tref = $res;
}

sub process_text1($$;$$){
    my( $lev, $rstr, $func, $closing ) = @_;
    my $res = '';

    unless (defined $func) {
	$func = '';
	$lev++;
    }

    if( $func eq 'B' ){
	# B<text> - boldface
	$res = '<STRONG>' . process_text1( $lev, $rstr ) . '</STRONG>';

    } elsif( $func eq 'C' ){
	# C<code> - can be a ref or <CODE></CODE>
	# need to extract text
	my $par = go_ahead( $rstr, 'C', $closing );

	## clean-up of the link target
        my $text = depod( $par );

	### my $x = $par =~ /[BI]</ ? 'yes' : 'no' ;
        ### print STDERR "-->call emit_C($par) lev=$lev, par with BI=$x\n"; 

	$res = emit_C( $text, $lev > 1 || ($par =~ /[BI]</) );

    } elsif( $func eq 'E' ){
	# E<x> - convert to character
	$$rstr =~ s/^([^>]*)>//;
	my $escape = $1;
	$escape =~ s/^(\d+|X[\dA-F]+)$/#$1/i;
	$res = "&$escape;";

    } elsif( $func eq 'F' ){
	# F<filename> - italizice
	$res = '<EM>' . process_text1( $lev, $rstr ) . '</EM>';

    } elsif( $func eq 'I' ){
	# I<text> - italizice
	$res = '<EM>' . process_text1( $lev, $rstr ) . '</EM>';

    } elsif( $func eq 'L' ){
	# L<link> - link
	## L<text|cross-ref> => produce text, use cross-ref for linking 
	## L<cross-ref> => make text from cross-ref
	## need to extract text
	my $par = go_ahead( $rstr, 'L', $closing );

        # some L<>'s that shouldn't be:
	# a) full-blown URL's are emitted as-is
        if( $par =~ m{^\w+://}s ){
	    return make_URL_href( $par );
	}
        # b) C<...> is stripped and treated as C<>
        if( $par =~ /^C<(.*)>$/ ){
	    my $text = depod( $1 );
 	    return emit_C( $text, $lev > 1 || ($par =~ /[BI]</) );
	}

	# analyze the contents
	$par =~ s/\n/ /g;   # undo word-wrapped tags
        my $opar = $par;
	my $linktext;
	if( $par =~ s{^([^|]+)\|}{} ){
	    $linktext = $1;
	}
    
	# make sure sections start with a /
	$par =~ s{^"}{/"};

	my( $page, $section, $ident );

	# check for link patterns
	if( $par =~ m{^([^/]+?)/(?!")(.*?)$} ){     # name/ident
            # we've got a name/ident (no quotes) 
            ( $page, $ident ) = ( $1, $2 );
            ### print STDERR "--> L<$par> to page $page, ident $ident\n";

	} elsif( $par =~ m{^(.*?)/"?(.*?)"?$} ){ # [name]/"section"
            # even though this should be a "section", we go for ident first
	    ( $page, $ident ) = ( $1, $2 );
            ### print STDERR "--> L<$par> to page $page, section $section\n";

	} elsif( $par =~ /\s/ ){  # this must be a section with missing quotes
	    ( $page, $section ) = ( '', $par );
            ### print STDERR "--> L<$par> to void page, section $section\n";

        } else {
	    ( $page, $section ) = ( $par, '' );
            ### print STDERR "--> L<$par> to page $par, void section\n";
	}

        # now, either $section or $ident is defined. the convoluted logic
        # below tries to resolve L<> according to what the user specified.
        # failing this, we try to find the next best thing...
        my( $url, $ltext, $fid );

        RESOLVE: {
            if( defined $ident ){
                ## try to resolve $ident as an item
	        ( $url, $fid ) = coderef( $page, $ident );
                if( $url ){
                    if( ! defined( $linktext ) ){
                        $linktext = $ident;
                        $linktext .= " in " if $ident && $page;
                        $linktext .= "the $page manpage" if $page;
                    }
                    ###  print STDERR "got coderef url=$url\n";
                    last RESOLVE;
                }
                ## no luck: go for a section (auto-quoting!)
                $section = $ident;
            }
            ## now go for a section
            my $htmlsection = htmlify( $section );
 	    $url = page_sect( $page, $htmlsection );
            if( $url ){
                if( ! defined( $linktext ) ){
                    $linktext = $section;
                    $linktext .= " in " if $section && $page;
                    $linktext .= "the $page manpage" if $page;
                }
                ### print STDERR "got page/section url=$url\n";
                last RESOLVE;
            }
            ## no luck: go for an ident 
            if( $section ){
                $ident = $section;
            } else {
                $ident = $page;
                $page  = undef();
            }
            ( $url, $fid ) = coderef( $page, $ident );
            if( $url ){
                if( ! defined( $linktext ) ){
                    $linktext = $ident;
                    $linktext .= " in " if $ident && $page;
                    $linktext .= "the $page manpage" if $page;
                }
                ### print STDERR "got section=>coderef url=$url\n";
                last RESOLVE;
            }

            # warning; show some text.
            $linktext = $opar unless defined $linktext;
            warn "$0: $podfile: cannot resolve L<$opar> in paragraph $paragraph.";
        }

        # now we have an URL or just plain code
        $$rstr = $linktext . '>' . $$rstr;
        if( defined( $url ) ){
            $res = "<A HREF=\"$url\">" . process_text1( $lev, $rstr ) . '</A>';
        } else {
	    $res = '<EM>' . process_text1( $lev, $rstr ) . '</EM>';
        }

    } elsif( $func eq 'S' ){
	# S<text> - non-breaking spaces
	$res = process_text1( $lev, $rstr );
	$res =~ s/ /&nbsp;/g;

    } elsif( $func eq 'X' ){
	# X<> - ignore
	$$rstr =~ s/^[^>]*>//;

    } elsif( $func eq 'Z' ){
	# Z<> - empty 
	warn "$0: $podfile: invalid X<> in paragraph $paragraph."
	    unless $$rstr =~ s/^>//;

    } else {
        my $term = pattern $closing;
	while( $$rstr =~ s/\A(.*?)(([BCEFILSXZ])<(<+[^\S\n]+)?|$term)//s ){
	    # all others: either recurse into new function or
	    # terminate at closing angle bracket(s)
	    my $pt = $1;
            $pt .= $2 if !$3 &&  $lev == 1;
	    $res .= $lev == 1 ? pure_text( $pt ) : inIS_text( $pt );
	    return $res if !$3 && $lev > 1;
            if( $3 ){
		$res .= process_text1( $lev, $rstr, $3, closing $4 );
 	    }
	}
	if( $lev == 1 ){
	    $res .= pure_text( $$rstr );
	} else {
	    warn "$0: $podfile: undelimited $func<> in paragraph $paragraph.";
	}
    }
    return $res;
}

#
# go_ahead: extract text of an IS (can be nested)
#
sub go_ahead($$$){
    my( $rstr, $func, $closing ) = @_;
    my $res = '';
    my @closing = ($closing);
    while( $$rstr =~
      s/\A(.*?)(([BCEFILSXZ])<(<+[^\S\n]+)?|@{[pattern $closing[0]]})//s ){
	$res .= $1;
	unless( $3 ){
	    shift @closing;
	    return $res unless @closing;
	} else {
	    unshift @closing, closing $4;
	}
	$res .= $2;
    }
    warn "$0: $podfile: undelimited $func<> in paragraph $paragraph.";
    return $res;
}

#
# emit_C - output result of C<text>
#    $text is the depod-ed text
#
sub emit_C($;$$){
    my( $text, $nocode, $args ) = @_;
    $args = '' unless defined $args;
    my $res;
    my( $url, $fid ) = coderef( undef(), $text );

    # need HTML-safe text
    my $linktext = html_escape( "$text$args" );

    if( defined( $url ) &&
        (!defined( $EmittedItem ) || $EmittedItem ne $fid ) ){
	$res = "<A HREF=\"$url\"><CODE>$linktext</CODE></A>";
    } elsif( 0 && $nocode ){
	$res = $linktext;
    } else {
	$res = "<CODE>$linktext</CODE>";
    }
    return $res;
}

#
# html_escape: make text safe for HTML
#
sub html_escape {
    my $rest = $_[0];
    $rest   =~ s/&/&amp;/g;
    $rest   =~ s/</&lt;/g;
    $rest   =~ s/>/&gt;/g;
    $rest   =~ s/"/&quot;/g;
    return $rest;
} 


#
# dosify - convert filenames to 8.3
#
sub dosify {
    my($str) = @_;
    return lc($str) if $^O eq 'VMS';     # VMS just needs casing
    if ($Is83) {
        $str = lc $str;
        $str =~ s/(\.\w+)/substr ($1,0,4)/ge;
        $str =~ s/(\w+)/substr ($1,0,8)/ge;
    }
    return $str;
}

#
# page_sect - make an URL from the text of a L<>
#
sub page_sect($$) {
    my( $page, $section ) = @_;
    my( $linktext, $page83, $link);	# work strings

    # check if we know that this is a section in this page
    if (!defined $pages{$page} && defined $sections{$page}) {
	$section = $page;
	$page = "";
        ### print STDERR "reset page='', section=$section\n";
    }

    $page83=dosify($page);
    $page=$page83 if (defined $pages{$page83});
    if ($page eq "") {
	$link = "#" . htmlify( $section );
    } elsif ( $page =~ /::/ ) {
	$page =~ s,::,/,g;
	# Search page cache for an entry keyed under the html page name,
	# then look to see what directory that page might be in.  NOTE:
	# this will only find one page. A better solution might be to produce
	# an intermediate page that is an index to all such pages.
	my $page_name = $page ;
	$page_name =~ s,^.*/,,s ;
	if ( defined( $pages{ $page_name } ) && 
	     $pages{ $page_name } =~ /([^:]*$page)\.(?:pod|pm):/ 
	   ) {
	    $page = $1 ;
	}
	else {
	    # NOTE: This branch assumes that all A::B pages are located in
	    # $htmlroot/A/B.html . This is often incorrect, since they are
	    # often in $htmlroot/lib/A/B.html or such like. Perhaps we could
	    # analyze the contents of %pages and figure out where any
	    # cousins of A::B are, then assume that.  So, if A::B isn't found,
	    # but A::C is found in lib/A/C.pm, then A::B is assumed to be in
	    # lib/A/B.pm. This is also limited, but it's an improvement.
	    # Maybe a hints file so that the links point to the correct places
	    # nonetheless?

	}
	$link = "$htmlroot/$page.html";
	$link .= "#" . htmlify( $section ) if ($section);
    } elsif (!defined $pages{$page}) {
	$link = "";
    } else {
	$section = htmlify( $section ) if $section ne "";
        ### print STDERR "...section=$section\n";

	# if there is a directory by the name of the page, then assume that an
	# appropriate section will exist in the subdirectory
#	if ($section ne "" && $pages{$page} =~ /([^:]*[^(\.pod|\.pm)]):/) {
	if ($section ne "" && $pages{$page} =~ /([^:]*(?<!\.pod)(?<!\.pm)):/) {
	    $link = "$htmlroot/$1/$section.html";
            ### print STDERR "...link=$link\n";

	# since there is no directory by the name of the page, the section will
	# have to exist within a .html of the same name.  thus, make sure there
	# is a .pod or .pm that might become that .html
	} else {
	    $section = "#$section" if $section;
            ### print STDERR "...section=$section\n";

	    # check if there is a .pod with the page name
	    if ($pages{$page} =~ /([^:]*)\.pod:/) {
		$link = "$htmlroot/$1.html$section";
	    } elsif ($pages{$page} =~ /([^:]*)\.pm:/) {
		$link = "$htmlroot/$1.html$section";
	    } else {
		$link = "";
	    }
	}
    }

    if ($link) {
	# Here, we take advantage of the knowledge that $htmlfileurl ne ''
	# implies $htmlroot eq ''. This means that the link in question
	# needs a prefix of $htmldir if it begins with '/'. The test for
	# the initial '/' is done to avoid '#'-only links, and to allow
	# for other kinds of links, like file:, ftp:, etc.
        my $url ;
        if (  $htmlfileurl ne '' ) {
            $link = "$htmldir$link" if $link =~ m{^/}s;
            $url = relativize_url( $link, $htmlfileurl );
# print( "  b: [$link,$htmlfileurl,$url]\n" );
	}
	else {
            $url = $link ;
	}
	return $url;

    } else {
	return undef();
    }
}

#
# relativize_url - convert an absolute URL to one relative to a base URL.
# Assumes both end in a filename.
#
sub relativize_url {
    my ($dest,$source) = @_ ;

    my ($dest_volume,$dest_directory,$dest_file) = 
        File::Spec::Unix->splitpath( $dest ) ;
    $dest = File::Spec::Unix->catpath( $dest_volume, $dest_directory, '' ) ;

    my ($source_volume,$source_directory,$source_file) = 
        File::Spec::Unix->splitpath( $source ) ;
    $source = File::Spec::Unix->catpath( $source_volume, $source_directory, '' ) ;

    my $rel_path = '' ;
    if ( $dest ne '' ) {
       $rel_path = File::Spec::Unix->abs2rel( $dest, $source ) ;
    }

    if ( $rel_path ne ''                && 
         substr( $rel_path, -1 ) ne '/' &&
         substr( $dest_file, 0, 1 ) ne '#' 
        ) {
        $rel_path .= "/$dest_file" ;
    }
    else {
        $rel_path .= "$dest_file" ;
    }

    return $rel_path ;
}


#
# coderef - make URL from the text of a C<>
#
sub coderef($$){
    my( $page, $item ) = @_;
    my( $url );

    my $fid = fragment_id( $item );
    if( defined( $page ) ){
	# we have been given a $page...
	$page =~ s{::}{/}g;

	# Do we take it? Item could be a section!
	my $base = $items{$fid} || "";
	$base =~ s{[^/]*/}{};
	if( $base ne "$page.html" ){
            ###   print STDERR "coderef( $page, $item ): items{$fid} = $items{$fid} = $base => discard page!\n";
	    $page = undef();
	}

    } else {
        # no page - local items precede cached items
	if( defined( $fid ) ){
	    if(  exists $local_items{$fid} ){
		$page = $local_items{$fid};
	    } else {
		$page = $items{$fid};
	    }
	}
    }

    # if there was a pod file that we found earlier with an appropriate
    # =item directive, then create a link to that page.
    if( defined $page ){
	if( $page ){
            if( exists $pages{$page} and $pages{$page} =~ /([^:.]*)\.[^:]*:/){
		$page = $1 . '.html';
	    }
	    my $link = "$htmlroot/$page#item_$fid";

	    # Here, we take advantage of the knowledge that $htmlfileurl
	    # ne '' implies $htmlroot eq ''.
	    if (  $htmlfileurl ne '' ) {
		$link = "$htmldir$link" ;
		$url = relativize_url( $link, $htmlfileurl ) ;
	    } else {
		$url = $link ;
	    }
	} else {
	    $url = "#item_" . $fid;
	}

	confess "url has space: $url" if $url =~ /"[^"]*\s[^"]*"/;
    }       
    return( $url, $fid );
}



#
# Adapted from Nick Ing-Simmons' PodToHtml package.
sub relative_url {
    my $source_file = shift ;
    my $destination_file = shift;

    my $source = URI::file->new_abs($source_file);
    my $uo = URI::file->new($destination_file,$source)->abs;
    return $uo->rel->as_string;
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
# specification for HTML. Note that we keep spaces and special characters
# except ", ? (Netscape problem) and the hyphen (writer's problem...).
#
sub htmlify {
    my( $heading) = @_;
    $heading =~ s/(\s+)/ /g;
    $heading =~ s/\s+\Z//;
    $heading =~ s/\A\s+//;
    # The hyphen is a disgrace to the English language.
    $heading =~ s/[-"?]//g;
    $heading = lc( $heading );
    return $heading;
}

#
# depod - convert text by eliminating all interior sequences
# Note: can be called with copy or modify semantics
#
my %E2c;
$E2c{lt}     = '<';
$E2c{gt}     = '>';
$E2c{sol}    = '/';
$E2c{verbar} = '|';
$E2c{amp}    = '&'; # in Tk's pods

sub depod1($;$$);

sub depod($){
    my $string;
    if( ref( $_[0] ) ){
	$string =  ${$_[0]};
        ${$_[0]} = depod1( \$string );
    } else {
	$string =  $_[0];
        depod1( \$string );
    }    
}

sub depod1($;$$){
  my( $rstr, $func, $closing ) = @_;
  my $res = '';
  return $res unless defined $$rstr;
  if( ! defined( $func ) ){
      # skip to next begin of an interior sequence
      while( $$rstr =~ s/\A(.*?)([BCEFILSXZ])<(<+[^\S\n]+)?// ){
         # recurse into its text
	  $res .= $1 . depod1( $rstr, $2, closing $3);
      }
      $res .= $$rstr;
  } elsif( $func eq 'E' ){
      # E<x> - convert to character
      $$rstr =~ s/^([^>]*)>//;
      $res .= $E2c{$1} || "";
  } elsif( $func eq 'X' ){
      # X<> - ignore
      $$rstr =~ s/^[^>]*>//;
  } elsif( $func eq 'Z' ){
      # Z<> - empty 
      $$rstr =~ s/^>//;
  } else {
      # all others: either recurse into new function or
      # terminate at closing angle bracket
      my $term = pattern $closing;
      while( $$rstr =~ s/\A(.*?)(([BCEFILSXZ])<(<+[^\S\n]+)?|$term)// ){
	  $res .= $1;
	  last unless $3;
          $res .= depod1( $rstr, $3, closing $4 );
      }
      ## If we're here and $2 ne '>': undelimited interior sequence.
      ## Ignored, as this is called without proper indication of where we are.
      ## Rely on process_text to produce diagnostics.
  }
  return $res;
}

#
# fragment_id - construct a fragment identifier from:
#   a) =item text
#   b) contents of C<...>
#
my @hc;
sub fragment_id {
    my $text = shift();
    $text =~ s/\s+\Z//s;
    if( $text ){
	# a method or function?
	return $1 if $text =~ /(\w+)\s*\(/;
	return $1 if $text =~ /->\s*(\w+)\s*\(?/;

	# a variable name?
	return $1 if $text =~ /^([$@%*]\S+)/;

	# some pattern matching operator?
	return $1 if $text =~ m|^(\w+/).*/\w*$|;

	# fancy stuff... like "do { }"
	return $1 if $text =~ m|^(\w+)\s*{.*}$|;

	# honour the perlfunc manpage: func [PAR[,[ ]PAR]...]
	# and some funnies with ... Module ...
	return $1 if $text =~ m{^([a-z\d]+)(\s+[A-Z\d,/& ]+)?$};
	return $1 if $text =~ m{^([a-z\d]+)\s+Module(\s+[A-Z\d,/& ]+)?$};

	# text? normalize!
	$text =~ s/\s+/_/sg;
	$text =~ s{(\W)}{
         defined( $hc[ord($1)] ) ? $hc[ord($1)]
                 : ( $hc[ord($1)] = sprintf( "%%%02X", ord($1) ) ) }gxe;
        $text = substr( $text, 0, 50 );
    } else {
	return undef();
    }
}

#
# make_URL_href - generate HTML href from URL
# Special treatment for CGI queries.
#
sub make_URL_href($){
    my( $url ) = @_;
    if( $url !~ 
        s{^(http:[-\w/#~:.+=&%@!]+)(\?.*)$}{<A HREF="$1$2">$1</A>}i ){
        $url = "<A HREF=\"$url\">$url</A>";
    }
    return $url;
}

1;
