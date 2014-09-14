/* Copyright (C) 2005-2011, Thorvald Natvig <thorvald@natvig.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef MUMBLE_MUMBLE_AUDIOSTATS_H_
#define MUMBLE_MUMBLE_AUDIOSTATS_H_

#include <QtCore/QtGlobal>
#include <QtCore/QList>
#include <QtCore/QTimer>
#if QT_VERSION >= 0x050000
# include <QtWidgets/QWidget>
#else
# include <QtGui/QWidget>
#endif

class AudioBar : public QWidget {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(AudioBar)
	protected:
		void paintEvent(QPaintEvent *event);
	public:
		AudioBar(QWidget *parent = NULL);
		int iMin, iMax;
		int iBelow, iAbove;
		int iValue, iPeak;
		QColor qcBelow, qcInside, qcAbove;

		QList<QColor> qlReplacableColors;
		QList<Qt::BrushStyle> qlReplacementBrushes;
};

class AudioEchoWidget : public QWidget {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(AudioEchoWidget)
	public:
		AudioEchoWidget(QWidget *parent);
	protected slots:
		void paintEvent(QPaintEvent *event);
};

class AudioNoiseWidget : public QWidget {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(AudioNoiseWidget)
	public:
		AudioNoiseWidget(QWidget *parent);
	protected slots:
		void paintEvent(QPaintEvent *event);
};

#include "ui_AudioStats.h"

class AudioStats : public QDialog, public Ui::AudioStats {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(AudioStats)
	protected:
		QTimer *qtTick;
		bool bTalking;
	public:
		AudioStats(QWidget *parent);
		~AudioStats();
	public slots:
		void on_Tick_timeout();
};

#else
class AudioStats;
#endif
