# $FreeBSD$
#
# Props to Daniel Stone for starting this script for me.  I hate perl.

my $lastline = "";
my $foundopening = 0;
my $foundclosing = 0;

while (<>) {
	$curline = $_;
	if (!$foundopening) {
		if (/Copyright/) {
			$foundopening = 1;
			# print the previous line we buffered, but with /*-
			if ($lastline !~ /\/\*-/) {
				$lastline =~ s/\/\*/\/\*-/;
			}
			print $lastline;
			# now, print the current line.
			print $curline;
		} else {
			# print the previous line and continue on
			print $lastline;
		}
	} elsif ($foundopening && !$foundclosing && /\*\//) {
		# print the $FreeBSD$ bits after the end of the license block
		$foundclosing = 1;
		print;
		print "\n";
		print "#include <sys/cdefs.h>\n";
		print "__FBSDID(\"\$FreeBSD\$\");\n";
	} elsif ($foundopening) {
		# Replace headers with the system's paths.  the headers we're
		# concerned with are drm*.h, *_drm.h and *_drv.h
		# 
		s/#include "(.*)_drv.h/#include "dev\/drm\/\1_drv.h/;
		s/#include "(.*)_drm.h/#include "dev\/drm\/\1_drm.h/;
		s/#include "mga_ucode.h/#include "dev\/drm\/mga_ucode.h/;
		s/#include "r300_reg.h/#include "dev\/drm\/r300_reg.h/;
		s/#include "radeon_microcode.h/#include "dev\/drm\/radeon_microcode.h/;
		s/#include "sis_ds.h/#include "dev\/drm\/sis_ds.h/;
		s/#include "drm/#include "dev\/drm\/drm/;
		print;
	}
	$lastline = $curline;
}

# if we never found the copyright header, then we're still a line behind.
if (!$foundopening) {
	print $lastline;
}
