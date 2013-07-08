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

#include "normalization_p.h"
#include "synchronizelists_p.h"
#include "constants_p.h"

#include <QCoreApplication>
#ifdef USING_QTPIM
#include <QStandardPaths>
#else
#include <QDesktopServices>
#endif
#include <QDir>
#include <QEvent>
#include <QFile>

#include <QContactAvatar>
#include <QContactDetailFilter>
#include <QContactDisplayLabel>
#include <QContactEmailAddress>
#include <QContactFavorite>
#include <QContactName>
#include <QContactNickname>
#include <QContactOnlineAccount>
#include <QContactOrganization>
#include <QContactPhoneNumber>
#include <QContactGlobalPresence>
#include <QContactSyncTarget>

#include <QVersitContactExporter>
#include <QVersitContactImporter>
#include <QVersitReader>
#include <QVersitWriter>

#include <QtDebug>

USE_VERSIT_NAMESPACE

static QList<QChar> getAllContactNameGroups()
{
    QList<QChar> groups;
    groups << QLatin1Char('A')
           << QLatin1Char('B')
           << QLatin1Char('C')
           << QLatin1Char('D')
           << QLatin1Char('E')
           << QLatin1Char('F')
           << QLatin1Char('G')
           << QLatin1Char('H')
           << QLatin1Char('I')
           << QLatin1Char('J')
           << QLatin1Char('K')
           << QLatin1Char('L')
           << QLatin1Char('M')
           << QLatin1Char('N')
           << QLatin1Char('O')
           << QLatin1Char('P')
           << QLatin1Char('Q')
           << QLatin1Char('R')
           << QLatin1Char('S')
           << QLatin1Char('T')
           << QLatin1Char('U')
           << QLatin1Char('V')
           << QLatin1Char('W')
           << QLatin1Char('X')
           << QLatin1Char('Y')
           << QLatin1Char('Z')
           << QChar(0x00c5)     // Å
           << QChar(0x00c4)     // Ä
           << QChar(0x00d6)     // Ö
           << QLatin1Char('#');
    return groups;
}

SeasideCache *SeasideCache::instancePtr = 0;
QList<QChar> SeasideCache::allContactNameGroups = getAllContactNameGroups();

static QString managerName()
{
#ifdef USING_QTPIM
    // Temporary override until qtpim supports QTCONTACTS_MANAGER_OVERRIDE
    return QStringLiteral("org.nemomobile.contacts.sqlite");
#endif
    QByteArray environmentManager = qgetenv("NEMO_CONTACT_MANAGER");
    return !environmentManager.isEmpty()
            ? QString::fromLatin1(environmentManager, environmentManager.length())
            : QString();
}

template<typename T, typename Filter, typename Field>
void setDetailType(Filter &filter, Field field)
{
#ifdef USING_QTPIM
    filter.setDetailType(T::Type, field);
#else
    filter.setDetailDefinitionName(T::DefinitionName, field);
#endif
}

SeasideCache* SeasideCache::instance()
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
#ifdef USING_QTPIM
    // Currently only works with qtcontacts-sqlite
    QContactId contactId;
    if (iid != 0) {
        static const QString idStr(QString::fromLatin1("qtcontacts:org.nemomobile.contacts.sqlite::sql-%1"));
        contactId = QContactId::fromString(idStr.arg(iid));
        if (contactId.isNull()) {
            qWarning() << "Unable to formulate valid ID from:" << iid;
        }
    }
    return contactId;
#else
    return static_cast<ContactIdType>(iid);
#endif
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
#ifdef USING_QTPIM
    // We need to be able to represent an ID as a 32-bit int; we could use
    // hashing, but for now we will just extract the integral part of the ID
    // string produced by qtcontacts-sqlite
    if (!id.isNull()) {
        QStringList components = id.toString().split(QChar::fromLatin1(':'));
        const QString &idComponent = components.isEmpty() ? QString() : components.last();
        if (idComponent.startsWith(QString::fromLatin1("sql-"))) {
            return idComponent.mid(4).toUInt();
        }
    }
    return 0;
#else
    return static_cast<quint32>(id.localId());
#endif
}

#ifndef USING_QTPIM
quint32 SeasideCache::internalId(QContactLocalId id)
{
    return static_cast<quint32>(id);
}
#endif

SeasideCache::SeasideCache()
    : m_manager(managerName())
#ifdef HAS_MLITE
    , m_displayLabelOrderConf(QLatin1String("/org/nemomobile/contacts/display_label_order"))
#endif
    , m_resultsRead(0)
    , m_populated(0)
    , m_cacheIndex(0)
    , m_queryIndex(0)
    , m_appendIndex(0)
    , m_fetchFilter(FilterFavorites)
    , m_displayLabelOrder(FirstNameFirst)
    , m_updatesPending(true)
    , m_refreshRequired(false)
    , m_contactsUpdated(false)
{
    Q_ASSERT(!instancePtr);
    instancePtr = this;

    m_timer.start();

#ifdef HAS_MLITE
    connect(&m_displayLabelOrderConf, SIGNAL(valueChanged()), this, SLOT(displayLabelOrderChanged()));
    QVariant displayLabelOrder = m_displayLabelOrderConf.value();
    if (displayLabelOrder.isValid())
        m_displayLabelOrder = static_cast<DisplayLabelOrder>(displayLabelOrder.toInt());
#endif

#ifdef USING_QTPIM
    connect(&m_manager, SIGNAL(dataChanged()), this, SLOT(updateContacts()));
    connect(&m_manager, SIGNAL(contactsChanged(QList<QContactId>)),
            this, SLOT(updateContacts(QList<QContactId>)));
    connect(&m_manager, SIGNAL(contactsAdded(QList<QContactId>)),
            this, SLOT(updateContacts(QList<QContactId>)));
    connect(&m_manager, SIGNAL(contactsRemoved(QList<QContactId>)),
            this, SLOT(contactsRemoved(QList<QContactId>)));
#else
    connect(&m_manager, SIGNAL(dataChanged()), this, SLOT(updateContacts()));
    connect(&m_manager, SIGNAL(contactsChanged(QList<QContactLocalId>)),
            this, SLOT(updateContacts(QList<QContactLocalId>)));
    connect(&m_manager, SIGNAL(contactsAdded(QList<QContactLocalId>)),
            this, SLOT(updateContacts(QList<QContactLocalId>)));
    connect(&m_manager, SIGNAL(contactsRemoved(QList<QContactLocalId>)),
            this, SLOT(contactsRemoved(QList<QContactLocalId>)));
#endif

    connect(&m_fetchRequest, SIGNAL(resultsAvailable()), this, SLOT(contactsAvailable()));
    connect(&m_fetchByIdRequest, SIGNAL(resultsAvailable()), this, SLOT(contactsAvailable()));
    connect(&m_contactIdRequest, SIGNAL(resultsAvailable()), this, SLOT(contactIdsAvailable()));
    connect(&m_relationshipsFetchRequest, SIGNAL(resultsAvailable()), this, SLOT(relationshipsAvailable()));

    connect(&m_fetchRequest, SIGNAL(stateChanged(QContactAbstractRequest::State)),
            this, SLOT(requestStateChanged(QContactAbstractRequest::State)));
    connect(&m_fetchByIdRequest, SIGNAL(stateChanged(QContactAbstractRequest::State)),
            this, SLOT(requestStateChanged(QContactAbstractRequest::State)));
    connect(&m_contactIdRequest, SIGNAL(stateChanged(QContactAbstractRequest::State)),
            this, SLOT(requestStateChanged(QContactAbstractRequest::State)));
    connect(&m_relationshipsFetchRequest, SIGNAL(stateChanged(QContactAbstractRequest::State)),
            this, SLOT(requestStateChanged(QContactAbstractRequest::State)));
    connect(&m_removeRequest, SIGNAL(stateChanged(QContactAbstractRequest::State)),
            this, SLOT(requestStateChanged(QContactAbstractRequest::State)));
    connect(&m_saveRequest, SIGNAL(stateChanged(QContactAbstractRequest::State)),
            this, SLOT(requestStateChanged(QContactAbstractRequest::State)));

    m_fetchRequest.setManager(&m_manager);
    m_fetchByIdRequest.setManager(&m_manager);
    m_contactIdRequest.setManager(&m_manager);
    m_relationshipsFetchRequest.setManager(&m_manager);
    m_removeRequest.setManager(&m_manager);
    m_saveRequest.setManager(&m_manager);

    QContactFetchHint fetchHint;
    fetchHint.setOptimizationHints(QContactFetchHint::NoRelationships
            | QContactFetchHint::NoActionPreferences
            | QContactFetchHint::NoBinaryBlobs);

    // Note: no restriction on detail definitions - the cache should contain the entire contact

    m_fetchRequest.setFetchHint(fetchHint);

    setSortOrder(m_displayLabelOrder);

    m_fetchRequest.setFilter(QContactFavorite::match());
    m_fetchRequest.start();
}

SeasideCache::~SeasideCache()
{
    if (instancePtr == this)
        instancePtr = 0;
}

void SeasideCache::checkForExpiry()
{
    if (instancePtr->m_users.isEmpty()) {
        bool unused = true;
        for (int i = 0; i < FilterTypesCount; ++i) {
            unused &= instancePtr->m_models[i].isEmpty();
        }
        if (unused) {
            instancePtr->m_expiryTimer.start(30000, instancePtr);
        }
    }
}

void SeasideCache::registerModel(ListModel *model, FilterType type)
{
    if (!instancePtr) {
        new SeasideCache;
    } else {
        instancePtr->m_expiryTimer.stop();
        for (int i = 0; i < FilterTypesCount; ++i)
            instancePtr->m_models[i].removeAll(model);
    }
    instancePtr->m_models[type].append(model);
}

void SeasideCache::unregisterModel(ListModel *model)
{
    for (int i = 0; i < FilterTypesCount; ++i)
        instancePtr->m_models[i].removeAll(model);

    checkForExpiry();
}

void SeasideCache::registerUser(QObject *user)
{
    if (!instancePtr) {
        new SeasideCache;
    } else {
        instancePtr->m_expiryTimer.stop();
    }
    instancePtr->m_users.insert(user);
}

void SeasideCache::unregisterUser(QObject *user)
{
    instancePtr->m_users.remove(user);

    checkForExpiry();
}

void SeasideCache::registerNameGroupChangeListener(SeasideNameGroupChangeListener *listener)
{
    if (!instancePtr)
        new SeasideCache;
    instancePtr->m_nameGroupChangeListeners.append(listener);
}

void SeasideCache::unregisterNameGroupChangeListener(SeasideNameGroupChangeListener *listener)
{
    if (!instancePtr)
        return;
    instancePtr->m_nameGroupChangeListeners.removeAll(listener);
}

QChar SeasideCache::nameGroupForCacheItem(CacheItem *cacheItem)
{
    if (!cacheItem)
        return QChar();

    QChar group;
    QString first;
    QString last;
    QContactName nameDetail = cacheItem->contact.detail<QContactName>();
    if (SeasideCache::displayLabelOrder() == FirstNameFirst) {
        first = nameDetail.firstName();
        last = nameDetail.lastName();
    } else {
        first = nameDetail.lastName();
        last = nameDetail.firstName();
    }
    if (!first.isEmpty()) {
        group = first[0].toUpper();
    } else if (!last.isEmpty()) {
        group = last[0].toUpper();
    } else {
        QString displayLabel = (cacheItem->itemData)
                ? cacheItem->itemData->getDisplayLabel()
                : generateDisplayLabel(cacheItem->contact);
        if (!displayLabel.isEmpty())
            group = displayLabel[0].toUpper();
    }

    // XXX temporary workaround for non-latin names: use non-name details to try to find a
    // latin character group
    if (!group.isNull() && group.toLatin1() != group) {
        QString displayLabel = generateDisplayLabelFromNonNameDetails(cacheItem->contact);
        if (!displayLabel.isEmpty())
            group = displayLabel[0].toUpper();
    }

    if (group.isNull() || !allContactNameGroups.contains(group)) {
        group = QLatin1Char('#');   // 'other' group
    }
    return group;
}

QList<QChar> SeasideCache::allNameGroups()
{
    return allContactNameGroups;
}

QHash<QChar, int> SeasideCache::nameGroupCounts()
{
    if (instancePtr)
        return instancePtr->m_contactNameGroups;
    return QHash<QChar, int>();
}

SeasideCache::DisplayLabelOrder SeasideCache::displayLabelOrder()
{
    return instancePtr->m_displayLabelOrder;
}

int SeasideCache::contactId(const QContact &contact)
{
    quint32 internal = internalId(contact);
    return static_cast<int>(internal);
}

SeasideCache::CacheItem *SeasideCache::itemById(const ContactIdType &id)
{
    if (!validId(id))
        return 0;

    quint32 iid = internalId(id);

    CacheItem *item = 0;

    QHash<quint32, CacheItem>::iterator it = instancePtr->m_people.find(iid);
    if (it != instancePtr->m_people.end()) {
        item = &(*it);
    } else {
        // Insert a new item into the cache if the one doesn't exist.
        item = &(instancePtr->m_people[iid]);
#ifdef USING_QTPIM
        item->contact.setId(id);
#else
        QContactId contactId;
        contactId.setLocalId(id);
        item->contact.setId(contactId);
#endif
    }

    if (item->contactState == ContactAbsent) {
        item->contactState = ContactRequested;
        instancePtr->m_changedContacts.append(item->apiId());
        instancePtr->fetchContacts();
    }

    return item;
}

#ifdef USING_QTPIM
SeasideCache::CacheItem *SeasideCache::itemById(int id)
{
    if (id != 0) {
        QContactId contactId(apiId(static_cast<quint32>(id)));
        if (!contactId.isNull()) {
            return itemById(contactId);
        }
    }

    return 0;
}
#endif

SeasideCache::CacheItem *SeasideCache::existingItem(const ContactIdType &id)
{
    quint32 iid = internalId(id);

    QHash<quint32, CacheItem>::iterator it = instancePtr->m_people.find(iid);
    return it != instancePtr->m_people.end()
            ? &(*it)
            : 0;
}

QContact SeasideCache::contactById(const ContactIdType &id)
{
    quint32 iid = internalId(id);
    return instancePtr->m_people.value(iid, CacheItem()).contact;
}

SeasideCache::CacheItem *SeasideCache::itemByPhoneNumber(const QString &msisdn)
{
    QString normalizedNumber = Normalization::normalizePhoneNumber(msisdn);
    QHash<QString, quint32>::const_iterator it = instancePtr->m_phoneNumberIds.find(normalizedNumber);
    if (it != instancePtr->m_phoneNumberIds.end())
        return itemById(*it);
    return 0;
}

SeasideCache::CacheItem *SeasideCache::itemByEmailAddress(const QString &email)
{
    QHash<QString, quint32>::const_iterator it = instancePtr->m_emailAddressIds.find(email.toLower());
    if (it != instancePtr->m_emailAddressIds.end())
        return itemById(*it);
    return 0;
}

SeasideCache::ContactIdType SeasideCache::selfContactId()
{
    return instancePtr->m_manager.selfContactId();
}

void SeasideCache::requestUpdate()
{
    if (!m_updatesPending)
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    m_updatesPending = true;
}

bool SeasideCache::saveContact(const QContact &contact)
{
    ContactIdType id = apiId(contact);
    if (validId(id)) {
        instancePtr->m_contactsToSave[id] = contact;

        instancePtr->updateContactData(id, FilterFavorites);
        instancePtr->updateContactData(id, FilterOnline);
        instancePtr->updateContactData(id, FilterAll);
    } else {
        instancePtr->m_contactsToCreate.append(contact);
    }

    instancePtr->requestUpdate();

    return true;
}

void SeasideCache::updateContactData(
        const ContactIdType &contactId, FilterType filter)
{
    int row = m_contacts[filter].indexOf(contactId);

    QList<ListModel *> &models = m_models[filter];
    for (int i = 0; row != -1 && i < models.count(); ++i)
        models.at(i)->sourceDataChanged(row, row);
}

void SeasideCache::removeContact(const QContact &contact)
{
    ContactIdType id = apiId(contact);

    instancePtr->m_contactsToRemove.append(id);
    instancePtr->removeContactData(id, FilterFavorites);
    instancePtr->removeContactData(id, FilterOnline);
    instancePtr->removeContactData(id, FilterAll);

    instancePtr->requestUpdate();
}

void SeasideCache::removeContactData(
        const ContactIdType &contactId, FilterType filter)
{
    int row = m_contacts[filter].indexOf(contactId);
    if (row == -1)
        return;

    QList<ListModel *> &models = m_models[filter];
    for (int i = 0; i < models.count(); ++i)
        models.at(i)->sourceAboutToRemoveItems(row, row);

    m_contacts[filter].remove(row);

    for (int i = 0; i < models.count(); ++i)
        models.at(i)->sourceItemsRemoved();
}

void SeasideCache::fetchConstituents(const QContact &contact)
{
    QContactId personId(contact.id());

    if (!instancePtr->m_contactsToFetchConstituents.contains(personId)) {
        instancePtr->m_contactsToFetchConstituents.append(personId);
        instancePtr->requestUpdate();
    }
}

void SeasideCache::fetchMergeCandidates(const QContact &contact)
{
    QContactId personId(contact.id());

    if (!instancePtr->m_contactsToFetchCandidates.contains(personId)) {
        instancePtr->m_contactsToFetchCandidates.append(personId);
        instancePtr->requestUpdate();
    }
}

const QVector<SeasideCache::ContactIdType> *SeasideCache::contacts(FilterType type)
{
    return &instancePtr->m_contacts[type];
}

bool SeasideCache::isPopulated(FilterType filterType)
{
    return instancePtr->m_populated & (1 << filterType);
}

// small helper to avoid inconvenience
QString SeasideCache::generateDisplayLabel(const QContact &contact, DisplayLabelOrder order)
{
    QContactName name = contact.detail<QContactName>();

#ifdef USING_QTPIM
    QString customLabel = name.value<QString>(QContactName__FieldCustomLabel);
#else
    QString customLabel = name.customLabel();
#endif
    if (!customLabel.isNull())
        return customLabel;

    QString displayLabel;

    QString nameStr1;
    QString nameStr2;
    if (order == LastNameFirst) {
        nameStr1 = name.lastName();
        nameStr2 = name.firstName();
    } else {
        nameStr1 = name.firstName();
        nameStr2 = name.lastName();
    }

    if (!nameStr1.isNull())
        displayLabel.append(nameStr1);

    if (!nameStr2.isNull()) {
        if (!displayLabel.isEmpty())
            displayLabel.append(" ");
        displayLabel.append(nameStr2);
    }

    if (!displayLabel.isEmpty()) {
        return displayLabel;
    }

    displayLabel = generateDisplayLabelFromNonNameDetails(contact);
    if (!displayLabel.isEmpty()) {
        return displayLabel;
    }

    return "(Unnamed)"; // TODO: localisation
}

QString SeasideCache::generateDisplayLabelFromNonNameDetails(const QContact &contact)
{
    foreach (const QContactNickname& nickname, contact.details<QContactNickname>()) {
        if (!nickname.nickname().isNull()) {
            return nickname.nickname();
        }
    }

    foreach (const QContactGlobalPresence& gp, contact.details<QContactGlobalPresence>()) {
        // should only be one of these, but qtct is strange, and doesn't list it as a unique detail in the schema...
        if (!gp.nickname().isNull()) {
            return gp.nickname();
        }
    }

    foreach (const QContactPresence& presence, contact.details<QContactPresence>()) {
        if (!presence.nickname().isNull()) {
            return presence.nickname();
        }
    }

    foreach (const QContactOnlineAccount& account, contact.details<QContactOnlineAccount>()) {
        if (!account.accountUri().isNull()) {
            return account.accountUri();
        }
    }

    foreach (const QContactEmailAddress& email, contact.details<QContactEmailAddress>()) {
        if (!email.emailAddress().isNull()) {
            return email.emailAddress();
        }
    }

    QContactOrganization company = contact.detail<QContactOrganization>();
    if (!company.name().isNull())
        return company.name();

    foreach (const QContactPhoneNumber& phone, contact.details<QContactPhoneNumber>()) {
        if (!phone.number().isNull())
            return phone.number();
    }

    return QString();
}

static QContactFilter filterForMergeCandidates(const QContact &contact)
{
    // Find any contacts that we might merge with the supplied contact
    QContactFilter rv;

    QContactName name(contact.detail<QContactName>());
    QString firstName(name.firstName());
    QString lastName(name.lastName());

    if (firstName.isEmpty() && lastName.isEmpty()) {
        // Use the displayLabel to match with
        QString label(contact.detail<QContactDisplayLabel>().label());

        // Partial match to first name
        QContactDetailFilter firstNameFilter;
        setDetailType<QContactName>(firstNameFilter, QContactName::FieldFirstName);
        firstNameFilter.setMatchFlags(QContactFilter::MatchContains | QContactFilter::MatchFixedString);
        firstNameFilter.setValue(label);
        rv = rv | firstNameFilter;

        // Partial match to last name
        QContactDetailFilter lastNameFilter;
        setDetailType<QContactName>(lastNameFilter, QContactName::FieldLastName);
        lastNameFilter.setMatchFlags(QContactFilter::MatchContains | QContactFilter::MatchFixedString);
        lastNameFilter.setValue(label);
        rv = rv | lastNameFilter;

        // Partial match to nickname
        QContactDetailFilter nicknameFilter;
        setDetailType<QContactNickname>(nicknameFilter, QContactNickname::FieldNickname);
        nicknameFilter.setMatchFlags(QContactFilter::MatchContains | QContactFilter::MatchFixedString);
        nicknameFilter.setValue(label);
        rv = rv | nicknameFilter;
    } else {
        if (!firstName.isEmpty()) {
            // Partial match to first name
            QContactDetailFilter nameFilter;
            setDetailType<QContactName>(nameFilter, QContactName::FieldFirstName);
            nameFilter.setMatchFlags(QContactFilter::MatchContains | QContactFilter::MatchFixedString);
            nameFilter.setValue(firstName);
            rv = rv | nameFilter;

            // Partial match to first name in the nickname
            QContactDetailFilter nicknameFilter;
            setDetailType<QContactNickname>(nicknameFilter, QContactNickname::FieldNickname);
            nicknameFilter.setMatchFlags(QContactFilter::MatchContains | QContactFilter::MatchFixedString);
            nicknameFilter.setValue(firstName);
            rv = rv | nicknameFilter;
        }
        if (!lastName.isEmpty()) {
            // Partial match to last name
            QContactDetailFilter nameFilter;
            setDetailType<QContactName>(nameFilter, QContactName::FieldLastName);
            nameFilter.setMatchFlags(QContactFilter::MatchContains | QContactFilter::MatchFixedString);
            nameFilter.setValue(lastName);
            rv = rv | nameFilter;

            // Partial match to last name in the nickname
            QContactDetailFilter nicknameFilter;
            setDetailType<QContactNickname>(nicknameFilter, QContactNickname::FieldNickname);
            nicknameFilter.setMatchFlags(QContactFilter::MatchContains | QContactFilter::MatchFixedString);
            nicknameFilter.setValue(lastName);
            rv = rv | nicknameFilter;
        }
    }

    // Phone number match
    foreach (const QContactPhoneNumber &number, contact.details<QContactPhoneNumber>()) {
        rv = rv | QContactPhoneNumber::match(number.number());
    }

    // Email address match
    foreach (const QContactEmailAddress &emailAddress, contact.details<QContactEmailAddress>()) {
        QString address(emailAddress.emailAddress());
        int index = address.indexOf(QChar::fromLatin1('@'));
        if (index > 0) {
            // Match any address that is the same up to the @ symbol
            address = address.left(index);
        }

        QContactDetailFilter filter;
        setDetailType<QContactEmailAddress>(filter, QContactEmailAddress::FieldEmailAddress);
        filter.setMatchFlags((index > 0 ? QContactFilter::MatchStartsWith : QContactFilter::MatchExactly) | QContactFilter::MatchFixedString);
        filter.setValue(address);
        rv = rv | filter;
    }

    // Account URI match
    foreach (const QContactOnlineAccount &account, contact.details<QContactOnlineAccount>()) {
        QString uri(account.accountUri());
        int index = uri.indexOf(QChar::fromLatin1('@'));
        if (index > 0) {
            // Match any account URI that is the same up to the @ symbol
            uri = uri.left(index);
        }

        QContactDetailFilter filter;
        setDetailType<QContactOnlineAccount>(filter, QContactOnlineAccount::FieldAccountUri);
        filter.setMatchFlags((index > 0 ? QContactFilter::MatchStartsWith : QContactFilter::MatchExactly) | QContactFilter::MatchFixedString);
        filter.setValue(uri);
        rv = rv | filter;
    }

    // Only return aggregate contact IDs
    QContactDetailFilter syncTarget;
    setDetailType<QContactSyncTarget>(syncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTarget.setValue(QString::fromLatin1("aggregate"));
    rv = rv & syncTarget;

    return rv;
}

bool SeasideCache::event(QEvent *event)
{
    if (event->type() != QEvent::UpdateRequest) {
        return QObject::event(event);
    } else if (!m_contactsToRemove.isEmpty()) {
        m_removeRequest.setContactIds(m_contactsToRemove);
        m_removeRequest.start();

        m_contactsToRemove.clear();
    } else if (!m_contactsToCreate.isEmpty() || !m_contactsToSave.isEmpty()) {
        m_contactsToCreate.reserve(m_contactsToCreate.count() + m_contactsToSave.count());

        typedef QHash<ContactIdType, QContact>::iterator iterator;
        for (iterator it = m_contactsToSave.begin(); it != m_contactsToSave.end(); ++it) {
            m_contactsToCreate.append(*it);
        }

        m_saveRequest.setContacts(m_contactsToCreate);
        m_saveRequest.start();

        m_contactsToCreate.clear();
        m_contactsToSave.clear();
    } else if (!m_constituentIds.isEmpty()) {
        // Fetch the constituent information (even if they're already in the
        // cache, because we don't update non-aggregates on change notifications)
#ifdef USING_QTPIM
        m_fetchByIdRequest.setIds(m_constituentIds);
#else
        m_fetchByIdRequest.setLocalIds(m_constituentIds);
#endif
        m_fetchByIdRequest.start();
    } else if (!m_contactsToFetchConstituents.isEmpty()) {
        QContactId aggregateId = m_contactsToFetchConstituents.first();

        // Find the constituents of this contact
#ifdef USING_QTPIM
        QContact first;
        first.setId(aggregateId);
        m_relationshipsFetchRequest.setFirst(first);
        m_relationshipsFetchRequest.setRelationshipType(QContactRelationship::Aggregates());
#else
        m_relationshipsFetchRequest.setFirst(aggregateId);
        m_relationshipsFetchRequest.setRelationshipType(QContactRelationship::Aggregates);
#endif

        m_relationshipsFetchRequest.start();
    } else if (!m_contactsToFetchCandidates.isEmpty()) {
#ifdef USING_QTPIM
        ContactIdType contactId(m_contactsToFetchCandidates.first());
#else
        ContactIdType contactId(m_contactsToFetchCandidates.first().localId());
#endif
        const QContact contact(contactById(contactId));

        // Find candidates to merge with this contact
        m_contactIdRequest.setFilter(filterForMergeCandidates(contact));
        m_contactIdRequest.start();
    } else if (!m_changedContacts.isEmpty()) {
        m_resultsRead = 0;

#ifdef USING_QTPIM
        QContactIdFilter filter;
#else
        QContactLocalIdFilter filter;
#endif
        filter.setIds(m_changedContacts);
        m_changedContacts.clear();

        // A local ID filter will fetch all contacts, rather than just aggregates;
        // we only want to retrieve aggregate contacts that have changed
        QContactDetailFilter stFilter;
        setDetailType<QContactSyncTarget>(stFilter, QContactSyncTarget::FieldSyncTarget);
        stFilter.setValue("aggregate");

        m_appendIndex = 0;
        m_fetchRequest.setFilter(filter & stFilter);
        m_fetchRequest.start();
    } else if (m_refreshRequired) {
        m_resultsRead = 0;
        m_refreshRequired = false;
        m_fetchFilter = FilterFavorites;

        m_contactIdRequest.setFilter(QContactFavorite::match());
        m_contactIdRequest.start();
    } else {
        m_updatesPending = false;

        const QHash<ContactIdType,int> expiredContacts = m_expiredContacts;
        m_expiredContacts.clear();

        typedef QHash<ContactIdType,int>::const_iterator iterator;
        for (iterator it = expiredContacts.begin(); it != expiredContacts.end(); ++it) {
            if (*it >= 0)
                continue;

            quint32 iid = internalId(it.key());
            QHash<quint32, CacheItem>::iterator cacheItem = m_people.find(iid);
            if (cacheItem != m_people.end()) {
                delete cacheItem->itemData;
                delete cacheItem->modelData;
                m_people.erase(cacheItem);
            }
        }
    }
    return true;
}

void SeasideCache::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_fetchTimer.timerId()) {
        fetchContacts();
    }

    if (event->timerId() == m_expiryTimer.timerId()) {
        m_expiryTimer.stop();
        instancePtr = 0;
        deleteLater();
    }
}

void SeasideCache::contactsRemoved(const QList<ContactIdType> &)
{
    m_refreshRequired = true;
    requestUpdate();
}

void SeasideCache::updateContacts()
{
    QList<ContactIdType> contactIds;

    typedef QHash<quint32, CacheItem>::iterator iterator;
    for (iterator it = m_people.begin(); it != m_people.end(); ++it) {
        if (it->contactState != ContactAbsent)
            contactIds.append(it->apiId());
    }

    updateContacts(contactIds);
}

void SeasideCache::fetchContacts()
{
    static const int WaitIntervalMs = 250;

    if (m_fetchRequest.isActive()) {
        // The current fetch is still active - we may as well continue to accumulate
        m_fetchTimer.start(WaitIntervalMs , this);
    } else {
        m_fetchTimer.stop();
        m_fetchPostponed.invalidate();

        // Fetch any changed contacts immediately
        if (m_contactsUpdated) {
            m_contactsUpdated = false;
            m_refreshRequired = true;
        }
        requestUpdate();
    }
}

void SeasideCache::updateContacts(const QList<ContactIdType> &contactIds)
{
    // Wait for new changes to be reported
    static const int PostponementIntervalMs = 500;

    // Maximum wait until we fetch all changes previously reported
    static const int MaxPostponementMs = 5000;

    m_contactsUpdated = true;
    m_changedContacts.append(contactIds);

    if (m_fetchPostponed.isValid()) {
        // We are waiting to accumulate further changes
        int remainder = MaxPostponementMs - m_fetchPostponed.elapsed();
        if (remainder > 0) {
            // We can postpone further
            m_fetchTimer.start(std::min(remainder, PostponementIntervalMs), this);
        }
    } else {
        // Wait for further changes before we query for the ones we have now
        m_fetchPostponed.restart();
        m_fetchTimer.start(PostponementIntervalMs, this);
    }
}

void SeasideCache::contactsAvailable()
{
    QContactAbstractRequest *request = static_cast<QContactAbstractRequest *>(sender());

    QList<QContact> contacts;
    if (request == &m_fetchByIdRequest) {
        contacts = m_fetchByIdRequest.contacts();
    } else {
        contacts = m_fetchRequest.contacts();
    }

    if (m_fetchFilter == FilterFavorites
            || m_fetchFilter == FilterOnline
            || m_fetchFilter == FilterAll) {
        // Part of an initial query.
        appendContacts(contacts);
    } else {
        // An update.
        QList<QChar> modifiedGroups;

        for (int i = m_resultsRead; i < contacts.count(); ++i) {
            QContact contact = contacts.at(i);
            ContactIdType apiId = SeasideCache::apiId(contact);
            quint32 iid = internalId(contact);

            CacheItem &item = m_people[iid];
            QContactName oldName = item.contact.detail<QContactName>();
            QContactName newName = contact.detail<QContactName>();
            QChar oldNameGroup;

            if (m_fetchFilter == FilterAll)
                oldNameGroup = nameGroupForCacheItem(&item);

#ifdef USING_QTPIM
            if (newName.value<QString>(QContactName__FieldCustomLabel).isEmpty()) {
#else
            if (newName.customLabel().isEmpty()) {
#endif
#ifdef USING_QTPIM
                newName.setValue(QContactName__FieldCustomLabel, oldName.value(QContactName__FieldCustomLabel));
#else
                newName.setCustomLabel(oldName.customLabel());
#endif
                contact.saveDetail(&newName);
            }

            const bool roleDataChanged = newName != oldName
                    || contact.detail<QContactAvatar>().imageUrl() != item.contact.detail<QContactAvatar>().imageUrl();

            if (item.modelData) {
                item.modelData->contactChanged(contact);
            }
            if (item.itemData) {
                item.itemData->updateContact(contact, &item.contact);
            } else {
                item.contact = contact;
            }
            item.contactState = ContactFetched;

             QList<QContactPhoneNumber> phoneNumbers = contact.details<QContactPhoneNumber>();
             for (int j = 0; j < phoneNumbers.count(); ++j) {
                 QString normalizedNumber = Normalization::normalizePhoneNumber(phoneNumbers.at(j).number());
                 m_phoneNumberIds[normalizedNumber] = iid;
             }

             QList<QContactEmailAddress> emailAddresses = contact.details<QContactEmailAddress>();
             for (int j = 0; j < emailAddresses.count(); ++j) {
                 m_emailAddressIds[emailAddresses.at(j).emailAddress().toLower()] = iid;
             }

             if (m_fetchFilter == FilterAll) {
                 // do this even if !roleDataChanged as name groups are affected by other display label changes
                 QChar newNameGroup = nameGroupForCacheItem(&item);
                 if (newNameGroup != oldNameGroup) {
                     addToContactNameGroup(newNameGroup, &modifiedGroups);
                     removeFromContactNameGroup(oldNameGroup, &modifiedGroups);
                 }
             }

             if (roleDataChanged) {
                instancePtr->updateContactData(apiId, FilterFavorites);
                instancePtr->updateContactData(apiId, FilterOnline);
                instancePtr->updateContactData(apiId, FilterAll);
             }
        }
        m_resultsRead = contacts.count();
        notifyNameGroupsChanged(modifiedGroups);
    }
}

void SeasideCache::addToContactNameGroup(const QChar &group, QList<QChar> *modifiedGroups)
{
    if (!group.isNull()) {
        m_contactNameGroups[group] += 1;
        if (modifiedGroups && !m_nameGroupChangeListeners.isEmpty())
            modifiedGroups->append(group);
    }
}

void SeasideCache::removeFromContactNameGroup(const QChar &group, QList<QChar> *modifiedGroups)
{
    if (!group.isNull() && m_contactNameGroups.contains(group)) {
        m_contactNameGroups[group] -= 1;
        if (modifiedGroups && !m_nameGroupChangeListeners.isEmpty())
            modifiedGroups->append(group);
    }
}

void SeasideCache::notifyNameGroupsChanged(const QList<QChar> &groups)
{
    if (groups.isEmpty() || m_nameGroupChangeListeners.isEmpty())
        return;

    QHash<QChar, int> updates;
    for (int i = 0; i < groups.count(); ++i)
        updates[groups[i]] = m_contactNameGroups[groups[i]];

    for (int i = 0; i < m_nameGroupChangeListeners.count(); ++i)
        m_nameGroupChangeListeners[i]->nameGroupsUpdated(updates);
}

void SeasideCache::contactIdsAvailable()
{
    if (!m_contactsToFetchCandidates.isEmpty()) {
        m_candidateIds.append(m_contactIdRequest.ids());
        return;
    }

    synchronizeList(
            this,
            m_contacts[m_fetchFilter],
            m_cacheIndex,
            m_contactIdRequest.ids(),
            m_queryIndex);
}

void SeasideCache::relationshipsAvailable()
{
#ifdef USING_QTPIM
    static const QString aggregatesRelationship = QContactRelationship::Aggregates();
#else
    static const QString aggregatesRelationship = QContactRelationship::Aggregates;
#endif

    foreach (const QContactRelationship &rel, m_relationshipsFetchRequest.relationships()) {
        if (rel.relationshipType() == aggregatesRelationship) {
#ifdef USING_QTPIM
            m_constituentIds.append(apiId(rel.second()));
#else
            m_constituentIds.append(rel.second().localId());
#endif
        }
    }
}

void SeasideCache::finalizeUpdate(FilterType filter)
{
    const QList<ContactIdType> queryIds = m_contactIdRequest.ids();
    QVector<ContactIdType> &cacheIds = m_contacts[filter];

    if (m_cacheIndex < cacheIds.count())
        removeRange(filter, m_cacheIndex, cacheIds.count() - m_cacheIndex);

    if (m_queryIndex < queryIds.count()) {
        const int count = queryIds.count() - m_queryIndex;
        if (count)
            insertRange(filter, cacheIds.count(), count, queryIds, m_queryIndex);
    }

    m_cacheIndex = 0;
    m_queryIndex = 0;
}

void SeasideCache::removeRange(
        FilterType filter, int index, int count)
{
    QVector<ContactIdType> &cacheIds = m_contacts[filter];
    QList<ListModel *> &models = m_models[filter];
    QList<QChar> modifiedNameGroups;

    for (int i = 0; i < models.count(); ++i)
        models[i]->sourceAboutToRemoveItems(index, index + count - 1);

    for (int i = 0; i < count; ++i) {
        if (filter == FilterAll) {
            m_expiredContacts[cacheIds.at(index)] -= 1;

            removeFromContactNameGroup(nameGroupForCacheItem(existingItem(cacheIds.at(index))), &modifiedNameGroups);
        }

        cacheIds.remove(index);
    }

    for (int i = 0; i < models.count(); ++i)
        models[i]->sourceItemsRemoved();

    notifyNameGroupsChanged(modifiedNameGroups);
}

int SeasideCache::insertRange(
        FilterType filter,
        int index,
        int count,
        const QList<ContactIdType> &queryIds,
        int queryIndex)
{
    QVector<ContactIdType> &cacheIds = m_contacts[filter];
    QList<ListModel *> &models = m_models[filter];
    QList<QChar> modifiedNameGroups;

    const ContactIdType selfId = m_manager.selfContactId();

    int end = index + count - 1;
    for (int i = 0; i < models.count(); ++i)
        models[i]->sourceAboutToInsertItems(index, end);

    for (int i = 0; i < count; ++i) {
        if (queryIds.at(queryIndex + i) == selfId)
            continue;

        if (filter == FilterAll) {
            m_expiredContacts[queryIds.at(queryIndex + i)] += 1;

            addToContactNameGroup(nameGroupForCacheItem(existingItem(queryIds.at(queryIndex + i))), &modifiedNameGroups);
        }

        cacheIds.insert(index + i, queryIds.at(queryIndex + i));
    }

    for (int i = 0; i < models.count(); ++i)
        models[i]->sourceItemsInserted(index, end);

    notifyNameGroupsChanged(modifiedNameGroups);

    return end - index + 1;
}

void SeasideCache::appendContacts(const QList<QContact> &contacts)
{
    if (!contacts.isEmpty()) {
        QVector<ContactIdType> &cacheIds = m_contacts[m_fetchFilter];
        QList<ListModel *> &models = m_models[m_fetchFilter];

        cacheIds.reserve(contacts.count());

        const int begin = cacheIds.count();
        int end = cacheIds.count() + contacts.count() - m_appendIndex - 1;

        if (begin <= end) {
            for (int i = 0; i < models.count(); ++i)
                models.at(i)->sourceAboutToInsertItems(begin, end);

            for (; m_appendIndex < contacts.count(); ++m_appendIndex) {
                QContact contact = contacts.at(m_appendIndex);
                ContactIdType apiId = SeasideCache::apiId(contact);
                quint32 iid = internalId(contact);

                cacheIds.append(apiId);
                CacheItem &cacheItem = m_people[iid];
                cacheItem.contact = contact;
                cacheItem.contactState = ContactFetched;

                if (m_fetchFilter == FilterAll)
                    addToContactNameGroup(nameGroupForCacheItem(&cacheItem), 0);

                foreach (const QContactPhoneNumber &phoneNumber, contact.details<QContactPhoneNumber>()) {
                    QString normalizedNumber = Normalization::normalizePhoneNumber(phoneNumber.number());
                    m_phoneNumberIds[normalizedNumber] = iid;
                }

                foreach (const QContactEmailAddress &emailAddress, contact.details<QContactEmailAddress>()) {
                    m_emailAddressIds[emailAddress.emailAddress().toLower()] = iid;
                }
            }

            for (int i = 0; i < models.count(); ++i)
                models.at(i)->sourceItemsInserted(begin, end);

            if (!m_nameGroupChangeListeners.isEmpty())
                notifyNameGroupsChanged(m_contactNameGroups.keys());
        }
    }
}

void SeasideCache::requestStateChanged(QContactAbstractRequest::State state)
{
    if (state != QContactAbstractRequest::FinishedState)
        return;

    QContactAbstractRequest *request = static_cast<QContactAbstractRequest *>(sender());

    if (request == &m_relationshipsFetchRequest) {
        if (!m_contactsToFetchConstituents.isEmpty() && m_constituentIds.isEmpty()) {
            // We didn't find any constituents - report the empty list
            QContactId aggregateId = m_contactsToFetchConstituents.takeFirst();
#ifdef USING_QTPIM
            CacheItem *cacheItem = itemById(aggregateId);
#else
            CacheItem *cacheItem = itemById(aggregateId.localId());
#endif
            if (cacheItem->itemData) {
                cacheItem->itemData->constituentsFetched(QList<int>());
            }
        }
    } else if (request == &m_fetchByIdRequest) {
        if (!m_contactsToFetchConstituents.isEmpty()) {
            // Report these results
            QContactId aggregateId = m_contactsToFetchConstituents.takeFirst();
#ifdef USING_QTPIM
            CacheItem *cacheItem = itemById(aggregateId);
#else
            CacheItem *cacheItem = itemById(aggregateId.localId());
#endif

            QList<int> constituentIds;
            foreach (const ContactIdType &id, m_constituentIds) {
                constituentIds.append(internalId(id));
            }
            m_constituentIds.clear();

            if (cacheItem->itemData) {
                cacheItem->itemData->constituentsFetched(constituentIds);
            }
        }
    } else if (request == &m_contactIdRequest) {
        if (!m_contactsToFetchCandidates.isEmpty()) {
            // Report these results
            QContactId contactId = m_contactsToFetchCandidates.takeFirst();
#ifdef USING_QTPIM
            CacheItem *cacheItem = itemById(contactId);
#else
            CacheItem *cacheItem = itemById(contactId.localId());
#endif

            const quint32 contactIid = internalId(contactId);

            QList<int> candidateIds;
            foreach (const ContactIdType &id, m_candidateIds) {
                // Exclude the original source contact
                const quint32 iid = internalId(id);
                if (iid != contactIid) {
                    candidateIds.append(iid);
                }
            }
            m_candidateIds.clear();

            if (cacheItem->itemData) {
                cacheItem->itemData->mergeCandidatesFetched(candidateIds);
            }
        }
    }

    if (m_fetchFilter == FilterFavorites) {
        // Next, query for all contacts
        m_fetchFilter = FilterAll;

        if (!isPopulated(FilterFavorites)) {
            qDebug() << "Favorites queried in" << m_timer.elapsed() << "ms";
            m_appendIndex = 0;
            m_fetchRequest.setFilter(QContactFilter());
            m_fetchRequest.start();
            makePopulated(FilterFavorites);
        } else {
            finalizeUpdate(FilterFavorites);
            m_contactIdRequest.setFilter(QContactFilter());
            m_contactIdRequest.start();
        }
    } else if (m_fetchFilter == FilterAll) {
        // Next, query for online contacts
        m_fetchFilter = FilterOnline;

        if (!isPopulated(FilterAll)) {
            qDebug() << "All queried in" << m_timer.elapsed() << "ms";
            // Not correct, but better than nothing...
            m_appendIndex = 0;
            m_fetchRequest.setFilter(QContactGlobalPresence::match(QContactPresence::PresenceAvailable));
            m_fetchRequest.start();
            makePopulated(FilterNone);
            makePopulated(FilterAll);
        } else {
            finalizeUpdate(FilterAll);
            m_contactIdRequest.setFilter(QContactGlobalPresence::match(QContactPresence::PresenceAvailable));
            m_contactIdRequest.start();
        }
    } else if (m_fetchFilter == FilterOnline) {
        m_fetchFilter = FilterNone;

        if (m_updatesPending) {
            QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
        }

        if (!isPopulated(FilterOnline)) {
            qDebug() << "Online queried in" << m_timer.elapsed() << "ms";
            m_fetchRequest.setFetchHint(QContactFetchHint());
            makePopulated(FilterOnline);
        } else {
            finalizeUpdate(FilterOnline);
        }
    } else if (m_fetchFilter == FilterNone) {
        // Result of a specific query
        if (m_updatesPending) {
            QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
        }
    }
}

void SeasideCache::makePopulated(FilterType filter)
{
    m_populated |= (1 << filter);

    QList<ListModel *> &models = m_models[filter];
    for (int i = 0; i < models.count(); ++i)
        models.at(i)->makePopulated();
}

void SeasideCache::setSortOrder(DisplayLabelOrder order)
{
    QContactSortOrder firstNameOrder;
    setDetailType<QContactName>(firstNameOrder, QContactName::FieldFirstName);
    firstNameOrder.setCaseSensitivity(Qt::CaseInsensitive);
    firstNameOrder.setDirection(Qt::AscendingOrder);
    firstNameOrder.setBlankPolicy(QContactSortOrder::BlanksFirst);

    QContactSortOrder lastNameOrder;
    setDetailType<QContactName>(lastNameOrder, QContactName::FieldLastName);
    lastNameOrder.setCaseSensitivity(Qt::CaseInsensitive);
    lastNameOrder.setDirection(Qt::AscendingOrder);
    lastNameOrder.setBlankPolicy(QContactSortOrder::BlanksFirst);

    QList<QContactSortOrder> sorting = (order == FirstNameFirst)
            ? (QList<QContactSortOrder>() << firstNameOrder << lastNameOrder)
            : (QList<QContactSortOrder>() << lastNameOrder << firstNameOrder);

    m_fetchRequest.setSorting(sorting);
    m_contactIdRequest.setSorting(sorting);
}

void SeasideCache::displayLabelOrderChanged()
{
#ifdef HAS_MLITE
    QVariant displayLabelOrder = m_displayLabelOrderConf.value();
    if (displayLabelOrder.isValid() && displayLabelOrder.toInt() != m_displayLabelOrder) {
        m_displayLabelOrder = static_cast<DisplayLabelOrder>(displayLabelOrder.toInt());

        setSortOrder(m_displayLabelOrder);

        typedef QHash<quint32, CacheItem>::iterator iterator;
        for (iterator it = m_people.begin(); it != m_people.end(); ++it) {
            if (it->itemData) {
                it->itemData->displayLabelOrderChanged(m_displayLabelOrder);
            } else {
                QContactName name = it->contact.detail<QContactName>();
#ifdef USING_QTPIM
                name.setValue(QContactName__FieldCustomLabel, generateDisplayLabel(it->contact));
#else
                name.setCustomLabel(generateDisplayLabel(it->contact));
#endif
                it->contact.saveDetail(&name);
            }
        }

        for (int i = 0; i < FilterTypesCount; ++i) {
            for (int j = 0; j < m_models[i].count(); ++j)
                m_models[i].at(j)->updateDisplayLabelOrder();
        }

        m_refreshRequired = true;
        requestUpdate();
    }
#endif
}

int SeasideCache::importContacts(const QString &path)
{
    QFile vcf(path);
    if (!vcf.open(QIODevice::ReadOnly)) {
        qWarning() << Q_FUNC_INFO << "Cannot open " << path;
        return 0;
    }

    // TODO: thread
    QVersitReader reader(&vcf);
    reader.startReading();
    reader.waitForFinished();

    QVersitContactImporter importer;
    importer.importDocuments(reader.results());

    QList<QContact> newContacts = importer.contacts();

    instancePtr->m_contactsToCreate += newContacts;
    instancePtr->requestUpdate();

    return newContacts.count();
}

QString SeasideCache::exportContacts()
{
    QVersitContactExporter exporter;

    QList<QContact> contacts;
    contacts.reserve(instancePtr->m_people.count());

    QList<ContactIdType> contactsToFetch;
    contactsToFetch.reserve(instancePtr->m_people.count());

    const quint32 selfId = internalId(instancePtr->m_manager.selfContactId());

    typedef QHash<quint32, CacheItem>::iterator iterator;
    for (iterator it = instancePtr->m_people.begin(); it != instancePtr->m_people.end(); ++it) {
        if (it.key() == selfId) {
            continue;
        } else if (it->contactState == ContactFetched) {
            contacts.append(it->contact);
        } else {
            contactsToFetch.append(apiId(it.key()));
        }
    }

    if (!contactsToFetch.isEmpty()) {
        QList<QContact> fetchedContacts = instancePtr->m_manager.contacts(contactsToFetch);
        contacts.append(fetchedContacts);
    }

    if (!exporter.exportContacts(contacts)) {
        qWarning() << Q_FUNC_INFO << "Failed to export contacts: " << exporter.errorMap();
        return QString();
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QString baseDir;
    foreach (const QString &loc, QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)) {
        baseDir = loc;
        break;
    }
#else
    const QString baseDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#endif
    QFile vcard(baseDir
              + QDir::separator()
              + QDateTime::currentDateTime().toString("ss_mm_hh_dd_mm_yyyy")
              + ".vcf");

    if (!vcard.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot open " << vcard.fileName();
        return QString();
    }

    QVersitWriter writer(&vcard);
    if (!writer.startWriting(exporter.documents())) {
        qWarning() << Q_FUNC_INFO << "Can't start writing vcards " << writer.error();
        return QString();
    }

    // TODO: thread
    writer.waitForFinished();
    return vcard.fileName();
}

