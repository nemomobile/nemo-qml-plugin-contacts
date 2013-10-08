/*
 * Copyright (C) 2013 Jolla Mobile <bea.lam@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "seasidenamegroupmodel.h"

#include <QContactStatusFlags>

#include <QContactOnlineAccount>
#include <QContactPhoneNumber>
#include <QContactEmailAddress>

#include <QDebug>

SeasideNameGroupModel::SeasideNameGroupModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_requiredProperty(NoPropertyRequired)
{
    SeasideCache::registerNameGroupChangeListener(this);

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    setRoleNames(roleNames());
#endif

    QStringList allGroups = SeasideCache::allNameGroups();
    QHash<QString, QSet<quint32> > existingGroups = SeasideCache::nameGroupMembers();
    if (!existingGroups.isEmpty()) {
        for (int i=0; i<allGroups.count(); i++)
            m_groups << SeasideNameGroup(allGroups[i], existingGroups.value(allGroups[i]));
    } else {
        for (int i=0; i<allGroups.count(); i++)
            m_groups << SeasideNameGroup(allGroups[i]);
    }
}

SeasideNameGroupModel::~SeasideNameGroupModel()
{
    SeasideCache::unregisterNameGroupChangeListener(this);
}

int SeasideNameGroupModel::requiredProperty() const
{
    return m_requiredProperty;
}

void SeasideNameGroupModel::setRequiredProperty(int properties)
{
    if (m_requiredProperty != properties) {
        m_requiredProperty = properties;

        // Update counts
        QList<SeasideNameGroup>::iterator it = m_groups.begin(), end = m_groups.end();
        for ( ; it != end; ++it) {
            SeasideNameGroup &existing(*it);

            int newCount = countFilteredContacts(existing.contactIds);
            if (existing.count != newCount) {
                existing.count = newCount;

                const int row = it - m_groups.begin();
                const QModelIndex updateIndex(createIndex(row, 0));
                emit dataChanged(updateIndex, updateIndex);
            }
        }

        emit requiredPropertyChanged();
    }
}

QHash<int, QByteArray> SeasideNameGroupModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(NameRole, "name");
    roles.insert(EntryCount, "entryCount");
    return roles;
}

int SeasideNameGroupModel::rowCount(const QModelIndex &) const
{
    return m_groups.count();
}

QVariant SeasideNameGroupModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
        case NameRole:
            return QString(m_groups[index.row()].name);
        case EntryCount:
            return m_groups[index.row()].count;
    }
    return QVariant();
}

void SeasideNameGroupModel::nameGroupsUpdated(const QHash<QString, QSet<quint32> > &groups)
{
    if (groups.isEmpty())
        return;

    bool wasEmpty = m_groups.isEmpty();
    if (wasEmpty) {
        QStringList allGroups = SeasideCache::allNameGroups();
        beginInsertRows(QModelIndex(), 0, allGroups.count() - 1);
        for (int i=0; i<allGroups.count(); i++)
            m_groups << SeasideNameGroup(allGroups[i]);
    }

    QHash<QString, QSet<quint32> >::const_iterator it = groups.constBegin(), end = groups.constEnd();
    for ( ; it != end; ++it) {
        QList<SeasideNameGroup>::iterator existingIt = m_groups.begin(), existingEnd = m_groups.end();
        for ( ; existingIt != existingEnd; ++existingIt) {
            SeasideNameGroup &existing(*existingIt);
            if (existing.name == it.key()) {
                existing.contactIds = it.value();
                const int count = countFilteredContacts(existing.contactIds);
                if (existing.count != count) {
                    existing.count = count;
                    if (!wasEmpty) {
                        const int row = existingIt - m_groups.begin();
                        const QModelIndex updateIndex(createIndex(row, 0));
                        emit dataChanged(updateIndex, updateIndex);
                    }
                }
                break;
            }
        }
        if (existingIt == existingEnd) {
            qWarning() << "SeasideNameGroupModel: no match for group" << it.key();
        }
    }

    if (wasEmpty) {
        endInsertRows();
        emit countChanged();
    }
}

int SeasideNameGroupModel::countFilteredContacts(const QSet<quint32> &contactIds) const
{
    if (m_requiredProperty != NoPropertyRequired) {
        int count = 0;

        // Check if these contacts are included by the current filter
        foreach (quint32 iid, contactIds) {
            SeasideCache::CacheItem *item = SeasideCache::existingItem(iid);
            Q_ASSERT(item);

            bool haveMatch = (m_requiredProperty & AccountUriRequired) && (item->statusFlags & QContactStatusFlags::HasOnlineAccount);
            haveMatch |= (m_requiredProperty & PhoneNumberRequired) && (item->statusFlags & QContactStatusFlags::HasPhoneNumber);
            haveMatch |= (m_requiredProperty & EmailAddressRequired) && (item->statusFlags & QContactStatusFlags::HasEmailAddress);
            if (haveMatch)
                ++count;
        }

        return count;
    }

    return contactIds.count();
}

