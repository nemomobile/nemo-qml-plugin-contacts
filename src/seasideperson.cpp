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

#include <QDebug>

#include <qtcontacts-extensions.h>

#include <QContactAvatar>
#include <QContactBirthday>
#include <QContactAnniversary>
#include <QContactName>
#include <QContactNickname>
#include <QContactFavorite>
#include <QContactPhoneNumber>
#include <QContactEmailAddress>
#include <QContactAddress>
#include <QContactOnlineAccount>
#include <QContactGlobalPresence>
#include <QContactPresence>
#include <QContactOrganization>
#include <QContactUrl>
#include <QContactSyncTarget>

#include <QVersitWriter>
#include <QVersitContactExporter>

#include "seasideperson.h"

USE_VERSIT_NAMESPACE

SeasidePersonAttached::SeasidePersonAttached(QObject *parent)
    : QObject(parent)
{
    SeasideCache::registerUser(this);
}

SeasidePersonAttached::~SeasidePersonAttached()
{
    SeasideCache::unregisterUser(this);
}

SeasidePerson *SeasidePersonAttached::selfPerson() const
{
    SeasideCache::CacheItem *item = SeasideCache::itemById(SeasideCache::selfContactId());
    if (!item->itemData) {
        item->itemData = new SeasidePerson(&item->contact, (item->contactState == SeasideCache::ContactComplete), SeasideCache::instance());
    }

    return static_cast<SeasidePerson *>(item->itemData);
}

QString SeasidePersonAttached::normalizePhoneNumber(const QString &input)
{
    return SeasideCache::normalizePhoneNumber(input);
}

QString SeasidePersonAttached::minimizePhoneNumber(const QString &input)
{
    return SeasideCache::minimizePhoneNumber(input);
}

QVariantList SeasidePersonAttached::removeDuplicatePhoneNumbers(const QVariantList &phoneNumbers)
{
    return SeasidePerson::removeDuplicatePhoneNumbers(phoneNumbers);
}

QVariantList SeasidePersonAttached::removeDuplicateOnlineAccounts(const QVariantList &onlineAccounts)
{
    return SeasidePerson::removeDuplicateOnlineAccounts(onlineAccounts);
}

SeasidePerson::SeasidePerson(QObject *parent)
    : QObject(parent)
    , mContact(new QContact)
    , mComplete(true)
    , mAttachState(Unattached)
    , mItem(0)
{
}

SeasidePerson::SeasidePerson(const QContact &contact, QObject *parent)
    : QObject(parent)
    , mContact(new QContact(contact))
    , mDisplayLabel(generateDisplayLabel(contact))
    , mComplete(true)
    , mAttachState(Unattached)
    , mItem(0)
{
}

SeasidePerson::SeasidePerson(QContact *contact, bool complete, QObject *parent)
    : QObject(parent)
    , mContact(contact)
    , mDisplayLabel(generateDisplayLabel(*contact))
    , mComplete(complete)
    , mAttachState(Attached)
    , mItem(0)
{
}

SeasidePerson::~SeasidePerson()
{
    SeasideCache::unregisterResolveListener(this);

    emit contactRemoved();

    if (mAttachState == Unattached) {
        delete mContact;
    } else if (mAttachState == Listening) {
        mItem->removeListener(this);
    }
}

// QT5: this needs to change type
int SeasidePerson::id() const
{
    return SeasideCache::contactId(*mContact);
}

bool SeasidePerson::isComplete() const
{
    return mComplete;
}

void SeasidePerson::setComplete(bool complete)
{
    if (mComplete != complete) {
        mComplete = complete;
        emit completeChanged();
    }
}

QString SeasidePerson::firstName() const
{
    QContactName nameDetail = mContact->detail<QContactName>();
    return nameDetail.firstName();
}

void SeasidePerson::setFirstName(const QString &name)
{
    QContactName nameDetail = mContact->detail<QContactName>();
    nameDetail.setFirstName(name);
    mContact->saveDetail(&nameDetail);

    emit firstNameChanged();

    const bool firstNameFirst(SeasideCache::displayLabelOrder() == SeasideCache::FirstNameFirst);
    emit firstNameFirst ? primaryNameChanged() : secondaryNameChanged();

    recalculateDisplayLabel();
}

QString SeasidePerson::lastName() const
{
    QContactName nameDetail = mContact->detail<QContactName>();
    return nameDetail.lastName();
}

void SeasidePerson::setLastName(const QString &name)
{
    QContactName nameDetail = mContact->detail<QContactName>();
    nameDetail.setLastName(name);
    mContact->saveDetail(&nameDetail);

    emit lastNameChanged();

    const bool firstNameFirst(SeasideCache::displayLabelOrder() == SeasideCache::FirstNameFirst);
    emit firstNameFirst ? secondaryNameChanged() : primaryNameChanged();

    recalculateDisplayLabel();
}

QString SeasidePerson::middleName() const
{
    QContactName nameDetail = mContact->detail<QContactName>();
    return nameDetail.middleName();
}

void SeasidePerson::setMiddleName(const QString &name)
{
    QContactName nameDetail = mContact->detail<QContactName>();
    nameDetail.setMiddleName(name);
    mContact->saveDetail(&nameDetail);
    emit middleNameChanged();
    recalculateDisplayLabel();
}

QString SeasidePerson::namePrefix() const
{
    QContactName nameDetail = mContact->detail<QContactName>();
    return nameDetail.prefix();
}

void SeasidePerson::setNamePrefix(const QString &prefix)
{
    QContactName nameDetail = mContact->detail<QContactName>();
    nameDetail.setPrefix(prefix);
    mContact->saveDetail(&nameDetail);
    emit namePrefixChanged();
    // Prefix is not currently used in display label
    //recalculateDisplayLabel();
}

QString SeasidePerson::nameSuffix() const
{
    QContactName nameDetail = mContact->detail<QContactName>();
    return nameDetail.suffix();
}

void SeasidePerson::setNameSuffix(const QString &suffix)
{
    QContactName nameDetail = mContact->detail<QContactName>();
    nameDetail.setSuffix(suffix);
    mContact->saveDetail(&nameDetail);
    emit nameSuffixChanged();
    // Suffix is not currently used in display label
    //recalculateDisplayLabel();
}

// small helper to avoid inconvenience
QString SeasidePerson::generateDisplayLabel(const QContact &contact, SeasideCache::DisplayLabelOrder order)
{
    return SeasideCache::generateDisplayLabel(contact, order);
}

QString SeasidePerson::generateDisplayLabelFromNonNameDetails(const QContact &contact)
{
    return SeasideCache::generateDisplayLabelFromNonNameDetails(contact);
}

void SeasidePerson::recalculateDisplayLabel(SeasideCache::DisplayLabelOrder order) const
{
    QString oldDisplayLabel = mDisplayLabel;
    QString newDisplayLabel = generateDisplayLabel(*mContact, order);

    if (oldDisplayLabel != newDisplayLabel) {
        mDisplayLabel = newDisplayLabel;
        emit const_cast<SeasidePerson*>(this)->displayLabelChanged();

        // It's also possible the primaryName and secondaryName changed
        emit const_cast<SeasidePerson*>(this)->primaryNameChanged();
        emit const_cast<SeasidePerson*>(this)->secondaryNameChanged();
    }
}

QString SeasidePerson::displayLabel() const
{
    if (mDisplayLabel.isEmpty()) {
        // TODO: localization?
        return QString::fromLatin1("(Unnamed)");
    }
    return mDisplayLabel;
}

QString SeasidePerson::primaryName() const
{
    QString primaryName(getPrimaryName(*mContact));
    if (!primaryName.isEmpty())
        return primaryName;

    if (secondaryName().isEmpty()) {
        // No real name details - fall back to the display label for primary name
        return mDisplayLabel;
    }

    return QString();
}

QString SeasidePerson::secondaryName() const
{
    return getSecondaryName(*mContact);
}

QString SeasidePerson::sectionBucket() const
{
    if (id() != 0) {
        SeasideCache::CacheItem *cacheItem = SeasideCache::existingItem(mContact->id());
        return SeasideCache::nameGroup(cacheItem);
    }

    if (!mDisplayLabel.isEmpty()) {
        return mDisplayLabel.at(0).toUpper();
    }

    return QString();
}

QString SeasidePerson::companyName() const
{
    QContactOrganization company = mContact->detail<QContactOrganization>();
    return company.name();
}

void SeasidePerson::setCompanyName(const QString &name)
{
    QContactOrganization companyNameDetail = mContact->detail<QContactOrganization>();
    companyNameDetail.setName(name);
    mContact->saveDetail(&companyNameDetail);
    emit companyNameChanged();
}

QString SeasidePerson::title() const
{
    QContactOrganization company = mContact->detail<QContactOrganization>();
    return company.role();
}

void SeasidePerson::setTitle(const QString &role)
{
    QContactOrganization companyDetail = mContact->detail<QContactOrganization>();
    companyDetail.setRole(role);
    mContact->saveDetail(&companyDetail);
    emit titleChanged();
}

bool SeasidePerson::favorite() const
{
    QContactFavorite favoriteDetail = mContact->detail<QContactFavorite>();
    return favoriteDetail.isFavorite();
}

void SeasidePerson::setFavorite(bool favorite)
{
    QContactFavorite favoriteDetail = mContact->detail<QContactFavorite>();
    favoriteDetail.setFavorite(favorite);
    mContact->saveDetail(&favoriteDetail);
    emit favoriteChanged();
}

QUrl SeasidePerson::avatarPath() const
{
    QUrl url = avatarUrl();
    if (!url.isEmpty())
        return url;

    return QUrl("image://theme/icon-m-telephony-contact-avatar");
}

void SeasidePerson::setAvatarPath(QUrl avatarPath)
{
    setAvatarUrl(avatarPath);
}

QUrl SeasidePerson::avatarUrl() const
{
    return filteredAvatarUrl();
}

void SeasidePerson::setAvatarUrl(QUrl avatarUrl)
{
    static const QString localMetadata(QString::fromLatin1("local"));

    QContactAvatar localAvatar;
    foreach (const QContactAvatar &avatar, mContact->details<QContactAvatar>()) {
        // Find the existing local data, if there is one
        if (avatar.value(QContactAvatar__FieldAvatarMetadata).toString() == localMetadata) {
            if (localAvatar.isEmpty()) {
                localAvatar = avatar;
            } else {
                // We can only have one local avatar
                QContactAvatar obsoleteAvatar(avatar);
                mContact->removeDetail(&obsoleteAvatar);
            }
        }
    }

    localAvatar.setImageUrl(avatarUrl);
    localAvatar.setValue(QContactAvatar__FieldAvatarMetadata, localMetadata);
    mContact->saveDetail(&localAvatar);

    emit avatarUrlChanged();
    emit avatarPathChanged();
}

QUrl SeasidePerson::filteredAvatarUrl(const QStringList &metadataFragments) const
{
    return SeasideCache::filteredAvatarUrl(*mContact, metadataFragments);
}

namespace { // Helper functions

#ifdef USING_QTPIM
QVariant detailLabelType(const QContactDetail &detail)
{
    const QList<int> &contexts(detail.contexts());

    // We do not support multiple conflicting labels
    if (contexts.contains(QContactDetail::ContextHome)) {
        return QVariant(static_cast<int>(SeasidePerson::HomeLabel));
    } else if (contexts.contains(QContactDetail::ContextWork)) {
        return QVariant(static_cast<int>(SeasidePerson::WorkLabel));
    } else if (contexts.contains(QContactDetail::ContextOther)) {
        return QVariant(static_cast<int>(SeasidePerson::OtherLabel));
    }

    return QVariant();
}

QContactDetail::DetailContext contextType(int label)
{
    if (label == SeasidePerson::HomeLabel) {
        return QContactDetail::ContextHome;
    } else if (label == SeasidePerson::WorkLabel) {
        return QContactDetail::ContextWork;
    }

    return QContactDetail::ContextOther;
}

void setDetailLabelType(QContactDetail &detail, int label)
{
    QList<int> contexts(detail.contexts());

    // Replace any of the supported types with the new type
    bool modified = false;
    bool replace = (label != SeasidePerson::NoLabel);
    for (QList<int>::iterator it = contexts.begin(); it != contexts.end(); ) {
        if (*it == QContactDetail::ContextHome ||
            *it == QContactDetail::ContextWork ||
            *it == QContactDetail::ContextOther) {
            if (replace) {
                replace = false;
                (*it) = contextType(label);
                ++it;
            } else {
                it = contexts.erase(it);
            }
            modified = true;
        } else {
            ++it;
        }
    }

    if (replace && !modified) {
        contexts.append(contextType(label));
        modified = true;
    }

    if (modified) {
        detail.setContexts(contexts);
    }
}
#else
#error "Unsupported"
#endif

const QString detailReadOnly(QString::fromLatin1("readOnly"));
const QString detailOriginId(QString::fromLatin1("originId"));
const QString detailSyncTarget(QString::fromLatin1("syncTarget"));
const QString detailType(QString::fromLatin1("type"));
const QString detailSubType(QString::fromLatin1("subType"));
const QString detailSubTypes(QString::fromLatin1("subTypes"));
const QString detailLabel(QString::fromLatin1("label"));
const QString detailIndex(QString::fromLatin1("index"));

QVariantMap detailProperties(const QContactDetail &detail)
{
    static const QChar colon(QChar::fromLatin1(':'));

    QVariantMap rv;

    const bool readOnly((detail.accessConstraints() & QContactDetail::ReadOnly) == QContactDetail::ReadOnly);
    rv.insert(detailReadOnly, readOnly);

    const QString provenance(detail.value(QContactDetail__FieldProvenance).toString());
    if (!provenance.isEmpty()) {
        int index = provenance.indexOf(colon);
        if (index != -1) {
            // The first field is the contact ID where this detaiol originates
            quint32 originId = provenance.left(index).toUInt();
            rv.insert(detailOriginId, originId);

            // Bypass the detail ID field
            index = provenance.indexOf(colon, index + 1);
        }
        if (index != -1) {
            // The remainder is the syncTarget
            rv.insert(detailSyncTarget, provenance.mid(index + 1));
        }
    }

    return rv;
}

template<typename T>
void updateDetails(QList<T> &existing, QList<T> &modified, QContact *contact, const QSet<int> &validIndices)
{
    // Remove any modifiable details that are not present in the modified list
    typename QList<T>::iterator it = existing.begin(), end = existing.end();
    for (int index = 0; it != end; ++it, ++index) {
        T &item(*it);
        if (!validIndices.contains(index) && (item.accessConstraints() & QContactDetail::ReadOnly) == 0) {
            if (!contact->removeDetail(&item)) {
                qWarning() << "Unable to remove detail at:" << index << item;
            }
        }
    }

    // Store any changed details to the contact
    for (it = modified.begin(), end = modified.end(); it != end; ++it) {
        T &item(*it);
        if (!contact->saveDetail(&item)) {
            qWarning() << "Unable to save modified detail:" << item;
        }
    }
}

const QString nicknameDetailNickname(QString::fromLatin1("nickname"));

}

QVariantList SeasidePerson::nicknameDetails(const QContact &contact)
{
    QVariantList rv;

    int index = 0;
    foreach (const QContactNickname &detail, contact.details<QContactNickname>()) {
        const QString nickname(detail.value(QContactNickname::FieldNickname).toString());

        QVariantMap item(detailProperties(detail));
        item.insert(nicknameDetailNickname, nickname);
        item.insert(detailType, NicknameType);
        item.insert(detailLabel, ::detailLabelType(detail));
        item.insert(detailIndex, index++);
        rv.append(item);
    }

    return rv;
}

QVariantList SeasidePerson::nicknameDetails() const
{
    return nicknameDetails(*mContact);
}

void SeasidePerson::setNicknameDetails(const QVariantList &nicknameDetails)
{
    QList<QContactNickname> nicknames(mContact->details<QContactNickname>());

    QList<QContactNickname> updatedNicknames;
    QSet<int> validIndices;

    foreach (const QVariant &item, nicknameDetails) {
        const QVariantMap detail(item.value<QVariantMap>());

        const QVariant typeValue = detail[detailType];
        if (typeValue.toInt() != NicknameType) {
            qWarning() << "Invalid type value for nickname:" << typeValue;
            continue;
        }

        QContactNickname updated;

        const QVariant indexValue = detail[detailIndex];
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < nicknames.count()) {
            // Modify the existing detail
            updated = nicknames.at(index);
            Q_ASSERT(!validIndices.contains(index));
        } else if (index != -1) {
            qWarning() << "Invalid index value for nickname details:" << index;
            continue;
        }

        const QVariant nicknameValue = detail[nicknameDetailNickname];
        const QString updatedNickname(nicknameValue.value<QString>());
        if (updatedNickname.trimmed().isEmpty()) {
            // Remove this nickname from the list
            continue;
        }

        updated.setValue(QContactNickname::FieldNickname, updatedNickname);

        const QVariant labelValue = detail[detailLabel];
        ::setDetailLabelType(updated, labelValue.isValid() ? labelValue.toInt() : NoLabel);

        updatedNicknames.append(updated);
        if (index != -1) {
            validIndices.insert(index);
        }
    }

    updateDetails(nicknames, updatedNicknames, mContact, validIndices);
    emit nicknameDetailsChanged();

    recalculateDisplayLabel();
}

namespace {

#ifdef USING_QTPIM
QList<QPair<QContactPhoneNumber::SubType, SeasidePerson::DetailSubType> > getPhoneSubTypeMapping()
{
    QList<QPair<QContactPhoneNumber::SubType, SeasidePerson::DetailSubType> > rv;

    rv.append(qMakePair(QContactPhoneNumber::SubTypeLandline, SeasidePerson::PhoneSubTypeLandline));
    rv.append(qMakePair(QContactPhoneNumber::SubTypeMobile, SeasidePerson::PhoneSubTypeMobile));
    rv.append(qMakePair(QContactPhoneNumber::SubTypeFax, SeasidePerson::PhoneSubTypeFax));
    rv.append(qMakePair(QContactPhoneNumber::SubTypePager, SeasidePerson::PhoneSubTypePager));
    rv.append(qMakePair(QContactPhoneNumber::SubTypeVoice, SeasidePerson::PhoneSubTypeVoice));
    rv.append(qMakePair(QContactPhoneNumber::SubTypeModem, SeasidePerson::PhoneSubTypeModem));
    rv.append(qMakePair(QContactPhoneNumber::SubTypeVideo, SeasidePerson::PhoneSubTypeVideo));
    rv.append(qMakePair(QContactPhoneNumber::SubTypeCar, SeasidePerson::PhoneSubTypeCar));
    rv.append(qMakePair(QContactPhoneNumber::SubTypeBulletinBoardSystem, SeasidePerson::PhoneSubTypeBulletinBoardSystem));
    rv.append(qMakePair(QContactPhoneNumber::SubTypeMessagingCapable, SeasidePerson::PhoneSubTypeMessagingCapable));
    rv.append(qMakePair(QContactPhoneNumber::SubTypeAssistant, SeasidePerson::PhoneSubTypeAssistant));
    rv.append(qMakePair(QContactPhoneNumber::SubTypeDtmfMenu, SeasidePerson::PhoneSubTypeDtmfMenu));

    return rv;
}

const QList<QPair<QContactPhoneNumber::SubType, SeasidePerson::DetailSubType> > &phoneSubTypeMapping()
{
    static const QList<QPair<QContactPhoneNumber::SubType, SeasidePerson::DetailSubType> > mapping(getPhoneSubTypeMapping());
    return mapping;
}

int phoneNumberSubType(QContactPhoneNumber::SubType subType)
{
    typedef QList<QPair<QContactPhoneNumber::SubType, SeasidePerson::DetailSubType> > List;
    for (List::const_iterator it = phoneSubTypeMapping().constBegin(), end = phoneSubTypeMapping().constEnd(); it != end; ++it) {
        if ((*it).first == subType)
            return static_cast<int>((*it).second);
    }

    qWarning() << "Invalid phone number sub-type:" << subType;
    return -1;
}

int phoneNumberSubType(SeasidePerson::DetailSubType subType)
{
    typedef QList<QPair<QContactPhoneNumber::SubType, SeasidePerson::DetailSubType> > List;
    for (List::const_iterator it = phoneSubTypeMapping().constBegin(), end = phoneSubTypeMapping().constEnd(); it != end; ++it) {
        if ((*it).second == subType)
            return static_cast<int>((*it).first);
    }

    qWarning() << "Invalid phone number detail sub-type:" << subType;
    return -1;
}

QVariantList phoneNumberSubTypes(const QContactPhoneNumber &number)
{
    QVariantList rv;

    foreach (int type, number.subTypes()) {
        int result = phoneNumberSubType(static_cast<QContactPhoneNumber::SubType>(type));
        if (result != -1) {
            rv.append(QVariant(result));
        }
    }

    return rv;
}

void setPhoneNumberSubTypes(QContactPhoneNumber &number, const QVariantList &types)
{
    QList<int> subTypes;

    if (types.count() != 1 || types.at(0).toInt() != SeasidePerson::NoSubType) {
        foreach (const QVariant &type, types) {
            int result = phoneNumberSubType(static_cast<SeasidePerson::DetailSubType>(type.toInt()));
            if (result != -1) {
                subTypes.append(result);
            }
        }
    }

    number.setSubTypes(subTypes);
}
#else
#error "Unsupported"
#endif

const QString phoneDetailNumber(QString::fromLatin1("number"));
const QString phoneDetailNormalizedNumber(QString::fromLatin1("normalizedNumber"));
const QString phoneDetailMinimizedNumber(QString::fromLatin1("minimizedNumber"));
const QString phoneDetailSubTypes(QString::fromLatin1("subTypes"));

}

QVariantList SeasidePerson::phoneDetails(const QContact &contact)
{
    QVariantList rv;

    int index = 0;
    foreach (const QContactPhoneNumber &detail, contact.details<QContactPhoneNumber>()) {
        const QString number(detail.value(QContactPhoneNumber::FieldNumber).toString());
        const QString normalized(SeasideCache::normalizePhoneNumber(number));
        const QString minimized(SeasideCache::minimizePhoneNumber(normalized));

        QVariantMap item(detailProperties(detail));
        item.insert(phoneDetailNumber, number);
        item.insert(phoneDetailNormalizedNumber, normalized);
        item.insert(phoneDetailMinimizedNumber, minimized);
        item.insert(detailType, PhoneNumberType);
        item.insert(detailSubTypes, ::phoneNumberSubTypes(detail));
        item.insert(detailLabel, ::detailLabelType(detail));
        item.insert(detailIndex, index++);
        rv.append(item);
    }

    return rv;
}

QVariantList SeasidePerson::phoneDetails() const
{
    return phoneDetails(*mContact);
}

void SeasidePerson::setPhoneDetails(const QVariantList &phoneDetails)
{
    QList<QContactPhoneNumber> numbers(mContact->details<QContactPhoneNumber>());

    QList<QContactPhoneNumber> updatedNumbers;
    QSet<int> validIndices;

    foreach (const QVariant &item, phoneDetails) {
        const QVariantMap detail(item.value<QVariantMap>());

        const QVariant typeValue = detail[detailType];
        if (typeValue.toInt() != PhoneNumberType) {
            qWarning() << "Invalid type value for phone number:" << typeValue;
            continue;
        }

        QContactPhoneNumber updated;

        const QVariant indexValue = detail[detailIndex];
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < numbers.count()) {
            // Modify the existing detail
            updated = numbers.at(index);
            Q_ASSERT(!validIndices.contains(index));
        } else if (index != -1) {
            qWarning() << "Invalid index value for phone details:" << index;
            continue;
        }

        const QVariant numberValue = detail[phoneDetailNumber];
        const QString updatedNumber(numberValue.value<QString>());
        if (updatedNumber.trimmed().isEmpty()) {
            // Remove this number from the list
            continue;
        }

        // Ignore normalized/minimized number variants
        updated.setValue(QContactPhoneNumber::FieldNumber, updatedNumber);

        const QVariant subTypesValue = detail[detailSubTypes];
        ::setPhoneNumberSubTypes(updated, subTypesValue.value<QVariantList>());

        const QVariant labelValue = detail[detailLabel];
        ::setDetailLabelType(updated, labelValue.isValid() ? labelValue.toInt() : NoLabel);

        updatedNumbers.append(updated);
        if (index != -1) {
            validIndices.insert(index);
        }
    }

    updateDetails(numbers, updatedNumbers, mContact, validIndices);
    emit phoneDetailsChanged();

    recalculateDisplayLabel();
}

namespace {

const QString emailDetailAddress(QString::fromLatin1("address"));

}

QVariantList SeasidePerson::emailDetails(const QContact &contact)
{
    QVariantList rv;

    int index = 0;
    foreach (const QContactEmailAddress &detail, contact.details<QContactEmailAddress>()) {
        QVariantMap item(detailProperties(detail));
        item.insert(emailDetailAddress, detail.value(QContactEmailAddress::FieldEmailAddress).toString());
        item.insert(detailType, EmailAddressType);
        item.insert(detailLabel, ::detailLabelType(detail));
        item.insert(detailIndex, index++);
        rv.append(item);
    }

    return rv;
}

QVariantList SeasidePerson::emailDetails() const
{
    return emailDetails(*mContact);
}

void SeasidePerson::setEmailDetails(const QVariantList &emailDetails)
{
    QList<QContactEmailAddress> addresses(mContact->details<QContactEmailAddress>());

    QList<QContactEmailAddress> updatedAddresses;
    QSet<int> validIndices;

    foreach (const QVariant &item, emailDetails) {
        const QVariantMap detail(item.value<QVariantMap>());

        const QVariant typeValue = detail[detailType];
        if (typeValue.toInt() != EmailAddressType) {
            qWarning() << "Invalid type value for email address:" << typeValue;
            continue;
        }

        QContactEmailAddress updated;

        const QVariant indexValue = detail[detailIndex];
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < addresses.count()) {
            // Modify the existing detail
            updated = addresses.at(index);
            Q_ASSERT(!validIndices.contains(index));
        } else if (index != -1) {
            qWarning() << "Invalid index value for email details:" << index;
            continue;
        }

        const QVariant addressValue = detail[emailDetailAddress];
        const QString updatedAddress(addressValue.value<QString>());
        if (updatedAddress.trimmed().isEmpty()) {
            // Remove this address from the list
            continue;
        }

        updated.setValue(QContactEmailAddress::FieldEmailAddress, updatedAddress);

        const QVariant labelValue = detail[detailLabel];
        ::setDetailLabelType(updated, labelValue.isValid() ? labelValue.toInt() : NoLabel);

        updatedAddresses.append(updated);
        if (index != -1) {
            validIndices.insert(index);
        }
    }

    updateDetails(addresses, updatedAddresses, mContact, validIndices);
    emit emailDetailsChanged();

    recalculateDisplayLabel();
}

namespace {

QString addressString(const QContactAddress &address)
{
    QString rv;

    rv.append(address.street());
    rv.append("\n");
    rv.append(address.locality());
    rv.append("\n");
    rv.append(address.region());
    rv.append("\n");
    rv.append(address.postcode());
    rv.append("\n");
    rv.append(address.country());
    rv.append("\n");
    rv.append(address.postOfficeBox());

    return rv;
}

void setAddress(QContactAddress &address, const QStringList &addressStrings)
{
    if (addressStrings.length() == 6) {
        address.setStreet(addressStrings.at(0));
        address.setLocality(addressStrings.at(1));
        address.setRegion(addressStrings.at(2));
        address.setPostcode(addressStrings.at(3));
        address.setCountry(addressStrings.at(4));
        address.setPostOfficeBox(addressStrings.at(5));
    }
}

#ifdef USING_QTPIM
QList<QPair<QContactAddress::SubType, SeasidePerson::DetailSubType> > getAddressSubTypeMapping()
{
    QList<QPair<QContactAddress::SubType, SeasidePerson::DetailSubType> > rv;

    rv.append(qMakePair(QContactAddress::SubTypeParcel, SeasidePerson::AddressSubTypeParcel));
    rv.append(qMakePair(QContactAddress::SubTypePostal, SeasidePerson::AddressSubTypePostal));
    rv.append(qMakePair(QContactAddress::SubTypeDomestic, SeasidePerson::AddressSubTypeDomestic));
    rv.append(qMakePair(QContactAddress::SubTypeInternational, SeasidePerson::AddressSubTypeInternational));

    return rv;
}

const QList<QPair<QContactAddress::SubType, SeasidePerson::DetailSubType> > &addressSubTypeMapping()
{
    static const QList<QPair<QContactAddress::SubType, SeasidePerson::DetailSubType> > mapping(getAddressSubTypeMapping());
    return mapping;
}

int addressSubType(QContactAddress::SubType subType)
{
    typedef QList<QPair<QContactAddress::SubType, SeasidePerson::DetailSubType> > List;
    for (List::const_iterator it = addressSubTypeMapping().constBegin(), end = addressSubTypeMapping().constEnd(); it != end; ++it) {
        if ((*it).first == subType)
            return static_cast<int>((*it).second);
    }

    qWarning() << "Invalid address sub-type:" << subType;
    return -1;
}

int addressSubType(SeasidePerson::DetailSubType subType)
{
    typedef QList<QPair<QContactAddress::SubType, SeasidePerson::DetailSubType> > List;
    for (List::const_iterator it = addressSubTypeMapping().constBegin(), end = addressSubTypeMapping().constEnd(); it != end; ++it) {
        if ((*it).second == subType)
            return static_cast<int>((*it).first);
    }

    qWarning() << "Invalid address detail sub-type:" << subType;
    return -1;
}

QVariantList addressSubTypes(const QContactAddress &address)
{
    QVariantList rv;

    foreach (int type, address.subTypes()) {
        int result = addressSubType(static_cast<QContactAddress::SubType>(type));
        if (result != -1) {
            rv.append(QVariant(result));
        }
    }

    return rv;
}

void setAddressSubTypes(QContactAddress &address, const QVariantList &types)
{
    QList<int> subTypes;

    if (types.count() != 1 || types.at(0) != SeasidePerson::NoSubType) {
        foreach (const QVariant &type, types) {
            int result = addressSubType(static_cast<SeasidePerson::DetailSubType>(type.toInt()));
            if (result != -1) {
                subTypes.append(result);
            }
        }
    }

    address.setSubTypes(subTypes);
}
#else
#error "Unsupported"
#endif

const QString addressDetailAddress(QString::fromLatin1("address"));

}

QVariantList SeasidePerson::addressDetails(const QContact &contact)
{
    QVariantList rv;

    int index = 0;
    foreach (const QContactAddress &detail, contact.details<QContactAddress>()) {
        QVariantMap item(detailProperties(detail));
        item.insert(addressDetailAddress, ::addressString(detail));
        item.insert(detailType, AddressType);
        item.insert(detailSubTypes, ::addressSubTypes(detail));
        item.insert(detailLabel, ::detailLabelType(detail));
        item.insert(detailIndex, index++);
        rv.append(item);
    }

    return rv;
}

QVariantList SeasidePerson::addressDetails() const
{
    return addressDetails(*mContact);
}

void SeasidePerson::setAddressDetails(const QVariantList &addressDetails)
{
    QList<QContactAddress> addresses(mContact->details<QContactAddress>());

    QList<QContactAddress> updatedAddresses;
    QSet<int> validIndices;

    foreach (const QVariant &item, addressDetails) {
        const QVariantMap detail(item.value<QVariantMap>());

        const QVariant typeValue = detail[detailType];
        if (typeValue.toInt() != AddressType) {
            qWarning() << "Invalid type value for address:" << typeValue;
            continue;
        }

        QContactAddress updated;

        const QVariant indexValue = detail[detailIndex];
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < addresses.count()) {
            // Modify the existing detail
            updated = addresses.at(index);
            Q_ASSERT(!validIndices.contains(index));
        } else if (index != -1) {
            qWarning() << "Invalid index value for address details:" << index;
            continue;
        }

        const QVariant addressValue = detail[addressDetailAddress];
        const QString updatedAddress(addressValue.value<QString>());
        if (updatedAddress.trimmed().isEmpty()) {
            // Remove this address from the list
            continue;
        }

        const QStringList split = updatedAddress.split("\n");
        if (split.count() != 6) {
            qWarning() << "Warning: Could not save addresses - invalid format for address:" << updatedAddress;
            continue;
        }

        ::setAddress(updated, split);

        const QVariant subTypesValue = detail[detailSubTypes];
        ::setAddressSubTypes(updated, subTypesValue.value<QVariantList>());

        const QVariant labelValue = detail[detailLabel];
        ::setDetailLabelType(updated, labelValue.isValid() ? labelValue.toInt() : NoLabel);

        updatedAddresses.append(updated);
        if (index != -1) {
            validIndices.insert(index);
        }
    }

    updateDetails(addresses, updatedAddresses, mContact, validIndices);
    emit addressDetailsChanged();
}

namespace {

const QString websiteDetailUrl(QString::fromLatin1("url"));

}

QVariantList SeasidePerson::websiteDetails(const QContact &contact)
{
    QVariantList rv;

    int index = 0;
    foreach (const QContactUrl &detail, contact.details<QContactUrl>()) {
        QVariantMap item(detailProperties(detail));
        item.insert(websiteDetailUrl, detail.value(QContactUrl::FieldUrl).toUrl().toString());
        item.insert(detailType, WebsiteType);
        item.insert(detailLabel, ::detailLabelType(detail));
        item.insert(detailIndex, index++);
        rv.append(item);
    }

    return rv;
}

QVariantList SeasidePerson::websiteDetails() const
{
    return websiteDetails(*mContact);
}

void SeasidePerson::setWebsiteDetails(const QVariantList &websiteDetails)
{
    QList<QContactUrl> urls(mContact->details<QContactUrl>());

    QList<QContactUrl> updatedUrls;
    QSet<int> validIndices;

    foreach (const QVariant &item, websiteDetails) {
        const QVariantMap detail(item.value<QVariantMap>());

        const QVariant typeValue = detail[detailType];
        if (typeValue.toInt() != WebsiteType) {
            qWarning() << "Invalid type value for website:" << typeValue;
            continue;
        }

        QContactUrl updated;

        const QVariant indexValue = detail[detailIndex];
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < urls.count()) {
            // Modify the existing detail
            updated = urls.at(index);
            Q_ASSERT(!validIndices.contains(index));
        } else if (index != -1) {
            qWarning() << "Invalid index value for website details:" << index;
            continue;
        }

        const QVariant urlValue = detail[websiteDetailUrl];
        const QString updatedUrl(urlValue.value<QString>());
        if (updatedUrl.trimmed().isEmpty()) {
            // Remove this URL from the list
            continue;
        }

        updated.setValue(QContactUrl::FieldUrl, QUrl(updatedUrl));

        const QVariant labelValue = detail[detailLabel];
        ::setDetailLabelType(updated, labelValue.isValid() ? labelValue.toInt() : NoLabel);

        updatedUrls.append(updated);
        if (index != -1) {
            validIndices.insert(index);
        }
    }

    updateDetails(urls, updatedUrls, mContact, validIndices);
    emit websiteDetailsChanged();
}

QDateTime SeasidePerson::birthday() const
{
    return mContact->detail<QContactBirthday>().dateTime();
}

void SeasidePerson::setBirthday(const QDateTime &bd)
{
    QContactBirthday birthday = mContact->detail<QContactBirthday>();
    birthday.setDateTime(bd);
    mContact->saveDetail(&birthday);
    emit birthdayChanged();
}

void SeasidePerson::resetBirthday()
{
    setBirthday(QDateTime());
}

namespace {

#ifdef USING_QTPIM
QList<QPair<QContactAnniversary::SubType, SeasidePerson::DetailSubType> > getAnniversarySubTypeMapping()
{
    QList<QPair<QContactAnniversary::SubType, SeasidePerson::DetailSubType> > rv;

    rv.append(qMakePair(QContactAnniversary::SubTypeWedding, SeasidePerson::AnniversarySubTypeWedding));
    rv.append(qMakePair(QContactAnniversary::SubTypeEngagement, SeasidePerson::AnniversarySubTypeEngagement));
    rv.append(qMakePair(QContactAnniversary::SubTypeHouse, SeasidePerson::AnniversarySubTypeHouse));
    rv.append(qMakePair(QContactAnniversary::SubTypeEmployment, SeasidePerson::AnniversarySubTypeEmployment));
    rv.append(qMakePair(QContactAnniversary::SubTypeMemorial, SeasidePerson::AnniversarySubTypeMemorial));

    return rv;
}

const QList<QPair<QContactAnniversary::SubType, SeasidePerson::DetailSubType> > &anniversarySubTypeMapping()
{
    static const QList<QPair<QContactAnniversary::SubType, SeasidePerson::DetailSubType> > mapping(getAnniversarySubTypeMapping());
    return mapping;
}

int anniversarySubType(QContactAnniversary::SubType subType)
{
    typedef QList<QPair<QContactAnniversary::SubType, SeasidePerson::DetailSubType> > List;
    for (List::const_iterator it = anniversarySubTypeMapping().constBegin(), end = anniversarySubTypeMapping().constEnd(); it != end; ++it) {
        if ((*it).first == subType)
            return static_cast<int>((*it).second);
    }

    qWarning() << "Invalid anniversary sub-type:" << subType;
    return -1;
}

int anniversarySubType(SeasidePerson::DetailSubType subType)
{
    typedef QList<QPair<QContactAnniversary::SubType, SeasidePerson::DetailSubType> > List;
    for (List::const_iterator it = anniversarySubTypeMapping().constBegin(), end = anniversarySubTypeMapping().constEnd(); it != end; ++it) {
        if ((*it).second == subType)
            return static_cast<int>((*it).first);
    }

    qWarning() << "Invalid anniversary detail sub-type:" << subType;
    return -1;
}

QVariant anniversarySubType(const QContactAnniversary &anniversary)
{
    return anniversarySubType(static_cast<QContactAnniversary::SubType>(anniversary.subType()));
}

void setAnniversarySubType(QContactAnniversary &anniversary, const QVariant &type)
{
    // There is no possibility of NoSubType here
    int st = type.toInt();
    if (!type.isValid() || st == SeasidePerson::NoSubType) {
        st = SeasidePerson::AnniversarySubTypeWedding;
        anniversary.setSubType(QContactAnniversary::SubTypeWedding);
    } else {
        anniversary.setSubType(static_cast<QContactAnniversary::SubType>(anniversarySubType(static_cast<SeasidePerson::DetailSubType>(st))));
    }
}
#else
#error "Unsupported"
#endif

const QString anniversaryDetailOriginalDate(QString::fromLatin1("originalDate"));

}

QVariantList SeasidePerson::anniversaryDetails(const QContact &contact)
{
    QVariantList rv;

    int index = 0;
    foreach (const QContactAnniversary &detail, contact.details<QContactAnniversary>()) {
        QVariantMap item(detailProperties(detail));
        item.insert(anniversaryDetailOriginalDate, detail.value(QContactAnniversary::FieldOriginalDate).toDateTime());
        item.insert(detailType, AnniversaryType);
        item.insert(detailSubType, anniversarySubType(detail));
        item.insert(detailLabel, ::detailLabelType(detail));
        item.insert(detailIndex, index++);
        rv.append(item);
    }

    return rv;
}

QVariantList SeasidePerson::anniversaryDetails() const
{
    return anniversaryDetails(*mContact);
}

void SeasidePerson::setAnniversaryDetails(const QVariantList &anniversaryDetails)
{
    QList<QContactAnniversary> anniversaries(mContact->details<QContactAnniversary>());

    QList<QContactAnniversary> updatedAnniversaries;
    QSet<int> validIndices;

    foreach (const QVariant &item, anniversaryDetails) {
        const QVariantMap detail(item.value<QVariantMap>());

        const QVariant typeValue = detail[detailType];
        if (typeValue.toInt() != AnniversaryType) {
            qWarning() << "Invalid type value for anniversary:" << typeValue;
            continue;
        }

        QContactAnniversary updated;

        const QVariant indexValue = detail[detailIndex];
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < anniversaries.count()) {
            // Modify the existing detail
            updated = anniversaries.at(index);
            Q_ASSERT(!validIndices.contains(index));
        } else if (index != -1) {
            qWarning() << "Invalid index value for anniversary details:" << index;
            continue;
        }

        const QVariant originalDateValue = detail[anniversaryDetailOriginalDate];
        const QDateTime updatedOriginalDate(originalDateValue.value<QDateTime>());
        if (!updatedOriginalDate.isValid()) {
            // Remove this anniversary from the list
            continue;
        }

        updated.setValue(QContactAnniversary::FieldOriginalDate, updatedOriginalDate);

        const QVariant subTypeValue = detail[detailSubType];
        ::setAnniversarySubType(updated, subTypeValue.value<int>());

        const QVariant labelValue = detail[detailLabel];
        ::setDetailLabelType(updated, labelValue.isValid() ? labelValue.toInt() : NoLabel);

        updatedAnniversaries.append(updated);
        if (index != -1) {
            validIndices.insert(index);
        }
    }

    updateDetails(anniversaries, updatedAnniversaries, mContact, validIndices);
    emit anniversaryDetailsChanged();
}

SeasidePerson::PresenceState SeasidePerson::globalPresenceState() const
{
    return static_cast<SeasidePerson::PresenceState>(mContact->detail<QContactGlobalPresence>().presenceState());
}

namespace {

#ifdef USING_QTPIM
QList<QPair<QContactOnlineAccount::SubType, SeasidePerson::DetailSubType> > getOnlineAccountSubTypeMapping()
{
    QList<QPair<QContactOnlineAccount::SubType, SeasidePerson::DetailSubType> > rv;

    rv.append(qMakePair(QContactOnlineAccount::SubTypeSip, SeasidePerson::OnlineAccountSubTypeSip));
    rv.append(qMakePair(QContactOnlineAccount::SubTypeSipVoip, SeasidePerson::OnlineAccountSubTypeSipVoip));
    rv.append(qMakePair(QContactOnlineAccount::SubTypeImpp, SeasidePerson::OnlineAccountSubTypeImpp));
    rv.append(qMakePair(QContactOnlineAccount::SubTypeVideoShare, SeasidePerson::OnlineAccountSubTypeVideoShare));

    return rv;
}

const QList<QPair<QContactOnlineAccount::SubType, SeasidePerson::DetailSubType> > &onlineAccountSubTypeMapping()
{
    static const QList<QPair<QContactOnlineAccount::SubType, SeasidePerson::DetailSubType> > mapping(getOnlineAccountSubTypeMapping());
    return mapping;
}

int onlineAccountSubType(QContactOnlineAccount::SubType subType)
{
    typedef QList<QPair<QContactOnlineAccount::SubType, SeasidePerson::DetailSubType> > List;
    for (List::const_iterator it = onlineAccountSubTypeMapping().constBegin(), end = onlineAccountSubTypeMapping().constEnd(); it != end; ++it) {
        if ((*it).first == subType)
            return static_cast<int>((*it).second);
    }

    qWarning() << "Invalid online account sub-type:" << subType;
    return -1;
}

int onlineAccountSubType(SeasidePerson::DetailSubType subType)
{
    typedef QList<QPair<QContactOnlineAccount::SubType, SeasidePerson::DetailSubType> > List;
    for (List::const_iterator it = onlineAccountSubTypeMapping().constBegin(), end = onlineAccountSubTypeMapping().constEnd(); it != end; ++it) {
        if ((*it).second == subType)
            return static_cast<int>((*it).first);
    }

    qWarning() << "Invalid online account detail sub-type:" << subType;
    return -1;
}

QVariantList onlineAccountSubTypes(const QContactOnlineAccount &account)
{
    QVariantList rv;

    foreach (int type, account.subTypes()) {
        int result = onlineAccountSubType(static_cast<QContactOnlineAccount::SubType>(type));
        if (result != -1) {
            rv.append(QVariant(result));
        }
    }

    return rv;
}

void setOnlineAccountSubTypes(QContactOnlineAccount &account, const QVariantList &types)
{
    QList<int> subTypes;

    if (types.count() != 1 || types.at(0) != SeasidePerson::NoSubType) {
        foreach (const QVariant &type, types) {
            int result = onlineAccountSubType(static_cast<SeasidePerson::DetailSubType>(type.toInt()));
            if (result != -1) {
                subTypes.append(result);
            }
        }
    }

    account.setSubTypes(subTypes);
}
#else
#error "Unsupported"
#endif

const QString accountDetailUri(QString::fromLatin1("accountUri"));
const QString accountDetailPath(QString::fromLatin1("accountPath"));
const QString accountDetailDisplayName(QString::fromLatin1("accountDisplayName"));
const QString accountDetailIconPath(QString::fromLatin1("iconPath"));
const QString accountDetailServiceProvider(QString::fromLatin1("serviceProvider"));
const QString accountDetailServiceProviderDisplayName(QString::fromLatin1("serviceProviderDisplayName"));
const QString accountDetailPresenceState(QString::fromLatin1("presenceState"));
const QString accountDetailPresenceMessage(QString::fromLatin1("presenceMessage"));

template<typename ListType>
typename ListType::value_type linkedDetail(const ListType &details, const QString &uri)
{
    typename ListType::const_iterator it = details.constBegin(), end = details.constEnd();
    for ( ; it != end; ++it) {
        if ((*it).linkedDetailUris().contains(uri)) {
            return (*it);
        }
    }

    return typename ListType::value_type();
}

}

QVariantList SeasidePerson::accountDetails(const QContact &contact)
{
    QVariantList rv;

    const QList<QContactPresence> presenceDetails(contact.details<QContactPresence>());

    int index = 0;
    foreach (const QContactOnlineAccount &detail, contact.details<QContactOnlineAccount>()) {
        QVariantMap item(detailProperties(detail));
        item.insert(accountDetailUri, detail.value(QContactOnlineAccount::FieldAccountUri).toString());
        item.insert(accountDetailPath, detail.value(QContactOnlineAccount__FieldAccountPath).toString());
        item.insert(accountDetailDisplayName, detail.value(QContactOnlineAccount__FieldAccountDisplayName).toString());
        item.insert(accountDetailIconPath, detail.value(QContactOnlineAccount__FieldAccountIconPath).toString());
        item.insert(accountDetailServiceProvider, detail.value(QContactOnlineAccount::FieldServiceProvider).toString());
        item.insert(accountDetailServiceProviderDisplayName, detail.value(QContactOnlineAccount__FieldServiceProviderDisplayName).toString());

        QVariant state;
        QVariant message;

        // Find the presence detail linked to this account
        const QContactPresence &presence(linkedDetail(presenceDetails, detail.detailUri()));
        if (!presence.isEmpty()) {
            state = static_cast<int>(presence.presenceState());
            message = presence.customMessage();
        }

        item.insert(accountDetailPresenceState, state);
        item.insert(accountDetailPresenceMessage, message);

        item.insert(detailType, OnlineAccountType);
        item.insert(detailSubTypes, ::onlineAccountSubTypes(detail));
        item.insert(detailLabel, ::detailLabelType(detail));
        item.insert(detailIndex, index++);

        rv.append(item);
    }

    return rv;
}

QVariantList SeasidePerson::accountDetails() const
{
    return accountDetails(*mContact);
}

void SeasidePerson::setAccountDetails(const QVariantList &accountDetails)
{
    QList<QContactOnlineAccount> accounts(mContact->details<QContactOnlineAccount>());

    QList<QContactOnlineAccount> updatedAccounts;
    QSet<int> validIndices;

    foreach (const QVariant &item, accountDetails) {
        const QVariantMap detail(item.value<QVariantMap>());

        const QVariant typeValue = detail[detailType];
        if (typeValue.toInt() != OnlineAccountType) {
            qWarning() << "Invalid type value for online account:" << typeValue;
            continue;
        }

        QContactOnlineAccount updated;

        const QVariant indexValue = detail[detailIndex];
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < accounts.count()) {
            // Modify the existing detail
            updated = accounts.at(index);
            Q_ASSERT(!validIndices.contains(index));
        } else if (index != -1) {
            qWarning() << "Invalid index value for account details:" << index;
            continue;
        }

        const QVariant accountUriValue = detail[accountDetailUri];
        const QString updatedUri(accountUriValue.value<QString>());
        if (updatedUri.trimmed().isEmpty()) {
            // Remove this address from the list
            continue;
        }

        updated.setAccountUri(updatedUri);

        const QVariant accountPathValue = detail[accountDetailPath];
        updated.setValue(QContactOnlineAccount__FieldAccountPath, accountPathValue.value<QString>());

        const QVariant accountDisplayNameValue = detail[accountDetailDisplayName];
        updated.setValue(QContactOnlineAccount__FieldAccountDisplayName, accountDisplayNameValue.value<QString>());

        const QVariant iconPathValue = detail[accountDetailIconPath];
        updated.setValue(QContactOnlineAccount__FieldAccountIconPath, iconPathValue.value<QString>());

        const QVariant serviceProviderValue = detail[accountDetailServiceProvider];
        updated.setServiceProvider(serviceProviderValue.value<QString>());

        const QVariant serviceProviderDisplayNameValue = detail[accountDetailServiceProviderDisplayName];
        updated.setValue(QContactOnlineAccount__FieldServiceProviderDisplayName, serviceProviderDisplayNameValue.value<QString>());

        const QVariant subTypesValue = detail[detailSubTypes];
        ::setOnlineAccountSubTypes(updated, subTypesValue.value<QVariantList>());

        const QVariant labelValue = detail[detailLabel];
        ::setDetailLabelType(updated, labelValue.isValid() ? labelValue.toInt() : NoLabel);

        updatedAccounts.append(updated);
        if (index != -1) {
            validIndices.insert(index);
        }
    }

    updateDetails(accounts, updatedAccounts, mContact, validIndices);
    emit accountDetailsChanged();

    recalculateDisplayLabel();
}

QString SeasidePerson::syncTarget() const
{
    return mContact->detail<QContactSyncTarget>().syncTarget();
}

QList<int> SeasidePerson::constituents() const
{
    return mConstituents;
}

void SeasidePerson::setConstituents(const QList<int> &constituents)
{
    mConstituents = constituents;
}

QList<int> SeasidePerson::mergeCandidates() const
{
    return mCandidates;
}

void SeasidePerson::setMergeCandidates(const QList<int> &candidates)
{
    mCandidates = candidates;
}

void SeasidePerson::addAccount(const QString &path, const QString &uri, const QString &provider, const QString &iconPath)
{
    QContactOnlineAccount detail;
    detail.setValue(QContactOnlineAccount__FieldAccountPath, path);
    detail.setAccountUri(uri);
    detail.setServiceProvider(provider);
    detail.setValue(QContactOnlineAccount__FieldAccountIconPath, iconPath);

    mContact->saveDetail(&detail);

    QContactPresence presence;
    presence.setLinkedDetailUris(QStringList() << detail.detailUri());
    
    mContact->saveDetail(&presence);
}

QContact SeasidePerson::contact() const
{
    return *mContact;
}

void SeasidePerson::setContact(const QContact &contact)
{
    QContact oldContact = *mContact;
    *mContact = contact;

    recalculateDisplayLabel();
    updateContactDetails(oldContact);
}

void SeasidePerson::updateContactDetails(const QContact &oldContact)
{
    m_changesReported = false;

    if (oldContact.id() != mContact->id())
        emitChangeSignal(&SeasidePerson::contactChanged);

    if (getPrimaryName(oldContact) != primaryName())
        emitChangeSignal(&SeasidePerson::primaryNameChanged);

    if (getSecondaryName(oldContact) != secondaryName())
        emitChangeSignal(&SeasidePerson::secondaryNameChanged);

    QContactName oldName = oldContact.detail<QContactName>();
    QContactName newName = mContact->detail<QContactName>();

    if (oldName.firstName() != newName.firstName())
        emitChangeSignal(&SeasidePerson::firstNameChanged);

    if (oldName.lastName() != newName.lastName())
        emitChangeSignal(&SeasidePerson::lastNameChanged);

    if (oldName.middleName() != newName.middleName())
        emitChangeSignal(&SeasidePerson::middleNameChanged);

    QContactOrganization oldCompany = oldContact.detail<QContactOrganization>();
    QContactOrganization newCompany = mContact->detail<QContactOrganization>();

    if (oldCompany.name() != newCompany.name())
        emitChangeSignal(&SeasidePerson::companyNameChanged);

    if (oldCompany.role() != newCompany.role())
        emitChangeSignal(&SeasidePerson::titleChanged);

    QContactFavorite oldFavorite = oldContact.detail<QContactFavorite>();
    QContactFavorite newFavorite = mContact->detail<QContactFavorite>();

    if (oldFavorite.isFavorite() != newFavorite.isFavorite())
        emitChangeSignal(&SeasidePerson::favoriteChanged);

    QContactAvatar oldAvatar = oldContact.detail<QContactAvatar>();
    QContactAvatar newAvatar = mContact->detail<QContactAvatar>();

    if (oldAvatar.imageUrl() != newAvatar.imageUrl()) {
        emitChangeSignal(&SeasidePerson::avatarUrlChanged);
        emitChangeSignal(&SeasidePerson::avatarPathChanged);
    }

    QContactGlobalPresence oldPresence = oldContact.detail<QContactGlobalPresence>();
    QContactGlobalPresence newPresence = mContact->detail<QContactGlobalPresence>();

    if (oldPresence.presenceState() != newPresence.presenceState())
        emitChangeSignal(&SeasidePerson::globalPresenceStateChanged);

    QList<QContactPresence> oldPresences = oldContact.details<QContactPresence>();
    QList<QContactPresence> newPresences = mContact->details<QContactPresence>();

    bool presenceInfoChanged = (oldPresences.count() != newPresences.count());
    if (!presenceInfoChanged) {
        QList<QContactPresence>::const_iterator oldIt = oldPresences.constBegin();
        QList<QContactPresence>::const_iterator newIt = newPresences.constBegin();

        for ( ; oldIt != oldPresences.constEnd(); ++oldIt, ++newIt) {
            if ((*oldIt).detailUri() != (*newIt).detailUri() ||
                (*oldIt).presenceState() != (*newIt).presenceState() ||
                (*oldIt).customMessage() != (*newIt).customMessage()) {
                presenceInfoChanged = true;
                break;
            }
        }
    }

    if (oldContact.details<QContactNickname>() != mContact->details<QContactNickname>()) {
        emitChangeSignal(&SeasidePerson::nicknameDetailsChanged);
    }
    if (oldContact.details<QContactPhoneNumber>() != mContact->details<QContactPhoneNumber>()) {
        emitChangeSignal(&SeasidePerson::phoneDetailsChanged);
    }
    if (oldContact.details<QContactEmailAddress>() != mContact->details<QContactEmailAddress>()) {
        emitChangeSignal(&SeasidePerson::emailDetailsChanged);
    }
    if (presenceInfoChanged ||
        oldContact.details<QContactOnlineAccount>() != mContact->details<QContactOnlineAccount>()) {
        emitChangeSignal(&SeasidePerson::accountDetailsChanged);
    }
    if (oldContact.details<QContactUrl>() != mContact->details<QContactUrl>()) {
        emitChangeSignal(&SeasidePerson::websiteDetailsChanged);
    }
    if (oldContact.detail<QContactBirthday>() != mContact->detail<QContactBirthday>()) {
        emitChangeSignal(&SeasidePerson::birthdayChanged);
    }
    if (oldContact.details<QContactAnniversary>() != mContact->details<QContactAnniversary>()) {
        emitChangeSignal(&SeasidePerson::anniversaryDetailsChanged);
    }

    if (m_changesReported) {
        emit dataChanged();
    }
}

void SeasidePerson::emitChangeSignals()
{
    emitChangeSignal(&SeasidePerson::contactChanged);
    emitChangeSignal(&SeasidePerson::primaryNameChanged);
    emitChangeSignal(&SeasidePerson::secondaryNameChanged);
    emitChangeSignal(&SeasidePerson::firstNameChanged);
    emitChangeSignal(&SeasidePerson::lastNameChanged);
    emitChangeSignal(&SeasidePerson::middleNameChanged);
    emitChangeSignal(&SeasidePerson::companyNameChanged);
    emitChangeSignal(&SeasidePerson::titleChanged);
    emitChangeSignal(&SeasidePerson::favoriteChanged);
    emitChangeSignal(&SeasidePerson::avatarUrlChanged);
    emitChangeSignal(&SeasidePerson::avatarPathChanged);
    emitChangeSignal(&SeasidePerson::globalPresenceStateChanged);
    emitChangeSignal(&SeasidePerson::nicknameDetailsChanged);
    emitChangeSignal(&SeasidePerson::phoneDetailsChanged);
    emitChangeSignal(&SeasidePerson::emailDetailsChanged);
    emitChangeSignal(&SeasidePerson::accountDetailsChanged);
    emitChangeSignal(&SeasidePerson::websiteDetailsChanged);
    emitChangeSignal(&SeasidePerson::birthdayChanged);
    emitChangeSignal(&SeasidePerson::anniversaryDetailsChanged);
    emit dataChanged();
}

QString SeasidePerson::getPrimaryName(const QContact &contact) const
{
    const QContactName nameDetail = contact.detail<QContactName>();
    const QString firstName(nameDetail.firstName());
    const QString lastName(nameDetail.lastName());

    if (firstName.isEmpty() && lastName.isEmpty()) {
        return QString();
    }

    const bool firstNameFirst(SeasideCache::displayLabelOrder() == SeasideCache::FirstNameFirst);
    return firstNameFirst ? firstName : lastName;
}

QString SeasidePerson::getSecondaryName(const QContact &contact) const
{
    const QContactName nameDetail = contact.detail<QContactName>();

    const bool firstNameFirst(SeasideCache::displayLabelOrder() == SeasideCache::FirstNameFirst);
    return firstNameFirst ? nameDetail.lastName() : nameDetail.firstName();
}

void SeasidePerson::ensureComplete()
{
    if (SeasideCache::CacheItem *item = SeasideCache::itemById(SeasideCache::apiId(*mContact))) {
        SeasideCache::ensureCompletion(item);
    }
}

QVariant SeasidePerson::contactData() const
{
    return QVariant::fromValue(contact());
}

void SeasidePerson::setContactData(const QVariant &data)
{
    if (mAttachState == Attached) {
        // Just update the existing contact's data
        setContact(data.value<QContact>());
        return;
    }

    if (mAttachState == Unattached) {
        delete mContact;
    } else if (mAttachState == Listening) {
        mItem->removeListener(this);
        mItem = 0;
    }

    mContact = new QContact(data.value<QContact>());
    mAttachState = Unattached;

    // We don't know if this contact is complete or not - assume it isn't if it has an ID
    mComplete = (id() == 0);

    recalculateDisplayLabel();
}

void SeasidePerson::resetContactData()
{
    if (SeasideCache::CacheItem *item = SeasideCache::itemById(SeasideCache::apiId(*mContact))) {
        // Fetch details for this contact again
        SeasideCache::refreshContact(item);
    }
}

QString SeasidePerson::vCard() const
{
    QVersitContactExporter exporter;
    if (!exporter.exportContacts(QList<QContact>() << *mContact, QVersitDocument::VCard21Type)) {
        qWarning() << Q_FUNC_INFO << "Failed to create vCard: " << exporter.errorMap();
        return QString();
    }

    QByteArray vcard;
    QVersitWriter writer(&vcard);
    if (!writer.startWriting(exporter.documents())) {
        qWarning() << Q_FUNC_INFO << "Can't start writing vcard " << writer.error();
        return QString();
    }
    writer.waitForFinished();

    return QString::fromUtf8(vcard);
}

QStringList SeasidePerson::avatarUrls() const
{
    return avatarUrlsExcluding(QStringList());
}

QStringList SeasidePerson::avatarUrlsExcluding(const QStringList &excludeMetadata) const
{
    QSet<QString> urls;

    foreach (const QContactAvatar &avatar, mContact->details<QContactAvatar>()) {
        const QString metadata(avatar.value(QContactAvatar__FieldAvatarMetadata).toString());
        if (excludeMetadata.contains(metadata))
            continue;

        urls.insert(avatar.imageUrl().toString());
    }

    return urls.toList();
}

void SeasidePerson::fetchConstituents()
{
    if (SeasideCache::validId(SeasideCache::apiId(*mContact))) {
        SeasideCache::fetchConstituents(contact());
    } else {
        // No constituents
        QMetaObject::invokeMethod(this, "constituentsChanged", Qt::QueuedConnection);
    }
}

void SeasidePerson::fetchMergeCandidates()
{
    SeasideCache::fetchMergeCandidates(contact());
}

void SeasidePerson::resolvePhoneNumber(const QString &number, bool requireComplete)
{
    if (SeasideCache::CacheItem *item = SeasideCache::resolvePhoneNumber(this, number, requireComplete)) {
        // TODO: should this be invoked async?
        addressResolved(QString(), number, item);
    }
}

void SeasidePerson::resolveEmailAddress(const QString &address, bool requireComplete)
{
    if (SeasideCache::CacheItem *item = SeasideCache::resolveEmailAddress(this, address, requireComplete)) {
        addressResolved(address, QString(), item);
    }
}

void SeasidePerson::resolveOnlineAccount(const QString &localUid, const QString &remoteUid, bool requireComplete)
{
    if (SeasideCache::CacheItem *item = SeasideCache::resolveOnlineAccount(this, localUid, remoteUid, requireComplete)) {
        addressResolved(localUid, remoteUid, item);
    }
}

QVariantList SeasidePerson::removeDuplicatePhoneNumbers(const QVariantList &phoneNumbers)
{
    QVariantList rv;

    foreach (const QVariant &item, phoneNumbers) {
        const QVariantMap detail(item.value<QVariantMap>());
        const QVariant &minimized = detail.value(phoneDetailMinimizedNumber);

        // See if we already have a match for this number in minimized form
        QVariantList::iterator it = rv.begin(), end = rv.end();
        for ( ; it != end; ++it) {
            const QVariant &rvItem(*it);
            const QVariantMap prior(rvItem.value<QVariantMap>());
            const QVariant &priorMinimized = prior.value(phoneDetailMinimizedNumber);

            if (priorMinimized == minimized) {
                // This number is already present in minimized form - which is preferred?
                const QString &normalized = detail.value(phoneDetailNormalizedNumber).toString();
                const QString &priorNormalized = prior.value(phoneDetailNormalizedNumber).toString();

                const QString &number = detail.value(phoneDetailNumber).toString();
                const QString &priorNumber = prior.value(phoneDetailNumber).toString();

                // We will allow up to two forms: the longest initial-plus variant, and
                // the longest non-plus variant, but only if that exceeds the length of
                // the initial-plus form, where present
                bool replace(false);
                bool append(false);

                const QChar plus(QChar::fromLatin1('+'));
                if (normalized[0] == plus) {
                    if (priorNormalized[0] == plus) {
                        // Prefer the longer normalized form, or the longer unnormalized form
                        // if the normalized forms are equal
                        replace = (normalized.length() > priorNormalized.length() ||
                                   (normalized.length() == priorNormalized.length() &&
                                    number.length() > priorNumber.length()));
                    } else {
                        if (normalized.length() >= priorNormalized.length()) {
                            replace = true;
                        } else {
                            append = true;
                        }
                    }
                } else {
                    if (normalized.length() > priorNormalized.length() ||
                        (normalized.length() == priorNormalized.length() &&
                         number.length() > priorNumber.length())) {
                        if (priorNormalized[0] == plus) {
                            append = true;
                        } else {
                            replace = true;
                        }
                    }
                }

                if (replace) {
                    // Prefer this form of the number to the shorter form already present
                    *it = detail;
                    break;
                } else if (!append) {
                    break;
                }
            }
        }
        if (it == end) {
            rv.append(detail);
        }
    }

    return rv;
}

QVariantList SeasidePerson::removeDuplicateOnlineAccounts(const QVariantList &onlineAccounts)
{
    QVariantList rv;

    foreach (const QVariant &item, onlineAccounts) {
        const QVariantMap detail(item.value<QVariantMap>());
        const QString uri = detail.value(accountDetailUri).toString().toLower();
        const QString path = detail.value(accountDetailPath).toString();

        // See if we already have this URI (case-insensitive)
        QVariantList::iterator it = rv.begin(), end = rv.end();
        for ( ; it != end; ++it) {
            const QVariant &rvItem(*it);
            const QVariantMap prior(rvItem.value<QVariantMap>());
            const QString priorUri = prior.value(accountDetailUri).toString().toLower();

            if (priorUri == uri) {
                // This URI is already present - does the path differ?
                const QString priorPath = prior.value(accountDetailPath).toString();
                if (priorPath == path) {
                    // This is a duplicate
                    break;
                } else if (priorPath.isEmpty() && !path.isEmpty()) {
                    // This is a better option - replace the prior instance
                    *it = detail;
                    break;
                } else if (path.isEmpty()) {
                    // We don't need to add another without a path
                    break;
                }
            }
        }
        if (it == end) {
            // No match found, or differs on path
            rv.append(detail);
        }
    }

    return rv;
}

void SeasidePerson::updateContact(const QContact &newContact, QContact *oldContact, SeasideCache::ContactState state)
{
    Q_UNUSED(oldContact)
    Q_ASSERT(oldContact == mContact);

    setContact(newContact);
    setComplete(state == SeasideCache::ContactComplete);
}

void SeasidePerson::addressResolved(const QString &, const QString &, SeasideCache::CacheItem *item)
{
    if (item) {
        if (&item->contact != mContact) {
            QContact *oldContact = mContact;

            // Attach to the contact in the cache item
            mContact = &item->contact;
            recalculateDisplayLabel();
            updateContactDetails(*oldContact);

            // Release our previous contact info
            delete oldContact;

            // We need to be informed of any changes to this contact in the cache
            item->appendListener(this, this);
            mItem = item;
            mAttachState = Listening;
        }

        setComplete(item->contactState == SeasideCache::ContactComplete);
    }

    emit addressResolved();
}

void SeasidePerson::itemUpdated(SeasideCache::CacheItem *)
{
    // We don't know what has changed - report everything changed
    recalculateDisplayLabel();
    emitChangeSignals();
}

void SeasidePerson::itemAboutToBeRemoved(SeasideCache::CacheItem *item)
{
    if (&item->contact == mContact) {
        // Our contact is being destroyed - copy the address details to an internal contact
        mContact = new QContact;
        if (mAttachState == Listening) {
            Q_ASSERT(mItem == item);
            mItem->removeListener(this);
            mItem = 0;
        }
        mAttachState = Unattached;

        foreach (QContactPhoneNumber number, item->contact.details<QContactPhoneNumber>()) {
            mContact->saveDetail(&number);
        }
        foreach (QContactEmailAddress address, item->contact.details<QContactEmailAddress>()) {
            mContact->saveDetail(&address);
        }
        foreach (QContactOnlineAccount account, item->contact.details<QContactOnlineAccount>()) {
            mContact->saveDetail(&account);
        }

        recalculateDisplayLabel();
        updateContactDetails(item->contact);
    }
}

void SeasidePerson::displayLabelOrderChanged(SeasideCache::DisplayLabelOrder order)
{
    recalculateDisplayLabel(order);
}

void SeasidePerson::constituentsFetched(const QList<int> &ids)
{
    setConstituents(ids);
    emit constituentsChanged();
}

void SeasidePerson::mergeCandidatesFetched(const QList<int> &ids)
{
    setMergeCandidates(ids);
    emit mergeCandidatesChanged();
}

void SeasidePerson::aggregationOperationCompleted()
{
    emit aggregationOperationFinished();
}

void SeasidePerson::aggregateInto(SeasidePerson *person)
{
    if (!person)
        return;
    // linking must done between two aggregates
    if (syncTarget() != QLatin1String("aggregate")) {
        qWarning() << "SeasidePerson::aggregateInto() failed, this person is not an aggregate contact";
        return;
    }
    if (person->syncTarget() != QLatin1String("aggregate")) {
        qWarning() << "SeasidePerson::aggregateInto() failed, given person is not an aggregate contact";
        return;
    }
    SeasideCache::aggregateContacts(person->contact(), *mContact);
}

void SeasidePerson::disaggregateFrom(SeasidePerson *person)
{
    if (!person)
        return;
    // unlinking must be done between an aggregate and a non-aggregate
    if (person->syncTarget() != QLatin1String("aggregate")) {
        qWarning() << "SeasidePerson::disaggregateFrom() failed, given person is not an aggregate contact";
        return;
    }
    if (syncTarget() == QLatin1String("aggregate")) {
        qWarning() << "SeasidePerson::disaggregateFrom() failed, this person is already an aggregate";
        return;
    }
    SeasideCache::disaggregateContacts(person->contact(), *mContact);
}

SeasidePersonAttached *SeasidePerson::qmlAttachedProperties(QObject *object)
{
    return new SeasidePersonAttached(object);
}

