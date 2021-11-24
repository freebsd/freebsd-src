# BSDDialog

**Work In Progress!**

This project provides **bsddialog** and **libbsddialog**, an utility and a
library to build scripts and tools with *TUI Widgets*.

Description:
<https://www.freebsd.org/status/report-2021-04-2021-06/#_bsddialog_tui_widgets>


## Getting Started

FreeBSD:

```
% git clone https://gitlab.com/alfix/bsddialog.git
% cd bsddialog
% make
% ./bsddialog --msgbox "Hello World!" 8 20
```

If you are using XFCE install 
[devel/ncurses](https://www.freshports.org/devel/ncurses/)

```
% sudo pkg install ncurses
% git clone https://gitlab.com/alfix/bsddialog.git
% cd bsddialog
% make -DPORTNCURSES
% ./bsddialog --msgbox "Hello World!" 8 20
```

Linux:

```
% git clone https://gitlab.com/alfix/bsddialog.git
% cd bsddialog
% make -GNUMakefile
% ./bsddialog --msgbox "Hello World!" 8 20
```

Output:

![screenshot](screenshot.png)


Examples utility:
```
% ./bsddialog --title msgbox --msgbox "Hello World!" 5 30
% ./bsddialog --theme default --title msgbox --msgbox "Hello World!" 5 30
% ./bsddialog --begin-y 2 --title yesno --yesno "Hello World!" 5 30
% ./bsddialog --ascii-lines --pause "Hello World!" 8 50 5
% ./bsddialog --checklist "Space to select" 0 0 0 Name1 Desc1 off Name2 Desc2 on Name3 Desc3 off
% ./bsddialog --backtitle "TITLE" --title yesno --hline "bsddialog" --yesno "Hello World!" 5 25
% ./bsddialog --extra-button --help-button --defaultno --yesno "Hello World!" 0 0
```

Examples library:
```
% cd library_examples
% sh compile
% ./buildlist
% ./infobox
% ./menu
% ./mixedlist
% ./msgbox
% ./ports
% ./radiolist
% ./theme
% ./treeview
% ./yesno
```

Use Cases:

 - [portconfig](https://gitlab.com/alfix/portconfig)


## Features

**Common Options:**
 
--ascii-lines, --aspect *ratio* (for infobox, msgbox and yesno),
--backtitle *backtitle*, --begin-x *x* (--begin *y y*),
(--begin *y x*), --cancel-label *string*, -clear (test with multiple widgets),
--colors, --date-format *format*, --default-button *string*, --defaultno,
--default-item *string*, 
--exit-label *string*, --extra-button, --extra-label *string*,
--hfile *filename* (for completed widgets), n--help-button,
--help-label *string*, --help-status, --help-tags, --hline *string*, --ignore,
--item-help, --no-cancel, --nocancel, --no-label *string*, --no-items,
--no-lines, --no-ok,
--nook, --no-shadow, --no-tags, --ok-label *string*, --output-fd *fd*,
--output-separator *string*, --print-version,
--print-size (todo move lib -> utility), --quoted (quotes all != dialog),
--print-maxsize, --shadow, --single-quoted (add --quote-with *ch*?), 
--separator *string* (alias --output-separator *string*),
--separate-output (rename --separate-output-withnl?), --sleep *secs*, --stderr,
--stdout, --theme *string* ("bsddialog", "dialog", "blackwhite" and "magenta"),
--time-format *format*, --title *title*, --version, --yes-label *string*.

**Widgets:**
 
 infobox (do not clear the screen), msgbox,
 yesno (dialog renames "yes/no" -> "ok/cancel" with --extra-button --help-button).
 checklist, radiolist, menu, mixedlist, treeview and textbox.

## TODO

**Common Options:**

|  Option                      | Status      | Note                            |
| ---------------------------- | ----------- | ------------------------------- |
| --cr-wrap                    | Coding      |                                 |
| --help                       | In progress |                                 |
| --input-fd *fd*              |             |                                 |
| --insecure                   |             |                                 |
| --keep-tite                  |             |                                 |
| --keep-window                |             |                                 |
| --last-key                   |             |                                 |
| --max-input *size*           |             |                                 |
| --no-collapse                | Coding      |                                 |
| --no-kill                    |             |                                 |
| --no-nl-expand               | Coding      |                                 |
| --tab-correct                |             |                                 |
| --tab-len *n*                |             |                                 |
| --trim                       | Coding      |                                 |


To evaluate / Not planned in the short term: --column-separator *string*,
--create-rc *file*, --iso-week, --no-mouse, --print-text-only *str h w*,
--print-text-size *str h w*, --reorder, -scrollbar, --separate-widget *string*,
--size-err, --timeout *secs*,--trace *filename*, --visit-items,
--week-start *day*.


**Widgets:**

| Widget         | Status      | Note                                          |
|--------------- | ----------- | ----------------------------------------------|
| --buildlist    | In progress | todo autosize, resize, F1                     |
| --calendar     | In progress | todo autosize, resize, F1, leap year, year <=0, month days |
| --editbox      |             |                                               |
| --form         | In progress | implemented via --mixedform                   |
| --gauge        | In progress |                                               |
| --inputbox     | In progress | implemented via --mixedform, todo \<init\>    |
| --mixedform    | In progress | todo autosize, resize, F1                     |
| --mixedgauge   | In progress | todo autosize, resize, F1                     |
| --passwordbox  | In progress | implemented via --mixedform, todo \<init\>    |
| --passwordform | In progress | implemented via --mixedform                   |
| --pause        | In progress | todo autosize, resize, F1                     |
| --prgbox       | In progress | add command opts                              |
| --programbox   | Coding      |                                               |
| --progressbox  |             |                                               |
| --rangebox     | In progress | todo autosize, resize, F1, PAGE-UP/PAGE-DOWN/HOME/END keys |
| --timebox      | In progress | todo autosize, resize, F1                     |

To evaluate / Not planned in the short term: tailbox (textbox/fseek), tailboxbg,
dselect, fselect, inputmenu.
