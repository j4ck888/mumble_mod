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

#ifndef _SCRIPT_H
#define _SCRIPT_H

#include "mumble_pch.hpp"
#include "User.h"
#include "Channel.h"

class ScriptUser;
class ScriptChannel;
class MumbleScript;
class MumbleScripts;

class ScriptUser : public QObject, protected QScriptable {
		friend class MumbleScript;
		Q_OBJECT
		Q_DISABLE_COPY(ScriptUser)
		Q_PROPERTY(QString name READ getName)
		Q_PROPERTY(unsigned int session READ getSession)
		Q_PROPERTY(int id READ getId)
		Q_PROPERTY(QScriptValue channel READ getChannel WRITE setChannel)
		Q_PROPERTY(bool mute READ isMute WRITE setMute)
		Q_PROPERTY(bool deaf READ isDeaf WRITE setDeaf)
	public:
		ScriptUser(ClientUser *p);
		QString getName() const;
		unsigned int getSession() const;
		int getId() const;
		QScriptValue getChannel() const;
		bool isMute() const;
		bool isDeaf() const;
	public slots:
		void setMute(bool);
		void setDeaf(bool);
		void setChannel(const QScriptValue &);
		void sendMessage(const QString &);
	signals:
		void moved();
};

class ScriptChannel : public QObject, protected QScriptable {
		friend class MumbleScript;
		Q_OBJECT
		Q_DISABLE_COPY(ScriptChannel)
		Q_PROPERTY(QString name READ getName WRITE setName)
		Q_PROPERTY(int id READ getId)
		Q_PROPERTY(int parent READ getParent WRITE setParent)
		Q_PROPERTY(QScriptValue children READ getChildren)
		Q_PROPERTY(QScriptValue users READ getUsers)
	public:
		ScriptChannel(Channel *c);
		QString getName() const;
		int getId() const;
		int getParent() const;
	public slots:
		void setName(const QString &);
		void setParent(int);
		QScriptValue getUsers() const;
		QScriptValue getChildren() const;
		void sendMessage(const QString &, bool);
	signals:
		void moved();
};

class ScriptServer : public QObject, protected QScriptable {
		friend class MumbleScript;
		Q_OBJECT
		Q_DISABLE_COPY(ScriptServer)
		Q_PROPERTY(QScriptValue userContextMenu)
		Q_PROPERTY(QScriptValue channelContextMenu)
		Q_PROPERTY(QScriptValue msgHandler)
		Q_PROPERTY(QScriptValue root READ getRoot)
	public:
		ScriptServer(QObject *p);
		QScriptValue getRoot() const;
	public slots:
		QScriptValue loadUi(const QString &);
		void sendMsg(const QString &, const QStringList &);
	signals:
		void newUser(QScriptValue);
		void newChannel(QScriptValue);
		void connected();
		void disconnected(QString reason);
};

class MumbleScript : public QObject {
		Q_OBJECT
		Q_DISABLE_COPY(MumbleScript)
	public:
		QScriptEngine *qseEngine;
		ScriptServer *ssServer;
		QString qsFilename;
		QMap<ClientUser *, ScriptUser *> qmUsers;
		QMap<Channel *, ScriptChannel *> qmChannels;
		QMap<QString, QByteArray> qmResources;
		bool bLocal;

		MumbleScript(MumbleScripts *p);
		~MumbleScript();
		void load(const QString &source);
		void evaluate(const QString &code);

		void addUser(ClientUser *p);
		void moveUser(ClientUser *p);
		void addChannel(Channel *c);
		void moveChannel(Channel *c);
		void connected();
	public slots:
		void userDeleted(QObject *);
		void channelDeleted(QObject *);
		void errorHandler(const QScriptValue &);
};

class MumbleScripts : public QObject {
		friend class MumbleScript;
		Q_OBJECT
		Q_DISABLE_COPY(MumbleScripts)
	protected:
		QList<MumbleScript *> qlScripts;
	public:
		MumbleScripts(QObject *p);
		~MumbleScripts();
		void createEvaluate(const QString &code);
		void addUser(ClientUser *p);
		void moveUser(ClientUser *p);
		void addChannel(Channel *c);
		void moveChannel(Channel *c);
		void connected();
};

#endif
