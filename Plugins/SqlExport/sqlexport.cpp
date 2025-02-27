#include "sqlexport.h"
#include "common/utils_sql.h"
#include "sqlitestudio.h"
#include "config_builder.h"
#include "services/exportmanager.h"
#include "common/unused.h"
#include "services/codeformatter.h"
#include <QTextCodec>

SqlExport::SqlExport()
{
}

QString SqlExport::getFormatName() const
{
    return "SQL";
}

ExportManager::StandardConfigFlags SqlExport::standardOptionsToEnable() const
{
    return ExportManager::CODEC;
}

CfgMain* SqlExport::getConfig()
{
    return &cfg;
}

QString SqlExport::defaultFileExtension() const
{
    return "sql";
}

QString SqlExport::getExportConfigFormName() const
{
    if (exportMode == ExportManager::QUERY_RESULTS)
        return "sqlExportQueryConfig";

    return "sqlExportCommonConfig";
}

bool SqlExport::beforeExportQueryResults(const QString& query, QList<QueryExecutor::ResultColumnPtr>& columns, const QHash<ExportManager::ExportProviderFlag, QVariant> providedData)
{
    UNUSED(providedData);
    static_qstring(dropDdl, "DROP TABLE IF EXISTS %1;");

    QStringList colDefs;
    for (QueryExecutor::ResultColumnPtr resCol : columns)
        colDefs << wrapObjIfNeeded(resCol->displayName);

    this->columns = colDefs.join(", ");

    writeHeader();
    if (cfg.SqlExport.IncludeQueryInComments.get())
    {
        writeln(tr("-- Results of query:"));
        writeln(commentAllSqlLines(query));
        writeln("--");
    }

    writeBegin();

    theTable = wrapObjIfNeeded(cfg.SqlExport.QueryTable.get());
    if (!cfg.SqlExport.GenerateCreateTable.get())
        return true;

    QString ddl = "CREATE TABLE " + theTable + " (" + this->columns + ");";
    writeln("");

    if (cfg.SqlExport.GenerateDrop.get())
        writeln(formatQuery(dropDdl.arg(theTable)));

    writeln(formatQuery(ddl));
    return true;
}

bool SqlExport::exportQueryResultsRow(SqlResultsRowPtr row)
{
    QStringList argList = rowToArgList(row);
    QString argStr = argList.join(", ");
    QString sql = "INSERT INTO " + theTable + " (" + this->columns + ") VALUES (" + argStr + ");";
    writeln(sql);
    return true;
}

bool SqlExport::exportTable(const QString& database, const QString& table, const QStringList& columnNames, const QString& ddl, SqliteCreateTablePtr createTable, const QHash<ExportManager::ExportProviderFlag, QVariant> providedData)
{
    UNUSED(createTable);
    UNUSED(providedData);

    tableGeneratedColumns.clear();
    for (SqliteCreateTable::Column* col : createTable->columns)
    {
        if (col->hasConstraint(SqliteCreateTable::Column::Constraint::GENERATED))
            tableGeneratedColumns << col->name;
    }

    return exportTable(database, table, columnNames, ddl);
}

bool SqlExport::exportVirtualTable(const QString& database, const QString& table, const QStringList& columnNames, const QString& ddl, SqliteCreateVirtualTablePtr createTable, const QHash<ExportManager::ExportProviderFlag, QVariant> providedData)
{
    UNUSED(createTable);
    UNUSED(providedData);
    return exportTable(database, table, columnNames, ddl);
}

bool SqlExport::exportTable(const QString& database, const QString& table, const QStringList& columnNames, const QString& ddl)
{
    static_qstring(dropDdl, "DROP TABLE IF EXISTS %1;");

    generatedColumnIndexes.clear();
    QStringList colList;
    int colIdx = 0;
    for (const QString& colName : columnNames)
    {
        if (tableGeneratedColumns.contains(colName, Qt::CaseInsensitive))
        {
            generatedColumnIndexes << colIdx++;
            continue;
        }

        colList << wrapObjIfNeeded(colName);
        colIdx++;
    }

    columns = colList.join(", ");

    if (isTableExport())
    {
        writeHeader();
        writeFkDisable();
        writeBegin();
    }

    QString fullName = getNameForObject(database, table, false);
    writeln("");
    writeln(tr("-- Table: %1").arg(fullName));

    theTable = getNameForObject(database, table, true);

    if (cfg.SqlExport.GenerateDrop.get())
        writeln(formatQuery(dropDdl.arg(theTable)));

    writeln(formatQuery(ddl));
    return true;
}

bool SqlExport::exportTableRow(SqlResultsRowPtr data)
{
    QStringList argList = rowToArgList(data, true);
    QString argStr = argList.join(", ");
    QString sql = "INSERT INTO " + theTable + " (" + columns + ") VALUES (" + argStr + ");";
    if (!cfg.SqlExport.FormatDdlsOnly.get())
        sql = formatQuery(sql);

    writeln(sql);
    return true;
}

bool SqlExport::afterExport()
{
    writeCommit();
    writeFkEnable();
    return true;
}

bool SqlExport::beforeExportDatabase(const QString& database)
{
    UNUSED(database);
    writeHeader();
    writeFkDisable();
    writeBegin();
    return true;
}

bool SqlExport::exportIndex(const QString& database, const QString& name, const QString& ddl, SqliteCreateIndexPtr createIndex)
{
    UNUSED(createIndex);
    static_qstring(dropDdl, "DROP INDEX IF EXISTS %1;");

    QString index = getNameForObject(database, name, false);
    writeln("");
    writeln(tr("-- Index: %1").arg(index));

    QString fullName = getNameForObject(database, name, true);
    if (cfg.SqlExport.GenerateDrop.get())
        writeln(formatQuery(dropDdl.arg(fullName)));

    writeln(formatQuery(ddl));
    return true;
}

bool SqlExport::exportTrigger(const QString& database, const QString& name, const QString& ddl, SqliteCreateTriggerPtr createTrigger)
{
    UNUSED(createTrigger);
    static_qstring(dropDdl, "DROP TRIGGER IF EXISTS %1;");

    QString trig = getNameForObject(database, name, false);
    writeln("");
    writeln(tr("-- Trigger: %1").arg(trig));

    QString fullName = getNameForObject(database, name, true);
    if (cfg.SqlExport.GenerateDrop.get())
        writeln(dropDdl.arg(fullName));

    writeln(formatQuery(formatQuery(ddl)));
    return true;
}

bool SqlExport::exportView(const QString& database, const QString& name, const QString& ddl, SqliteCreateViewPtr createView)
{
    UNUSED(createView);
    static_qstring(dropDdl, "DROP VIEW IF EXISTS %1;");

    QString view = getNameForObject(database, name, false);
    writeln("");
    writeln(tr("-- View: %1").arg(view));

    QString fullName = getNameForObject(database, name, true);
    if (cfg.SqlExport.GenerateDrop.get())
        writeln(dropDdl.arg(fullName));

    writeln(formatQuery(formatQuery(ddl)));
    return true;
}

void SqlExport::writeHeader()
{
    QDateTime ctime = QDateTime::currentDateTime();
    writeln("--");
    writeln(tr("-- File generated with SQLiteStudio v%1 on %2").arg(SQLITESTUDIO->getVersionString()).arg(ctime.toString()));
    writeln("--");
    if (standardOptionsToEnable().testFlag(ExportManager::CODEC))
    {
        writeln(tr("-- Text encoding used: %1").arg(QString::fromLatin1(codec->name())));
        writeln("--");
    }
}

void SqlExport::writeBegin()
{
    writeln("BEGIN TRANSACTION;");
}

void SqlExport::writeCommit()
{
    writeln("");
    writeln("COMMIT TRANSACTION;");
}

void SqlExport::writeFkDisable()
{
    writeln("PRAGMA foreign_keys = off;");
}

void SqlExport::writeFkEnable()
{
    writeln("PRAGMA foreign_keys = on;");
}

QString SqlExport::formatQuery(const QString& sql)
{
    if (cfg.SqlExport.UseFormatter.get())
        return FORMATTER->format("sql", sql, db);

    if (sql.trimmed().endsWith(";"))
        return sql;

    return sql.trimmed() + ";";
}

QString SqlExport::getNameForObject(const QString& database, const QString& name, bool wrapped)
{
    QString obj = wrapped ? wrapObjIfNeeded(name) : name;
    if (!database.isNull() && database.toLower() != "main")
        obj = (wrapped ?  wrapObjIfNeeded(database) : database) + "." + obj;

    return obj;
}

QStringList SqlExport::rowToArgList(SqlResultsRowPtr row, bool honorGeneratedColumns)
{
    if (honorGeneratedColumns)
    {
        QList<QVariant> filteredValues;
        int i = 0;
        for (const QVariant& value : row->valueList())
        {
            if (generatedColumnIndexes.contains(i++))
                continue;

            filteredValues << value;
        }
        return valueListToSqlList(filteredValues);
    }

    return valueListToSqlList(row->valueList());
}

void SqlExport::validateOptions()
{
    if (exportMode == ExportManager::QUERY_RESULTS)
    {
        bool valid = !cfg.SqlExport.QueryTable.get().isEmpty();
        EXPORT_MANAGER->handleValidationFromPlugin(valid, cfg.SqlExport.QueryTable, tr("Table name for INSERT statements is mandatory."));
    }

    bool useFormatter = cfg.SqlExport.UseFormatter.get();
    EXPORT_MANAGER->updateVisibilityAndEnabled(cfg.SqlExport.FormatDdlsOnly, true, useFormatter);
    if (!useFormatter)
        cfg.SqlExport.FormatDdlsOnly.set(false);

    if (exportMode == ExportManager::QUERY_RESULTS)
    {
        bool generateCreate = cfg.SqlExport.GenerateCreateTable.get();
        EXPORT_MANAGER->updateVisibilityAndEnabled(cfg.SqlExport.GenerateDrop, true, generateCreate);
        if (!generateCreate)
            cfg.SqlExport.GenerateDrop.set(false);
    }
}

bool SqlExport::init()
{
    SQLS_INIT_RESOURCE(sqlexport);
    return GenericExportPlugin::init();
}

void SqlExport::deinit()
{
    SQLS_CLEANUP_RESOURCE(sqlexport);
}
