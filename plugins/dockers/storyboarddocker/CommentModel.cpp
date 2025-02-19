/*
 *  SPDX-FileCopyrightText: 2020 Saurabh Kumar <saurabhk660@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CommentModel.h"

#include <QDebug>
#include <QMimeData>
#include <QRegularExpression>
#include <QIODevice>

#include <kis_icon.h>

StoryboardCommentModel::StoryboardCommentModel(QObject *parent)
        : QAbstractListModel(parent) 
{
    //initialize variables
}

int StoryboardCommentModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
   return m_commentList.count();
}

QVariant StoryboardCommentModel::data(const QModelIndex &index, int role) const
{
    
    if (!index.isValid()) {
        return QVariant();
    }
    if (index.row() >= m_commentList.size()) {
        return QVariant();
    }
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        return m_commentList.at(index.row()).name;
    }
    if (role == Qt::DecorationRole) {
        if (m_commentList.at(index.row()).visibility) {
            return KisIconUtils::loadIcon("visible");
        }
        else {
            return KisIconUtils::loadIcon("novisible");
        }
    }
    return QVariant();
    
}

bool StoryboardCommentModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    if (index.isValid() && (role == Qt::EditRole || role == Qt::DisplayRole)) {
        // POST KRITA/5.0 TODO -- we should be storing this as a map, not an array!
        // We only want 1 comment field per comment track title. A data change would
        // be appropriate here.
        QStringList nameList;
        Q_FOREACH(const StoryboardComment& comment, m_commentList) {
            nameList << comment.name;
        }


        QString desiredName = value.toString();
        int splitPoint = desiredName.length();
        while (desiredName.at(splitPoint - 1).isDigit()) {
            splitPoint--;
        }

        const QString prefix = desiredName.left(splitPoint);
        int existingNames = prefix.mid(splitPoint).toInt();

        while(nameList.contains(desiredName)) {
            existingNames++;
            desiredName = prefix + QString::number(existingNames);
        }

        m_commentList[index.row()].name = desiredName;
        Q_EMIT dataChanged(index, index);
        Q_EMIT sigCommentListChanged();
        return true;
    }
    
    if (index.isValid() && role == Qt::DecorationRole) {
        m_commentList[index.row()].visibility = !m_commentList[index.row()].visibility;
        Q_EMIT dataChanged(index, index);
        Q_EMIT sigCommentListChanged();
        return true;
    }
    return false;
}

Qt::ItemFlags StoryboardCommentModel::flags(const QModelIndex & index) const
{
    if (!index.isValid()) {
        return Qt::ItemIsDropEnabled;
    }
    return Qt::ItemIsDragEnabled | Qt::ItemIsSelectable |
           Qt::ItemIsEditable | Qt::ItemIsEnabled ;
}

bool StoryboardCommentModel::insertRows(int position, int rows, const QModelIndex &/*parent*/)
{
    beginInsertRows(QModelIndex(), position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        StoryboardComment newcomment;
        newcomment.name = "Comment";
        newcomment.visibility = true;

        if (position < 0 || position > m_commentList.size()) {
            return false;
        }
        m_commentList.insert(position, newcomment);
    }

    endInsertRows();
    Q_EMIT sigCommentListChanged();
    return true;
}

bool StoryboardCommentModel::removeRows(int position, int rows, const QModelIndex &/*parent*/)
{
    if (rows <= 0) {
        return false;
    }
    beginRemoveRows(QModelIndex(), position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        if (position < 0 || position >= m_commentList.size()) {
            return false;
        }
        m_commentList.removeAt(position);
    }
    endRemoveRows();
    Q_EMIT sigCommentListChanged();
    return true;
}

bool StoryboardCommentModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                            const QModelIndex &destinationParent, int destinationChild)
{
    if (destinationChild == sourceRow || destinationChild == sourceRow + 1) {
        return false;
    }
    if (destinationChild > sourceRow + count - 1) {
        //we adjust for the upward shift, see qt doc for why this is needed
        beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent, destinationChild + count - 1);
        destinationChild = destinationChild - count;
    }
    else {
        beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent, destinationChild);
    }
    for (int row = 0; row < count; row++) {
        if (sourceRow < 0 || sourceRow >= m_commentList.size()) {
            return false;
        }
        if (destinationChild + row < 0 || destinationChild + row >= m_commentList.size()) {
            return false;
        }
        m_commentList.move(sourceRow, destinationChild + row);
    }
    endMoveRows();
    Q_EMIT sigCommentListChanged();
    return true;
}

QStringList StoryboardCommentModel::mimeTypes() const
{
    QStringList types;
    types << QLatin1String("application/x-krita-storyboard");
    return types;
}

QMimeData *StoryboardCommentModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodeData;

    QDataStream stream(&encodeData, QIODevice::WriteOnly);

    //take the row number of the index where drag started
    foreach (QModelIndex index, indexes) {
        if (index.isValid()) {
            int row = index.row();
            stream << row;
        }
    }

    mimeData->setData("application/x-krita-storyboard", encodeData); //default mimetype
    return mimeData;
}

bool StoryboardCommentModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                          int row, int /*column*/, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction) {
        return false;
    }
    if (action == Qt::MoveAction && data->hasFormat("application/x-krita-storyboard")) {
        QByteArray bytes = data->data("application/x-krita-storyboard");
        QDataStream stream(&bytes, QIODevice::ReadOnly);

        if (parent.isValid()) {
            return false;
        }
        int sourceRow;
        QModelIndexList moveRowIndexes;
        while (!stream.atEnd()) {
            stream >> sourceRow;
            QModelIndex index = createIndex(sourceRow, 0);
            moveRowIndexes.append(index);
        }
        moveRows(QModelIndex(), moveRowIndexes.at(0).row(), moveRowIndexes.count(), parent, row);

        //returning true deletes the source row
        return false;
    }
    return false;
}

Qt::DropActions StoryboardCommentModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::DropActions StoryboardCommentModel::supportedDragActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

void StoryboardCommentModel::resetData(QVector<StoryboardComment> list)
{
    beginResetModel();
    m_commentList = list;
    Q_EMIT dataChanged(QModelIndex(), QModelIndex());
    endResetModel();
}

QVector<StoryboardComment> StoryboardCommentModel::getData()
{
    return m_commentList;
}
