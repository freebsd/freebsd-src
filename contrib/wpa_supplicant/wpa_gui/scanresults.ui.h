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

void ScanResults::init()
{
    wpagui = NULL;
}


void ScanResults::destroy()
{
    delete timer;
}


void ScanResults::setWpaGui(WpaGui *_wpagui)
{
    wpagui = _wpagui;
    updateResults();
    
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), SLOT(getResults()));
    timer->start(10000, FALSE);
}


void ScanResults::updateResults()
{
    char reply[8192];
    size_t reply_len;
    
    if (wpagui == NULL)
	return;

    reply_len = sizeof(reply) - 1;
    if (wpagui->ctrlRequest("SCAN_RESULTS", reply, &reply_len) < 0)
	return;
    reply[reply_len] = '\0';

    scanResultsView->clear();
    
    QString res(reply);
    QStringList lines = QStringList::split(QChar('\n'), res);
    bool first = true;
    for (QStringList::Iterator it = lines.begin(); it != lines.end(); it++) {
	if (first) {
	    first = false;
	    continue;
	}
	
	QStringList cols = QStringList::split(QChar('\t'), *it, true);
	QString ssid, bssid, freq, signal, flags;
	bssid = cols.count() > 0 ? cols[0] : "";
	freq = cols.count() > 1 ? cols[1] : "";
	signal = cols.count() > 2 ? cols[2] : "";
	flags = cols.count() > 3 ? cols[3] : "";
	ssid = cols.count() > 4 ? cols[4] : "";
	new QListViewItem(scanResultsView, ssid, bssid, freq, signal, flags);
    }
}


void ScanResults::scanRequest()
{
    char reply[10];
    size_t reply_len = sizeof(reply);
    
    if (wpagui == NULL)
	return;
    
    wpagui->ctrlRequest("SCAN", reply, &reply_len);
}


void ScanResults::getResults()
{
    updateResults();
}




void ScanResults::bssSelected( QListViewItem * sel )
{
    NetworkConfig *nc = new NetworkConfig();
    if (nc == NULL)
	return;
    nc->setWpaGui(wpagui);
    nc->paramsFromScanResults(sel);
    nc->show();
    nc->exec();
 }
