/*
 * wpa_gui - SignalBar class
 * Copyright (c) 2011, Kel Modderman <kel@otaku42.de>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef SIGNALBAR_H
#define SIGNALBAR_H

#include <QObject>
#include <QStyledItemDelegate>

class SignalBar : public QStyledItemDelegate
{
	Q_OBJECT

public:
	SignalBar(QObject *parent = 0);
	~SignalBar();

	virtual void paint(QPainter *painter,
			   const QStyleOptionViewItem &option,
			   const QModelIndex &index) const ;
};

#endif /* SIGNALBAR_H */
