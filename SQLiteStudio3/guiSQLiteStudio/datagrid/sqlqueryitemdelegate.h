#ifndef SQLQUERYITEMDELEGATE_H
#define SQLQUERYITEMDELEGATE_H

#include "guiSQLiteStudio_global.h"
#include "db/sqlquery.h"
#include <QStyledItemDelegate>
#include <QSet>

class SqlQueryItem;
class QComboBox;
class QStandardItemModel;
class SqlQueryModel;
class SqlQueryView;

class GUI_API_EXPORT SqlQueryItemDelegate : public QStyledItemDelegate
{
        Q_OBJECT
    public:
        explicit SqlQueryItemDelegate(QObject *parent = 0);

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
        QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const;
        QString	displayText(const QVariant & value, const QLocale & locale) const;
        void setEditorData(QWidget * editor, const QModelIndex & index) const;
        void setModelData(QWidget * editor, QAbstractItemModel * model, const QModelIndex & index) const;
        void mouseLeftIndex(const QModelIndex& index);

    private:
        class FkComboShowFilter : public QObject
        {
            public:
                explicit FkComboShowFilter(const SqlQueryItemDelegate* delegate, SqlQueryView* comboView, QObject* parent = 0);
                bool eventFilter(QObject *obj, QEvent *event);

            private:
                const SqlQueryItemDelegate* delegate = nullptr;
                SqlQueryView* comboView = nullptr;
        };

        SqlQueryItem* getItem(const QModelIndex &index) const;
        QWidget* getEditor(int type, QWidget* parent) const;
        QWidget* getFkEditor(SqlQueryItem* item, QWidget* parent, const SqlQueryModel *model) const;
        void setEditorDataForLineEdit(QLineEdit* le, const QModelIndex& index) const;
        void setEditorDataForFk(QComboBox* cb, const QModelIndex& index) const;
        void setModelDataForFk(QComboBox* editor, QAbstractItemModel* model, const QModelIndex& index) const;
        void setModelDataForLineEdit(QLineEdit* editor, QAbstractItemModel* model, const QModelIndex& index) const;
        QString getSqlForFkEditor(SqlQueryItem* item) const;
        qlonglong getRowCountForFkEditor(Db* db, const QString& query, bool *isError) const;
        int getFkViewHeaderWidth(SqlQueryView* fkView, bool includeScrollBar) const;
        void updateComboViewGeometry(SqlQueryView* comboView, bool initial) const;

        QSet<QWidget*> editorsWithAsyncExecution;
        mutable int fkViewParentItemSize = 0;
        mutable QHash<SqlQueryModel*, QComboBox*> modelToFkCombo;
        mutable QHash<SqlQueryModel*, QVariant> modelToFkInitialValue;

        static bool warnedAboutHugeContents;
        static const qlonglong MAX_ROWS_FOR_FK = 10000L;
        static const int FK_CELL_LENGTH_LIMIT = 30;
        static const int HUGE_CONTENTS_WARNING_LIMIT = 32767; // pow(2, 16) / 2 - 1

    private slots:
        void fkDataAboutToLoad();
        void fkDataReady();
        void fkDataFailed(const QString& errorText);
};

#endif // SQLQUERYITEMDELEGATE_H
