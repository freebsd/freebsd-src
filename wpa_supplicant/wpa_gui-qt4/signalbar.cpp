/*
 * wpa_gui - SignalBar class
 * Copyright (c) 2011, Kel Modderman <kel@otaku42.de>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <cstdio>
#include <qapplication.h>

#include "signalbar.h"


SignalBar::SignalBar(QObject *parent)
	: QStyledItemDelegate(parent)
{
}


SignalBar::~SignalBar()
{
}


void SignalBar::paint(QPainter *painter,
		      const QStyleOptionViewItem &option,
		      const QModelIndex &index) const
{
	QStyleOptionProgressBar opts;
	int signal;

	if (index.column() != 3) {
		QStyledItemDelegate::paint(painter, option, index);
		return;
	}

	if (index.data().toInt() > 0)
		signal = 0 - (256 - index.data().toInt());
	else
		signal = index.data().toInt();

	opts.minimum = -95;
	opts.maximum = -35;
	if (signal < opts.minimum)
		opts.progress = opts.minimum;
	else if (signal > opts.maximum)
		opts.progress = opts.maximum;
	else
		opts.progress = signal;

	opts.text = QString::number(signal) + " dBm";
	opts.textVisible = true;
	opts.rect = option.rect;

	QApplication::style()->drawControl(QStyle::CE_ProgressBar,
					   &opts, painter);
}
