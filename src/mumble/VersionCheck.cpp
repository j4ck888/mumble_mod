// Copyright 2005-2020 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "VersionCheck.h"
#include "MainWindow.h"
#include "Utils.h"
#ifndef USE_APPIMAGE_UPDATER_BRIDGE
# include "WebFetch.h"
#else
# include <AppImageUpdaterBridge>
# include <AppImageUpdaterDialog>
# include <QPixmap>
using AppImageUpdaterBridge::AppImageDeltaRevisioner;
using AppImageUpdaterBridge::AppImageUpdaterDialog;
// We can avoid checking if this is a windows 
// machine since appimage is only for linux machines
#endif // USE_APPIMAGE_UPDATER_BRIDGE

#ifdef Q_OS_WIN
# include "win.h"
#endif

#include <QtCore/QUrlQuery>
#include <QtXml/QDomDocument>
#include <QtWidgets/QMessageBox>

#ifdef Q_OS_WIN
# include <shellapi.h>
# include <softpub.h>
#endif

// We define a global macro called 'g'. This can lead to issues when included code uses 'g' as a type or parameter name (like protobuf 3.7 does). As such, for now, we have to make this our last include.
#include "Global.h"

VersionCheck::VersionCheck(bool autocheck, QObject *p, bool focus) : QObject(p) {
#ifndef USE_APPIMAGE_UPDATER_BRIDGE
	QUrl url;
	url.setPath(focus ? QLatin1String("/v1/banner") : QLatin1String("/v1/version-check"));

	QList<QPair<QString, QString> > queryItems;
	queryItems << qMakePair(QString::fromLatin1("ver"), QString::fromLatin1(QUrl::toPercentEncoding(QLatin1String(MUMBLE_RELEASE))));
#if defined(Q_OS_WIN)
# if defined(Q_OS_WIN64)
	queryItems << qMakePair(QString::fromLatin1("os"), QString::fromLatin1("WinX64"));
# else
	queryItems << qMakePair(QString::fromLatin1("os"), QString::fromLatin1("Win32"));
# endif
#elif defined(Q_OS_MAC)
# if defined(USE_MAC_UNIVERSAL)
	queryItems << qMakePair(QString::fromLatin1("os"), QString::fromLatin1("MacOSX-Universal"));
# else
	queryItems << qMakePair(QString::fromLatin1("os"), QString::fromLatin1("MacOSX"));
# endif
#else
	queryItems << qMakePair(QString::fromLatin1("os"), QString::fromLatin1("Unix"));
#endif
	if (! g.s.bUsage)
		queryItems << qMakePair(QString::fromLatin1("nousage"), QString::fromLatin1("1"));
	if (autocheck)
		queryItems << qMakePair(QString::fromLatin1("auto"), QString::fromLatin1("1"));

	queryItems << qMakePair(QString::fromLatin1("locale"), g.s.qsLanguage.isEmpty() ? QLocale::system().name() : g.s.qsLanguage);

	QFile f(qApp->applicationFilePath());
	if (! f.open(QIODevice::ReadOnly)) {
		qWarning("VersionCheck: Failed to open binary");
	} else {
		QByteArray a = f.readAll();
		if (a.size() < 1) {
			qWarning("VersionCheck: suspiciously small binary");
		} else {
			QCryptographicHash qch(QCryptographicHash::Sha1);
			qch.addData(a);
			queryItems << qMakePair(QString::fromLatin1("sha1"), QString::fromLatin1(qch.result().toHex()));
		}
	}

	QUrlQuery query;
	query.setQueryItems(queryItems);
	url.setQuery(query);

	WebFetch::fetch(QLatin1String("update"), url, this, SLOT(fetched(QByteArray,QUrl)));
#else // When USE_APPIMAGE_UPDATER_BRIDGE is defined
// USE_APPIMAGE_UPDATER_BRIDGE implies that we are on a linux machine 
// so we are going to assume that.

	m_UpdaterDialog = nullptr;
	m_Revisioner = new AppImageDeltaRevisioner(/*singleThreaded=*/true,/*parent=*/this);
	
	connect(m_Revisioner, &AppImageDeltaRevisioner::updateAvailable,
		this, &VersionCheck::handleUpdateCheck);
	connect(m_Revisioner, &AppImageDeltaRevisioner::error,
		this, &VersionCheck::handleUpdateCheckError);

	if(!autocheck)
		g.mw->msgBox(QString::fromUtf8("Checking for update."));

	m_Revisioner->checkForUpdate();
#endif // if not defined USE_APPIMAGE_UPDATER_BRIDGE
}

#ifndef USE_APPIMAGE_UPDATER_BRIDGE
void VersionCheck::fetched(QByteArray a, QUrl url) {
	if (! a.isNull()) {
		if (! a.isEmpty()) {
#ifdef SNAPSHOT_BUILD
			if (url.path() == QLatin1String("/v1/banner")) {
				g.mw->msgBox(QString::fromUtf8(a));
			} else if (url.path() == QLatin1String("/v1/version-check")) {
#ifndef Q_OS_WIN
				g.mw->msgBox(QString::fromUtf8(a));
#else
				QDomDocument qdd;
				qdd.setContent(a);

				QDomElement elem = qdd.firstChildElement(QLatin1String("p"));
				elem = elem.firstChildElement(QLatin1String("a"));

				QUrl fetch = QUrl(elem.attribute(QLatin1String("href")));
				fetch.setHost(QString());
				fetch.setScheme(QString());
				if (! fetch.isValid()) {
					g.mw->msgBox(QString::fromUtf8(a));
				} else {
					QString filename = g.qdBasePath.absoluteFilePath(QLatin1String("Snapshots/") + QFileInfo(fetch.path()).fileName());

					QFile qf(filename);
					if (qf.exists()) {
						std::wstring native = QDir::toNativeSeparators(filename).toStdWString();

						WINTRUST_FILE_INFO file;
						ZeroMemory(&file, sizeof(file));
						file.cbStruct = sizeof(file);
						file.pcwszFilePath = native.c_str();

						WINTRUST_DATA data;
						ZeroMemory(&data, sizeof(data));
						data.cbStruct = sizeof(data);
						data.dwUIChoice = WTD_UI_NONE;
						data.fdwRevocationChecks = WTD_REVOKE_NONE;
						data.dwUnionChoice = WTD_CHOICE_FILE;
						data.pFile = &file;
						data.dwProvFlags = WTD_SAFER_FLAG | WTD_USE_DEFAULT_OSVER_CHECK;
						data.dwUIContext = WTD_UICONTEXT_INSTALL;

						static GUID guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

						LONG ts = WinVerifyTrust(0, &guid , &data);

						if (ts == 0) {
							if (QMessageBox::question(g.mw,
							                          tr("Upgrade Mumble"),
							                          tr("A new version of Mumble has been detected and automatically downloaded. It is recommended that you either upgrade to this version, or downgrade to the latest stable release. Do you want to launch the installer now?"),
							                          QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
								SHELLEXECUTEINFOW execinfo;
								std::wstring filenative = filename.toStdWString();
								std::wstring dirnative = QDir::toNativeSeparators(QDir::tempPath()).toStdWString();
								ZeroMemory(&execinfo, sizeof(execinfo));
								execinfo.cbSize = sizeof(execinfo);
								execinfo.lpFile = filenative.c_str();
								execinfo.lpDirectory = dirnative.c_str();
								execinfo.nShow = SW_NORMAL;

								if (ShellExecuteExW(&execinfo)) {
									g.mw->bSuppressAskOnQuit = true;
									qApp->closeAllWindows();
								} else {
									g.mw->msgBox(tr("Failed to launch snapshot installer."));
								}
							}

						} else {
							g.mw->msgBox(tr("Corrupt download of new version detected. Automatically removed."));
							qf.remove();
						}

						// Delete all but the N most recent snapshots
						size_t numberOfSnapshotsToKeep = 1;

						QDir snapdir(g.qdBasePath.absolutePath() + QLatin1String("/Snapshots/"),
						             QString(),
						             QDir::Name,
						             QDir::Files);

						foreach(const QFileInfo fileInfo, snapdir.entryInfoList(QStringList(), QDir::NoFilter, QDir::Time)) {
							if (numberOfSnapshotsToKeep) {
								--numberOfSnapshotsToKeep;
								continue;
							}

							qWarning() << "Purging old snapshot" << fileInfo.fileName();
							QFile file(fileInfo.absoluteFilePath());
							file.remove();
						}
					} else {
						g.mw->msgBox(tr("Downloading new snapshot from %1 to %2").arg(fetch.toString().toHtmlEscaped(), filename.toHtmlEscaped()));
						WebFetch::fetch(QLatin1String("dl"), fetch, this, SLOT(fetched(QByteArray,QUrl)));
						return;
					}
				}
			} else {
				QString filename = g.qdBasePath.absoluteFilePath(QLatin1String("Snapshots/") + QFileInfo(url.path()).fileName());

				QFile qf(filename);
				if (qf.open(QIODevice::WriteOnly)) {
					qf.write(a);
					qf.close();
					new VersionCheck(true, g.mw);
				} else {
					g.mw->msgBox(tr("Failed to write new version to disk."));
				}
#endif
			}
#else
			Q_UNUSED(url);
			g.mw->msgBox(QString::fromUtf8(a));
#endif
		}
	} else {
		g.mw->msgBox(tr("Mumble failed to retrieve version information from the central server."));
	}

	deleteLater();
}
#else // USE_APPIMAGE_UPDATER_BRIDGE
void VersionCheck::handleUpdateCheck(bool updateAvailable,const QJsonObject &info){
	(void)info;
	disconnect(m_Revisioner, &AppImageDeltaRevisioner::updateAvailable,
		   this, &VersionCheck::handleUpdateCheck);
	disconnect(m_Revisioner, &AppImageDeltaRevisioner::error,
		   this, &VersionCheck::handleUpdateCheckError);

	if(!updateAvailable){ // Notify the user they are using the latest version.
		g.mw->msgBox(QString::fromUtf8("You are currently using the latest version of Mumble AppImage."));
		//Qt parent to child deallocation should take care of m_Revisioner
		deleteLater();
		return;
	}
	
	g.mw->msgBox(QString::fromUtf8("A new version of Mumble AppImage is available."));
	
	int flags = AppImageUpdaterDialog::Default;
	flags ^= AppImageUpdaterDialog::ShowBeforeProgress; // No show before progress
	flags ^= AppImageUpdaterDialog::NotifyWhenNoUpdateIsAvailable;

	m_UpdaterDialog = new AppImageUpdaterDialog(QPixmap(QString::fromUtf8(":/mumble.svg")), nullptr, flags);

	connect(m_UpdaterDialog, &AppImageUpdaterDialog::error,
		this, &VersionCheck::handleUpdateError);
	connect(m_UpdaterDialog, &AppImageUpdaterDialog::finished,
		this, &VersionCheck::handleUpdateFinish);
	connect(m_UpdaterDialog, &AppImageUpdaterDialog::canceled,
		this, &VersionCheck::handleUpdateCancel);

	m_UpdaterDialog->init(m_Revisioner);
}

void VersionCheck::handleUpdateCheckError(short code){
	g.mw->msgBox(AppImageUpdaterBridge::errorCodeToDescriptionString(code));
	if(m_UpdaterDialog){ // We don't own the dialog
		m_UpdaterDialog->deleteLater();
	}	 
	deleteLater();
}

void VersionCheck::handleUpdateFinish(const QJsonObject &info){
	Q_UNUSED(info);
	m_UpdaterDialog->deleteLater();
	deleteLater();
	return;
}

void VersionCheck::handleUpdateError(const QString &str, short code){
	Q_UNUSED(str);
	Q_UNUSED(code);
	m_UpdaterDialog->deleteLater();
	deleteLater();
	return;
}

void VersionCheck::handleUpdateCancel(){
	m_UpdaterDialog->deleteLater();
	deleteLater();	
	return;
}

#endif // USE_APPIMAGE_UPDATER_BRIDGE
