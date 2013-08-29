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

#ifndef SEASIDEFILTEREDMODEL_H
#define SEASIDEFILTEREDMODEL_H

#include <seasidecache.h>

#include <QStringList>
#include <QVector>

#include <QContact>

class SeasidePerson;

USE_CONTACTS_NAMESPACE

class SeasideFilteredModel : public SeasideCache::ListModel
{
    Q_OBJECT
    Q_PROPERTY(bool populated READ isPopulated NOTIFY populatedChanged)
    Q_PROPERTY(FilterType filterType READ filterType WRITE setFilterType NOTIFY filterTypeChanged)
    Q_PROPERTY(DisplayLabelOrder displayLabelOrder READ displayLabelOrder WRITE setDisplayLabelOrder NOTIFY displayLabelOrderChanged)
    Q_PROPERTY(QString sortProperty READ sortProperty NOTIFY sortPropertyChanged)
    Q_PROPERTY(QString filterPattern READ filterPattern WRITE setFilterPattern NOTIFY filterPatternChanged)
    Q_PROPERTY(int requiredProperty READ requiredProperty WRITE setRequiredProperty NOTIFY requiredPropertyChanged)
    Q_PROPERTY(bool searchByFirstNameCharacter READ searchByFirstNameCharacter WRITE setSearchByFirstNameCharacter NOTIFY searchByFirstNameCharacterChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_ENUMS(FilterType RequiredPropertyType DisplayLabelOrder)

public:
    enum FilterType {
        FilterNone = SeasideCache::FilterNone,
        FilterAll = SeasideCache::FilterAll,
        FilterFavorites = SeasideCache::FilterFavorites,
        FilterOnline = SeasideCache::FilterOnline,
        FilterTypesCount = SeasideCache::FilterTypesCount
    };

    enum RequiredPropertyType {
        NoPropertyRequired = 0,
        AccountUriRequired = SeasideCache::FetchAccountUri,
        PhoneNumberRequired = SeasideCache::FetchPhoneNumber,
        EmailAddressRequired = SeasideCache::FetchEmailAddress
    };

    enum DisplayLabelOrder {
        FirstNameFirst = SeasideCache::FirstNameFirst,
        LastNameFirst = SeasideCache::LastNameFirst
    };

    enum PeopleRoles {
        FirstNameRole = Qt::UserRole,
        LastNameRole,
        FavoriteRole,
        AvatarRole,
        AvatarUrlRole,
        SectionBucketRole,
        GlobalPresenceStateRole,
        ContactIdRole,
        PhoneNumbersRole,
        EmailAddressesRole,
        AccountUrisRole,
        AccountPathsRole,
        PersonRole,
        PrimaryNameRole,
        SecondaryNameRole
    };

    typedef SeasideCache::ContactIdType ContactIdType;

    SeasideFilteredModel(QObject *parent = 0);
    ~SeasideFilteredModel();

    bool isPopulated() const;

    FilterType filterType() const;
    void setFilterType(FilterType type);

    QString filterPattern() const;
    void setFilterPattern(const QString &pattern);

    int requiredProperty() const;
    void setRequiredProperty(int type);

    bool searchByFirstNameCharacter() const;
    void setSearchByFirstNameCharacter(bool searchByFirstNameCharacter);

    DisplayLabelOrder displayLabelOrder() const;
    void setDisplayLabelOrder(DisplayLabelOrder order);

    QString sortProperty() const;

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QVariant get(int row, int role) const;

    Q_INVOKABLE bool savePerson(SeasidePerson *person);
    Q_INVOKABLE SeasidePerson *personByRow(int row) const;
    Q_INVOKABLE SeasidePerson *personById(int id) const;
    Q_INVOKABLE SeasidePerson *personByPhoneNumber(const QString &number, bool requireComplete = true) const;
    Q_INVOKABLE SeasidePerson *personByEmailAddress(const QString &email, bool requireComplete = true) const;
    Q_INVOKABLE SeasidePerson *personByOnlineAccount(const QString &localUid, const QString &remoteUid, bool requireComplete = true) const;
    Q_INVOKABLE SeasidePerson *selfPerson() const;
    Q_INVOKABLE void removePerson(SeasidePerson *person);

    Q_INVOKABLE int importContacts(const QString &path);
    Q_INVOKABLE QString exportContacts();

    QModelIndex index(const QModelIndex &parent, int row, int column) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant data(SeasideCache::CacheItem *item, int role) const;

    // Backward-compatibilty:
    Q_INVOKABLE void setFilter(FilterType type) { setFilterType(type); }
    Q_INVOKABLE void search(const QString &pattern) { setFilterPattern(pattern); }

    bool filterId(const ContactIdType &contactId) const;

    // For synchronizeLists()
    bool filterValue(const ContactIdType &contactId) const { return filterId(contactId); }
    void insertRange(int index, int count, const QVector<ContactIdType> &source, int sourceIndex);
    void removeRange(int index, int count);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    virtual
#endif
    QHash<int, QByteArray> roleNames() const;

    // Implementations for SeasideCache::ListModel:
    void sourceAboutToRemoveItems(int begin, int end);
    void sourceItemsRemoved();

    void sourceAboutToInsertItems(int begin, int end);
    void sourceItemsInserted(int begin, int end);

    void sourceDataChanged(int begin, int end);

    void sourceItemsChanged();

    void makePopulated();
    void updateDisplayLabelOrder();
    void updateSortProperty();

signals:
    void populatedChanged();
    void filterTypeChanged();
    void filterPatternChanged();
    void requiredPropertyChanged();
    void searchByFirstNameCharacterChanged();
    void displayLabelOrderChanged();
    void sortPropertyChanged();
    void countChanged();

private:
    void populateIndex();
    void refineIndex();
    void updateIndex();
    void updateContactData(const ContactIdType &contactId, FilterType filter);
    void updateRegistration();

    bool isFiltered() const;
    void updateFilters(const QString &pattern, int property);

    void invalidateRows(int begin, int count, bool filteredIndex = true, bool removeFromModel = true);

    SeasideCache::CacheItem *existingItem(const ContactIdType &contactId) const;

    SeasidePerson *personFromItem(SeasideCache::CacheItem *item) const;

    QVector<ContactIdType> m_filteredContactIds;
    const QVector<ContactIdType> *m_contactIds;
    const QVector<ContactIdType> *m_referenceContactIds;
    QStringList m_filterParts;
    QString m_filterPattern;
    int m_filterIndex;
    int m_referenceIndex;
    FilterType m_filterType;
    FilterType m_effectiveFilterType;
    SeasideCache::FetchDataType m_fetchTypes;
    int m_requiredProperty;
    bool m_searchByFirstNameCharacter;

    mutable SeasideCache::CacheItem *m_lastItem;
    mutable ContactIdType m_lastId;
};

#endif
