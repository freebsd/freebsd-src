#!./perl

BEGIN {
    chdir 't' if -d 't';
    if ($^O eq 'MacOS') { 
	@INC = qw(: ::lib ::macos:lib); 
    } else { 
	@INC = '.'; 
	push @INC, '../lib'; 
    }
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bFile\/Glob\b/i) {
        print "1..0\n";
        exit 0;
    }
    print "1..7\n";
}
END {
    print "not ok 1\n" unless $loaded;
}
use File::Glob qw(:glob csh_glob);
$loaded = 1;
print "ok 1\n";

my $pat = $^O eq "MacOS" ? ":lib:G*.t" : "lib/G*.t";

# Test the actual use of the case sensitivity tags, via csh_glob()
import File::Glob ':nocase';
@a = csh_glob($pat); # At least glob-basic.t glob-case.t glob-global.t
print "not " unless @a >= 3;
print "ok 2\n";

# This may fail on systems which are not case-PRESERVING
import File::Glob ':case';
@a = csh_glob($pat); # None should be uppercase
print "not " unless @a == 0;
print "ok 3\n";

# Test the explicit use of the GLOB_NOCASE flag
@a = bsd_glob($pat, GLOB_NOCASE);
print "not " unless @a >= 3;
print "ok 4\n";

# Test Win32 backslash nastiness...
if ($^O ne 'MSWin32') {
    print "ok 5\nok 6\nok 7\n";
}
else {
    @a = File::Glob::glob("lib\\g*.t");
    print "not " unless @a >= 3;
    print "ok 5\n";
    mkdir "[]", 0;
    @a = File::Glob::glob("\\[\\]", GLOB_QUOTE);
    rmdir "[]";
    print "# returned @a\nnot " unless @a == 1;
    print "ok 6\n";
    @a = bsd_glob("lib\\*", GLOB_QUOTE);
    print "not " if @a == 0;
    print "ok 7\n";
}
