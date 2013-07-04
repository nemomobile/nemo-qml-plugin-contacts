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

#ifndef SEASIDECACHE_H
#define SEASIDECACHE_H

#include <QContact>
#include <QContactManager>
#include <QContactFetchRequest>
#include <QContactFetchByIdRequest>
#include <QContactRemoveRequest>
#include <QContactSaveRequest>
#include <QContactRelationshipFetchRequest>
#ifdef USING_QTPIM
#include <QContactIdFilter>
#include <QContactIdFetchRequest>
#else
#include <QContactLocalIdFilter>
#include <QContactLocalIdFetchRequest>
#endif

#include <QBasicTimer>
#include <QSet>

#include <QElapsedTimer>

#ifdef HAS_MLITE
#include <mgconfitem.h>
#endif

#include "seasidefilteredmodel.h"

struct SeasideCacheItem
{
    SeasideCacheItem() : person(0), hasCompleteContact(false) {}
    SeasideCacheItem(const QContact &contact) : contact(contact), person(0), hasCompleteContact(false) {}

    SeasideFilteredModel::ContactIdType apiId() const { return SeasideFilteredModel::apiId(contact); }

    QContact contact;
    SeasidePerson *person;
    QStringList filterKey;
    bool hasCompleteContact;
};

class SeasideNameGroupChangeListener
{
public:
    SeasideNameGroupChangeListener() {}
    ~SeasideNameGroupChangeListener() {}

    virtual void nameGroupsUpdated(const QHash<QChar, int> &groups) = 0;
};

class SeasideCache : public QObject
{
    Q_OBJECT
public:
    typedef SeasideFilteredModel::ContactIdType ContactIdType;

    static void registerModel(SeasideFilteredModel *model, SeasideFilteredModel::FilterType type);
    static void unregisterModel(SeasideFilteredModel *model);

    static void registerUser(QObject *user);
    static void unregisterUser(QObject *user);

    static void registerNameGroupChangeListener(SeasideNameGroupChangeListener *listener);
    static void unregisterNameGroupChangeListener(SeasideNameGroupChangeListener *listener);

    static SeasideFilteredModel::DisplayLabelOrder displayLabelOrder();

    static int contactId(const QContact &contact);

    static SeasideCacheItem *cacheItemById(const ContactIdType &id);
    static SeasidePerson *personById(const ContactIdType &id);
#ifdef USING_QTPIM
    static SeasidePerson *personById(int id);
#endif
    static SeasidePerson *selfPerson();
    static QContact contactById(const ContactIdType &id);
    static QChar nameGroupForCacheItem(SeasideCacheItem *cacheItem);
    static QList<QChar> allNameGroups();
    static QHash<QChar, int> nameGroupCounts();

    static SeasidePerson *person(SeasideCacheItem *item);

    static SeasidePerson *personByPhoneNumber(const QString &msisdn);
    static SeasidePerson *personByEmailAddress(const QString &email);
    static bool savePerson(SeasidePerson *person);
    static void removePerson(SeasidePerson *person);

    static void fetchConstituents(SeasidePerson *person);

    static void fetchMergeCandidates(SeasidePerson *person);

    static int importContacts(const QString &path);
    static QString exportContacts();

    static const QVector<ContactIdType> *contacts(SeasideFilteredModel::FilterType filterType);
    static bool isPopulated(SeasideFilteredModel::FilterType filterType);

    bool event(QEvent *event);

    // For synchronizeLists()
    int insertRange(int index, int count, const QList<ContactIdType> &source, int sourceIndex) {
        return insertRange(m_fetchFilter, index, count, source, sourceIndex); }
    int removeRange(int index, int count) { removeRange(m_fetchFilter, index, count); return 0; }

protected:
    void timerEvent(QTimerEvent *event);

private slots:
    void contactsAvailable();
    void contactIdsAvailable();
    void relationshipsAvailable();
    void requestStateChanged(QContactAbstractRequest::State state);
#ifdef USING_QTPIM
    void contactsRemoved(const QList<QContactId> &contactIds);
#else
    void contactsRemoved(const QList<QContactLocalId> &contactIds);
#endif
    void updateContacts();
#ifdef USING_QTPIM
    void updateContacts(const QList<QContactId> &contactIds);
#else
    void updateContacts(const QList<QContactLocalId> &contactIds);
#endif
    void displayLabelOrderChanged();

private:
    SeasideCache();
    ~SeasideCache();

    static void checkForExpiry();

    void requestUpdate();
    void appendContacts(const QList<QContact> &contacts);
    void fetchContacts();

    void finalizeUpdate(SeasideFilteredModel::FilterType filter);
    void removeRange(SeasideFilteredModel::FilterType filter, int index, int count);
    int insertRange(
            SeasideFilteredModel::FilterType filter,
            int index,
            int count,
            const QList<ContactIdType> &queryIds,
            int queryIndex);

    void updateContactData(const ContactIdType &contactId, SeasideFilteredModel::FilterType filter);
    void removeContactData(const ContactIdType &contactId, SeasideFilteredModel::FilterType filter);
    void makePopulated(SeasideFilteredModel::FilterType filter);

    void addToContactNameGroup(const QChar &group, QList<QChar> *modifiedGroups = 0);
    void removeFromContactNameGroup(const QChar &group, QList<QChar> *modifiedGroups = 0);
    void notifyNameGroupsChanged(const QList<QChar> &groups);

    QBasicTimer m_expiryTimer;
    QBasicTimer m_fetchTimer;
    QHash<quint32, SeasideCacheItem> m_people;
    QHash<QString, quint32> m_phoneNumberIds;
    QHash<QString, quint32> m_emailAddressIds;
    QHash<ContactIdType, QContact> m_contactsToSave;
    QHash<QChar, int> m_contactNameGroups;
    QList<QContact> m_contactsToCreate;
    QList<ContactIdType> m_contactsToRemove;
    QList<ContactIdType> m_changedContacts;
    QList<QContactId> m_contactsToFetchConstituents;
    QList<QContactId> m_contactsToFetchCandidates;
    QList<SeasideNameGroupChangeListener*> m_nameGroupChangeListeners;
    QVector<ContactIdType> m_contacts[SeasideFilteredModel::FilterTypesCount];
    QList<SeasideFilteredModel *> m_models[SeasideFilteredModel::FilterTypesCount];
    QSet<QObject *> m_users;
    QHash<ContactIdType,int> m_expiredContacts;
    QContactManager m_manager;
    QContactFetchRequest m_fetchRequest;
    QContactFetchByIdRequest m_fetchByIdRequest;
#ifdef USING_QTPIM
    QContactIdFetchRequest m_contactIdRequest;
#else
    QContactLocalIdFetchRequest m_contactIdRequest;
#endif
    QContactRelationshipFetchRequest m_relationshipsFetchRequest;
    QContactRemoveRequest m_removeRequest;
    QContactSaveRequest m_saveRequest;
#ifdef HAS_MLITE
    MGConfItem m_displayLabelOrderConf;
#endif
    int m_resultsRead;
    int m_populated;
    int m_cacheIndex;
    int m_queryIndex;
    int m_appendIndex;
    SeasideFilteredModel::FilterType m_fetchFilter;
    SeasideFilteredModel::DisplayLabelOrder m_displayLabelOrder;
    bool m_updatesPending;
    bool m_fetchActive;
    bool m_refreshRequired;
    bool m_contactsUpdated;
    QList<ContactIdType> m_constituentIds;
    QList<ContactIdType> m_candidateIds;

    QElapsedTimer m_timer;
    QElapsedTimer m_fetchPostponed;

    static SeasideCache *instance;
    static QList<QChar> allContactNameGroups;
};


#endif
