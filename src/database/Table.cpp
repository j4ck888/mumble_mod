// Copyright 2022 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Table.h"
#include "AccessException.h"
#include "DBUtils.h"
#include "FormatException.h"

#include <soci/soci.h>

#include <boost/algorithm/string.hpp>

#include <nlohmann/json.hpp>

#include <cassert>
#include <utility>
#include <vector>

namespace mumble {
namespace db {

	// If this looks weird to you, check out https://stackoverflow.com/a/8016853
	// (Essentially this is needed to avoid an undefined reference error for this constant)
	constexpr const char *Table::BACKUP_SUFFIX;

	Table::Table(soci::session &sql, Backend backend) : Table(sql, backend, std::string{}, {}) {}
	Table::Table(soci::session &sql, Backend backend, const std::string &name) : Table(sql, backend, name, {}) {}
	Table::Table(soci::session &sql, Backend backend, const std::string &name, const std::vector< Column > &columns)
		: m_name(name), m_columns(columns), m_sql(sql), m_backend(backend) {
		performCtorAssertions();
	}

	const std::string &Table::getName() const { return m_name; }
	void Table::setName(const std::string &name) { m_name = name; }

	const std::vector< Column > &Table::getColumns() const { return m_columns; }

	void Table::setColumns(const std::vector< Column > &columns) {
		// we don't support overwriting columns after they have been initialized already
		assert(m_columns.empty());

		m_columns = columns;
	}

	const Column *Table::findColumn(const std::string &name) const {
		for (const Column &currentCol : m_columns) {
			if (currentCol.getName() == name) {
				return &currentCol;
			}
		}

		return nullptr;
	}

	bool Table::containsColumn(const std::string &name) const { return findColumn(name) != nullptr; }

	void Table::create() {
		assert(!m_name.empty());
		assert(!m_columns.empty());

		std::string createQuery = "CREATE TABLE \"" + m_name + "\" (";
		std::string outOfLineConstraints;

		for (const Column &currentColumn : m_columns) {
			createQuery += currentColumn.getName() + " " + currentColumn.getType().sqlRepresentation();

			if (currentColumn.hasDefaultValue()) {
				createQuery += " DEFAULT ";
				std::string defaultValue = currentColumn.getDefaultValue();
				if (currentColumn.getType().getType() == DataType::String
					|| currentColumn.getType().getType() == DataType::FixedSizeString) {
					// Escape single quotes by doubling them up
					boost::replace_all(defaultValue, "'", "''");

					// We'll have to wrap the value in quotes in order to make sure it gets recognized as a String
					createQuery += "'" + defaultValue + "'";
				} else {
					// If the default is not a String-type, we don't want to see spaces in it
					assert(defaultValue.find(" ") == std::string::npos);

					createQuery += defaultValue;
				}
			}

			for (const Constraint &currentConstraint : currentColumn.getConstraints()) {
				if (currentConstraint.canInline()
					&& (currentConstraint.prefersInline() || !currentConstraint.canOutOfLine())) {
					createQuery += " " + currentConstraint.inlineSQL(m_backend);
				} else {
					assert(currentConstraint.canOutOfLine());

					outOfLineConstraints += currentConstraint.outOfLineSQL(currentColumn, m_backend) + ", ";
				}
			}

			createQuery += ", ";
		}

		// Append out-of-line constraints
		createQuery += outOfLineConstraints;

		// Remove trailing ", "
		createQuery.erase(createQuery.size() - 2);

		createQuery += ")";

		try {
			m_sql << createQuery;

			// Also create all necessary indices
			for (const Index &currentIndex : m_indices) {
				m_sql << currentIndex.creationQuery(*this, m_backend);
			}
		} catch (const soci::soci_error &e) {
			throw AccessException(e.what());
		}
	}

	void Table::migrate(unsigned int fromSchemeVersion, unsigned int toSchemeVersion) {
		(void) fromSchemeVersion;
		(void) toSchemeVersion;
		// The default implementation does nothing
	}

	void Table::destroy() {
		assert(!m_name.empty());

		try {
			m_sql << "DROP TABLE \"" << m_name + "\"";
		} catch (const soci::soci_error &e) {
			throw AccessException(e.what());
		}
	}

	void Table::clear() {
		assert(!m_name.empty());

		try {
			// Note that thanks to the missing WHERE clause, this deletes all rows in this table
			m_sql << "DELETE FROM \"" << m_name + "\"";
		} catch (const soci::soci_error &e) {
			throw AccessException(e.what());
		}
	}

	const std::vector< Index > &Table::getIndices() const { return m_indices; }

	void Table::addIndex(const Index &index, bool applyToDB) {
		if (applyToDB) {
			try {
				m_sql << index.creationQuery(*this, m_backend);
			} catch (const soci::soci_error &e) {
				throw AccessException("Failed at creating index \"" + index.getName() + "\": " + e.what());
			}
		}

		m_indices.push_back(index);
	}

	bool Table::removeIndex(const Index &index, bool applyToDB) {
		auto it = std::find(m_indices.begin(), m_indices.end(), index);
		if (it == m_indices.end()) {
			return false;
		}

		m_indices.erase(it);

		if (applyToDB) {
			try {
				m_sql << index.dropQuery(*this, m_backend);
			} catch (const soci::soci_error &e) {
				throw AccessException("Failed at dropping index \"" + index.getName() + "\": " + e.what());
			}
		}

		return true;
	}

	const std::vector< Trigger > &Table::getTrigger() const { return m_trigger; }

	void Table::addTrigger(const Trigger &trigger, bool applyToDB) {
		if (applyToDB) {
			try {
				m_sql << trigger.creationQuery(*this, m_backend);
			} catch (const soci::soci_error &e) {
				throw AccessException("Failed at creating trigger \"" + trigger.getName() + "\": " + e.what());
			}
		}

		m_trigger.push_back(trigger);
	}

	bool Table::removeTrigger(const Trigger &trigger, bool applyToDB) {
		auto it = std::find(m_trigger.begin(), m_trigger.end(), trigger);
		if (it == m_trigger.end()) {
			return false;
		}

		m_trigger.erase(it);

		if (applyToDB) {
			try {
				m_sql << trigger.dropQuery(*this, m_backend);
			} catch (const soci::soci_error &e) {
				throw AccessException("Failed at dropping trigger \"" + trigger.getName() + "\": " + e.what());
			}
		}

		return true;
	}

#define THROW_FORMATERROR(msg) throw FormatException(std::string("JSON-Import (table \"") + m_name + "\"): " + msg)
	void Table::importFromJSON(const nlohmann::json &json, bool create) {
		assert(!m_name.empty());

		if (!json.is_object()) {
			THROW_FORMATERROR("Expected table to represented as a single JSON object");
		}
		// Validate that the expected fields are present and of the expected type
		std::vector< std::pair< std::string, nlohmann::json::value_t > > expectedFields = {
			{ "column_names", nlohmann::json::value_t::array },
			{ "column_types", nlohmann::json::value_t::array },
			{ "rows", nlohmann::json::value_t::array }
		};
		for (const std::pair< std::string, nlohmann::json::value_t > &currentPair : expectedFields) {
			if (!json.contains(currentPair.first)) {
				THROW_FORMATERROR("Table specification is missing the \"" + currentPair.first + "\" field");
			}
			if (json[currentPair.first].type() != currentPair.second) {
				THROW_FORMATERROR("Field \"" + currentPair.first + "\" is of the wrong type");
			}
		}
		// Validate that there are no extra fields
		if (json.size() > expectedFields.size()) {
			THROW_FORMATERROR("Table spec is expected to contain only " + std::to_string(expectedFields.size())
							  + " but contained " + std::to_string(json.size()));
		}

		const nlohmann::json &colNames = json["column_names"];
		const nlohmann::json &colTypes = json["column_types"];
		const nlohmann::json &rows     = json["rows"];

		// Some more validations
		if (colNames.size() != colTypes.size()) {
			THROW_FORMATERROR("Amount of column names (" + std::to_string(colNames.size())
							  + " does not match column types (" + std::to_string(colTypes.size()) + ")");
		}
		for (std::size_t i = 0; i < colNames.size(); ++i) {
			if (!colNames[i].is_string()) {
				THROW_FORMATERROR("Encountered non-string column name specification at position "
								  + std::to_string(i + 1));
			}
			if (!colTypes[i].is_string()) {
				THROW_FORMATERROR("Encountered non-string column type specification at position "
								  + std::to_string(i + 1));
			}
			if (boost::contains(colNames[i].get< std::string >(), " ")) {
				THROW_FORMATERROR("Invalid column name \"" + colNames[i].get< std::string >() + "\"");
			}
			try {
				// Check if we can convert the given string to a known data type
				DataType::fromSQLRepresentation(colTypes[i].get< std::string >());
			} catch (const UnknownDataTypeException &e) {
				THROW_FORMATERROR("Unknown column type \"" + colTypes[i].get< std::string >() + "\" for column \""
								  + colNames[i].get< std::string >() + "\": " + e.what());
			}
		}
		for (std::size_t i = 0; i < rows.size(); ++i) {
			const nlohmann::json &currentRow = rows.at(i);

			if (!currentRow.is_array()) {
				THROW_FORMATERROR("Row entry " + std::to_string(i + 1) + " is not of type array");
			}
			if (currentRow.size() != colNames.size()) {
				THROW_FORMATERROR("Row " + std::to_string(i + 1) + " contains " + std::to_string(currentRow.size())
								  + " entries, but " + std::to_string(colNames.size()) + " were expected");
			}
		}

		if (!m_columns.empty()) {
			// Make sure that the specified columns and types match with our stored specification
			if (m_columns.size() != colNames.size()) {
				THROW_FORMATERROR("Attempted to import " + std::to_string(colNames.size())
								  + " into a pre-defined table that only contains " + std::to_string(m_columns.size())
								  + " columns");
			}
			for (std::size_t i = 0; i < m_columns.size(); ++i) {
				std::string currentName = colNames[i].get< std::string >();
				const Column *col       = findColumn(currentName);

				if (!col) {
					THROW_FORMATERROR("A column with the name \"" + currentName
									  + "\" is not part of the pre-defined columns for this table");
				}
				if (col->getType().sqlRepresentation() != colTypes[i].get< std::string >()) {
					THROW_FORMATERROR("Column type mismatch for column \"" + currentName + "\": Expected: \""
									  + col->getType().sqlRepresentation() + "\", got \""
									  + colTypes[i].get< std::string >() + "\"");
				}
			}
		} else {
			// Import columns as specified
			m_columns.resize(colNames.size());
			for (std::size_t i = 0; i < colNames.size(); ++i) {
				Column column(colNames[i].get< std::string >(),
							  DataType::fromSQLRepresentation(colTypes[i].get< std::string >()));

				m_columns[i] = std::move(column);
			}
		}

		if (create) {
			// Now we have all information together that we need in order to create the table
			this->create();
		}

		// From this point on we are assuming that the table represented by this object actually exists in the
		// respective database, so we can now start inserting the provided data into it.
		std::string query            = "INSERT INTO \"" + m_name + "\" (";
		std::string valuePlaceholder = "";
		for (std::size_t i = 0; i < colNames.size(); ++i) {
			query += colNames[i].get< std::string >();
			valuePlaceholder += ":" + colNames[i].get< std::string >();

			if (i + 1 < colNames.size()) {
				query += ", ";
				valuePlaceholder += ", ";
			}
		}
		query += ") VALUES(" + valuePlaceholder + ")";

		// We assume that this function will be called from a place that already started a transaction
		// on the DB, so we can't initiate another one here.
		soci::statement stmt = m_sql.prepare << query;

		std::vector< std::string > values;
		values.reserve(colNames.size());
		for (const nlohmann::json &currentRow : rows) {
			assert(currentRow.size() == colNames.size());

			// We have to first transfer our values into the values vector in order to guarantee that they
			// are not destroyed in the middle of the DB statement (which might happen, if we were to use
			// the temporaries directly)
			for (const nlohmann::json &currentVal : currentRow) {
				values.push_back(utils::to_string(currentVal));
			}
			for (std::size_t i = 0; i < values.size(); ++i) {
				stmt.exchange(soci::use(values[i]));
			}

			stmt.define_and_bind();
			stmt.execute(true);
			stmt.bind_clean_up();

			values.clear();
		}
	}
#undef THROW_FORMATERROR

	nlohmann::json Table::exportToJSON() {
		assert(!m_columns.empty());
		assert(!m_name.empty());

		nlohmann::json json;

		std::string query = "SELECT ";
		for (const Column &currentColumn : m_columns) {
			json["column_names"].push_back(currentColumn.getName());
			json["column_types"].push_back(currentColumn.getType().sqlRepresentation());

			query += currentColumn.getName() + ", ";
		}
		// Remove trailing ", "
		query.erase(query.size() - 2);

		query += " FROM \"" + m_name + "\"";

		nlohmann::json rows = nlohmann::json::array_t();

		try {
			soci::rowset< soci::row > rowSet = m_sql.prepare << query;

			for (auto it = rowSet.begin(); it != rowSet.end(); ++it) {
				const soci::row &currentRow = *it;

				nlohmann::json jsonRow = nlohmann::json::array_t();
				for (std::size_t i = 0; i < currentRow.size(); ++i) {
					jsonRow.push_back(utils::to_json(currentRow, i));
				}

				rows.push_back(std::move(jsonRow));
			}

			json["rows"] = std::move(rows);
		} catch (const soci::soci_error &e) {
			throw AccessException(e.what());
		}

		return json;
	}

	void Table::performCtorAssertions() {
		// Names with spaces are not allowed as these cause issues
		assert(!boost::contains(m_name, " "));
#ifndef NDEBUG
		for (const Column &currentColumn : m_columns) {
			assert(!boost::contains(currentColumn.getName(), " "));
		}
#endif

		// We reserve the name for a table's backup (needed during migrations) right from the start
		assert(!boost::ends_with(m_name, Table::BACKUP_SUFFIX));
	}

} // namespace db
} // namespace mumble
