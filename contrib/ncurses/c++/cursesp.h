// * This makes emacs happy -*-Mode: C++;-*-
#ifndef _CURSESP_H
#define _CURSESP_H

// $Id: cursesp.h,v 1.11 1999/07/31 09:46:05 juergen Exp $

#include <cursesw.h>

extern "C" {
#  include <panel.h>
}

class NCursesPanel : public NCursesWindow {
protected:
  PANEL *p;
  static NCursesPanel *dummy;

private:
  // This structure is used for the panel's user data field to link the
  // PANEL* to the C++ object and to provide extra space for a user pointer.
  typedef struct {
    void*               m_user;      // the pointer for the user's data
    const NCursesPanel* m_back;      // backward pointer to C++ object
    const PANEL*        m_owner;     // the panel itself
  } UserHook;

  void init();                       // Initialize the panel object

protected:
  void set_user(void *user) {
    UserHook* uptr = (UserHook*)::panel_userptr (p);
    assert (uptr && uptr->m_back==this && uptr->m_owner==p);
    uptr->m_user = user;
  }
  // Set the user pointer of the panel.
  
  void *get_user() {
    UserHook* uptr = (UserHook*)::panel_userptr (p);
    assert (uptr && uptr->m_back==this && uptr->m_owner==p);
    return uptr->m_user;
  }
  
  void OnError (int err) const THROWS((NCursesPanelException)) {
    if (err==ERR)
      THROW(new NCursesPanelException (this, err));
  }
  // If err is equal to the curses error indicator ERR, an error handler
  // is called.

public:
  NCursesPanel(int lines,
	       int cols,
	       int begin_y = 0,
	       int begin_x = 0)
    : NCursesWindow(lines,cols,begin_y,begin_x) {
      init();
  } 
  // Create a panel with this size starting at the requested position.

  NCursesPanel() : NCursesWindow(::stdscr) { init(); }
  // This constructor creates the default Panel associated with the
  // ::stdscr window

  virtual ~NCursesPanel();
  
  // basic manipulation
  inline void hide() {
    OnError (::hide_panel(p));
  }
  // Hide the panel. It stays in the stack but becomes invisible.

  inline void show() {
    OnError (::show_panel(p));
  }
  // Show the panel, i.e. make it visible.

  inline void top() {
    OnError (::top_panel(p));
  }
  // Make this panel the top panel in the stack.
  
  inline void bottom() {
    OnError (::bottom_panel(p));
  }
  // Make this panel the bottom panel in the stack.
  // N.B.: The panel associated with ::stdscr is always on the bottom. So
  // actually bottom() makes the panel the first above ::stdscr.
  
  virtual int mvwin(int y, int x) {
    OnError(::move_panel(p, y, x));
    return OK;
  }
  
  inline bool hidden() const {
    return (::panel_hidden (p) ? TRUE : FALSE);
  }
  // Return TRUE if the panel is hidden, FALSE otherwise.

/* The functions panel_above() and panel_below() are not reflected in
   the NCursesPanel class. The reason for this is, that we cannot
   assume that a panel retrieved by those operations is one wrapped
   by a C++ class. Although this situation might be handled, we also
   need a reverse mapping from PANEL to NCursesPanel which needs some
   redesign of the low level stuff. At the moment, we define them in the
   interface but they will always produce an error. */
  inline NCursesPanel& above() const {
    OnError(ERR);
    return *dummy;
  }

  inline NCursesPanel& below() const {
    OnError(ERR);
    return *dummy;
  }

  // Those two are rewrites of the corresponding virtual members of
  // NCursesWindow
  virtual int refresh();
  // Propagate all panel changes to the virtual screen and update the 
  // physical screen.

  virtual int noutrefresh();
  // Propagate all panel changes to the virtual screen.

  static void redraw();
  // Redraw all panels.
 
  // decorations
  virtual void frame(const char* title=NULL, 
		     const char* btitle=NULL);
  // Put a frame around the panel and put the title centered in the top line
  // and btitle in the bottom line.

  virtual void boldframe(const char* title=NULL,
			 const char* btitle=NULL);
  // Same as frame(), but use highlighted attributes.

  virtual void label(const char* topLabel,
		     const char* bottomLabel);
  // Put the title centered in the top line and btitle in the bottom line.

  virtual void centertext(int row,const char* label);
  // Put the label text centered in the specified row.
};

/* We use templates to provide a typesafe mechanism to associate
 * user data with a panel. A NCursesUserPanel<T> is a panel 
 * associated with some user data of type T.
 */
template<class T> class NCursesUserPanel : public NCursesPanel
{
public:
  NCursesUserPanel (int lines,
		    int cols,
		    int begin_y = 0,
		    int begin_x = 0,
		    const T* p_UserData = (T*)0)
    : NCursesPanel (lines, cols, begin_y, begin_x) {
      if (p)
	set_user ((void *)p_UserData);
  };
  // This creates an user panel of the requested size with associated
  // user data pointed to by p_UserData.
  
  NCursesUserPanel(const T* p_UserData = (T*)0) : NCursesPanel() {
    if (p)
      set_user((void *)p_UserData);
  };
  // This creates an user panel associated with the ::stdscr and user data
  // pointed to by p_UserData.

  virtual ~NCursesUserPanel() {};

  T* UserData (void) const {
    return (T*)get_user ();
  };
  // Retrieve the user data associated with the panel.
  
  virtual void setUserData (const T* p_UserData) {
    if (p)
      set_user ((void *)p_UserData);
  }
  // Associate the user panel with the user data pointed to by p_UserData.
};

#endif // _CURSESP_H
