// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "TrayIcon.h"

#include "../ClientUser.h"
#include "../MainWindow.h"
#include "../Global.h"

#include <QApplication>

TrayIcon::TrayIcon() : QSystemTrayIcon(Global::get().mw), m_statusIcon(Global::get().mw->qiIcon) {
	setIcon(m_statusIcon);

	setToolTip("Mumble");

	assert(Global::get().mw);

	QObject::connect(Global::get().mw, &MainWindow::talkingStatusChanged, this, &TrayIcon::on_icon_update);
	QObject::connect(Global::get().mw, &MainWindow::disconnectedFromServer, this, &TrayIcon::on_icon_update);

	QObject::connect(this, &QSystemTrayIcon::activated, this, &TrayIcon::on_icon_clicked);

	// messageClicked is buggy in Qt on some platforms and we can not do anything about this (QTBUG-87329)
	QObject::connect(this, &QSystemTrayIcon::messageClicked, this, &TrayIcon::on_showAction_triggered);

	m_showAction = new QAction(tr("Show"), Global::get().mw);
	QObject::connect(m_showAction, &QAction::triggered, this, &TrayIcon::on_showAction_triggered);

	m_hideAction = new QAction(tr("Hide"), Global::get().mw);
	QObject::connect(m_hideAction, &QAction::triggered, this, &TrayIcon::on_hideAction_triggered);

	QObject::connect(Global::get().mw->qaTalkingUIToggle, &QAction::triggered, this, &TrayIcon::updateContextMenu);

	m_contextMenu = new QMenu(Global::get().mw);
	QObject::connect(m_contextMenu, &QMenu::aboutToShow, this, &TrayIcon::updateContextMenu);

	// Some window managers hate it when a tray icon sets an empty context menu...
	updateContextMenu();

	setContextMenu(m_contextMenu);

	show();
}

void TrayIcon::on_icon_update() {
	std::reference_wrapper< QIcon > newIcon = Global::get().mw->qiIcon;

	ClientUser *p = ClientUser::get(Global::get().uiSession);

	if (Global::get().s.bDeaf) {
		newIcon = Global::get().mw->qiIconDeafSelf;
	} else if (p && p->bDeaf) {
		newIcon = Global::get().mw->qiIconDeafServer;
	} else if (Global::get().s.bMute) {
		newIcon = Global::get().mw->qiIconMuteSelf;
	} else if (p && p->bMute) {
		newIcon = Global::get().mw->qiIconMuteServer;
	} else if (p && p->bSuppress) {
		newIcon = Global::get().mw->qiIconMuteSuppressed;
	} else if (Global::get().s.bStateInTray && Global::get().bPushToMute) {
		newIcon = Global::get().mw->qiIconMutePushToMute;
	} else if (p && Global::get().s.bStateInTray) {
		switch (p->tsState) {
			case Settings::Talking:
			case Settings::MutedTalking:
				newIcon = Global::get().mw->qiTalkingOn;
				break;
			case Settings::Whispering:
				newIcon = Global::get().mw->qiTalkingWhisper;
				break;
			case Settings::Shouting:
				newIcon = Global::get().mw->qiTalkingShout;
				break;
			case Settings::Passive:
			default:
				newIcon = Global::get().mw->qiTalkingOff;
				break;
		}
	}

	if (&newIcon.get() != &m_statusIcon.get()) {
		m_statusIcon = newIcon;
		setIcon(m_statusIcon);
	}
}

void TrayIcon::on_icon_clicked(QSystemTrayIcon::ActivationReason reason) {
	switch (reason) {
		case QSystemTrayIcon::Trigger:
#ifndef Q_OS_MAC
			// macOS is special as it both shows the context menu AND triggers the action.
			// We only want at most one of those and since we can not prevent showing
			// the menu, we skip the action.
			toggleShowHide();
#endif
			break;
		case QSystemTrayIcon::Unknown:
		case QSystemTrayIcon::Context:
		case QSystemTrayIcon::DoubleClick:
		case QSystemTrayIcon::MiddleClick:
			break;
	}
}

void TrayIcon::updateContextMenu() {
	m_contextMenu->clear();

	if (Global::get().mw->isVisible() && !Global::get().mw->isMinimized()) {
		m_hideAction->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());
		m_contextMenu->addAction(m_hideAction);
	} else {
		m_contextMenu->addAction(m_showAction);
	}

	m_contextMenu->addSeparator();

	m_contextMenu->addAction(Global::get().mw->qaAudioMute);
	m_contextMenu->addAction(Global::get().mw->qaAudioDeaf);
	m_contextMenu->addAction(Global::get().mw->qaTalkingUIToggle);
	m_contextMenu->addSeparator();
	m_contextMenu->addAction(Global::get().mw->qaQuit);
}

void TrayIcon::toggleShowHide() {
	if (Global::get().mw->isVisible() && !Global::get().mw->isMinimized()) {
		on_hideAction_triggered();
	} else {
		on_showAction_triggered();
	}
}

void TrayIcon::on_showAction_triggered() {
	Global::get().mw->showRaiseWindow();
	updateContextMenu();
}

void TrayIcon::on_hideAction_triggered() {
	if (!QSystemTrayIcon::isSystemTrayAvailable()) {
		// The system reports that no system tray is available.
		// If we would hide Mumble now, there would be no way to
		// get it back...
		return;
	}

	if (qApp->activeModalWidget() || qApp->activePopupWidget()) {
		// There is one or multiple modal or popup window(s) active, which
		// would not be hidden by this call. So we also do not hide
		// the MainWindow...
		return;
	}

#ifndef Q_OS_MAC
	Global::get().mw->hide();
#else
	// Qt can not hide the window via the native macOS hide function. This should be re-evaluated with new Qt versions.
	// Instead we just minimize.
	Global::get().mw->setWindowState(Global::get().mw->windowState() | Qt::WindowMinimized);
#endif

	updateContextMenu();
}
