# BSDDialog 1.0

This project provides **bsddialog** and **libbsddialog**, an utility
and a library to build scripts and tools with TUI dialogs and widgets.


## Demo

[Screenshots](https://www.flickr.com/photos/alfonsosiciliano/albums/72157720215006074).


## Getting Started

FreeBSD and Linux:

```
% git clone https://gitlab.com/alfix/bsddialog.git
% cd bsddialog
% make
% ./bsddialog --msgbox "Hello World!" 8 20
```

Output:

![screenshot](screenshot.png)


## Utility

**Dialogs:**

--calendar, --checklist, --datebox, --form, --gauge, --infobox, --inputbox,
--menu, --mixedform, --mixedgauge, --msgbox, --passwordbox, --passwordform,
--pause, --radiolist, --rangebox, --textbox, --timebox, --treeview, --yesno.

**Manual**

 - [bsddialog(1)](https://alfonsosiciliano.gitlab.io/posts/2022-01-26-manual-bsddialog.html)


**Examples**:

```
% ./bsddialog --backtitle "TITLE" --title msgbox --msgbox "Hello World!" 5 30
% ./bsddialog --theme blackwhite --title msgbox --msgbox "Hello World!" 5 30
% ./bsddialog --begin-y 2 --default-no --title yesno --yesno "Hello World!" 5 30
% ./bsddialog --ascii-lines --pause "Hello World!" 8 50 10
% ./bsddialog --checklist "Space to select" 0 0 0 Name1 Desc1 off Name2 Desc2 on
% ./bsddialog --title yesno --hline "bsddialog" --yesno "Hello World!" 5 25
% ./bsddialog --extra-button --help-button --yesno "Hello World!" 0 0
```

and [Examples](https://gitlab.com/alfix/bsddialog/-/tree/main/examples_utility)
in the _Public Domain_ to build new projects:
```
% sh ./examples_utility/calendar.sh
% sh ./examples_utility/checklist.sh
% sh ./examples_utility/datebox.sh
% sh ./examples_utility/form.sh
% sh ./examples_utility/gauge.sh
% sh ./examples_utility/infobox.sh
% sh ./examples_utility/inputbox.sh
% sh ./examples_utility/menu.sh
% sh ./examples_utility/mixedform.sh
% sh ./examples_utility/mixedgauge.sh
% sh ./examples_utility/msgbox.sh
% sh ./examples_utility/passwordbox.sh
% sh ./examples_utility/passwordform.sh
% sh ./examples_utility/pause.sh
% sh ./examples_utility/radiolist.sh
% sh ./examples_utility/rangebox.sh
% sh ./examples_utility/timebox.sh
% sh ./examples_utility/yesno.sh
```

## Library

**API**

 - [bsddialog.h](https://gitlab.com/alfix/bsddialog/-/blob/main/lib/bsddialog.h)
 - [bsddialog\_theme.h](https://gitlab.com/alfix/bsddialog/-/blob/main/lib/bsddialog_theme.h)


**Manual**

 - [bsddialog(3)](https://alfonsosiciliano.gitlab.io/posts/2022-01-15-manual-libbsddialog.html)


**Examples**:

[Examples](https://gitlab.com/alfix/bsddialog/-/tree/main/examples_library)
in the _Public Domain_ to build new projects:
```
% cd examples_library
% sh compile
% ./calendar
% ./checklist
% ./datebox
% ./form
% ./gauge
% ./infobox
% ./menu
% ./mixedgauge
% ./mixedlist
% ./msgbox
% ./pause
% ./radiolist
% ./rangebox
% ./theme
% ./timebox
% ./yesno
```


## TODO and Ideas

 - menubar feature
 - key callback
 - Right-To-Left text
 - some terminal does not hide the cursor, move it bottom-right before to getch.
 - refactor backtitle: multiline, conf.backtitle, WINDOW \*dialog.backtitle.
 - refactor bottomdesc: WINDOW \*dialog.bottomdesc -> fix expandig screen.
 - accessibility https://wiki.freebsd.org/Accessibility/Wishlist/Base
 - add bool conf.menu.depthlines.
 - implement custom getopt\_long().
 - refactor/redesign gauge().
 - improve grey lines expanding terminal (maybe redrawwin() in hide\_dialog()).
 - more restrictive strtol() and strtoul().
 - implement global buttons handler.
 - add/move external tutorial.
 - implement menutype.min_on.
