   -----------------------------------------
      FreeBSD 2.2.8  ---  RELEASE Version              ,        ,
   -----------------------------------------          /(        )`
                                                      \ \___   / |
Welcome to the 2.2.8-RELEASE version of FreeBSD!      /- _  `-/  '
                                                     (/\/ \ \   /\
This is a full release to follow 2.2.7-RELEASE,      / /   | `    \
which was released in July, 1998.  Many bugs         O O   ) /    |
were fixed and the general stability of the system   `-^--'`<     '
enhanced.  For more information on these changes,   (_.)  _  )   /
please see the release notes (RELNOTES.TXT).         `.___/`    /
                                                       `-----' /
Feedback or offers to help with anything  <----.     __ / __   \
you see in this release are most welcome  <----|====O)))==) \) /====
and should be sent to one of the approp-  <----'    `--' `.__,' \
riate mailing lists - please see the                   |        |
ABOUT.TXT file for more information.                    \       /       /\
                                                   ______( (_  / \______/
                                                 ,'  ,-----'   |
                                                 `--{__________)


NOTE: Unless circumstances dictate otherwise, this is the *last* release
      on the RELENG_2_2 branch.  As of 2.2.8-RELEASE, the 2.2-stable
      branch is in maintenance mode only.  This means that certain critical
      patches and security fixes may still go into the 2.2-stable tree from
      time to time, but people are encouraged to transition to 3.0-stable
      just as soon as it's created (est: Jan 1999) if they want to track
      a new and "living" -stable branch.  The 3.0-current branch has not
      yet been forked into the 3.1-current and 3.0-stable branches due to
      the fact that we're giving feedback for 3.0-RELEASE more time to
      come back before doing so.  This will result in less post-branch
      work for the release engineers who would otherwise have to merge a
      lot more changes from the very outset of the 3.0-stable branch.


            Most files here are also in the Documentation
                    Menu of the boot floppy

ROAD MAP:

  README.TXT    This file

  ABOUT.TXT     All about FreeBSD, including contact information

  RELNOTES.TXT  Release Notes - what's new & different in this
                release

  LAYOUT.TXT    Information about the layout of the release
                directory. If you are installing from floppies,
                it is especially important that you
                *read this section!*

  ERRATA.TXT    Any late-breaking errata information for this
                release.  On FTP sites, this file may be frequently
                updated so it's a good idea to check it first
                (the master copy is always on ftp.freebsd.org)
                before reporting problems

  HARDWARE.TXT  Information about the configuration of the
                GENERIC kernel and supported hardware

  INSTALL.TXT   How to make a new installation of FreeBSD
                on your PC using the data you see here

  UPGRADE.TXT   How to upgrade an existing FreeBSD
                installation

  TROUBLE.TXT   Troubleshooting information


o For new installation instructions, see the INSTALL.TXT
  and HARDWARE.TXT files.

o If you are upgrading from a previous FreeBSD version,
  please take a look at UPGRADE.TXT.

o It is also important to check the ERRATA.TXT file for any
  late-breaking issues with this release. This file contains
  the latest information on upgrade, security or other problems
  which an administrator should be aware of.

o Information regarding problems which arise after the CD
  is printed can be found at:
  ftp://ftp.freebsd.org/pub/FreeBSD/2.2.8-RELEASE/ERRATA.TXT

For the latest 3.0-Current (HEAD branch) snapshot releases, please
install from:

        ftp://current.freebsd.org/pub/FreeBSD

If you plan to run either -Stable or -Current you MUST be subscribed
to the appropriate mailing list, either freebsd-stable or
freebsd-current. For information on subscribing to either list (or
both), send an e-mail to majordomo@freebsd.org with the following in
the body, not the subject of the letter as appropriate:

subscribe freebsd-stable
subscribe freebsd-current
end

We hope you enjoy using FreeBSD as much as we enjoyed creating it!

- The FreeBSD Project
