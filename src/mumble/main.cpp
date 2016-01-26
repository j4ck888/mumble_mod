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

#include "mumble_pch.hpp"

#include "Overlay.h"
#include "MainWindow.h"
#include "ServerHandler.h"
#include "AudioInput.h"
#include "AudioOutput.h"
#include "AudioWizard.h"
#include "Cert.h"
#include "Database.h"
#include "Log.h"
#include "Plugins.h"
#include "Global.h"
#include "LCD.h"
#ifdef USE_BONJOUR
#include "BonjourClient.h"
#endif
#ifdef USE_DBUS
#include "DBus.h"
#endif
#ifdef USE_VLD
#include "vld.h"
#endif
#include "VersionCheck.h"
#include "NetworkConfig.h"
#include "CrashReporter.h"
#include "SocketRPC.h"
#include "MumbleApplication.h"
#include "ApplicationPalette.h"
#include "Themes.h"
#include "UserLockFile.h"

#ifdef PLUTOVR_BUILD

struct PlutoDefaultSettings {
	LPCWSTR audioInputDeviceId;
	LPCWSTR audioOutputDeviceId;
	float fVADmax;
	float fVADmin;
};

static HANDLE settingsMapHandle = NULL;
static PlutoDefaultSettings* plutoSettings = NULL;

void InitializePlutoSettingsMap()
{
		settingsMapHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, L"PlutoSettings");
		if (NULL == settingsMapHandle)
		{
			settingsMapHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(PlutoDefaultSettings), L"PlutoSettings");
		}
		if (NULL == settingsMapHandle)
		{
			return;
		}
		plutoSettings = static_cast<PlutoDefaultSettings*>(MapViewOfFile(settingsMapHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0));
		if (NULL == plutoSettings)
		{
			CloseHandle(settingsMapHandle);
			settingsMapHandle = NULL;
			return;
		}
		memset(plutoSettings, 0, sizeof(PlutoDefaultSettings));
}

void TeardownPlutoSettingsMap()
{
	if (plutoSettings)
	{
		UnmapViewOfFile(plutoSettings);
		plutoSettings = NULL;
	}
	if (settingsMapHandle)
	{
		CloseHandle(settingsMapHandle);
		settingsMapHandle = NULL;
	}
}

void InitializePlutoSettings()
{
	qWarning("Initializing Pluto Default Settings");
	if (plutoSettings)
	{	g.s.fVADmin = plutoSettings->fVADmin;
		g.s.fVADmax = plutoSettings->fVADmax;
	}

	g.s.bPositionalAudio = true;
	g.s.bPositionalHeadphone = true;
	g.s.fAudioMinDistance = 1.0f;
	g.s.fAudioMaxDistance = 15.0f;
	g.s.fAudioMaxDistVolume = 0.0f;
	g.s.fAudioBloom = 0.75f;
	g.s.bTransmitPosition = true;
	g.s.bTTS = false;
	g.s.bAskOnQuit = false;
	g.s.iQuality = 72000;
	g.s.iMinLoudness = 2250;
	g.s.fVADmax = 0.636799;
	g.s.fVADmin = 0.443403;
	g.s.fVolume = 1;
	g.s.fOtherVolume = 0.95;
	g.s.iNoiseSuppress = -30;
	g.s.bUpdateCheck = false;
	g.s.bPluginCheck = false;
	g.s.bQoS = false;
	g.s.bReconnect = false;
	g.s.bHideInTray = true;
	g.s.disablePublicList = true;
	g.s.disableConnectDialogEditing = true;
	g.s.bSuppressIdentity = true;
	g.s.bEcho = true;
	g.s.bEchoMulti = true;
	g.s.bHideFrame = false;
	g.s.bMinimalView = false;
}
#endif

#if defined(USE_STATIC_QT_PLUGINS) && QT_VERSION < 0x050000
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
Q_IMPORT_PLUGIN(qico)
Q_IMPORT_PLUGIN(qsvg)
Q_IMPORT_PLUGIN(qsvgicon)
# ifdef Q_OS_MAC
   Q_IMPORT_PLUGIN(qicnsicon)
# endif
#endif

#ifdef BOOST_NO_EXCEPTIONS
namespace boost {
	void throw_exception(std::exception const &) {
		qFatal("Boost exception caught!");
	}
}
#endif

extern void os_init();
extern char *os_lang;

#if defined(Q_OS_WIN) && !defined(QT_NO_DEBUG)
extern "C" _declspec(dllexport) int main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
	int res = 0;

	QT_REQUIRE_VERSION(argc, argv, "4.4.0");

#if defined(Q_OS_WIN)
	SetDllDirectory(L"");
#else
#ifndef Q_OS_MAC
	setenv("AVAHI_COMPAT_NOWARN", "1", 1);
#endif
#endif

#ifdef PLUTOVR_BUILD
	InitializePlutoSettingsMap();
#endif

	// Initialize application object.
	MumbleApplication a(argc, argv);
	a.setApplicationName(QLatin1String("Mumble"));
	a.setOrganizationName(QLatin1String("Mumble"));
	a.setOrganizationDomain(QLatin1String("mumble.sourceforge.net"));
	a.setQuitOnLastWindowClosed(false);

#if QT_VERSION >= 0x050100
	a.setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

#if QT_VERSION >= 0x050000 && defined(Q_OS_WIN)
	a.installNativeEventFilter(&a);
#endif

	#ifdef USE_SBCELT
	{
		QDir d(a.applicationVersionRootPath());
		QString helper = d.absoluteFilePath(QString::fromLatin1("sbcelt-helper"));
		setenv("SBCELT_HELPER_BINARY", helper.toUtf8().constData(), 1);
	}
#endif

	Global::g_global_struct = new Global();

	qsrand(QDateTime::currentDateTime().toTime_t());

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
	os_init();
#endif

	bool bAllowMultiple = false;
	bool suppressIdentity = false;
	bool bRpcMode = false;
	QString rpcCommand;
	QUrl url;
	if (a.arguments().count() > 1) {
		QStringList args = a.arguments();
		for (int i = 1; i < args.count(); ++i) {
			if (args.at(i) == QLatin1String("-h") || args.at(i) == QLatin1String("--help")
#if defined(Q_OS_WIN)
				|| args.at(i) == QLatin1String("/?")
#endif
			) {
				QString helpMessage = MainWindow::tr(
					"Usage: mumble [options] [<url>]\n"
					"\n"
					"<url> specifies a URL to connect to after startup instead of showing\n"
					"the connection window, and has the following form:\n"
					"mumble://[<username>[:<password>]@]<host>[:<port>][/<channel>[/<subchannel>...]][?version=<x.y.z>]\n"
					"\n"
					"The version query parameter has to be set in order to invoke the\n"
					"correct client version. It currently defaults to 1.2.0.\n"
					"\n"
					"Valid options are:\n"
					"  -h, --help    Show this help text and exit.\n"
					"  -m, --multiple\n"
					"                Allow multiple instances of the client to be started.\n"
					"  -n, --noidentity\n"
					"                Suppress loading of identity files (i.e., certificates.)\n"
					"\n"
				);
				QString rpcHelpBanner = MainWindow::tr(
					"Remote controlling Mumble:\n"
					"\n"
				);
				QString rpcHelpMessage = MainWindow::tr(
					"Usage: mumble rpc <action> [options]\n"
					"\n"
					"It is possible to remote control a running instance of Mumble by using\n"
					"the 'mumble rpc' command.\n"
					"\n"
					"Valid actions are:\n"
					"  mute\n"
					"                Mute self\n"
					"  unmute\n"
					"                Unmute self\n"
					"  deaf\n"
					"                Deafen self\n"
					"  undeaf\n"
					"                Undeafen self\n"
					"\n"
				);

				QString helpOutput = helpMessage + rpcHelpBanner + rpcHelpMessage;
				if (bRpcMode) {
					helpOutput = rpcHelpMessage;
				}

#if defined(Q_OS_WIN)
				QMessageBox::information(NULL, MainWindow::tr("Invocation"), helpOutput);
#else
				printf("%s", qPrintable(helpOutput));
#endif
				return 1;
			} else if (args.at(i) == QLatin1String("-m") || args.at(i) == QLatin1String("--multiple")) {
				bAllowMultiple = true;
			} else if (args.at(i) == QLatin1String("-n") || args.at(i) == QLatin1String("--noidentity")) {
				suppressIdentity = true;
				g.s.bSuppressIdentity = true;
			} else if (args.at(i) == QLatin1String("rpc")) {
				bRpcMode = true;
				if (args.count() - 1 > i) {
					rpcCommand = QString(args.at(i + 1));
				}
				else {
					QString rpcError = MainWindow::tr("Error: No RPC command specified");
#if defined(Q_OS_WIN)
					QMessageBox::information(NULL, MainWindow::tr("RPC"), rpcError);
#else
					printf("%s\n", qPrintable(rpcError));
#endif
					return 1;
				}
			} else {
				if (!bRpcMode) {
					QUrl u = QUrl::fromEncoded(args.at(i).toUtf8());
					if (u.isValid() && (u.scheme() == QLatin1String("mumble"))) {
						url = u;
					} else {
						QFile f(args.at(i));
						if (f.exists()) {
							url = QUrl::fromLocalFile(f.fileName());
						}
					}
				}
			}
		}
	}

#ifdef USE_DBUS
#ifdef Q_OS_WIN
	// By default, windbus expects the path to dbus-daemon to be in PATH, and the path
	// should contain bin\\, and the path to the config is hardcoded as ..\etc

	{
		size_t reqSize;
		_wgetenv_s(&reqSize, NULL, 0, L"PATH");
		if (reqSize > 0) {
			STACKVAR(wchar_t, buff, reqSize+1);
			_wgetenv_s(&reqSize, buff, reqSize, L"PATH");
			QString path = QString::fromLatin1("%1;%2").arg(QDir::toNativeSeparators(MumbleApplication::instance()->applicationVersionRootPath())).arg(QString::fromWCharArray(buff));
			STACKVAR(wchar_t, buffout, path.length() + 1);
			path.toWCharArray(buffout);
			_wputenv_s(L"PATH", buffout);
		}
	}
#endif
#endif

	if (bRpcMode) {
		bool sent = false;
		QMap<QString, QVariant> param;
		param.insert(rpcCommand, rpcCommand);
		sent = SocketRPC::send(QLatin1String("Mumble"), QLatin1String("self"), param);
		if (sent) {
			return 0;
		} else {
			return 1;
		}
	}

	if (! bAllowMultiple) {
		if (url.isValid()) {
#ifndef USE_DBUS
			QMap<QString, QVariant> param;
			param.insert(QLatin1String("href"), url);
#endif
			bool sent = false;
#ifdef USE_DBUS
			QDBusInterface qdbi(QLatin1String("net.sourceforge.mumble.mumble"), QLatin1String("/"), QLatin1String("net.sourceforge.mumble.Mumble"));

			QDBusMessage reply=qdbi.call(QLatin1String("openUrl"), QLatin1String(url.toEncoded()));
			sent = (reply.type() == QDBusMessage::ReplyMessage);
#else
			sent = SocketRPC::send(QLatin1String("Mumble"), QLatin1String("url"), param);
#endif
			if (sent)
				return 0;
		} else {
			bool sent = false;
#ifdef USE_DBUS
			QDBusInterface qdbi(QLatin1String("net.sourceforge.mumble.mumble"), QLatin1String("/"), QLatin1String("net.sourceforge.mumble.Mumble"));

			QDBusMessage reply=qdbi.call(QLatin1String("focus"));
			sent = (reply.type() == QDBusMessage::ReplyMessage);
#else
			sent = SocketRPC::send(QLatin1String("Mumble"), QLatin1String("focus"));
#endif
			if (sent)
				return 0;

		}
	}

#ifdef Q_OS_WIN
	// The code above this block is somewhat racy, in that it might not
	// be possible to do RPC/DBus if two processes start at almost the
	// same time.
	//
	// In order to be completely sure we don't open multiple copies of
	// Mumble, we open a lock file. The file is opened without any sharing
	// modes enabled. This gives us exclusive access to the file.
	// If another Mumble instance attempts to open the file, it will fail,
	// and that instance will know to terminate itself.
	UserLockFile userLockFile(g.qdBasePath.filePath(QLatin1String("mumble.lock")));
	if (! bAllowMultiple) {
		if (!userLockFile.acquire()) {
			qWarning("Another process has already acquired the lock file at '%s'. Terminating...", qPrintable(userLockFile.path()));
			return 1;
		}
	}
#endif

	// Load preferences
	g.s.load();

#ifdef PLUTOVR_BUILD
	InitializePlutoSettings();
#endif

	// Check whether we need to enable accessibility features
#ifdef Q_OS_WIN
	// Only windows for now. Could not find any information on how to query this for osx or linux
	{
		HIGHCONTRAST hc;
		hc.cbSize = sizeof(HIGHCONTRAST);
		SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(HIGHCONTRAST), &hc, 0);

		if (hc.dwFlags & HCF_HIGHCONTRASTON)
			g.s.bHighContrast = true;

	}
#endif

	DeferInit::run_initializers();

	ApplicationPalette applicationPalette;
	
	Themes::apply();

	QString qsSystemLocale = QLocale::system().name();

#ifdef Q_OS_MAC
	if (os_lang) {
		qWarning("Using Mac OS X system langauge as locale name");
		qsSystemLocale = QLatin1String(os_lang);
	}
#endif

	const QString locale = g.s.qsLanguage.isEmpty() ? qsSystemLocale : g.s.qsLanguage;
	qWarning("Locale is \"%s\" (System: \"%s\")", qPrintable(locale), qPrintable(qsSystemLocale));

	QTranslator translator;
	if (translator.load(QLatin1String(":mumble_") + locale))
		a.installTranslator(&translator);

	QTranslator loctranslator;
	if (loctranslator.load(QLatin1String("mumble_") + locale, a.applicationDirPath()))
		a.installTranslator(&loctranslator); // Can overwrite strings from bundled mumble translation

	// With modularization of Qt 5 some - but not all - of the qt_<locale>.ts files have become
	// so-called meta catalogues which no longer contain actual translations but refer to other
	// more specific ts files like qtbase_<locale>.ts . To successfully load a meta catalogue all
	// of its referenced translations must be available. As we do not want to bundle them all
	// we now try to load the old qt_<locale>.ts file first and then fall back to loading
	// qtbase_<locale>.ts if that failed.
	//
	// See http://doc.qt.io/qt-5/linguist-programmers.html#deploying-translations for more information
	QTranslator qttranslator;
	if (qttranslator.load(QLatin1String("qt_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) { // Try system qt translations
		a.installTranslator(&qttranslator);
	} else if (qttranslator.load(QLatin1String("qtbase_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
		a.installTranslator(&qttranslator);
	} else if (qttranslator.load(QLatin1String(":qt_") + locale)) { // Try bundled translations
		a.installTranslator(&qttranslator);
	} else if (qttranslator.load(QLatin1String(":qtbase_") + locale)) {
		a.installTranslator(&qttranslator);
	}
	
	// Initialize proxy settings
	NetworkConfig::SetupProxy();

	g.nam = new QNetworkAccessManager();

#ifndef NO_CRASH_REPORT
	CrashReporter *cr = new CrashReporter();
	cr->run();
	delete cr;
#endif

	// Initialize logger
	g.l = new Log();

	// Initialize database
	g.db = new Database();

#ifdef USE_BONJOUR
	// Initialize bonjour
	g.bc = new BonjourClient();
#endif

	//TODO: This already loads up the DLL and does some initial hooking, even
	// when the OL is disabled. This should either not be done (object instantiation)
	// or the dll loading and preparation be delayed to first use.
	g.o = new Overlay();
	g.o->setActive(g.s.os.bEnable);

	g.lcd = new LCD();

	// Process any waiting events before initializing our MainWindow.
	// The mumble:// URL support for Mac OS X happens through AppleEvents,
	// so we need to loop a little before we begin.
	a.processEvents();

	// Main Window
	g.mw=new MainWindow(NULL);
#ifdef PLUTOVR_BUILD
	g.mw->hide();
#else
	g.mw->show();
#endif

#ifdef USE_DBUS
	new MumbleDBus(g.mw);
	QDBusConnection::sessionBus().registerObject(QLatin1String("/"), g.mw);
	QDBusConnection::sessionBus().registerService(QLatin1String("net.sourceforge.mumble.mumble"));
#endif

	SocketRPC *srpc = new SocketRPC(QLatin1String("Mumble"));

	g.l->log(Log::Information, MainWindow::tr("Welcome to Mumble."));

#ifdef PLUTOVR_BUILD
	qWarning().nospace() << " vsVAD " << g.s.vsVAD << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " fVADmax " << g.s.fVADmax << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " fVADmin " << g.s.fVADmin << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " iQuality " << g.s.iQuality << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " bAskOnQuit " << g.s.bAskOnQuit << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " iMinLoudness " << g.s.iMinLoudness << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " bTTS " << g.s.bTTS << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " fVolume " << g.s.fVolume << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " fOtherVolume " << g.s.fOtherVolume << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " iNoiseSuppress " << g.s.iNoiseSuppress << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " bEcho " << g.s.bEcho << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " bEchoMulti " << g.s.bEchoMulti << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " bHideFrame " << g.s.bHideFrame << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " bMinimalView " << g.s.bMinimalView << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
	qWarning().nospace() << " bPositionalHeadphone " << g.s.bPositionalHeadphone << " @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
#endif

	// Plugins
	g.p = new Plugins(NULL);
	g.p->rescanPlugins();

	Audio::start();

	a.setQuitOnLastWindowClosed(false);

	// Configuration updates
	bool runaudiowizard = false;
	if (g.s.uiUpdateCounter == 0) {
		// Previous version was an pre 1.2.3 release or this is the first run
		runaudiowizard = true;

	} else if (g.s.uiUpdateCounter == 1) {
		// Previous versions used old idle action style, convert it

		if (g.s.iIdleTime == 5 * 60) { // New default
			g.s.iaeIdleAction = Settings::Nothing;
		} else {
			g.s.iIdleTime = 60 * qRound(g.s.iIdleTime / 60.); // Round to minutes
			g.s.iaeIdleAction = Settings::Deafen; // Old behavior
		}
	}

	if (runaudiowizard) {
		AudioWizard *aw = new AudioWizard(g.mw);
		aw->exec();
		delete aw;
	}

	g.s.uiUpdateCounter = 2;

	if (! CertWizard::validateCert(g.s.kpCertificate)) {
#if QT_VERSION >= 0x050000
		QDir qd(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
#else
		QDir qd(QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation));
#endif

		QFile qf(qd.absoluteFilePath(QLatin1String("MumbleAutomaticCertificateBackup.p12")));
		if (qf.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
			Settings::KeyPair kp = CertWizard::importCert(qf.readAll());
			qf.close();
			if (CertWizard::validateCert(kp))
				g.s.kpCertificate = kp;
		}
		if (! CertWizard::validateCert(g.s.kpCertificate)) {
			CertWizard *cw = new CertWizard(g.mw);
			cw->exec();
			delete cw;

			if (! CertWizard::validateCert(g.s.kpCertificate)) {
				g.s.kpCertificate = CertWizard::generateNewCert();
				if (qf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Unbuffered)) {
					qf.write(CertWizard::exportCert(g.s.kpCertificate));
					qf.close();
				}
			}
		}
	}

	if (QDateTime::currentDateTime().daysTo(g.s.kpCertificate.first.first().expiryDate()) < 14)
		g.l->log(Log::Warning, CertWizard::tr("<b>Certificate Expiry:</b> Your certificate is about to expire. You need to renew it, or you will no longer be able to connect to servers you are registered on."));

#ifdef QT_NO_DEBUG
#ifndef SNAPSHOT_BUILD
	if (g.s.bUpdateCheck)
#endif
		new VersionCheck(true, g.mw);
#ifdef SNAPSHOT_BUILD
	new VersionCheck(false, g.mw, true);
#endif
#else
	g.mw->msgBox(MainWindow::tr("Skipping version check in debug mode."));
#endif
	if (g.s.bPluginCheck) {
		g.p->checkUpdates();
	}

	if (url.isValid()) {
		OpenURLEvent *oue = new OpenURLEvent(url);
		qApp->postEvent(g.mw, oue);
#ifdef Q_OS_MAC
	} else if (! a.quLaunchURL.isEmpty()) {
		OpenURLEvent *oue = new OpenURLEvent(a.quLaunchURL);
		qApp->postEvent(g.mw, oue);
#endif
	} else {
#ifndef PLUTOVR_BUILD
		g.mw->on_qaServerConnect_triggered(true);
#endif
	}

#pragma message("SCOTTRA_TODO - remove this but they are placeholders to remember the settings")
  qWarning().nospace() << "SCOTTRA - input device: " << g.s.qsWASAPIInput << "  @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;
  qWarning().nospace() << "SCOTTRA - output device: " << g.s.qsWASAPIOutput << "  @" << __FUNCTION__ <<"():" << __FILE__ << ":" << __LINE__;

	if (! g.bQuit)
		res=a.exec();

	g.s.save();

	url.clear();
	
	ServerHandlerPtr sh = g.sh;
	if (sh && sh->isRunning()) {
		url = sh->getServerURL();
		Database::setShortcuts(g.sh->qbaDigest, g.s.qlShortcuts);
	}

	Audio::stop();

	if (sh)
		sh->disconnect();

	delete srpc;

	g.sh.reset();
	while (sh && ! sh.unique())
		QThread::yieldCurrentThread();
	sh.reset();

	delete g.mw;

	delete g.nam;
	delete g.lcd;

	delete g.db;
	delete g.p;
	delete g.l;

#ifdef USE_BONJOUR
	delete g.bc;
#endif

	delete g.o;

	DeferInit::run_destroyers();

	delete Global::g_global_struct;
	Global::g_global_struct = NULL;

#ifndef QT_NO_DEBUG
	// Hide Qt memory leak.
	QSslSocket::setDefaultCaCertificates(QList<QSslCertificate>());
	// Release global protobuf memory allocations.
#if (GOOGLE_PROTOBUF_VERSION >= 2001000)
	google::protobuf::ShutdownProtobufLibrary();
#endif
#endif

#ifdef Q_OS_WIN
	// Release the userLockFile.
	//
	// It is important that we release it before we attempt to
	// restart Mumble (if requested). If we do not release it
	// before that, the new instance might not be able to start
	// correctly.
	userLockFile.release();
#endif
	
	// At this point termination of our process is immenent. We can safely
	// launch another version of Mumble. The reason we do an actual
	// restart instead of re-creating our data structures is that making
	// sure we won't leave state is quite tricky. Mumble has quite a
	// few spots which might not consider seeing to basic initializations.
	// Until we invest the time to verify this, rather be safe (and a bit slower)
	// than sorry (and crash/bug out). Also take care to reconnect if possible.
	if (res == MUMBLE_EXIT_CODE_RESTART) {
		QStringList arguments;
		
		if (bAllowMultiple)   arguments << QLatin1String("--multiple");
		if (suppressIdentity) arguments << QLatin1String("--noidentity");
		if (!url.isEmpty())   arguments << url.toString();
		
		qWarning() << "Triggering restart of Mumble with arguments: " << arguments;
		
#ifdef Q_OS_WIN
		// Work around bug related to QTBUG-7645. Mumble has uiaccess=true set
		// on windows which makes normal CreateProcess calls (like Qt uses in
		// startDetached) fail unless they specifically enable additional priviledges.
		// Note that we do not actually require user interaction by UAC nor full admin
		// rights but only the right token on launch. Here we use ShellExecuteEx
		// which handles this transparently for us.
		const std::wstring applicationFilePath = qApp->applicationFilePath().toStdWString();
		const std::wstring argumentsString = arguments.join(QLatin1String(" ")).toStdWString();
		
		SHELLEXECUTEINFO si;
		ZeroMemory(&si, sizeof(SHELLEXECUTEINFO));
		si.cbSize = sizeof(SHELLEXECUTEINFO);
		si.lpFile = applicationFilePath.data();
		si.lpParameters = argumentsString.data();
		
		bool ok = (ShellExecuteEx(&si) == TRUE);
#else
		bool ok = QProcess::startDetached(qApp->applicationFilePath(), arguments);
#endif
		if(!ok) {
			QMessageBox::warning(NULL,
			                     QApplication::tr("Failed to restart mumble"),
			                     QApplication::tr("Mumble failed to restart itself. Please restart it manually.")
			);
			return 1;
		}
		return 0;
	}
	return res;
}

#if defined(Q_OS_WIN) && defined(QT_NO_DEBUG)
extern void qWinMain(HINSTANCE, HINSTANCE, LPSTR, int, int &, QVector<char *> &);

extern "C" _declspec(dllexport) int MumbleMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdArg, int cmdShow) {
	Q_UNUSED(cmdArg);

	QByteArray cmdParam = QString::fromWCharArray(GetCommandLine()).toLocal8Bit();
	int argc = 0;

	// qWinMain takes argv as a reference.
	QVector<char *> argv;
	qWinMain(instance, prevInstance, cmdParam.data(), cmdShow, argc, argv);

	int result = main(argc, argv.data());
	return result;
}
#endif
