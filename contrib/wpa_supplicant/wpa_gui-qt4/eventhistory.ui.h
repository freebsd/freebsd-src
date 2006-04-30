/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

void EventHistory::init()
{
}


void EventHistory::destroy()
{
}


void EventHistory::addEvents(WpaMsgList msgs)
{
    WpaMsgList::iterator it;
    for (it = msgs.begin(); it != msgs.end(); it++) {
	addEvent(*it);
    }
}


void EventHistory::addEvent(WpaMsg msg)
{
    Q3ListViewItem *item;
    item = new Q3ListViewItem(eventListView,
			     msg.getTimestamp().toString("yyyy-MM-dd hh:mm:ss.zzz"),
			     msg.getMsg());
    if (item == NULL)
	return;
    eventListView->setSelected(item, false);
}
