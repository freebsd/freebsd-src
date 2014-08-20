--------------------------------------------------------------------------------
  Usage Instructions for Monkey NetSurf                            13 March 2011
--------------------------------------------------------------------------------

  This document provides usage instructions for the Monkey version of 
  NetSurf.

  Monkey NetSurf has been tested on Ubuntu.

Overview
========

  What it is
  ----------

  The NetSurf Monkey front end is a developer debug tool used to
  test how the core interacts with the user interface.  It allows
  the developers to profile NetSurf and to interact with the core
  directly as though the developer were a front end.
 
  What it is not
  --------------

  Monkey is not a tool for building web-crawling robots or indeed
  anything other than a debug tool for the NetSurf developers.

  How to interact with nsmonkey
  -----------------------------

  In brief, monkey will produce tagged output on stdout and expect
  commands on stdin.  Windows are numbered and for the most part
  tokens are space separated.  In some cases (e.g. title or status)
  the final element on the output line is a string which might have
  spaces embedded within it.  As such, output from nsmonkey should be
  parsed a token at a time, so that when such a string is encountered,
  the parser can stop splitting and return the rest.

  Commands to Monkey are namespaced.  For example commands related to
  browser windows are prefixed by WINDOW.

  Top level tags for nsmonkey
  ---------------------------

    QUIT

    WINDOW

  Top level response tags for nsmonkey
  ------------------------------------

    GENERIC

    WARN, ERROR, DIE

    WINDOW

    DOWNLOAD_WINDOW

    SSLCERT

    401LOGIN

    PLOT

  In the below, %something% indicates a substitution made by Monkey.

    %url% will be a URL
    %id% will be an opaque ID
    %n% will be a number
    %bool% will be TRUE or FALSE
    %str% is a string and will only ever be at the end of an output line.

  Warnings, errors etc
  --------------------

  Warnings (tagged WARN) come from the NetSurf core.
  Errors (tagged ERROR) tend to come from Monkey's parsers
  Death (tagged DIE) comes from the core and kills Monkey dead.

Commands
========

  Generic commands
  ----------------

  QUIT
    Cause monkey to quit cleanly.
    This will cleanly destroy open windows etc.

  Window commands
  ---------------

  WINDOW NEW [%url%]
    Create a new browser window, optionally giving the core
    a URL to immediately navigate to.
    Minimally you will receive a WINDOW NEW WIN %id% response.

  WINDOW DESTROY %id%
    Destroy the given browser window.
    Minimally you will recieve a WINDOW DESTROY WIN %id% response.

  WINDOW GO %id% %url% [%url%]
    Cause the given browser window to visit the given URL.
    Optionally you can give a referrer URL to also use (simulating
    a click in the browser on a link).
    Minimally you can expect throbber, url etc responses.

  WINDOW REDRAW %id% [%num% %num% %num% %num%]
    Cause a browser window to redraw.  Optionally you can give a
    set of coordinates to simulate a partial expose of the window.
    Said coordinates are in traditional X0 Y0 X1 Y1 order.
    The coordinates are in canvas, not window, coordinates.  So you
    should take into account the scroll offsets when issuing this
    command.
    Minimally you can expect redraw start/stop messages and you
    can likely expect some number of PLOT results.

  WINDOW RELOAD %id%
    Cause a browser window to reload its current content.
    Expect responses similar to a GO command.


Responses
=========

  Generic messages
  ----------------

  GENERIC STARTED
    Monkey has started and is ready for commands

  GENERIC CLOSING_DOWN
    Monkey has been told to shut down and is doing so

  GENERIC FINISHED
    Monkey has finished and will now exit

  GENERIC LAUNCH URL %url%
    The core asked monkey to launch the given URL

  GENERIC THUMBNAIL URL %url%
    The core asked monkey to thumbnail a content without
    a window.

  GENERIC POLL BLOCKING
    Monkey reached a point where it could sleep waiting for
    commands or scheduled timeouts.  No fetches nor redraws
    were pending.

  Window messages
  ---------------

  WINDOW NEW WIN %id% FOR %id% CLONE %id% NEWTAB %bool%
    The core asked Monkey to open a new window.  The IDs for 'FOR' and
    'CLONE' are core window IDs, the WIN id is a Monkey window ID.

  WINDOW SIZE WIN %id% WIDTH %n% HEIGHT %n%
    The window specified has been set to the shown width and height.

  WINDOW DESTROY WIN %id%
    The core has instructed Monkey to destroy the named window.

  WINDOW TITLE WIN %id% STR %str%
    The core supplied a titlebar title for the given window.

  WINDOW REDRAW WIN %id%
    The core asked that Monkey redraw the given window.

  WINDOW GET_DIMENSIONS WIN %id% WIDTH %n% HEIGHT %n%
    The core asked Monkey what the dimensions of the window are.
    Monkey has to respond immediately and returned the supplied width
    and height values to the core.

  WINDOW NEW_CONTENT WIN %id%
    The core has informed Monkey that the named window has a new
    content object.

  WINDOW NEW_ICON WIN %id%
    The core has informed Monkey that the named window hsa a new
    icon (favicon) available.

  WINDOW START_THROBBER WIN %id%
    The core asked Monkey to start the throbber for the named
    window.  This indicates to the user that the window is busy.

  WINDOW STOP_THROBBER WIN %id%
    The core asked Monkey to stop the throbber for the named
    window.  This indicates to the user that the window is finished.

  WINDOW SET_SCROLL WIN %id% X %n% Y %n%
    The core asked Monkey to set the named window's scroll offsets
    to the given X and Y position.

  WINDOW UPDATE_BOX WIN %id% X %n% Y %n% WIDTH %n% HEIGHT %n%
    The core asked Monkey to redraw the given portion of the content
    display.  Note these coordinates refer to the content, not the
    viewport which Monkey is simulating.

  WINDOW UPDATE_EXTENT WIN %id% WIDTH %n% HEIGHT %n%
    The core has told us that the content in the given window has a
    total width and height as shown.  This allows us (along with the
    window's width and height) to know the scroll limits.
    
  WINDOW SET_STATUS WIN %id% STR %str%
    The core has told us that the given window needs its status bar
    updating with the given message.

  WINDOW SET_POINTER WIN %id% POINTER %id%
    The core has told us to update the mouse pointer for the given
    window to the given pointer ID.

  WINDOW SET_SCALE WIN %id% SCALE %n%
    The core has asked us to scale the given window by the given scale
    factor.

  WINDOW SET_URL WIN %id% URL %url%
    The core has informed us that the given window's URL bar needs
    updating to the given url.

  WINDOW GET_SCROLL WIN %id% X %n% Y %n%
    The core asked Monkey for the scroll offsets.  Monkey returned the
    numbers shown for the window named.

  WINDOW SCROLL_START WIN %id%
    The core asked Monkey to scroll the named window to the top/left.

  WINDOW POSITION_FRAME WIN %id% X0 %n% Y0 %n% X1 %n% Y1 %n%
    The core asked Monkey to position the named window as a frame at
    the given coordinates of its parent.

  WINDOW SCROLL_VISIBLE WIN %id% X0 %n% Y0 %n% X1 %n% Y1 %n%
    The core asked Monkey to scroll the named window until the
    indicated box is visible.

  WINDOW PLACE_CARET WIN %id% X %n% Y %n% HEIGHT %n% 
    The core asked Monkey to render a caret in the named window at the
    indicated position with the indicated height.

  WINDOW REMOVE_CARET WIN %id%
    The core asked Monkey to remove any caret in the named window.

  WINDOW SCROLL_START WIN %id% X0 %n% Y0 %n% X1 %n% Y1 %n%
    The core asked Monkey to scroll the named window to the start of
    the given box.

  WINDOW SELECT_MENU WIN %id%
    The core asked Monkey to produce a selection menu for the named
    window.

  WINDOW SAVE_LINK WIN %id% URL %url% TITLE %str%
    The core asked Monkey to save a link from the given window with
    the given URL and anchor title.

  WINDOW THUMBNAIL WIN %id% URL %url%
    The core asked Monkey to render a thumbnail for the given window
    which is currently at the given URL.

  WINDOW REDRAW WIN %id% START
  WINDOW REDRAW WIN %id% STOP
    The core wraps redraws in these messages.  Thus PLOT responses can
    be allocated to the appropriate window.

  Download window messages
  ------------------------

  DOWNLOAD_WINDOW CREATE DWIN %id% WIN %id%
    The core asked Monkey to create a download window owned by the
    given browser window.

  DOWNLOAD_WINDOW DATA DWIN %id% SIZE %n% DATA %str%
    The core asked Monkey to update the named download window with
    the given byte size and data string.

  DOWNLOAD_WINDOW ERROR DWIN %id% ERROR %str%
    The core asked Monkey to update the named download window with
    the given error message.

  DOWNLOAD_WINDOW DONE DWIN %id%
    The core asked Monkey to destroy the named download window.

  SSL Certificate messages
  ------------------------

  SSLCERT VERIFY CERT %id% URL %url%
    The core asked Monkey to say whether or not a given SSL
    certificate is OK.

  401 Login messages
  ------------------

  401LOGIN OPEN M4 %id% URL %url% REALM %str%
    The core asked Monkey to ask for identification for the named
    realm at the given URL.

  Plotter messages
  ----------------

  Note, Monkey won't clip coordinates, but sometimes the core does.

  PLOT CLIP X0 %n% Y0 %n% X1 %n% Y1 %n%
    The core asked Monkey to clip plotting to the given clipping
    rectangle (X0,Y0) (X1,Y1)

  PLOT TEXT X %n% Y %n% STR %str%
    The core asked Monkey to plot the given string at the
    given coordinates.

  PLOT LINE X0 %n% Y0 %n% X1 %n% Y1 %n%
    The core asked Monkey to plot a line with the given start
    and end coordinates.

  PLOT RECT X0 %n% Y0 %n% X1 %n% Y1 %n%
    The core asked Monkey to plot a rectangle with the given
    coordinates as the corners.

  PLOT BITMAP X %n% Y %n% WIDTH %n% HEIGHT %n%
    The core asked Monkey to plot a bitmap at the given
    coordinates, scaled to the given width/height.
