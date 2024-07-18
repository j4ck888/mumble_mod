// Copyright 2022 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_DATABASE_SQLITECONNECTIONPARAMTER_H_
#define MUMBLE_DATABASE_SQLITECONNECTIONPARAMTER_H_

#include "database/Backend.h"
#include "database/ConnectionParameter.h"

#include <string>

namespace mumble {
namespace db {

	class SQLiteConnectionParameter : public ConnectionParameter {
	public:
		SQLiteConnectionParameter(const std::string &dbPath, bool useWAL = true);

		std::string dbPath;
		/** Whether to use write-ahead logging */
		bool useWAL;

		virtual Backend applicability() const override;
	};

} // namespace db
} // namespace mumble

#endif // MUMBLE_DATABASE_SQLITECONNECTIONPARAMTER_H_
