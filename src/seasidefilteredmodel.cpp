/*
 * Copyright (C) 2013 Jolla Mobile <andrew.den.exter@jollamobile.com>
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

#include "seasidefilteredmodel.h"
#include "seasidecache.h"
#include "seasideperson.h"
#include "synchronizelists_p.h"

#include <qtcontacts-extensions.h>

#include <QContactAvatar>
#include <QContactEmailAddress>
#include <QContactName>
#include <QContactNickname>
#include <QContactOnlineAccount>
#include <QContactOrganization>
#include <QContactPhoneNumber>
#include <QContactGlobalPresence>
#include <QContactPresence>
#include <QTextBoundaryFinder>

#include <QtDebug>

struct FilterData : public SeasideCache::ModelData
{
    // Store additional filter keys with the cache item
    QStringList filterKey;

    void contactChanged(const QContact &) { filterKey.clear(); }
};

// We could squeeze a little more performance out of QVector by inserting all the items in a
// single hit, but tests are more important right now.
static void insert(
        QVector<SeasideFilteredModel::ContactIdType> *destination, int to, const QVector<SeasideFilteredModel::ContactIdType> &source)
{
    for (int i = 0; i < source.count(); ++i)
        destination->insert(to + i, source.at(i));
}

// Splits a string at word boundaries identified by QTextBoundaryFinder and returns a list of
// of the fragments that occur between StartWord and EndWord boundaries.
static QStringList splitWords(const QString &string)
{
    QStringList words;
    QTextBoundaryFinder finder(QTextBoundaryFinder::Word, string);

    for (int start = 0; finder.position() != -1 && finder.position() < string.length();) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        if (!(finder.boundaryReasons() & QTextBoundaryFinder::StartOfItem)) {
#else
        if (!(finder.boundaryReasons() & QTextBoundaryFinder::StartWord)) {
#endif
            finder.toNextBoundary();
            start = finder.position();
        }

        finder.toNextBoundary();

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        if (finder.position() > start && finder.boundaryReasons() & QTextBoundaryFinder::EndOfItem) {
#else
        if (finder.position() > start && finder.boundaryReasons() & QTextBoundaryFinder::EndWord) {
#endif
            words.append(string.mid(start, finder.position() - start));
            start = finder.position();
        }
    }
    return words;
}

SeasideFilteredModel::SeasideFilteredModel(QObject *parent)
    : SeasideCache::ListModel(parent)
    , m_filterIndex(0)
    , m_referenceIndex(0)
    , m_filterType(FilterAll)
    , m_searchByFirstNameCharacter(false)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    setRoleNames(roleNames());
#endif

    SeasideCache::registerModel(this, SeasideCache::FilterAll);
    m_referenceContactIds = SeasideCache::contacts(SeasideCache::FilterAll);
    m_contactIds = m_referenceContactIds;
}

SeasideFilteredModel::~SeasideFilteredModel()
{
    SeasideCache::unregisterModel(this);
}

QHash<int, QByteArray> SeasideFilteredModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(Qt::DisplayRole, "display");
    roles.insert(FirstNameRole, "firstName");
    roles.insert(LastNameRole, "lastName");
    roles.insert(SectionBucketRole, "sectionBucket");
    roles.insert(PersonRole, "person");
    roles.insert(AvatarRole, "avatar");
    return roles;
}

bool SeasideFilteredModel::isPopulated() const
{
    return SeasideCache::isPopulated(static_cast<SeasideCache::FilterType>(m_filterType));
}

SeasideFilteredModel::DisplayLabelOrder SeasideFilteredModel::displayLabelOrder() const
{
    SeasideCache::DisplayLabelOrder order = SeasideCache::displayLabelOrder();
    return static_cast<SeasideFilteredModel::DisplayLabelOrder>(order);
}

void SeasideFilteredModel::setDisplayLabelOrder(DisplayLabelOrder)
{
    // For compatibility only.
}

SeasideFilteredModel::FilterType SeasideFilteredModel::filterType() const
{
    return m_filterType;
}

void SeasideFilteredModel::setFilterType(FilterType type)
{
    if (m_filterType != type) {
        const bool wasPopulated = SeasideCache::isPopulated(static_cast<SeasideCache::FilterType>(m_filterType));

        // FilterNone == FilterAll when there is a filter pattern.
        const bool equivalentFilter = (type == FilterAll || type == FilterNone) &&
                                      (m_filterType == FilterAll || m_filterType == FilterNone) &&
                                      !m_filterPattern.isEmpty();

        m_filterType = type;

        if (!equivalentFilter) {
            m_referenceIndex = 0;
            m_filterIndex = 0;

            SeasideCache::registerModel(this, m_filterType != FilterNone || m_filterPattern.isEmpty()
                    ? static_cast<SeasideCache::FilterType>(m_filterType)
                    : SeasideCache::FilterAll);

            if (m_filterPattern.isEmpty())
                m_filteredContactIds = *m_referenceContactIds;

            m_referenceContactIds = SeasideCache::contacts(static_cast<SeasideCache::FilterType>(m_filterType));

            updateIndex();

            if (m_filterPattern.isEmpty()) {
                m_contactIds = m_referenceContactIds;
                m_filteredContactIds.clear();
            }

            if (SeasideCache::isPopulated(static_cast<SeasideCache::FilterType>(m_filterType)) != wasPopulated)
                emit populatedChanged();
        }

        emit filterTypeChanged();
    }
}

QString SeasideFilteredModel::filterPattern() const
{
    return m_filterPattern;
}

void SeasideFilteredModel::setFilterPattern(const QString &pattern)
{
    if (m_filterPattern != pattern) {
        const bool wasEmpty = m_filterPattern.isEmpty();
        const bool subFilter = !wasEmpty && pattern.startsWith(m_filterPattern, Qt::CaseInsensitive);
        const int prevCount = rowCount();

        m_filterPattern = pattern;
        m_filterParts = splitWords(m_filterPattern);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        // Qt5 does not recognize '#' as a word
        if (m_filterParts.isEmpty() && !pattern.isEmpty()) {
            m_filterParts.append(pattern);
        }
#endif
        m_referenceIndex = 0;
        m_filterIndex = 0;

        if (wasEmpty && m_filterType == FilterNone) {
            SeasideCache::registerModel(this, SeasideCache::FilterAll);
            m_referenceContactIds = SeasideCache::contacts(SeasideCache::FilterAll);

            populateIndex();
        } else if (wasEmpty) {
            m_filteredContactIds = *m_referenceContactIds;
            m_contactIds = &m_filteredContactIds;

            refineIndex();
        } else if (subFilter) {
            refineIndex();
        } else if (m_filterPattern.isEmpty() && m_filterType == FilterNone) {
            SeasideCache::registerModel(this, SeasideCache::FilterNone);
            const bool hadMatches = m_contactIds->count() > 0;
            if (hadMatches) {
                beginRemoveRows(QModelIndex(), 0, m_contactIds->count() - 1);
            }

            m_referenceContactIds = SeasideCache::contacts(SeasideCache::FilterNone);
            m_contactIds = m_referenceContactIds;
            m_filteredContactIds.clear();

            if (hadMatches) {
                endRemoveRows();
            }
        } else {
            updateIndex();

            if (m_filterPattern.isEmpty()) {
                m_contactIds = m_referenceContactIds;
                m_filteredContactIds.clear();
            }
        }
        if (rowCount() != prevCount)
            emit countChanged();
        emit filterPatternChanged();
    }
}

bool SeasideFilteredModel::searchByFirstNameCharacter() const
{
    return m_searchByFirstNameCharacter;
}

void SeasideFilteredModel::setSearchByFirstNameCharacter(bool searchByFirstNameCharacter)
{
    if (m_searchByFirstNameCharacter != searchByFirstNameCharacter) {
        m_searchByFirstNameCharacter = searchByFirstNameCharacter;
        emit searchByFirstNameCharacterChanged();
    }
}

template<typename T>
void insert(QSet<T> &set, const QList<T> &list)
{
    foreach (const T &item, list)
        set.insert(item);
}

bool SeasideFilteredModel::filterId(const ContactIdType &contactId) const
{
    if (m_filterParts.isEmpty())
        return true;

    SeasideCache::CacheItem *item = SeasideCache::existingItem(contactId);
    if (!item)
        return false;

    if (m_searchByFirstNameCharacter && !m_filterPattern.isEmpty())
        return m_filterPattern[0].toUpper() == SeasideCache::nameGroupForCacheItem(item);

    // split the display label and filter into words.
    //
    // TODO: i18n will require different splitting for thai and possibly
    // other locales, see MBreakIterator
    if (!item->modelData) {
        item->modelData = new FilterData;
    }
    FilterData *filterData = static_cast<FilterData *>(item->modelData);
    if (filterData->filterKey.isEmpty()) {
        QSet<QString> matchTokens;

        QContactName name = item->contact.detail<QContactName>();
        insert(matchTokens, splitWords(name.firstName()));
        insert(matchTokens, splitWords(name.middleName()));
        insert(matchTokens, splitWords(name.lastName()));
        insert(matchTokens, splitWords(name.prefix()));
        insert(matchTokens, splitWords(name.suffix()));

        QContactNickname nickname = item->contact.detail<QContactNickname>();
        insert(matchTokens, splitWords(nickname.nickname()));

        // Include the custom label - it may contain the user's customized name for the contact
#ifdef USING_QTPIM
        insert(matchTokens, splitWords(item->contact.detail<QContactName>().value<QString>(QContactName__FieldCustomLabel)));
#else
        insert(matchTokens, splitWords(item->contact.detail<QContactName>().customLabel()));
#endif

        foreach (const QContactPhoneNumber &detail, item->contact.details<QContactPhoneNumber>())
            insert(matchTokens, splitWords(detail.number()));
        foreach (const QContactEmailAddress &detail, item->contact.details<QContactEmailAddress>())
            insert(matchTokens, splitWords(detail.emailAddress()));
        foreach (const QContactOrganization &detail, item->contact.details<QContactOrganization>())
            insert(matchTokens, splitWords(detail.name()));
        foreach (const QContactOnlineAccount &detail, item->contact.details<QContactOnlineAccount>()) {
            insert(matchTokens, splitWords(detail.accountUri()));
            insert(matchTokens, splitWords(detail.serviceProvider()));
        }
        foreach (const QContactGlobalPresence &detail, item->contact.details<QContactGlobalPresence>())
            insert(matchTokens, splitWords(detail.nickname()));
        foreach (const QContactPresence &detail, item->contact.details<QContactPresence>())
            insert(matchTokens, splitWords(detail.nickname()));

        filterData->filterKey = matchTokens.toList();
    }

    // search forwards over the label components for each filter word, making
    // sure to find all filter words before considering it a match.
    for (int i = 0; i < m_filterParts.size(); i++) {
        bool found = false;
        const QString &part(m_filterParts.at(i));
        for (int j = 0; j < filterData->filterKey.size(); j++) {
            // TODO: for good i18n, we need to search insensitively taking
            // diacritics into account, QString's functions alone aren't good
            // enough
            if (filterData->filterKey.at(j).startsWith(part, Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }

        // if the current filter word wasn't found in the search
        // string, then it wasn't a match. we require all words
        // to match.
        if (!found)
            return false;
    }

    return true;
}

void SeasideFilteredModel::insertRange(
        int index, int count, const QVector<ContactIdType> &source, int sourceIndex)
{
    beginInsertRows(QModelIndex(), index, index + count - 1);
    for (int i = 0; i < count; ++i)
        m_filteredContactIds.insert(index + i, source.at(sourceIndex + i));
    endInsertRows();
}

void SeasideFilteredModel::removeRange(int index, int count)
{
    beginRemoveRows(QModelIndex(), index, index + count - 1);
    m_filteredContactIds.remove(index, count);
    endRemoveRows();
}

void SeasideFilteredModel::refineIndex()
{
    // The filtered list is a guaranteed sub-set of the current list, so just scan through
    // and remove items that don't match the filter.
    for (int i = 0; i < m_filteredContactIds.count();) {
        int count = 0;
        for (; i + count < m_filteredContactIds.count(); ++count) {
            if (filterId(m_filteredContactIds.at(i + count)))
                break;
        }

        if (count > 0) {
            beginRemoveRows(QModelIndex(), i, i + count - 1);
            m_filteredContactIds.remove(i, count);
            endRemoveRows();
        } else {
            ++i;
        }
    }
}

void SeasideFilteredModel::updateIndex()
{
    int f = 0;
    int r = 0;
    synchronizeFilteredList(this, m_filteredContactIds, f, *m_referenceContactIds, r);

    if (f < m_filteredContactIds.count())
        removeRange(f, m_filteredContactIds.count() - f);

    if (r < m_referenceContactIds->count()) {
        QVector<ContactIdType> insertIds;
        for (; r < m_referenceContactIds->count(); ++r) {
            if (filterId(m_referenceContactIds->at(r)))
                insertIds.append(m_referenceContactIds->at(r));
        }
        if (insertIds.count() > 0) {
            beginInsertRows(
                    QModelIndex(),
                    m_filteredContactIds.count(),
                    m_filteredContactIds.count() + insertIds.count() - 1);
            m_filteredContactIds += insertIds;
            endInsertRows();
        }
    }
}

void SeasideFilteredModel::populateIndex()
{
    // The filtered list is empty, so just scan through the reference list and append any
    // items that match the filter.
    for (int i = 0; i < m_referenceContactIds->count(); ++i) {
        if (filterId(m_referenceContactIds->at(i)))
            m_filteredContactIds.append(m_referenceContactIds->at(i));
    }
    if (!m_filteredContactIds.isEmpty())
        beginInsertRows(QModelIndex(), 0, m_filteredContactIds.count() - 1);

    m_contactIds = &m_filteredContactIds;

    if (!m_filteredContactIds.isEmpty()) {
        endInsertRows();
        emit countChanged();
    }
}

QVariantMap SeasideFilteredModel::get(int row) const
{
    // needed for SectionScroller.
    SeasideCache::CacheItem *cacheItem = SeasideCache::existingItem(m_contactIds->at(row));
    QString sectionBucket;
    if (cacheItem && cacheItem->itemData) {
        sectionBucket = static_cast<SeasidePerson *>(cacheItem->itemData)->sectionBucket();
    } else if (cacheItem) {
#ifdef USING_QTPIM
        QString displayLabel = cacheItem->contact.detail<QContactName>().value<QString>(QContactName__FieldCustomLabel);
#else
        QString displayLabel = cacheItem->contact.detail<QContactName>().customLabel();
#endif
        if (!displayLabel.isEmpty())
            sectionBucket = displayLabel.at(0).toUpper();
    }
    QVariantMap m;
    m[QLatin1String("sectionBucket")] = sectionBucket;
    return m;
}

bool SeasideFilteredModel::savePerson(SeasidePerson *person)
{
    return SeasideCache::saveContact(person->contact());
}

SeasidePerson *SeasideFilteredModel::personByRow(int row) const
{
    return personFromItem(SeasideCache::itemById(m_contactIds->at(row)));
}

SeasidePerson *SeasideFilteredModel::personById(int id) const
{
    return personFromItem(SeasideCache::itemById(id));
}

SeasidePerson *SeasideFilteredModel::personByPhoneNumber(const QString &msisdn) const
{
    return personFromItem(SeasideCache::itemByPhoneNumber(msisdn));
}

SeasidePerson *SeasideFilteredModel::personByEmailAddress(const QString &email) const
{
    return personFromItem(SeasideCache::itemByEmailAddress(email));
}

SeasidePerson *SeasideFilteredModel::selfPerson() const
{
    return personFromItem(SeasideCache::itemById(SeasideCache::selfContactId()));
}

void SeasideFilteredModel::removePerson(SeasidePerson *person)
{
    SeasideCache::removeContact(person->contact());
}

QModelIndex SeasideFilteredModel::index(const QModelIndex &parent, int row, int column) const
{
    return !parent.isValid() && column == 0 && row >= 0 && row < m_contactIds->count()
            ? createIndex(row, column)
            : QModelIndex();
}

int SeasideFilteredModel::rowCount(const QModelIndex &parent) const
{
    return !parent.isValid()
            ? m_contactIds->count()
            : 0;
}

QVariant SeasideFilteredModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    SeasideCache::CacheItem *cacheItem = SeasideCache::existingItem(m_contactIds->at(index.row()));
    if (!cacheItem)
        return QVariant();

    // Avoid creating a Person instance for as long as possible.
    if (role == FirstNameRole || role == LastNameRole) {
        QContactName name = cacheItem->contact.detail<QContactName>();

        return role == FirstNameRole
                ? name.firstName()
                : name.lastName();
    } else if (role == AvatarRole) {
        QUrl avatarUrl = cacheItem->contact.detail<QContactAvatar>().imageUrl();
        return avatarUrl.isEmpty()
                ? QUrl(QLatin1String("image://theme/icon-m-telephony-contact-avatar"))
                : avatarUrl;
    } else if (role != PersonRole && !cacheItem->itemData) {  // Display or Section Bucket.
#ifdef USING_QTPIM
        QString displayLabel = cacheItem->contact.detail<QContactName>().value<QString>(QContactName__FieldCustomLabel);
#else
        QString displayLabel = cacheItem->contact.detail<QContactName>().customLabel();
#endif

        return role == Qt::DisplayRole || displayLabel.isEmpty()
                ? displayLabel
                : displayLabel.at(0).toUpper();
    }

    SeasidePerson *person = personFromItem(cacheItem);

    switch (role) {
    case Qt::DisplayRole:
        return person->displayLabel();
    case PersonRole:
        return QVariant::fromValue(person);
    case SectionBucketRole:
        return person->sectionBucket();
    default:
        return QVariant();
    }
}

void SeasideFilteredModel::sourceAboutToRemoveItems(int begin, int end)
{
    if (m_filterPattern.isEmpty()) {
        beginRemoveRows(QModelIndex(), begin, end);
    } else {
        // The items inserted/removed notifications arrive sequentially, so if begin is at or
        // after the index where the last search finished it's possible to continue from there.
        if (begin < m_referenceIndex) {
            m_referenceIndex = 0;
            m_filterIndex = 0;
        }
        // Find the position of filtered items in the reference list, and remove any that are
        // between begin and end.
        int &r = m_referenceIndex;
        int &f = m_filterIndex;
        for (; f < m_filteredContactIds.count(); ++f) {
            r = m_referenceContactIds->indexOf(m_filteredContactIds.at(f), r);
            if (r > end) {
                // The filtered contacts are all after the end, we're done.
                break;
            } else if (r >= begin) {
                // The filtered contacts intersect the removed items, scan ahead and find the
                // total number that intersect.
                int count = 0;
                while (r <= end) {
                    ++count;
                    if (f + count >= m_filteredContactIds.count())
                        break;
                    r = m_referenceContactIds->indexOf(m_filteredContactIds.at(f + count), r);
                }

                beginRemoveRows(QModelIndex(), f, f + count - 1);
                m_filteredContactIds.remove(f, count);
                endRemoveRows();
                emit countChanged();
                break;
            }
        }
    }
}

void SeasideFilteredModel::sourceItemsRemoved()
{
    if (m_filterPattern.isEmpty()) {
        endRemoveRows();
        emit countChanged();
    }
}

void SeasideFilteredModel::sourceAboutToInsertItems(int begin, int end)
{
    if (m_filterPattern.isEmpty())
        beginInsertRows(QModelIndex(), begin, end);
}

void SeasideFilteredModel::sourceItemsInserted(int begin, int end)
{
    if (m_filterPattern.isEmpty()) {
        endInsertRows();
        emit countChanged();
    } else {
        // Check if any of the inserted items match the filter.
        QVector<ContactIdType> insertIds;
        for (int r = begin; r <= end; ++r) {
            if (filterId(m_referenceContactIds->at(r)))
                insertIds.append(m_referenceContactIds->at(r));
        }
        if (!insertIds.isEmpty()) {
            if (begin < m_referenceIndex) {
                m_referenceIndex = 0;
                m_filterIndex = 0;
            }

            // Find the position to insert at in the filtered list.
            int r = m_referenceIndex;
            int f = m_filterIndex;
            for (; f < m_filteredContactIds.count(); ++f) {
                r = m_referenceContactIds->indexOf(m_filteredContactIds.at(f), r);
                if (r > begin)
                    break;
            }

            m_referenceIndex = end + 1;
            m_filterIndex = f + insertIds.count();

            beginInsertRows(QModelIndex(), f, f + insertIds.count() - 1);
            insert(&m_filteredContactIds, f, insertIds);
            endInsertRows();
            emit countChanged();
        }
    }
}

void SeasideFilteredModel::sourceDataChanged(int begin, int end)
{
    if (m_filterPattern.isEmpty()) {
        emit dataChanged(createIndex(begin, 0), createIndex(end, 0));
    } else {
        // the items inserted/removed notifications arrive sequentially.  All bets are off
        // for dataChanged so we want to reset the progressive indexes back to the beginning.
        m_referenceIndex = 0;
        m_filterIndex = 0;

        // This could be optimised to group multiple changes together, but as of right
        // now begin and end are always the same so theres no point.
        for (int i = begin; i <= end; ++i) {
            const int f = m_filteredContactIds.indexOf(m_referenceContactIds->at(i));
            const bool match = filterId(m_referenceContactIds->at(i));

            if (f < 0 && match) {
                // The contact is not in the filtered list but is a match to the filter; find the
                // correct position and insert it.
                int r = 0;
                int f = 0;
                for (; f < m_filteredContactIds.count(); ++f) {
                    r = m_referenceContactIds->indexOf(m_filteredContactIds.at(f), r);
                    if (r > begin)
                        break;
                }
                beginInsertRows(QModelIndex(), f, f);
                m_filteredContactIds.insert(f, m_referenceContactIds->at(i));
                endInsertRows();
            } else if (f >= 0 && !match) {
                // The contact is in the filtered set but is not a match to the filter; remove it.
                beginRemoveRows(QModelIndex(), f, f);
                m_filteredContactIds.remove(f);
                endRemoveRows();
            } else if (f >= 0) {
                const QModelIndex index = createIndex(f, 0);
                emit dataChanged(index, index);
            }
        }
    }
}

void SeasideFilteredModel::makePopulated()
{
    emit populatedChanged();
}

void SeasideFilteredModel::updateDisplayLabelOrder()
{
    if (!m_contactIds->isEmpty())
        emit dataChanged(createIndex(0, 0), createIndex(0, m_contactIds->count() - 1));

    emit displayLabelOrderChanged();
}

int SeasideFilteredModel::importContacts(const QString &path)
{
    return SeasideCache::importContacts(path);
}

QString SeasideFilteredModel::exportContacts()
{
    return SeasideCache::exportContacts();
}

SeasidePerson *SeasideFilteredModel::personFromItem(SeasideCache::CacheItem *item) const
{
    if (!item)
        return 0;

    if (!item->itemData) {
        item->itemData = new SeasidePerson(&item->contact, (item->contactState == SeasideCache::ContactFetched), SeasideCache::instance());
    }

    return static_cast<SeasidePerson *>(item->itemData);
}

