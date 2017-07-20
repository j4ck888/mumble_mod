// Copyright 2005-2017 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "CrashReporter.h"

#include "Global.h"
#include "NetworkConfig.h"
#include "OSInfo.h"
#include "EnvUtils.h"

CrashReporter::CrashReporter(QWidget *p) : QDialog(p) {
	setWindowTitle(tr("Mumble Crash Report"));

	QVBoxLayout *vbl= new QVBoxLayout(this);

	QLabel *l;

	l = new QLabel(tr("<p><b>Giving way to sincere apology, Mumble has crashed. Leaving only a trace of stripped memory behind.</b></p>"
	                  "<p>Sending it in provides clues to preventing it from happening in the future.</p>"));

	vbl->addWidget(l);

	QHBoxLayout *hbl = new QHBoxLayout();

	qleEmail = new QLineEdit(g.qs->value(QLatin1String("crashemail")).toString());
	l = new QLabel(tr("Email address (optional)"));
	l->setBuddy(qleEmail);

	hbl->addWidget(l);
	hbl->addWidget(qleEmail, 1);
	vbl->addLayout(hbl);

	qteDescription=new QTextEdit();
	l->setBuddy(qteDescription);
	l = new QLabel(tr("Please describe briefly, in English, what you were doing at the time of the crash"));

	vbl->addWidget(l);
	vbl->addWidget(qteDescription, 1);

	QPushButton *pbOk = new QPushButton(tr("Send Report"));
	pbOk->setDefault(true);

	QPushButton *pbCancel = new QPushButton(tr("Don't send report"));
	pbCancel->setAutoDefault(false);

	QDialogButtonBox *dbb = new QDialogButtonBox(Qt::Horizontal);
	dbb->addButton(pbOk, QDialogButtonBox::AcceptRole);
	dbb->addButton(pbCancel, QDialogButtonBox::RejectRole);
	connect(dbb, SIGNAL(accepted()), this, SLOT(accept()));
	connect(dbb, SIGNAL(rejected()), this, SLOT(reject()));
	vbl->addWidget(dbb);

	qelLoop = new QEventLoop(this);
	qpdProgress = NULL;
	qnrReply = NULL;
}

CrashReporter::~CrashReporter() {
	g.qs->setValue(QLatin1String("crashemail"), qleEmail->text());
	delete qnrReply;
}

void CrashReporter::uploadFinished() {
	qpdProgress->reset();
	if (qnrReply->error() == QNetworkReply::NoError) {
		if (qnrReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200)
			QMessageBox::information(NULL, tr("Crash report sent"), tr("Thank you for helping make Mumble better!"));
		else
			QMessageBox::critical(NULL, tr("Failed to send crash report"), tr("Unfortunately the crash report failed to upload, with error %1 %2. Please inform a developer.").arg(qnrReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(qnrReply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
	} else {
		QMessageBox::critical(NULL, tr("Crash uploader failed"), tr("Murphy\'s law in full effect, hitting a bug in the crash reporting code, so that the report could not be sent. Please inform a developer about this, error %1").arg(qnrReply->error()));
	}
	qelLoop->exit(0);
}

void CrashReporter::uploadProgress(qint64 sent, qint64 total) {
	qpdProgress->setMaximum(static_cast<int>(total));
	qpdProgress->setValue(static_cast<int>(sent));
}

void CrashReporter::run() {
	QByteArray qbaDumpContents;
	QFile qfCrashDump(g.qdBasePath.filePath(QLatin1String("mumble.dmp")));
	if (! qfCrashDump.exists())
		return;

	qfCrashDump.open(QIODevice::ReadOnly);

#if defined(Q_OS_WIN)
	/* On Windows, the .dmp file is a real minidump. */

	if (qfCrashDump.peek(4) != "MDMP")
		return;
	qbaDumpContents = qfCrashDump.readAll();

#elif defined(Q_OS_MAC)
	/*
	 * On macOS, the .dmp file is simply a dummy file that we
	 * use to find the *real* crash dump, made by the macOS
	 * built in crash reporter.
	 */
	QFileInfo qfiDump(qfCrashDump);
	QDateTime qdtModification = qfiDump.lastModified();

	/* Find the real crash report. */
	QDir qdCrashReports(QDir::home().absolutePath() + QLatin1String("/Library/Logs/DiagnosticReports/"));
	if (! qdCrashReports.exists()) {
		qdCrashReports = QDir::home().absolutePath() + QLatin1String("/Library/Logs/CrashReporter/");
	}

	QStringList qslFilters;
	qslFilters << QString::fromLatin1("Mumble_*.crash");
	qdCrashReports.setNameFilters(qslFilters);
	qdCrashReports.setSorting(QDir::Time);
	QFileInfoList qfilEntries = qdCrashReports.entryInfoList();

	/*
	 * Figure out if our delta is sufficiently close to the Apple crash dump, or
	 * if something weird happened.
	 */
	foreach(QFileInfo fi, qfilEntries) {
		qint64 delta = qAbs<qint64>(qdtModification.secsTo(fi.lastModified()));
		if (delta < 8) {
			QFile f(fi.absoluteFilePath());
			f.open(QIODevice::ReadOnly);
			qbaDumpContents = f.readAll();
			break;
		}
	}
#endif

	QString details;
#ifdef Q_OS_WIN
	{
		QTemporaryFile qtf;
		if (qtf.open()) {
			qtf.close();

			QProcess qp;
			QStringList qsl;

			qsl << QLatin1String("/t");
			qsl << qtf.fileName();

			QString app = QLatin1String("dxdiag.exe");
			QString systemRoot = EnvUtils::getenv(QLatin1String("SystemRoot"));

			if (systemRoot.count() > 0) {
				app = QDir::fromNativeSeparators(systemRoot + QLatin1String("/System32/dxdiag.exe"));
			}

			qp.start(app, qsl);
			if (qp.waitForFinished(30000)) {
				if (qtf.open()) {
					QByteArray qba = qtf.readAll();
					details = QString::fromLocal8Bit(qba);
				}
			} else {
				details = QLatin1String("Failed to run DxDiag");
			}
			qp.kill();
		}
	}
#endif

	if (qbaDumpContents.isEmpty()) {
		qWarning("CrashReporter: Empty crash dump file, not reporting.");
		return;
	}

	if (exec() == QDialog::Accepted) {
		qpdProgress = new QProgressDialog(tr("Sending crash report"), tr("Cancel"), 0, 100, this);
		qpdProgress->setMinimumDuration(500);
		qpdProgress->setValue(0);
		connect(qpdProgress, SIGNAL(canceled()), qelLoop, SLOT(quit()));

		QString boundary = QString::fromLatin1("---------------------------%1").arg(QDateTime::currentDateTime().toTime_t());

		QString os = QString::fromLatin1("--%1\r\nContent-Disposition: form-data; name=\"os\"\r\nContent-Transfer-Encoding: 8bit\r\n\r\n%2 %3\r\n").arg(boundary, OSInfo::getOS(), OSInfo::getOSVersion());
		QString ver = QString::fromLatin1("--%1\r\nContent-Disposition: form-data; name=\"ver\"\r\nContent-Transfer-Encoding: 8bit\r\n\r\n%2 %3\r\n").arg(boundary, QLatin1String(MUMTEXT(MUMBLE_VERSION_STRING)), QLatin1String(MUMBLE_RELEASE));
		QString email = QString::fromLatin1("--%1\r\nContent-Disposition: form-data; name=\"email\"\r\nContent-Transfer-Encoding: 8bit\r\n\r\n%2\r\n").arg(boundary, qleEmail->text());
		QString descr = QString::fromLatin1("--%1\r\nContent-Disposition: form-data; name=\"desc\"\r\nContent-Transfer-Encoding: 8bit\r\n\r\n%2\r\n").arg(boundary, qteDescription->toPlainText());
		QString det = QString::fromLatin1("--%1\r\nContent-Disposition: form-data; name=\"details\"\r\nContent-Transfer-Encoding: 8bit\r\n\r\n%2\r\n").arg(boundary, details);
		QString head = QString::fromLatin1("--%1\r\nContent-Disposition: form-data; name=\"dump\"; filename=\"mumble.dmp\"\r\nContent-Type: binary/octet-stream\r\n\r\n").arg(boundary);
		QString end = QString::fromLatin1("\r\n--%1--\r\n").arg(boundary);

		QByteArray post = os.toUtf8() + ver.toUtf8() + email.toUtf8() + descr.toUtf8() + det.toUtf8() + head.toUtf8() + qbaDumpContents + end.toUtf8();

		QUrl url(QLatin1String("https://crash-report.mumble.info/v1/report"));
		QNetworkRequest req(url);
		req.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromLatin1("multipart/form-data; boundary=%1").arg(boundary));
		req.setHeader(QNetworkRequest::ContentLengthHeader, QString::number(post.size()));
		Network::prepareRequest(req);
		qnrReply = g.nam->post(req, post);
		connect(qnrReply, SIGNAL(finished()), this, SLOT(uploadFinished()));
		connect(qnrReply, SIGNAL(uploadProgress(qint64, qint64)), this, SLOT(uploadProgress(qint64, qint64)));

		qelLoop->exec(QEventLoop::DialogExec);
	}

	if (! qfCrashDump.remove())
		qWarning("CrashReporeter: Unable to remove crash file.");
}
