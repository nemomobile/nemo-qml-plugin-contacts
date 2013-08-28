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
#include "seasideperson.h"

#include <synchronizelists.h>

#include <qtcontacts-extensions.h>
#include <QContactStatusFlags>

#include <QContactAvatar>
#include <QContactEmailAddress>
#include <QContactFavorite>
#include <QContactName>
#include <QContactNickname>
#include <QContactOnlineAccount>
#include <QContactOrganization>
#include <QContactPhoneNumber>
#include <QContactGlobalPresence>
#include <QContactPresence>
#include <QTextBoundaryFinder>

#include <QtDebug>

namespace {

const QByteArray displayLabelRole("displayLabel");
const QByteArray firstNameRole("firstName");
const QByteArray lastNameRole("lastName");
const QByteArray sectionBucketRole("sectionBucket");
const QByteArray favoriteRole("favorite");
const QByteArray avatarRole("avatar");
const QByteArray avatarUrlRole("avatarUrl");
const QByteArray globalPresenceStateRole("globalPresenceState");
const QByteArray contactIdRole("contactId");
const QByteArray phoneNumbersRole("phoneNumbers");
const QByteArray emailAddressesRole("emailAddresses");
const QByteArray accountUrisRole("accountUris");
const QByteArray accountPathsRole("accountPaths");
const QByteArray personRole("person");

}

struct FilterData : public SeasideCache::ItemListener
{
    // Store additional filter keys with the cache item
    QStringList filterKey;

    void itemUpdated(SeasideCache::CacheItem *) { filterKey.clear(); }
    void itemAboutToBeRemoved(SeasideCache::CacheItem *) { delete this; }
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
    , m_effectiveFilterType(FilterAll)
    , m_fetchTypes(SeasideCache::FetchNone)
    , m_requiredProperty(NoPropertyRequired)
    , m_searchByFirstNameCharacter(false)
    , m_lastItem(0)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    setRoleNames(roleNames());
#endif

    updateRegistration();

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
    roles.insert(Qt::DisplayRole, displayLabelRole);
    roles.insert(FirstNameRole, firstNameRole);
    roles.insert(LastNameRole, lastNameRole);
    roles.insert(SectionBucketRole, sectionBucketRole);
    roles.insert(FavoriteRole, favoriteRole);
    roles.insert(AvatarRole, avatarRole);
    roles.insert(AvatarUrlRole, avatarUrlRole);
    roles.insert(GlobalPresenceStateRole, globalPresenceStateRole);
    roles.insert(ContactIdRole, contactIdRole);
    roles.insert(PhoneNumbersRole, phoneNumbersRole);
    roles.insert(EmailAddressesRole, emailAddressesRole);
    roles.insert(AccountUrisRole, accountUrisRole);
    roles.insert(AccountPathsRole, accountPathsRole);
    roles.insert(PersonRole, personRole);
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
        const bool filtered = isFiltered();
        const bool equivalentFilter = (type == FilterAll || type == FilterNone) &&
                                      (m_filterType == FilterAll || m_filterType == FilterNone) &&
                                      filtered;

        m_filterType = type;

        if (!equivalentFilter) {
            m_referenceIndex = 0;
            m_filterIndex = 0;

            m_effectiveFilterType = (m_filterType != FilterNone || !filtered) ? m_filterType : FilterAll;
            updateRegistration();

            if (!filtered) {
                m_filteredContactIds = *m_referenceContactIds;
            }

            m_referenceContactIds = SeasideCache::contacts(static_cast<SeasideCache::FilterType>(m_filterType));

            updateIndex();

            if (!filtered) {
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
    updateFilters(pattern, m_requiredProperty);
}

int SeasideFilteredModel::requiredProperty() const
{
    return m_requiredProperty;
}

void SeasideFilteredModel::setRequiredProperty(int type)
{
    updateFilters(m_filterPattern, type);
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
    if (m_filterParts.isEmpty() && m_requiredProperty == NoPropertyRequired)
        return true;

    SeasideCache::CacheItem *item = existingItem(contactId);
    if (!item)
        return false;

    if (m_requiredProperty != NoPropertyRequired) {
        bool haveMatch = (m_requiredProperty & AccountUriRequired) && (item->statusFlags & QContactStatusFlags::HasOnlineAccount);
        haveMatch |= (m_requiredProperty & PhoneNumberRequired) && (item->statusFlags & QContactStatusFlags::HasPhoneNumber);
        haveMatch |= (m_requiredProperty & EmailAddressRequired) && (item->statusFlags & QContactStatusFlags::HasEmailAddress);
        if (!haveMatch)
            return false;
    }

    if (m_searchByFirstNameCharacter && !m_filterPattern.isEmpty())
        return m_filterPattern[0].toUpper() == SeasideCache::nameGroup(item);

    void *key = const_cast<void *>(static_cast<const void *>(this));
    SeasideCache::ItemListener *listener = item->listener(key);
    if (!listener) {
        listener = item->appendListener(new FilterData, key);
    }
    FilterData *filterData = static_cast<FilterData *>(listener);

    // split the display label and filter into words.
    //
    // TODO: i18n will require different splitting for thai and possibly
    // other locales, see MBreakIterator
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
    invalidateRows(index, count);
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
            invalidateRows(i, count);
            endRemoveRows();
        } else {
            ++i;
        }
    }
}

void SeasideFilteredModel::updateIndex()
{
    synchronizeFilteredList(this, m_filteredContactIds, *m_referenceContactIds);
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
    SeasideCache::CacheItem *cacheItem = existingItem(m_contactIds->at(row));
    if (!cacheItem)
        return QVariantMap();

    QVariantMap m;
    m.insert(displayLabelRole, data(cacheItem, Qt::DisplayRole));
    m.insert(firstNameRole, data(cacheItem, FirstNameRole));
    m.insert(lastNameRole, data(cacheItem, LastNameRole));
    m.insert(sectionBucketRole, data(cacheItem, SectionBucketRole));
    m.insert(favoriteRole, data(cacheItem, FavoriteRole));
    m.insert(avatarRole, data(cacheItem, AvatarRole));
    m.insert(avatarUrlRole, data(cacheItem, AvatarUrlRole));
    m.insert(globalPresenceStateRole, data(cacheItem, GlobalPresenceStateRole));
    m.insert(contactIdRole, data(cacheItem, ContactIdRole));
    m.insert(phoneNumbersRole, data(cacheItem, PhoneNumbersRole));
    m.insert(emailAddressesRole, data(cacheItem, EmailAddressesRole));
    m.insert(accountUrisRole, data(cacheItem, AccountUrisRole));
    m.insert(accountPathsRole, data(cacheItem, AccountPathsRole));
    return m;
}

QVariant SeasideFilteredModel::get(int row, int role) const
{
    SeasideCache::CacheItem *cacheItem = existingItem(m_contactIds->at(row));
    if (!cacheItem)
        return QVariant();

    return data(cacheItem, role);
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

SeasidePerson *SeasideFilteredModel::personByPhoneNumber(const QString &number, bool requireComplete) const
{
    return personFromItem(SeasideCache::itemByPhoneNumber(number, requireComplete));
}

SeasidePerson *SeasideFilteredModel::personByEmailAddress(const QString &email, bool requireComplete) const
{
    return personFromItem(SeasideCache::itemByEmailAddress(email, requireComplete));
}

SeasidePerson *SeasideFilteredModel::personByOnlineAccount(const QString &localUid, const QString &remoteUid, bool requireComplete) const
{
    return personFromItem(SeasideCache::itemByOnlineAccount(localUid, remoteUid, requireComplete));
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

    SeasideCache::CacheItem *cacheItem = existingItem(m_contactIds->at(index.row()));
    if (!cacheItem)
        return QVariant();

    return data(cacheItem, role);
}

QVariant SeasideFilteredModel::data(SeasideCache::CacheItem *cacheItem, int role) const
{
    const QContact &contact = cacheItem->contact;

    if (role == ContactIdRole) {
        return cacheItem->iid;
    } else if (role == FirstNameRole || role == LastNameRole) {
        QContactName name = contact.detail<QContactName>();
        return role == FirstNameRole
                ? name.firstName()
                : name.lastName();
    } else if (role == FavoriteRole) {
        return contact.detail<QContactFavorite>().isFavorite();
    } else if (role == AvatarRole || role == AvatarUrlRole) {
        QUrl avatarUrl = contact.detail<QContactAvatar>().imageUrl();
        if (role == AvatarUrlRole || !avatarUrl.isEmpty()) {
            return avatarUrl;
        }
        // Return the default avatar path for when no avatar URL is available
        return QUrl(QLatin1String("image://theme/icon-m-telephony-contact-avatar"));
    } else if (role == GlobalPresenceStateRole) {
        QContactGlobalPresence presence = contact.detail<QContactGlobalPresence>();
        return presence.isEmpty()
                ? QContactPresence::PresenceUnknown
                : presence.presenceState();
    } else if (role == PhoneNumbersRole || role == EmailAddressesRole || role == AccountUrisRole || role == AccountPathsRole) {
        QStringList rv;
        if (role == PhoneNumbersRole) {
            foreach (const QContactPhoneNumber &number, contact.details<QContactPhoneNumber>()) {
                rv.append(number.number());
            }
        } else if (role == EmailAddressesRole) {
            foreach (const QContactEmailAddress &address, contact.details<QContactEmailAddress>()) {
                rv.append(address.emailAddress());
            }
        } else if (role == AccountPathsRole){
            foreach (const QContactOnlineAccount &account, contact.details<QContactOnlineAccount>()) {
                rv.append(account.value<QString>(QContactOnlineAccount__FieldAccountPath));
            }
        } else {
            foreach (const QContactOnlineAccount &account, contact.details<QContactOnlineAccount>()) {
                rv.append(account.accountUri());
            }
        }
        return rv;
    } else if (role == Qt::DisplayRole || role == SectionBucketRole) {
        if (SeasidePerson *person = static_cast<SeasidePerson *>(cacheItem->itemData)) {
            // If we have a person instance, prefer to use that
            return role == Qt::DisplayRole ? person->displayLabel() : person->sectionBucket();
        }
        return role == Qt::DisplayRole ? cacheItem->displayLabel : cacheItem->nameGroup;
    } else if (role == PersonRole) {
        // Avoid creating a Person instance for as long as possible.
        SeasideCache::ensureCompletion(cacheItem);
        return QVariant::fromValue(personFromItem(cacheItem));
    } else {
        qWarning() << "Invalid role requested:" << role;
    }

    return QVariant();
}

void SeasideFilteredModel::sourceAboutToRemoveItems(int begin, int end)
{
    if (!isFiltered()) {
        beginRemoveRows(QModelIndex(), begin, end);
        invalidateRows(begin, (end - begin) + 1, false, false);
    }
}

void SeasideFilteredModel::sourceItemsRemoved()
{
    if (!isFiltered()) {
        endRemoveRows();
        emit countChanged();
    }
}

void SeasideFilteredModel::sourceAboutToInsertItems(int begin, int end)
{
    if (!isFiltered()) {
        beginInsertRows(QModelIndex(), begin, end);
    }
}

void SeasideFilteredModel::sourceItemsInserted(int begin, int end)
{
    Q_UNUSED(begin)
    Q_UNUSED(end)

    if (!isFiltered()) {
        endInsertRows();
        emit countChanged();
    }
}

void SeasideFilteredModel::sourceDataChanged(int begin, int end)
{
    if (!isFiltered()) {
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
                invalidateRows(f, 1);
                endRemoveRows();
            } else if (f >= 0) {
                const QModelIndex index = createIndex(f, 0);
                emit dataChanged(index, index);
            }
        }
    }
}

void SeasideFilteredModel::sourceItemsChanged()
{
    if (isFiltered()) {
        const int prevCount = rowCount();

        updateIndex();

        if (rowCount() != prevCount) {
            emit countChanged();
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
        item->itemData = new SeasidePerson(&item->contact, (item->contactState == SeasideCache::ContactComplete), SeasideCache::instance());
    }

    return static_cast<SeasidePerson *>(item->itemData);
}

bool SeasideFilteredModel::isFiltered() const
{
    return !m_filterPattern.isEmpty() || (m_requiredProperty != NoPropertyRequired);
}

void SeasideFilteredModel::updateFilters(const QString &pattern, int property)
{
    if ((pattern == m_filterPattern) && (property == m_requiredProperty))
        return;

    const bool filtered = isFiltered();
    const bool removeFilter = pattern.isEmpty() && property == NoPropertyRequired;
    const bool refinement = (pattern == m_filterPattern || pattern.startsWith(m_filterPattern, Qt::CaseInsensitive)) &&
                            (property == m_requiredProperty || m_requiredProperty == NoPropertyRequired);

    const int prevCount = rowCount();

    bool changedPattern(false);
    bool changedProperty(false);

    if (m_filterPattern != pattern) {
        m_filterPattern = pattern;
        m_filterParts = splitWords(m_filterPattern);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        // Qt5 does not recognize '#' as a word
        if (m_filterParts.isEmpty() && !pattern.isEmpty()) {
            m_filterParts.append(pattern);
        }
#endif
        changedPattern = true;
    }
    if (m_requiredProperty != property) {
        m_requiredProperty = property;
        changedProperty = true;

        // Update our registration to include the data type we need
        m_fetchTypes = static_cast<SeasideCache::FetchDataType>(m_requiredProperty);

        updateRegistration();
    }

    m_referenceIndex = 0;
    m_filterIndex = 0;

    if (!filtered && m_filterType == FilterNone) {
        m_effectiveFilterType = FilterAll;
        updateRegistration();

        m_referenceContactIds = SeasideCache::contacts(SeasideCache::FilterAll);
        populateIndex();
    } else if (!filtered) {
        m_filteredContactIds = *m_referenceContactIds;
        m_contactIds = &m_filteredContactIds;

        refineIndex();
    } else if (refinement) {
        refineIndex();
    } else if (removeFilter && m_filterType == FilterNone) {
        m_effectiveFilterType = FilterNone;
        updateRegistration();

        const bool hadMatches = m_contactIds->count() > 0;
        if (hadMatches) {
            beginRemoveRows(QModelIndex(), 0, m_contactIds->count() - 1);
            invalidateRows(0, m_contactIds->count(), true, false);
        }

        m_referenceContactIds = SeasideCache::contacts(SeasideCache::FilterNone);
        m_contactIds = m_referenceContactIds;
        m_filteredContactIds.clear();

        if (hadMatches) {
            endRemoveRows();
        }
    } else {
        updateIndex();

        if (removeFilter) {
            m_contactIds = m_referenceContactIds;
            m_filteredContactIds.clear();
        }
    }

    if (rowCount() != prevCount) {
        emit countChanged();
    }
    if (changedPattern) {
        emit filterPatternChanged();
    }
    if (changedProperty) {
        emit requiredPropertyChanged();
    }
}

void SeasideFilteredModel::updateRegistration()
{
    SeasideCache::registerModel(this, static_cast<SeasideCache::FilterType>(m_effectiveFilterType), m_fetchTypes);
}

void SeasideFilteredModel::invalidateRows(int begin, int count, bool filteredIndex, bool removeFromModel)
{
    const QVector<ContactIdType> *contactIds(filteredIndex ? &m_filteredContactIds : m_referenceContactIds);

    for (int index = begin; index < (begin + count); ++index) {
        if (contactIds->at(index) == m_lastId) {
            m_lastId = ContactIdType();
            m_lastItem = 0;
        }
    }

    if (removeFromModel) {
        Q_ASSERT(filteredIndex);
        m_filteredContactIds.remove(begin, count);
    }
}

SeasideCache::CacheItem *SeasideFilteredModel::existingItem(const ContactIdType &contactId) const
{
    // Cache the last item lookup - repeated property lookups will be for the same index
    if (contactId != m_lastId) {
        m_lastId = contactId;
        m_lastItem = SeasideCache::existingItem(m_lastId);
    }
    return m_lastItem;
}

