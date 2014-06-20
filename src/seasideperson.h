/*
 * Copyright (C) 2012 Robin Burchell <robin+nemo@viroteck.net>
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

#ifndef SEASIDEPERSON_H
#define SEASIDEPERSON_H

// Qt
#include <QObject>
#include <QUrl>

#include <qqml.h>

// Mobility
#include <QContact>
#include <QContactPresence>

// Seaside
#include <seasidecache.h>

QTCONTACTS_USE_NAMESPACE

Q_DECLARE_METATYPE(QContact)

class SeasidePerson;

class SeasidePersonAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SeasidePerson *selfPerson READ selfPerson NOTIFY selfPersonChanged)

public:
    SeasidePersonAttached(QObject *parent);
    ~SeasidePersonAttached();

    SeasidePerson *selfPerson() const;

    Q_INVOKABLE QString normalizePhoneNumber(const QString &input);
    Q_INVOKABLE QString minimizePhoneNumber(const QString &input);
    Q_INVOKABLE QString validatePhoneNumber(const QString &input);

    Q_INVOKABLE QVariantList removeDuplicatePhoneNumbers(const QVariantList &phoneNumbers);
    Q_INVOKABLE QVariantList removeDuplicateOnlineAccounts(const QVariantList &onlineAccounts);
    Q_INVOKABLE QVariantList removeDuplicateEmailAddresses(const QVariantList &emailAddresses);

signals:
    // Not currently emitted:
    void selfPersonChanged();
};

class SeasidePerson
    : public QObject,
      public SeasideCache::ItemData,
      public SeasideCache::ResolveListener,
      public SeasideCache::ItemListener
{
    Q_OBJECT
    Q_ENUMS(DetailType)
    Q_ENUMS(DetailSubType)
    Q_ENUMS(AddressField)
    Q_ENUMS(DetailLabel)
    Q_ENUMS(PresenceState)

public:
    /**
     * Identifiers of contact details for the UI.
     */
    enum DetailType {
        NoType,
        FirstNameType,
        LastNameType,
        MiddleNameType,
        PrefixType,
        SuffixType,
        CompanyType,
        TitleType,
        RoleType,
        DepartmentType,
        NicknameType,
        PhoneNumberType,
        EmailAddressType,
        OnlineAccountType,
        AddressType,
        WebsiteType,
        BirthdayType,
        AnniversaryType,
        GlobalPresenceStateType
    };

    enum DetailSubType {
        NoSubType,
        PhoneSubTypeLandline,
        PhoneSubTypeMobile,
        PhoneSubTypeFax,
        PhoneSubTypePager,
        PhoneSubTypeVoice,
        PhoneSubTypeModem,
        PhoneSubTypeVideo,
        PhoneSubTypeCar,
        PhoneSubTypeBulletinBoardSystem,
        PhoneSubTypeMessagingCapable,
        PhoneSubTypeAssistant,
        PhoneSubTypeDtmfMenu,
        AddressSubTypeParcel,
        AddressSubTypePostal,
        AddressSubTypeDomestic,
        AddressSubTypeInternational,
        OnlineAccountSubTypeSip,
        OnlineAccountSubTypeSipVoip,
        OnlineAccountSubTypeImpp,
        OnlineAccountSubTypeVideoShare,
        WebsiteSubTypeHomePage,
        WebsiteSubTypeBlog,
        WebsiteSubTypeFavorite,
        AnniversarySubTypeWedding,
        AnniversarySubTypeEngagement,
        AnniversarySubTypeHouse,
        AnniversarySubTypeEmployment,
        AnniversarySubTypeMemorial
    };

    enum AddressField {
        AddressStreetField,
        AddressLocalityField,
        AddressRegionField,
        AddressPostcodeField,
        AddressCountryField,
        AddressPOBoxField,
    };

    enum DetailLabel {
        NoLabel,
        HomeLabel,
        WorkLabel,
        OtherLabel
    };

    enum PresenceState {
        PresenceUnknown = QContactPresence::PresenceUnknown,
        PresenceAvailable = QContactPresence::PresenceAvailable,
        PresenceHidden = QContactPresence::PresenceHidden,
        PresenceBusy = QContactPresence::PresenceBusy,
        PresenceAway = QContactPresence::PresenceAway,
        PresenceExtendedAway = QContactPresence::PresenceExtendedAway,
        PresenceOffline = QContactPresence::PresenceOffline
    };

    explicit SeasidePerson(QObject *parent = 0);
    explicit SeasidePerson(const QContact &contact, QObject *parent = 0);
    SeasidePerson(QContact *contact, bool complete, QObject *parent = 0);

    ~SeasidePerson();

    Q_PROPERTY(int id READ id NOTIFY contactChanged)
    int id() const;

    Q_PROPERTY(bool complete READ isComplete NOTIFY completeChanged)
    bool isComplete() const;
    void setComplete(bool complete);

    Q_PROPERTY(QString firstName READ firstName WRITE setFirstName NOTIFY firstNameChanged)
    QString firstName() const;
    void setFirstName(const QString &name);

    Q_PROPERTY(QString lastName READ lastName WRITE setLastName NOTIFY lastNameChanged)
    QString lastName() const;
    void setLastName(const QString &name);

    Q_PROPERTY(QString middleName READ middleName WRITE setMiddleName NOTIFY middleNameChanged)
    QString middleName() const;
    void setMiddleName(const QString &name);

    Q_PROPERTY(QString namePrefix READ namePrefix WRITE setNamePrefix NOTIFY namePrefixChanged)
    QString namePrefix() const;
    void setNamePrefix(const QString &prefix);

    Q_PROPERTY(QString nameSuffix READ nameSuffix WRITE setNameSuffix NOTIFY nameSuffixChanged)
    QString nameSuffix() const;
    void setNameSuffix(const QString &suffix);

    Q_PROPERTY(QString sectionBucket READ sectionBucket NOTIFY displayLabelChanged)
    QString sectionBucket() const;

    Q_PROPERTY(QString displayLabel READ displayLabel NOTIFY displayLabelChanged)
    QString displayLabel() const;

    Q_PROPERTY(QString primaryName READ primaryName NOTIFY primaryNameChanged)
    QString primaryName() const;

    Q_PROPERTY(QString secondaryName READ secondaryName NOTIFY secondaryNameChanged)
    QString secondaryName() const;

    Q_PROPERTY(QString companyName READ companyName WRITE setCompanyName NOTIFY companyNameChanged)
    QString companyName() const;
    void setCompanyName(const QString &name);

    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    QString title() const;
    void setTitle(const QString &title);

    Q_PROPERTY(QString role READ role WRITE setRole NOTIFY roleChanged)
    QString role() const;
    void setRole(const QString &role);

    Q_PROPERTY(QString department READ department WRITE setDepartment NOTIFY departmentChanged)
    QString department() const;
    void setDepartment(const QString &department);

    Q_PROPERTY(bool favorite READ favorite WRITE setFavorite NOTIFY favoriteChanged)
    bool favorite() const;
    void setFavorite(bool favorite);

    Q_PROPERTY(QUrl avatarPath READ avatarPath WRITE setAvatarPath NOTIFY avatarPathChanged)
    QUrl avatarPath() const;
    void setAvatarPath(QUrl avatarPath);

    Q_PROPERTY(QUrl avatarUrl READ avatarUrl WRITE setAvatarUrl NOTIFY avatarUrlChanged)
    QUrl avatarUrl() const;
    void setAvatarUrl(QUrl avatarUrl);
    Q_INVOKABLE QUrl filteredAvatarUrl(const QStringList &metadataFragments = QStringList()) const;

    Q_PROPERTY(QVariantList nicknameDetails READ nicknameDetails WRITE setNicknameDetails NOTIFY nicknameDetailsChanged)
    QVariantList nicknameDetails() const;
    void setNicknameDetails(const QVariantList &nicknameDetails);

    Q_PROPERTY(QVariantList phoneDetails READ phoneDetails WRITE setPhoneDetails NOTIFY phoneDetailsChanged)
    QVariantList phoneDetails() const;
    void setPhoneDetails(const QVariantList &phoneDetails);

    Q_PROPERTY(QVariantList emailDetails READ emailDetails WRITE setEmailDetails NOTIFY emailDetailsChanged)
    QVariantList emailDetails() const;
    void setEmailDetails(const QVariantList &emailDetails);

    Q_PROPERTY(QVariantList addressDetails READ addressDetails WRITE setAddressDetails NOTIFY addressDetailsChanged)
    QVariantList addressDetails() const;
    void setAddressDetails(const QVariantList &addressDetails);

    Q_PROPERTY(QVariantList websiteDetails READ websiteDetails WRITE setWebsiteDetails NOTIFY websiteDetailsChanged)
    QVariantList websiteDetails() const;
    void setWebsiteDetails(const QVariantList &websiteDetails);

    Q_PROPERTY(QDateTime birthday READ birthday WRITE setBirthday NOTIFY birthdayChanged RESET resetBirthday)
    QDateTime birthday() const;
    void setBirthday(const QDateTime &bd);
    void resetBirthday();

    Q_PROPERTY(QVariantList anniversaryDetails READ anniversaryDetails WRITE setAnniversaryDetails NOTIFY anniversaryDetailsChanged)
    QVariantList anniversaryDetails() const;
    void setAnniversaryDetails(const QVariantList &anniversaryDetails);

    Q_PROPERTY(PresenceState globalPresenceState READ globalPresenceState NOTIFY globalPresenceStateChanged)
    PresenceState globalPresenceState() const;

    Q_PROPERTY(QVariantList accountDetails READ accountDetails WRITE setAccountDetails NOTIFY accountDetailsChanged)
    QVariantList accountDetails() const;
    void setAccountDetails(const QVariantList &accountDetails);

    Q_PROPERTY(QString syncTarget READ syncTarget CONSTANT)
    QString syncTarget() const;

    Q_PROPERTY(QList<int> constituents READ constituents NOTIFY constituentsChanged)
    QList<int> constituents() const;
    void setConstituents(const QList<int> &constituents);

    Q_PROPERTY(QList<int> mergeCandidates READ mergeCandidates NOTIFY mergeCandidatesChanged)
    QList<int> mergeCandidates() const;
    void setMergeCandidates(const QList<int> &candidates);

    QContact contact() const;
    void setContact(const QContact &contact);

    Q_INVOKABLE void ensureComplete();

    Q_INVOKABLE QVariant contactData() const;
    Q_INVOKABLE void setContactData(const QVariant &data);

    Q_INVOKABLE void resetContactData();

    Q_INVOKABLE QString vCard() const;
    Q_INVOKABLE QStringList avatarUrls() const;
    Q_INVOKABLE QStringList avatarUrlsExcluding(const QStringList &excludeMetadata) const;

    Q_INVOKABLE void aggregateInto(SeasidePerson *person);
    Q_INVOKABLE void disaggregateFrom(SeasidePerson *person);

    Q_INVOKABLE void fetchConstituents();
    Q_INVOKABLE void fetchMergeCandidates();

    Q_INVOKABLE void resolvePhoneNumber(const QString &number, bool requireComplete = true);
    Q_INVOKABLE void resolveEmailAddress(const QString &address, bool requireComplete = true);
    Q_INVOKABLE void resolveOnlineAccount(const QString &localUid, const QString &remoteUid, bool requireComplete = true);

    Q_INVOKABLE static QVariantList removeDuplicatePhoneNumbers(const QVariantList &phoneNumbers);
    Q_INVOKABLE static QVariantList removeDuplicateOnlineAccounts(const QVariantList &onlineAccounts);
    Q_INVOKABLE static QVariantList removeDuplicateEmailAddresses(const QVariantList &emailAddresses);

    void displayLabelOrderChanged(SeasideCache::DisplayLabelOrder order);

    void updateContact(const QContact &newContact, QContact *oldContact, SeasideCache::ContactState state);

    void addressResolved(const QString &first, const QString &second, SeasideCache::CacheItem *item);

    void itemUpdated(SeasideCache::CacheItem *item);
    void itemAboutToBeRemoved(SeasideCache::CacheItem *item);

    void constituentsFetched(const QList<int> &ids);
    void mergeCandidatesFetched(const QList<int> &ids);
    void aggregationOperationCompleted();

    static QVariantList nicknameDetails(const QContact &contact);
    static QVariantList phoneDetails(const QContact &contact);
    static QVariantList emailDetails(const QContact &contact);
    static QVariantList addressDetails(const QContact &contact);
    static QVariantList websiteDetails(const QContact &contact);
    static QVariantList anniversaryDetails(const QContact &contact);
    static QVariantList accountDetails(const QContact &contact);

    static QString generateDisplayLabel(
                const QContact &mContact,
                SeasideCache::DisplayLabelOrder order = SeasideCache::FirstNameFirst);
    static QString generateDisplayLabelFromNonNameDetails(const QContact &mContact);

    static SeasidePersonAttached *qmlAttachedProperties(QObject *object);

signals:
    void contactChanged();
    void contactRemoved();
    void completeChanged();
    void firstNameChanged();
    void lastNameChanged();
    void middleNameChanged();
    void namePrefixChanged();
    void nameSuffixChanged();
    void displayLabelChanged();
    void primaryNameChanged();
    void secondaryNameChanged();
    void companyNameChanged();
    void titleChanged();
    void roleChanged();
    void departmentChanged();
    void favoriteChanged();
    void avatarPathChanged();
    void avatarUrlChanged();
    void nicknameDetailsChanged();
    void phoneDetailsChanged();
    void emailDetailsChanged();
    void addressDetailsChanged();
    void websiteDetailsChanged();
    void birthdayChanged();
    void anniversaryDetailsChanged();
    void globalPresenceStateChanged();
    void accountDetailsChanged();
    void constituentsChanged();
    void mergeCandidatesChanged();
    void aggregationOperationFinished();
    void addressResolved();
    void dataChanged();

public slots:
    void recalculateDisplayLabel(SeasideCache::DisplayLabelOrder order = SeasideCache::displayLabelOrder()) const;

private:
    void updateContactDetails(const QContact &oldContact);
    void emitChangeSignals();

    QString getPrimaryName(const QContact &contact) const;
    QString getSecondaryName(const QContact &contact) const;

    enum AttachState {
        Unattached = 0,
        Attached,
        Listening
    };

    QContact *mContact;
    mutable QString mDisplayLabel;
    QList<int> mConstituents;
    QList<int> mCandidates;
    bool mComplete;
    bool m_changesReported;
    AttachState mAttachState;
    SeasideCache::CacheItem *mItem;

    void emitChangeSignal(void (SeasidePerson::*f)()) { m_changesReported = true; (this->*f)(); }

    friend class SeasideCache;
    friend class tst_SeasidePerson;
    friend class SeasidePeopleModelPriv;
};

QML_DECLARE_TYPEINFO(SeasidePerson, QML_HAS_ATTACHED_PROPERTIES);

QML_DECLARE_TYPE(SeasidePerson);

#endif // SEASIDEPERSON_H
