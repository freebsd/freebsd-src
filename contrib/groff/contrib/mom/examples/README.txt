The files in this directory show mom in action.

If you have downloaded and untarrred a version of mom from her
homepage, you'll see that none of the example files come with
corresponding PostScript (.ps) files, as they do with pre-compiled
versions of groff, or groff built from source.

I haven't included the PostScript output because I want to
keep the mom archive as lean as possible.  To view the PostScript
output, process the files with groff and either

    a) send the output to a separate file for previewing with a
       PostScript viewer such as gv (ghostview), or

    b) to your printer.

Using the file sample_docs.mom as an example, you would
accomplish a) like this:

    groff -mom -Tps sample_docs.mom > sample_docs.ps
    gv sample_docs.ps

Accomplishing b) depends on your printer setup, but a fairly
standard way to do it would be

    groff -mom -Tps sample_docs.mom | lpr

                  or

    groff -mom -Tps -l sample_docs.mom

Note: I don't recommend previewing with gxditview because it doesn't
render some of mom's effects properly.

The files themselves
--------------------

All are set up for 8.5x11 inch paper (US letter).

***typesetting.mom**

The file, typesetting.mom, demonstrates the use of typesetting tabs,
string tabs, line padding, multi-columns and various indent styles,
as well as some of the refinements and fine-tuning available via
macros and inline escapes.

Because the file also demonstrates a "cutaround" using a small
picture (of everybody's favourite mascot, Tux), the PostScript file,
penguin.ps has been included in the directory.

***sample_docs.mom***

The file, sample_docs.mom, shows examples of three of the document
styles available with the mom's document processing macros, as well
as demonstrating the use of COLLATE.

The PRINTSTYLE of this file is TYPESET, to give you an idea of mom's
default behaviour when typesetting a document.

The last sample, set in 2 columns, shows off mom's flexibility
when it comes to designing documents.

If you'd like to see how mom handles exactly the same file when the
PRINTSTYLE is TYPEWRITE (i.e. typewritten, double-spaced), simply
change

    .PRINTSTYLE TYPESET

to

    .PRINTSTYLE TYPEWRITE

near the top of the file. 

***letter.mom***

This is just the tutorial example from the momdocs, ready for
previewing.

***elvis_syntax.new***

For those who use the vi clone, elvis, you can paste this file into
your elvis.syn.  Provided your mom documents have the extension
.mom, they'll come out with colorized syntax highlighting.  The
rules in elvis_syntax aren't exhaustive, but they go a LONG way to
making mom files more readable.

The file elvis_syntax (for pre-2.2h versions of elvis) is no longer
being maintained.  Users are encouraged to update to elvis 2.2h or
higher, and to use elvis_syntax.new for mom highlighting.

I'll be very happy if someone decides to send me syntax highlighting
rules for emacs. :)

***mom.vim***

Christian V. J. Brüssow has kindly contributed a set of mom syntax
highlighting rules for use with vim.  Copy the file to your
~/.vim/syntax directory, then, if your vim isn't already set up to
do so, enable mom syntax highlighting with

    :syntax enable

or

    :syntax on

Please note: I don't use vim, so I won't be making changes to this
file myself.  Christian Brüssow is the maintainer of the ruleset,
which is available on the Web at

    http://www.cvjb.de/comp/vim/mom.vim

Contact Christian (cvjb@cvjb.de) if you have any suggestions or
requests.
