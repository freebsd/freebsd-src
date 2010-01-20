rlfe (ReadLine Front-End) is a "universal wrapper" around readline.
You specify an interactive program to run (typically a shell), and
readline is used to edit input lines.

There are other such front-ends; what distinguishes this one is that
it monitors the state of the inferior pty, and if the inferior program
switches its terminal to raw mode, then rlfe passes your characters
through directly.  This basically means you can run your entire
session (including bash and terminal-mode emacs) under rlfe.

FEATURES

* Can use all readline commands (and history) in commands that
read input lines in "canonical mode" - even 'cat'!

* Automatically switches between "readline-editing mode" and "raw mode"
depending on the terminal mode.  If the inferior program invokes
readline itself, it will do its own line editing.  (The inferior
readline will not know about rlfe, and it will have its own history.)
You can even run programs like 'emavs -nw' and 'vi' under rlfe.
The goal is you could leave rlfe always on without even knowing
about it.  (We're not quite there, but it works tolerably well.)

* The input line (after any prompt) is changed to bold-face.

INSTALL

The usual: ./configure && make && make install

Note so far rlfe has only been tested on GNU Linux (Fedora Core 2)
and Mac OS X (10.3).

This assumes readline header files and libraries are in the default
places.  If not, you can create a link named readline pointing to the
readline sources.  To link with libreadline.a and libhistory.a
you can copy or link them, or add LDFLAGS='-/path/to/readline' to
the make command-line.

USAGE

Just run it.  That by default runs bash.  You can run some other
command by giving it as command-line arguments.

There are a few tweaks:  -h allows you to name the history file,
and -s allows you to specify its size.  It default to "emacs" mode,
but if the the environment variable EDITOR is set to "vi" that
mode is chosen.

ISSUES

* The mode switching depends on the terminal mode set by the inferior
program.  Thus ssh/telnet/screen-type programs will typically be in
raw mode, so rlfe won't be much use, even if remote programs run in
canonical mode.  The work-around is to run rlfe on the remote end.

* Echo supression and prompt recognition are somewhat fragile.
(A protocol so that the o/s tty code can reliably communicate its
state to rlfe could solve this problem, and the previous one.)

* See the intro to rlfe.c for more notes.

* Assumes a VT100-compatible terminal, though that could be generalized
if anybody cares.

* Requires ncurses.

* It would be useful to integrate rlfe's logic in a terminal emulator.
That would make it easier to reposition the edit position with a mouse,
integrate cut-and-paste with the system clipboard, and more robustly
handle escape sequence and multi-byte characters more robustly.

AUTHOR

Per Bothner <per@bothner.com>

LICENSE

GPL.
