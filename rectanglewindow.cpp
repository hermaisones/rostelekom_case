#include "rectanglewindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHelpEvent>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPolygonF>
#include <QPushButton>
#include <QRectF>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStringList>
#include <QTableView>
#include <QToolTip>
#include <QVBoxLayout>
#include <utility>

namespace {

const QString rectangleType = QStringLiteral("rectangle");
const QString ellipseType = QStringLiteral("ellipse");
const QString polygonType = QStringLiteral("polygon");

constexpr int countIconWidth = 14;
constexpr int countIconHeight = 11;
constexpr int countIconGap = 4;

QString figureTypeName(const QString &type)
{
    if (type == rectangleType) {
        return QStringLiteral("Прямоугольник");
    }
    if (type == ellipseType) {
        return QStringLiteral("Эллипс");
    }
    if (type == polygonType) {
        return QStringLiteral("Многоугольник");
    }
    return type;
}

QPainterPath figurePath(const QString &type, const QRectF &bounds)
{
    QPainterPath path;
    if (type == ellipseType) {
        path.addEllipse(bounds);
    } else if (type == polygonType) {
        QPolygonF triangle;
        triangle << QPointF(bounds.center().x(), bounds.top())
                 << bounds.bottomLeft() << bounds.bottomRight();
        path.addPolygon(triangle);
        path.closeSubpath();
    } else {
        path.addRect(bounds);
    }
    return path;
}

} // namespace

RectangleWindow::FigureItem::FigureItem(RectangleWindow &window, const QPainterPath &path,
                                       const QPointF &position)
    : QGraphicsPathItem(path), window(window)
{
    setPos(position);
    setAcceptedMouseButtons(Qt::LeftButton);
    setFlags(ItemIsMovable | ItemSendsGeometryChanges);
}

QVariant RectangleWindow::FigureItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange) {
        const QPointF position = value.toPointF();
        return QPointF(qRound64(position.x()), qRound64(position.y()));
    }
    if (change == ItemPositionHasChanged) {
        window.updateLinkLines(this);
    }
    return QGraphicsPathItem::itemChange(change, value);
}

void RectangleWindow::FigureItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    dragStart = pos();
    QGraphicsPathItem::mousePressEvent(event);
}

void RectangleWindow::FigureItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsPathItem::mouseReleaseEvent(event);
    if (pos() == dragStart) {
        return;
    }

    if (!window.saveFigurePosition(*this)) {
        setPos(dragStart);
    }
    window.updateSceneBounds();
}

QString RectangleWindow::FigureTypeDelegate::displayText(const QVariant &value, const QLocale &) const
{
    return figureTypeName(value.toString());
}

RectangleWindow::FigureCountDelegate::FigureCountDelegate(int typeColumn, QObject *parent)
    : QStyledItemDelegate(parent), typeColumn(typeColumn)
{
}

void RectangleWindow::FigureCountDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                                 const QModelIndex &index) const
{
    const qint64 count = index.data().toLongLong();
    if (count < 1) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    int iconCount = 1;
    if (count > 10) {
        iconCount = 3;
    } else if (count > 3) {
        iconCount = 2;
    }

    QStyleOptionViewItem cell(option);
    initStyleOption(&cell, index);
    cell.text.clear();
    const QStyle *style = cell.widget ? cell.widget->style() : QApplication::style();

    const QString type = index.siblingAtColumn(typeColumn).data().toString();
    const int groupWidth = iconCount * countIconWidth + (iconCount - 1) * countIconGap;
    const qreal x = cell.rect.x() + (cell.rect.width() - groupWidth) / 2.0;
    const qreal y = cell.rect.y() + (cell.rect.height() - countIconHeight) / 2.0;
    const auto textRole = cell.state.testFlag(QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text;

    painter->save();
    painter->setClipRect(cell.rect, Qt::IntersectClip);
    style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, cell.widget);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(cell.palette.color(textRole), 1.2));
    painter->setBrush(Qt::NoBrush);
    for (int i = 0; i < iconCount; ++i) {
        const QRectF bounds(x + i * (countIconWidth + countIconGap), y, countIconWidth, countIconHeight);
        painter->drawPath(figurePath(type, bounds));
    }
    painter->restore();
}

QSize RectangleWindow::FigureCountDelegate::sizeHint(const QStyleOptionViewItem &option,
                                                     const QModelIndex &index) const
{
    const QSize iconsSize(3 * countIconWidth + 2 * countIconGap + 10, countIconHeight + 6);
    return iconsSize.expandedTo(QStyledItemDelegate::sizeHint(option, index));
}

bool RectangleWindow::FigureCountDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view,
                                                     const QStyleOptionViewItem &option,
                                                     const QModelIndex &index)
{
    if (event && view && event->type() == QEvent::ToolTip && index.isValid()) {
        QToolTip::showText(event->globalPos(), QStringLiteral("Фигур этого типа — %1").arg(index.data().toLongLong()),
                           view->viewport(), option.rect);
        return true;
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}

RectangleWindow::RectangleWindow(const QSqlDatabase &database)
{
    setWindowTitle(QStringLiteral("Фигуры"));
    resize(840, 500);

    auto *content = new QWidget(this);
    setCentralWidget(content);
    auto *layout = new QVBoxLayout(content);
    auto *controls = new QHBoxLayout;
    layout->addLayout(controls);

    typeInput = new QComboBox(content);
    typeInput->addItem(figureTypeName(rectangleType), rectangleType);
    typeInput->addItem(figureTypeName(ellipseType), ellipseType);
    typeInput->addItem(figureTypeName(polygonType), polygonType);
    auto *typeLabel = new QLabel(QStringLiteral("&Тип"), content);
    typeLabel->setBuddy(typeInput);
    controls->addWidget(typeLabel);
    controls->addWidget(typeInput);

    widthInput = new QSpinBox(content);
    widthInput->setRange(10, 500);
    widthInput->setValue(120);
    auto *widthLabel = new QLabel(QStringLiteral("&Ширина, px"), content);
    widthLabel->setBuddy(widthInput);
    controls->addWidget(widthLabel);
    controls->addWidget(widthInput);

    heightInput = new QSpinBox(content);
    heightInput->setRange(10, 500);
    heightInput->setValue(70);
    auto *heightLabel = new QLabel(QStringLiteral("&Высота, px"), content);
    heightLabel->setBuddy(heightInput);
    controls->addWidget(heightLabel);
    controls->addWidget(heightInput);

    auto *addButton = new QPushButton(QStringLiteral("Добавить фигуру"), content);
    controls->addWidget(addButton);
    connect(addButton, &QPushButton::clicked, this, &RectangleWindow::addFigure);

    auto *linkButton = new QPushButton(QStringLiteral("Соединить фигуры"), content);
    linkButton->setToolTip(QStringLiteral("Выберите две строки таблицы, удерживая Ctrl"));
    controls->addWidget(linkButton);
    connect(linkButton, &QPushButton::clicked, this, &RectangleWindow::addLink);

    auto *deleteButton = new QPushButton(QStringLiteral("Удалить фигуру"), content);
    deleteButton->setToolTip(QStringLiteral("Удалить выбранную фигуру и все её связи"));
    controls->addWidget(deleteButton);
    connect(deleteButton, &QPushButton::clicked, this, &RectangleWindow::deleteFigure);
    controls->addStretch();

    auto *filters = new QHBoxLayout;
    layout->addLayout(filters);

    idFilterInput = new QLineEdit(content);
    idFilterInput->setPlaceholderText(QStringLiteral("Любой"));
    idFilterInput->setMaximumWidth(100);
    auto *idFilterLabel = new QLabel(QStringLiteral("№ &фигуры"), content);
    idFilterLabel->setBuddy(idFilterInput);
    filters->addWidget(idFilterLabel);
    filters->addWidget(idFilterInput);

    typeFilterInput = new QComboBox(content);
    typeFilterInput->addItem(QStringLiteral("Все типы"));
    for (int index = 0; index < typeInput->count(); ++index) {
        typeFilterInput->addItem(typeInput->itemText(index), typeInput->itemData(index));
    }
    filters->addWidget(typeFilterInput);

    countFilterInput = new QLineEdit(content);
    countFilterInput->setPlaceholderText(QStringLiteral("Любое"));
    countFilterInput->setMaximumWidth(80);
    auto *countFilterLabel = new QLabel(QStringLiteral("&Фигур этого типа"), content);
    countFilterLabel->setBuddy(countFilterInput);
    filters->addWidget(countFilterLabel);
    filters->addWidget(countFilterInput);

    auto *applyButton = new QPushButton(QStringLiteral("Фильтровать"), content);
    auto *resetButton = new QPushButton(QStringLiteral("Сбросить"), content);
    filters->addWidget(applyButton);
    filters->addWidget(resetButton);
    filters->addStretch();
    connect(applyButton, &QPushButton::clicked, this, &RectangleWindow::applyFilters);
    connect(resetButton, &QPushButton::clicked, this, &RectangleWindow::resetFilters);
    connect(idFilterInput, &QLineEdit::returnPressed, this, &RectangleWindow::applyFilters);
    connect(countFilterInput, &QLineEdit::returnPressed, this, &RectangleWindow::applyFilters);

    auto *views = new QHBoxLayout;
    layout->addLayout(views, 1);

    scene = new QGraphicsScene(this);
    graphicsView = new QGraphicsView(scene, content);
    graphicsView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    views->addWidget(graphicsView, 3);

    model = new QSqlTableModel(this, database);
    model->setTable(QStringLiteral("rectangles"));
    const int idColumn = model->fieldIndex(QStringLiteral("id"));
    const int typeColumn = model->fieldIndex(QStringLiteral("type"));
    const int countColumn = model->fieldIndex(QStringLiteral("type_count"));
    model->setSort(idColumn, Qt::AscendingOrder);
    model->setHeaderData(idColumn, Qt::Horizontal, QStringLiteral("№"));
    model->setHeaderData(typeColumn, Qt::Horizontal, QStringLiteral("Тип"));
    model->setHeaderData(countColumn, Qt::Horizontal, QStringLiteral("Всего"));
    model->setHeaderData(model->fieldIndex(QStringLiteral("width")), Qt::Horizontal,
                         QStringLiteral("Ширина"));
    model->setHeaderData(model->fieldIndex(QStringLiteral("height")), Qt::Horizontal,
                         QStringLiteral("Высота"));

    table = new QTableView(content);
    table->setModel(model);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setItemDelegateForColumn(typeColumn, new FigureTypeDelegate(table));
    table->setItemDelegateForColumn(countColumn, new FigureCountDelegate(typeColumn, table));
    table->verticalHeader()->hide();
    auto *header = table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Stretch);
    header->setSectionResizeMode(idColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(typeColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(countColumn, QHeaderView::ResizeToContents);
    views->addWidget(table, 2);

    reloadFigures();
}

void RectangleWindow::addFigure()
{
    QSqlQuery insert(model->database());
    // Place the next figure below the existing ones, or at y = 20 on an empty scene.
    if (!insert.prepare(R"sql(
        INSERT INTO rectangles (x, y, width, height, type)
        VALUES (20, (SELECT COALESCE(MAX(y + height) + 20, 20) FROM rectangles),
                :width, :height, :type)
    )sql")) {
        QMessageBox::critical(this, QStringLiteral("Ошибка сохранения"), insert.lastError().text());
        return;
    }
    insert.bindValue(QStringLiteral(":width"), widthInput->value());
    insert.bindValue(QStringLiteral(":height"), heightInput->value());
    insert.bindValue(QStringLiteral(":type"), typeInput->currentData());

    if (!insert.exec()) {
        QMessageBox::critical(this, QStringLiteral("Ошибка сохранения"), insert.lastError().text());
        return;
    }

    const qint64 id = insert.lastInsertId().toLongLong();
    insert.finish();
    if (reloadFigures(id)) {
        statusBar()->showMessage(QStringLiteral("Добавлена фигура №%1").arg(id), 3000);
    }
}

void RectangleWindow::addLink()
{
    const auto rows = table->selectionModel()->selectedRows(model->fieldIndex(QStringLiteral("id")));
    if (rows.size() != 2) {
        statusBar()->showMessage(QStringLiteral("Выберите две строки таблицы, удерживая Ctrl"), 5000);
        return;
    }

    qint64 firstId = rows.at(0).data().toLongLong();
    qint64 secondId = rows.at(1).data().toLongLong();
    if (firstId > secondId) {
        std::swap(firstId, secondId);
    }

    QSqlQuery insert(model->database());
    if (!insert.prepare(R"sql(
        INSERT INTO figure_links (first_id, second_id)
        VALUES (:first_id, :second_id)
        ON CONFLICT (first_id, second_id) DO NOTHING
    )sql")) {
        QMessageBox::critical(this, QStringLiteral("Ошибка сохранения связи"), insert.lastError().text());
        return;
    }
    insert.bindValue(QStringLiteral(":first_id"), firstId);
    insert.bindValue(QStringLiteral(":second_id"), secondId);
    if (!insert.exec()) {
        QMessageBox::critical(this, QStringLiteral("Ошибка сохранения связи"), insert.lastError().text());
        return;
    }

    const bool inserted = insert.numRowsAffected() > 0;
    insert.finish();
    if (reloadLinks()) {
        const QString message = inserted
            ? QStringLiteral("Соединены фигуры №%1 и №%2").arg(firstId).arg(secondId)
            : QStringLiteral("Эти фигуры уже соединены");
        statusBar()->showMessage(message, 3000);
    }
}

void RectangleWindow::deleteFigure()
{
    const auto rows = table->selectionModel()->selectedRows(model->fieldIndex(QStringLiteral("id")));
    if (rows.size() != 1) {
        statusBar()->showMessage(QStringLiteral("Выберите одну строку таблицы для удаления"), 5000);
        return;
    }

    const qint64 id = rows.constFirst().data().toLongLong();
    QSqlQuery remove(model->database());
    if (!remove.prepare(QStringLiteral("DELETE FROM rectangles WHERE id = :id"))) {
        QMessageBox::critical(this, QStringLiteral("Ошибка удаления фигуры"), remove.lastError().text());
        return;
    }
    remove.bindValue(QStringLiteral(":id"), id);
    if (!remove.exec()) {
        QMessageBox::critical(this, QStringLiteral("Ошибка удаления фигуры"), remove.lastError().text());
        return;
    }

    const bool removed = remove.numRowsAffected() == 1;
    remove.finish();
    if (reloadFigures()) {
        const QString message = removed
            ? QStringLiteral("Удалена фигура №%1").arg(id)
            : QStringLiteral("Фигура №%1 отсутствует в базе").arg(id);
        statusBar()->showMessage(message, 3000);
    }
}

void RectangleWindow::applyFilters()
{
    const QString idText = idFilterInput->text().trimmed();
    const QString countText = countFilterInput->text().trimmed();
    bool validId = false;
    bool validCount = false;
    const qint64 id = idText.toLongLong(&validId);
    const qint64 count = countText.toLongLong(&validCount);
    if ((!idText.isEmpty() && (!validId || id < 1)) ||
        (!countText.isEmpty() && (!validCount || count < 1))) {
        statusBar()->showMessage(QStringLiteral("Номер и количество должны быть положительными целыми числами"), 5000);
        return;
    }

    QStringList conditions;
    if (validId) {
        conditions.append(QStringLiteral("id = %1").arg(id));
    }
    const QString type = typeFilterInput->currentData().toString();
    if (!type.isEmpty()) {
        conditions.append(QStringLiteral("type = '%1'").arg(type));
    }
    if (validCount) {
        conditions.append(QStringLiteral("type_count = %1").arg(count));
    }

    model->setFilter(conditions.join(QStringLiteral(" AND ")));
    if (model->lastError().isValid()) {
        QMessageBox::critical(this, QStringLiteral("Ошибка фильтрации"), model->lastError().text());
    } else if (!conditions.isEmpty() && model->rowCount() == 0) {
        statusBar()->showMessage(QStringLiteral("По фильтрам фигур не найдено"), 3000);
    } else {
        statusBar()->clearMessage();
    }
}

void RectangleWindow::resetFilters()
{
    idFilterInput->clear();
    typeFilterInput->setCurrentIndex(0);
    countFilterInput->clear();
    applyFilters();
}

bool RectangleWindow::reloadFigures(qint64 selectedId)
{
    if (!model->select()) {
        QMessageBox::critical(this, QStringLiteral("Ошибка чтения"), model->lastError().text());
        return false;
    }
    while (model->canFetchMore()) {
        model->fetchMore();
    }
    table->hideColumn(model->fieldIndex(QStringLiteral("x")));
    table->hideColumn(model->fieldIndex(QStringLiteral("y")));

    // The scene uses all stored figures regardless of the table's filter.
    QSqlQuery query(model->database());
    if (!query.exec(QStringLiteral("SELECT id, x, y, width, height, type FROM rectangles ORDER BY id"))) {
        QMessageBox::critical(this, QStringLiteral("Ошибка чтения фигур"), query.lastError().text());
        return false;
    }
    QList<QSqlRecord> storedFigures;
    while (query.next()) {
        storedFigures.append(query.record());
    }
    if (query.lastError().isValid()) {
        QMessageBox::critical(this, QStringLiteral("Ошибка чтения фигур"), query.lastError().text());
        return false;
    }

    links.clear();
    figures.clear();
    scene->clear();
    const QPen outline(QColor(QStringLiteral("#253958")));
    const QBrush fill(QColor(QStringLiteral("#a5caef")));

    for (const auto &record : storedFigures) {
        const qint64 id = record.value(QStringLiteral("id")).toLongLong();
        const QString type = record.value(QStringLiteral("type")).toString();
        const QPointF position(record.value(QStringLiteral("x")).toLongLong(),
                               record.value(QStringLiteral("y")).toLongLong());
        const QRectF bounds(0, 0,
                            record.value(QStringLiteral("width")).toInt(),
                            record.value(QStringLiteral("height")).toInt());
        auto *item = new FigureItem(*this, figurePath(type, bounds), position);
        item->setPen(outline);
        item->setBrush(fill);
        item->setData(0, id);
        item->setToolTip(QStringLiteral("Фигура №%1 (%2)").arg(id).arg(figureTypeName(type)));
        scene->addItem(item);
        figures.insert(id, item);
    }

    const bool linksLoaded = reloadLinks();
    updateSceneBounds();
    if (auto *selectedItem = figures.value(selectedId)) {
        const int idColumn = model->fieldIndex(QStringLiteral("id"));
        for (int row = 0; row < model->rowCount(); ++row) {
            if (model->index(row, idColumn).data().toLongLong() == selectedId) {
                table->selectRow(row);
                break;
            }
        }
        graphicsView->ensureVisible(selectedItem);
    }
    return linksLoaded;
}

bool RectangleWindow::reloadLinks()
{
    QSqlQuery query(model->database());
    if (!query.exec(QStringLiteral("SELECT first_id, second_id FROM figure_links"))) {
        QMessageBox::critical(this, QStringLiteral("Ошибка чтения связей"), query.lastError().text());
        return false;
    }

    QList<std::pair<FigureItem *, FigureItem *>> endpoints;
    while (query.next()) {
        auto *first = figures.value(query.value(0).toLongLong());
        auto *second = figures.value(query.value(1).toLongLong());
        if (first && second) {
            endpoints.emplaceBack(first, second);
        }
    }
    if (query.lastError().isValid()) {
        QMessageBox::critical(this, QStringLiteral("Ошибка чтения связей"), query.lastError().text());
        return false;
    }

    for (const auto &link : links) {
        delete link.line;
    }
    links.clear();

    const QPen pen(graphicsView->palette().color(QPalette::Text), 2);
    for (const auto &[first, second] : endpoints) {
        auto *line = scene->addLine(QLineF(), pen);
        line->setZValue(-1);
        links.append({first, second, line});
    }
    updateLinkLines();
    return true;
}

void RectangleWindow::updateLinkLines(const FigureItem *movedFigure)
{
    for (const auto &link : links) {
        if (!movedFigure || link.first == movedFigure || link.second == movedFigure) {
            link.line->setLine(QLineF(link.first->sceneBoundingRect().center(),
                                      link.second->sceneBoundingRect().center()));
        }
    }
}

bool RectangleWindow::saveFigurePosition(const FigureItem &figure)
{
    QSqlQuery update(model->database());
    if (!update.prepare(QStringLiteral("UPDATE rectangles SET x = :x, y = :y WHERE id = :id"))) {
        QMessageBox::critical(this, QStringLiteral("Ошибка сохранения позиции"), update.lastError().text());
        return false;
    }
    update.bindValue(QStringLiteral(":x"), qRound64(figure.x()));
    update.bindValue(QStringLiteral(":y"), qRound64(figure.y()));
    update.bindValue(QStringLiteral(":id"), figure.data(0));
    if (!update.exec()) {
        QMessageBox::critical(this, QStringLiteral("Ошибка сохранения позиции"), update.lastError().text());
        return false;
    }
    if (update.numRowsAffected() != 1) {
        statusBar()->showMessage(QStringLiteral("Эта фигура отсутствует в базе"), 5000);
        return false;
    }
    return true;
}

void RectangleWindow::updateSceneBounds()
{
    scene->setSceneRect(scene->itemsBoundingRect().adjusted(-20, -20, 20, 20));
}
