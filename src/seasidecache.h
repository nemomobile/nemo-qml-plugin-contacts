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
#include <QVector>

#include <QElapsedTimer>
#include <QAbstractListModel>

#ifdef HAS_MLITE
#include <mgconfitem.h>
#endif

USE_CONTACTS_NAMESPACE

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
#ifdef USING_QTPIM
    typedef QContactId ContactIdType;
#else
    typedef QContactLocalId ContactIdType;
#endif

    enum FilterType {
        FilterNone,
        FilterAll,
        FilterFavorites,
        FilterOnline,
        FilterTypesCount
    };

    enum DisplayLabelOrder {
        FirstNameFirst,
        LastNameFirst
    };

    enum ContactState {
        ContactAbsent,
        ContactRequested,
        ContactFetched
    };

    struct ItemData
    {
        virtual ~ItemData() {}

        virtual QString getDisplayLabel() const = 0;
        virtual void displayLabelOrderChanged(DisplayLabelOrder order) = 0;

        virtual void contactFetched(const QContact &contact) = 0;
        virtual void constituentsFetched(const QList<int> &ids) = 0;
        virtual void mergeCandidatesFetched(const QList<int> &ids) = 0;
    };

    struct CacheItem
    {
        CacheItem() : data(0), contactState(ContactAbsent) {}
        CacheItem(const QContact &contact) : contact(contact), data(0), contactState(ContactAbsent) {}

        ContactIdType apiId() const { return SeasideCache::apiId(contact); }

        QContact contact;
        ItemData *data;
        QStringList filterKey;
        ContactState contactState;
    };

    class ListModel : public QAbstractListModel
    {
    public:
        ListModel(QObject *parent = 0) : QAbstractListModel(parent) {}
        virtual ~ListModel() {}

        virtual void sourceAboutToRemoveItems(int begin, int end) = 0;
        virtual void sourceItemsRemoved() = 0;

        virtual void sourceAboutToInsertItems(int begin, int end) = 0;
        virtual void sourceItemsInserted(int begin, int end) = 0;

        virtual void sourceDataChanged(int begin, int end) = 0;

        virtual void makePopulated() = 0;
        virtual void updateDisplayLabelOrder() = 0;
    };

    static ContactIdType apiId(const QContact &contact);
    static ContactIdType apiId(quint32 iid);

    static bool validId(const ContactIdType &id);

    static quint32 internalId(const QContact &contact);
    static quint32 internalId(const QContactId &id);
#ifndef USING_QTPIM
    static quint32 internalId(QContactLocalId id);
#endif

    static void registerModel(ListModel *model, FilterType type);
    static void unregisterModel(ListModel *model);

    static void registerUser(QObject *user);
    static void unregisterUser(QObject *user);

    static void registerNameGroupChangeListener(SeasideNameGroupChangeListener *listener);
    static void unregisterNameGroupChangeListener(SeasideNameGroupChangeListener *listener);

    static DisplayLabelOrder displayLabelOrder();

    static int contactId(const QContact &contact);

    static CacheItem *existingItem(const ContactIdType &id);
    static CacheItem *itemById(const ContactIdType &id);
#ifdef USING_QTPIM
    static CacheItem *itemById(int id);
#endif
    static ContactIdType selfContactId();
    static QContact contactById(const ContactIdType &id);
    static QChar nameGroupForCacheItem(CacheItem *cacheItem);
    static QList<QChar> allNameGroups();
    static QHash<QChar, int> nameGroupCounts();

    static CacheItem *itemByPhoneNumber(const QString &msisdn);
    static CacheItem *itemByEmailAddress(const QString &email);
    static bool saveContact(const QContact &contact);
    static void removeContact(const QContact &contact);

    static void fetchConstituents(const QContact &contact);
    static void fetchMergeCandidates(const QContact &contact);

    static int importContacts(const QString &path);
    static QString exportContacts();

    static const QVector<ContactIdType> *contacts(FilterType filterType);
    static bool isPopulated(FilterType filterType);

    static QString generateDisplayLabel(const QContact &contact, DisplayLabelOrder order = FirstNameFirst);
    static QString generateDisplayLabelFromNonNameDetails(const QContact &contact);

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

    void finalizeUpdate(FilterType filter);
    void removeRange(FilterType filter, int index, int count);
    int insertRange(
            FilterType filter,
            int index,
            int count,
            const QList<ContactIdType> &queryIds,
            int queryIndex);

    void updateContactData(const ContactIdType &contactId, FilterType filter);
    void removeContactData(const ContactIdType &contactId, FilterType filter);
    void makePopulated(FilterType filter);

    void addToContactNameGroup(const QChar &group, QList<QChar> *modifiedGroups = 0);
    void removeFromContactNameGroup(const QChar &group, QList<QChar> *modifiedGroups = 0);
    void notifyNameGroupsChanged(const QList<QChar> &groups);

    QBasicTimer m_expiryTimer;
    QBasicTimer m_fetchTimer;
    QHash<quint32, CacheItem> m_people;
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
    QVector<ContactIdType> m_contacts[FilterTypesCount];
    QList<ListModel *> m_models[FilterTypesCount];
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
    FilterType m_fetchFilter;
    DisplayLabelOrder m_displayLabelOrder;
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
