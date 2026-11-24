#pragma once

#include <QGraphicsPathItem>
#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QStyledItemDelegate>
#include <QtGlobal>

class QComboBox;
class QGraphicsLineItem;
class QGraphicsScene;
class QGraphicsSceneMouseEvent;
class QGraphicsView;
class QHelpEvent;
class QLineEdit;
class QSpinBox;
class QSqlDatabase;
class QSqlTableModel;
class QTableView;

class RectangleWindow : public QMainWindow {
public:
    explicit RectangleWindow(const QSqlDatabase &database);

private:
    class FigureItem : public QGraphicsPathItem {
    public:
        FigureItem(RectangleWindow &window, const QPainterPath &path, const QPointF &position);

    protected:
        QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
        RectangleWindow &window;
        QPointF dragStart;
    };

    struct FigureLink {
        FigureItem *first;
        FigureItem *second;
        QGraphicsLineItem *line;
    };

    class FigureTypeDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;
        QString displayText(const QVariant &value, const QLocale &locale) const override;
    };

    class FigureCountDelegate : public QStyledItemDelegate {
    public:
        FigureCountDelegate(int typeColumn, QObject *parent);

        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                       const QModelIndex &index) override;

    private:
        int typeColumn;
    };

    void addFigure();
    void addLink();
    void deleteFigure();
    void applyFilters();
    void resetFilters();
    bool reloadFigures(qint64 selectedId = 0);
    bool reloadLinks();
    void updateLinkLines(const FigureItem *movedFigure = nullptr);
    bool saveFigurePosition(const FigureItem &figure);
    void updateSceneBounds();

    QComboBox *typeInput;
    QSpinBox *widthInput;
    QSpinBox *heightInput;
    QLineEdit *idFilterInput;
    QComboBox *typeFilterInput;
    QLineEdit *countFilterInput;
    QGraphicsScene *scene;
    QGraphicsView *graphicsView;
    QSqlTableModel *model;
    QTableView *table;
    QHash<qint64, FigureItem *> figures;
    QList<FigureLink> links;
};
