.Go 14 "QUESTIONS & ANSWERS"
.nr qN 0 1
.de Q
.sp
.IP \fB\\n+(qN) 0.4i
..
.de A
.IP \fR 0.75i
..
.Q
How can I make \*E run faster under DOS?
.A
There are several things you can do.
The first thing to do is get a good screen driver such as NANSI.SYS.
This can speed up screen redrawing by as much as a factor of eight!
The DOS-specific part of section 12 tells you how to do this.
.A
You might also consider reducing the size of the blocks that \*E uses.
You'll need to recompile \*E to do this.
The default BLKSIZE is 1024 byte for the DOS version of \*E, which means
that for each keystroke that you insert, \*E must shift an average of
about 500 bytes.
That's a lot to ask from a little old 5MHz 8088.
A BLKSIZE of 512 bytes might be more appropriate.
.A
If you're \fIreally\fR desperate for more speed, you might want to make
\*E store its temporary files on a RAM disk.
However, this limits the size of the file you can edit, and it eliminates any
chance you may have had to recover your work after a power failure
or system crash, but it might be worth it; you decide.
To do this, add ":set dir=R:\\" (or whatever your RAM disk's name is)
to the \fIelvis.rc\fP file.
.A
Next, consider turning off the "sync" option.
When the sync option is turned on, \*E will close the temporary file
and reopen it after every change, in order to force DOS to update
the file's directory entry.
If you put ":set nosync" into the \fIelvis.rc\fP file, then \*E will
only close the file when you start editing a different text file, or
when you're exiting \*E.
Consequently, there is no chance that you'll be able to recover your
changes after a power failure... so if you're going to this, then you
might as well store the temp files on the RAM disk, too.
.Q
Where's the <Esc> key on a DEC keyboard?
.A
I don't know.  Maybe the <F11> key?
You could always use ":map!" to make some other key act like the <Esc> key.
If all else fails, use <Control><[>.
.Q
Is there a way to show which keys do what?
.A
Yes.  The command ":map" will show what each key does in command mode,
and ":map!" (with an exclamation mark) shows what each key does in
input mode.
.A
The table is divided into three columns: the key's label, the characters
that it sends, and the characters that \*E pretends you typed.
.Q
How can I make \*E display long lines like the real vi?
.A
You can't yet.
The next version of \*E should support this, though.
.Q
I can't recover my text [under MS-DOS or Atari TOS].
According to the directory listing, the temporary file is 0 bytes long.
What went wrong?
.A
MS-DOS and TOS only update a file's directory entry when the file is closed.
If the system crashes while the file is still open, then the file's length
is stored as 0 bytes.
The ":set sync" option is supposed to prevent this;
you probably turned it off in the interest of speed, right?
.A
Under MS-DOS [I don't know about TOS], you should delete the empty
temporary file, and then run CHKDSK/F.
This \fImight\fP find the data that belonged in the empty file,
and place it in a new file with a name like "000001.CHK" -- something like that.
You can then try to extract the text from that temporary file by giving the
command "elvprsv -R 000001.chk".
If you're lucky, then this might recover your text.
.Q
What is the most current version of \*E?
.A
Each version of \*E that is released to the public has a version number
of the form "number point number".
As I write this, the most current version of \*E is \*V.
.A
The intermediate steps between one release and the next are labeled with
the \fInext\fP version number, with a letter appended.
For example, after 1.4 was released, I started working on 1.5a.
I am currently working on 2.0a.
When \*E reaches a stable state, I'll call it 2.0 and release it.
.A
Sometimes a beta-test version of \*E will be available via anonymous FTP
from m2xenix.psg.com, in the directory "pub/elvis/beta".
.Q
I only got executables, but now I want the source code.
Where can I get it?
.A
If you have access to the Internet, then you should be able to fetch it
from one of the public archives such as \fBplains.nodak.edu\fP.
It is accessible via anonymous FTP, or via an email server named
"archive-server@plains.nodak.edu".
Elvis is located in the directory "/pub/Minix/all.contrib".
.A
It is also available from the C Users' Group, in volume #365.
As I write this, they are asking $4 per disk plus $3.50 per order in the US,
and elvis requires three disks; this is subject to change.
Their phone number is (913) 841-1631, and their address is:
.LD
.ta 2.5i
.sp
	The C Users' Group
	1601 W. 23rd Street, #200
	Lawrence  KS  66046-2743
.DE
.Q
Is this shareware, or public domain, or what?
.A
It is not public domain; it is copyrighted by me, Steve Kirkendall.
However, this particular version is freely redistributable, in either
source form or executable form.
(I would prefer that you give copies away for free, complete with the
full source code... but I'm not going to force you.)
.A
It is not shareware; you aren't expected to send me anything.
You can use it without guilt.
.A
It is not "copylefted."
I hold a copyright, but currently I have not added any of the usual restrictions
that you would find on copylefted software.
If people start doing really obnoxious things to \*E, then I will start
adding restrictions to subsequent versions, but earlier versions won't
be affected.
(So far, everybody has been pretty good about this so no restrictions
have been necessary.)
.Q
Can I reuse parts of your source code?
.A
Yes.  Please be careful, though, to make sure that the code really is mine.
Some of the code was contributed by other people, and I don't have the
authority to give you permission to use it.
The author's name can be found near the top of each source file.
If it says "Steve Kirkendall" then you may use it;
otherwise, you'd better contact the author first.
.A
Please don't remove my name from the source code.
If you modify the source, please make a note of that fact in a comment
near the top of the source code.
And, finally, please mention my name in your documentation.
.Q
Can \*E work with non-ASCII files?
.A
\*E is 8-bit clean.
This means that \*E will allow you to edit files that use a European
extended ASCII character set.
However, some terminals are not 8-bit clean;
they treat characters in the range 0x80-0x9f as control characters.
\*E expects all characters above 0x7f to be treated as normal displayable
characters, so on these terminals \*E may produce a scrambled display.
.A
\*E can't edit binary files because it can't handle the NUL character,
and because of line-length limitations.
.A
\*E has also modified to work with 16-bit character sets, but that modification
is not part of the standard \*E distribution.
Yongguang Zhang (ygz@cs.purdue.edu) has created a Chinese version of \*E
that uses 16-bit characters and runs under cxterm (Chinese X-term)
on X-windows systems.
Junichiro Itoh (itojun@foretune.co.jp) has modified \*E to edit Japanese
text under MS-DOS.
