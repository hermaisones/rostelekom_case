#include "rectanglewindow.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>

namespace {

bool prepareFigureCounts(const QSqlDatabase &database, QSqlQuery &schema)
{
    if (!database.record(QStringLiteral("rectangles")).contains(QStringLiteral("type_count"))) {
        if (!schema.exec(QStringLiteral(
                "ALTER TABLE rectangles ADD COLUMN type_count INTEGER NOT NULL DEFAULT 1 CHECK (type_count > 0)")) ||
            !schema.exec(R"sql(
                WITH counts AS (SELECT type, COUNT(*) AS total FROM rectangles GROUP BY type)
                UPDATE rectangles
                SET type_count = (SELECT total FROM counts WHERE counts.type = rectangles.type)
            )sql")) {
            return false;
        }
    }

    return schema.exec(R"sql(
        CREATE TRIGGER IF NOT EXISTS update_figure_count_after_insert
        AFTER INSERT ON rectangles
        BEGIN
            UPDATE rectangles
            SET type_count = (SELECT COUNT(*) FROM rectangles WHERE type = NEW.type)
            WHERE type = NEW.type;
        END
    )sql") && schema.exec(R"sql(
        CREATE TRIGGER IF NOT EXISTS update_figure_count_after_delete
        AFTER DELETE ON rectangles
        BEGIN
            UPDATE rectangles
            SET type_count = (SELECT COUNT(*) FROM rectangles WHERE type = OLD.type)
            WHERE type = OLD.type;
        END
    )sql") && schema.exec(R"sql(
        CREATE TRIGGER IF NOT EXISTS update_figure_count_after_type_change
        AFTER UPDATE OF type ON rectangles
        WHEN OLD.type <> NEW.type
        BEGIN
            UPDATE rectangles
            SET type_count = (SELECT COUNT(*) FROM rectangles WHERE type = OLD.type)
            WHERE type = OLD.type;
            UPDATE rectangles
            SET type_count = (SELECT COUNT(*) FROM rectangles WHERE type = NEW.type)
            WHERE type = NEW.type;
        END
    )sql");
}

bool prepareFigureSchema(const QSqlDatabase &database, QString &error)
{
    QSqlQuery schema(database);
    // Enable foreign keys before the transaction and serialize schema changes.
    if (!schema.exec(QStringLiteral("PRAGMA foreign_keys = ON")) ||
        !schema.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
        error = schema.lastError().text();
        return false;
    }

    const bool tableReady = schema.exec(R"sql(
        CREATE TABLE IF NOT EXISTS rectangles (
            id INTEGER PRIMARY KEY,
            x INTEGER NOT NULL,
            y INTEGER NOT NULL,
            width INTEGER NOT NULL CHECK (width > 0),
            height INTEGER NOT NULL CHECK (height > 0)
        )
    )sql");
    const bool typeReady = tableReady &&
        (database.record(QStringLiteral("rectangles")).contains(QStringLiteral("type")) ||
         schema.exec(QStringLiteral("ALTER TABLE rectangles ADD COLUMN type TEXT NOT NULL DEFAULT 'rectangle'")));

    const bool linksReady = typeReady && schema.exec(R"sql(
        CREATE TABLE IF NOT EXISTS figure_links (
            first_id INTEGER NOT NULL REFERENCES rectangles(id) ON DELETE CASCADE,
            second_id INTEGER NOT NULL REFERENCES rectangles(id) ON DELETE CASCADE,
            PRIMARY KEY (first_id, second_id),
            CHECK (first_id < second_id)
        )
    )sql");

    if (linksReady && prepareFigureCounts(database, schema) && schema.exec(QStringLiteral("COMMIT"))) {
        return true;
    }

    error = schema.lastError().text();
    schema.exec(QStringLiteral("ROLLBACK"));
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RectangleLab"));

    const QDir dataDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    if (!dataDir.mkpath(QStringLiteral("."))) {
        QMessageBox::critical(nullptr, QStringLiteral("Ошибка запуска"),
                              QStringLiteral("Не удалось создать папку для базы данных: %1")
                                  .arg(dataDir.absolutePath()));
        return 1;
    }

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    database.setDatabaseName(dataDir.filePath(QStringLiteral("rectangles.sqlite")));
    if (!database.open()) {
        QMessageBox::critical(nullptr, QStringLiteral("Ошибка открытия SQLite"), database.lastError().text());
        return 1;
    }

    QString schemaError;
    if (!prepareFigureSchema(database, schemaError)) {
        QMessageBox::critical(nullptr, QStringLiteral("Ошибка подготовки базы SQLite"), schemaError);
        return 1;
    }

    RectangleWindow window(database);
    window.show();
    return app.exec();
}
