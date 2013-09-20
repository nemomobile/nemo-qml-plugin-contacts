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

#include "seasidecache.h"

#include <qcontactstatusflags_impl.h>

#include <QContactName>
#include <QContactAvatar>
#include <QContactEmailAddress>
#include <QContactPhoneNumber>

#include <QtDebug>

struct Contact
{
    const char *firstName;
    const char *lastName;
    const bool isFavorite;
    const bool isOnline;
    const char *email;
    const char *phoneNumber;
    const char *avatar;
};

static const Contact contactsData[] =
{
/*1*/   { "Aaron",  "Aaronson", false, false, "aaronaa@example.com",       "1234567", 0 },
/*2*/   { "Aaron",  "Arthur",   false, true,  "aaronar@example.com",       0,         0 },
/*3*/   { "Aaron",  "Johns",    true,  false, "johns@example.com",         0,         0 },
/*4*/   { "Arthur", "Johns",    false, true,  "arthur1.johnz@example.org", "2345678", 0 },
/*5*/   { "Jason",  "Aaronson", false, false, "jay@examplez.org",          "3456789", 0 },
/*6*/   { "Joe",    "Johns",    true,  true,  "jj@examplez.org",           0,         "file:///cache/joe.jpg" },
/*7*/   { "Robin",  "Burchell", true,  false, 0,                           "9876543", 0 }
};

static QStringList getAllContactNameGroups()
{
    QStringList groups;
    for (char c = 'A'; c <= 'Z'; ++c) {
        groups.append(QString(QChar::fromLatin1(c)));
    }
    groups.append(QString::fromLatin1("#"));
    return groups;
}

SeasideCache *SeasideCache::instancePtr = 0;
QStringList SeasideCache::allContactNameGroups = getAllContactNameGroups();

SeasideCache *SeasideCache::instance()
{
    return instancePtr;
}

SeasideCache::ContactIdType SeasideCache::apiId(const QContact &contact)
{
#ifdef USING_QTPIM
    return contact.id();
#else
    return contact.id().localId();
#endif
}

SeasideCache::ContactIdType SeasideCache::apiId(quint32 iid)
{
    return QtContactsSqliteExtensions::apiContactId(iid);
}

bool SeasideCache::validId(const ContactIdType &id)
{
#ifdef USING_QTPIM
    return !id.isNull();
#else
    return (id != 0);
#endif
}

quint32 SeasideCache::internalId(const QContact &contact)
{
    return internalId(contact.id());
}

quint32 SeasideCache::internalId(const QContactId &id)
{
    return QtContactsSqliteExtensions::internalContactId(id);
}

#ifndef USING_QTPIM
quint32 SeasideCache::internalId(QContactLocalId id)
{
    return QtContactsSqliteExtensions::internalContactId(id);
}
#endif

SeasideCache::SeasideCache()
{
    instancePtr = this;
    for (int i = 0; i < FilterTypesCount; ++i) {
        m_models[i] = 0;
        m_populated[i] = false;
    }
}

void SeasideCache::reset()
{
    for (int i = 0; i < FilterTypesCount; ++i) {
        m_contacts[i].clear();
        m_populated[i] = false;
        m_models[i] = 0;
    }

    m_cache.clear();
#ifdef USING_QTPIM
    m_cacheIndices.clear();
#endif

    for (uint i = 0; i < sizeof(contactsData) / sizeof(Contact); ++i) {
        QContact contact;

#ifdef USING_QTPIM
        // This is specific to the qtcontacts-sqlite backend:
        const QString idStr(QString::fromLatin1("qtcontacts:org.nemomobile.contacts.sqlite::sql-%1"));
        contact.setId(QContactId::fromString(idStr.arg(i + 1)));
#else
        QContactId contactId;
        contactId.setLocalId(i + 1);
        contact.setId(contactId);
#endif

        QContactName name;
        name.setFirstName(QLatin1String(contactsData[i].firstName));
        name.setLastName(QLatin1String(contactsData[i].lastName));
        contact.saveDetail(&name);

        if (contactsData[i].avatar) {
            QContactAvatar avatar;
            avatar.setImageUrl(QUrl(QLatin1String(contactsData[i].avatar)));
            contact.saveDetail(&avatar);
        }

        QContactStatusFlags statusFlags;

        if (contactsData[i].email) {
            QContactEmailAddress email;
            email.setEmailAddress(QLatin1String(contactsData[i].email));
            contact.saveDetail(&email);
            statusFlags.setFlag(QContactStatusFlags::HasEmailAddress, true);
        }

        if (contactsData[i].phoneNumber) {
            QContactPhoneNumber phoneNumber;
            phoneNumber.setNumber(QLatin1String(contactsData[i].phoneNumber));
            contact.saveDetail(&phoneNumber);
            statusFlags.setFlag(QContactStatusFlags::HasPhoneNumber, true);
        }

        contact.saveDetail(&statusFlags);

#ifdef USING_QTPIM
        m_cacheIndices.insert(apiId(contact), m_cache.count());
#endif
        m_cache.append(CacheItem(contact));

        QString fullName = name.firstName() + QChar::fromLatin1(' ') + name.lastName();

        CacheItem &cacheItem = m_cache.last();
        cacheItem.nameGroup = determineNameGroup(&cacheItem);
        cacheItem.displayLabel = fullName;
    }

    insert(FilterAll, 0, getContactsForFilterType(FilterAll));
    insert(FilterFavorites, 0, getContactsForFilterType(FilterFavorites));
    insert(FilterOnline, 0, getContactsForFilterType(FilterOnline));
}

QVector<SeasideCache::ContactIdType> SeasideCache::getContactsForFilterType(FilterType filterType)
{
    QVector<ContactIdType> ids;

    for (uint i = 0; i < sizeof(contactsData) / sizeof(Contact); ++i) {
        if ((filterType == FilterAll) ||
            (filterType == FilterFavorites && contactsData[i].isFavorite) ||
            (filterType == FilterOnline && contactsData[i].isOnline)) {
#ifdef USING_QTPIM
            ids.append(instancePtr->m_cache[i].contact.id());
#else
            ids.append(i + 1);
#endif
        }
    }

    return ids;
}

SeasideCache::~SeasideCache()
{
    instancePtr = 0;
}

void SeasideCache::registerModel(ListModel *model, FilterType type, FetchDataType)
{
    for (int i = 0; i < FilterTypesCount; ++i)
        instancePtr->m_models[i] = 0;
    instancePtr->m_models[type] = model;
}

void SeasideCache::unregisterModel(ListModel *)
{
    for (int i = 0; i < FilterTypesCount; ++i)
        instancePtr->m_models[i] = 0;
}

void SeasideCache::registerUser(QObject *)
{
}

void SeasideCache::unregisterUser(QObject *)
{
}

void SeasideCache::registerChangeListener(ChangeListener *)
{
}

void SeasideCache::unregisterChangeListener(ChangeListener *)
{
}

void SeasideCache::unregisterResolveListener(ResolveListener *)
{
}

int SeasideCache::contactId(const QContact &contact)
{
    quint32 internal = internalId(contact);
    return static_cast<int>(internal);
}

SeasideCache::CacheItem *SeasideCache::existingItem(const ContactIdType &id)
{
#ifdef USING_QTPIM
    if (instancePtr->m_cacheIndices.contains(id)) {
        return &instancePtr->m_cache[instancePtr->m_cacheIndices[id]];
    }
#else
    if (id != 0 && id <= instancePtr->m_cache.count()) {
        return &instancePtr->m_cache[id - 1];
    }
#endif
    return 0;
}

SeasideCache::CacheItem *SeasideCache::itemById(const ContactIdType &id, bool)
{
#ifdef USING_QTPIM
    if (instancePtr->m_cacheIndices.contains(id)) {
        return &instancePtr->m_cache[instancePtr->m_cacheIndices[id]];
    }
#else
    if (id != 0 && id <= instancePtr->m_cache.count()) {
        return &instancePtr->m_cache[id - 1];
    }
#endif
    return 0;
}

#ifdef USING_QTPIM
SeasideCache::CacheItem *SeasideCache::itemById(int id, bool)
{
    if (id == 0)
        return 0;

    // Construct a valid id from this value
    QString idStr(QString::fromLatin1("qtcontacts:org.nemomobile.contacts.sqlite::sql-%1"));
    QContactId contactId = QContactId::fromString(idStr.arg(id));
    if (contactId.isNull()) {
        qWarning() << "Unable to formulate valid ID from:" << id;
        return 0;
    }

    return itemById(contactId);
}
#endif

QContact SeasideCache::contactById(const ContactIdType &id)
{
#ifdef USING_QTPIM
    return instancePtr->m_cache[instancePtr->m_cacheIndices[id]].contact;
#else
    return instancePtr->m_cache[id - 1].contact;
#endif
}

QString SeasideCache::nameGroup(const CacheItem *cacheItem)
{
    if (!cacheItem)
        return QString();

    return cacheItem->nameGroup;
}

QString SeasideCache::determineNameGroup(const CacheItem *cacheItem)
{
    if (!cacheItem)
        return QString();

    const QContactName nameDetail = cacheItem->contact.detail<QContactName>();
    const QString sort(sortProperty() == QString::fromLatin1("firstName") ? nameDetail.firstName() : nameDetail.lastName());

    QString group;
    if (!sort.isEmpty()) {
        group = QString(sort[0].toUpper());
    } else if (!cacheItem->displayLabel.isEmpty()) {
        group = QString(cacheItem->displayLabel[0].toUpper());
    }

    if (group.isNull() || !allContactNameGroups.contains(group)) {
        group = QString::fromLatin1("#");   // 'other' group
    }
    return group;
}

QStringList SeasideCache::allNameGroups()
{
    return allContactNameGroups;
}

void SeasideCache::ensureCompletion(CacheItem *)
{
}

void SeasideCache::refreshContact(CacheItem *)
{
}

SeasideCache::CacheItem *SeasideCache::itemByPhoneNumber(const QString &, bool)
{
    return 0;
}

SeasideCache::CacheItem *SeasideCache::itemByEmailAddress(const QString &, bool)
{
    return 0;
}

SeasideCache::CacheItem *SeasideCache::itemByOnlineAccount(const QString &, const QString &, bool)
{
    return 0;
}

SeasideCache::CacheItem *SeasideCache::resolvePhoneNumber(ResolveListener *, const QString &, bool)
{
    // TODO: implement and test these functions
    return 0;
}

SeasideCache::CacheItem *SeasideCache::resolveEmailAddress(ResolveListener *, const QString &, bool)
{
    return 0;
}

SeasideCache::CacheItem *SeasideCache::resolveOnlineAccount(ResolveListener *, const QString &, const QString &, bool)
{
    return 0;
}

SeasideCache::ContactIdType SeasideCache::selfContactId()
{
    return SeasideCache::ContactIdType();
}

bool SeasideCache::saveContact(const QContact &)
{
    return false;
}

void SeasideCache::removeContact(const QContact &)
{
}

void SeasideCache::aggregateContacts(const QContact &, const QContact &)
{
}

void SeasideCache::disaggregateContacts(const QContact &, const QContact &)
{
}

void SeasideCache::fetchConstituents(const QContact &contact)
{
    if (SeasideCache::CacheItem *item = itemById(SeasideCache::apiId(contact))) {
        if (item->itemData) {
            item->itemData->constituentsFetched(QList<int>());
        }
    }
}

void SeasideCache::fetchMergeCandidates(const QContact &contact)
{
    if (SeasideCache::CacheItem *item = itemById(SeasideCache::apiId(contact))) {
        if (item->itemData) {
            item->itemData->mergeCandidatesFetched(QList<int>());
        }
    }
}

const QVector<SeasideCache::ContactIdType> *SeasideCache::contacts(FilterType filterType)
{
    return &instancePtr->m_contacts[filterType];
}

bool SeasideCache::isPopulated(FilterType filterType)
{
    return instancePtr->m_populated[filterType];
}

QString SeasideCache::generateDisplayLabel(const QContact &, DisplayLabelOrder)
{
    return QString();
}

QString SeasideCache::generateDisplayLabelFromNonNameDetails(const QContact &)
{
    return QString();
}

QUrl SeasideCache::filteredAvatarUrl(const QContact &contact, const QStringList &)
{
    foreach (const QContactAvatar &av, contact.details<QContactAvatar>()) {
        return av.imageUrl();
    }
    return QUrl();
}

SeasideCache::DisplayLabelOrder SeasideCache::displayLabelOrder()
{
    return FirstNameFirst;
}

QString SeasideCache::sortProperty()
{
    return QString::fromLatin1("firstName");
}

QString SeasideCache::groupProperty()
{
    return QString::fromLatin1("firstName");
}

void SeasideCache::populate(FilterType filterType)
{
    m_populated[filterType] = true;

    if (m_models[filterType])
        m_models[filterType]->makePopulated();
}

void SeasideCache::insert(FilterType filterType, int index, const QVector<ContactIdType> &ids)
{
    if (m_models[filterType])
        m_models[filterType]->sourceAboutToInsertItems(index, index + ids.count() - 1);

    for (int i = 0; i < ids.count(); ++i)
        m_contacts[filterType].insert(index + i, ids.at(i));

    if (m_models[filterType]) {
        m_models[filterType]->sourceItemsInserted(index, index + ids.count() - 1);
        m_models[filterType]->sourceItemsChanged();
    }
}

void SeasideCache::remove(FilterType filterType, int index, int count)
{
    if (m_models[filterType])
        m_models[filterType]->sourceAboutToRemoveItems(index, index + count - 1);

    m_contacts[filterType].remove(index, count);

    if (m_models[filterType]) {
        m_models[filterType]->sourceItemsRemoved();
        m_models[filterType]->sourceItemsChanged();
    }
}

int SeasideCache::importContacts(const QString &)
{
    return 0;
}

QString SeasideCache::exportContacts()
{
    return QString();
}

void SeasideCache::setFirstName(FilterType filterType, int index, const QString &firstName)
{
#ifdef USING_QTPIM
    CacheItem &cacheItem = m_cache[m_cacheIndices[m_contacts[filterType].at(index)]];
#else
    CacheItem &cacheItem = m_cache[m_contacts[filterType].at(index) - 1];
#endif

    QContactName name = cacheItem.contact.detail<QContactName>();
    name.setFirstName(firstName);
    cacheItem.contact.saveDetail(&name);

    QString fullName = name.firstName() + QChar::fromLatin1(' ') + name.lastName();
    cacheItem.nameGroup = determineNameGroup(&cacheItem);
    cacheItem.displayLabel = fullName;

    ItemListener *listener(cacheItem.listeners);
    while (listener) {
        listener->itemUpdated(&cacheItem);
        listener = listener->next;
    }

    if (m_models[filterType])
        m_models[filterType]->sourceDataChanged(index, index);
}

SeasideCache::ContactIdType SeasideCache::idAt(int index) const
{
#ifdef USING_QTPIM
    return m_cache[index].contact.id();
#else
    return index + 1;
#endif
}

// Instantiate the contact ID functions for qtcontacts-sqlite
#include <qtcontacts-extensions_impl.h>
