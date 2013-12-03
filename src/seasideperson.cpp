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
    , mComplete(true)
    , mAttachState(Unattached)
    , mItem(0)
{
}

SeasidePerson::SeasidePerson(QContact *contact, bool complete, QObject *parent)
    : QObject(parent)
    , mContact(contact)
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
        recalculateDisplayLabel();
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
        return displayLabel();
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

    if (!displayLabel().isEmpty()) {
        return displayLabel().at(0).toUpper();
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

QString SeasidePerson::nickname() const
{
    QContactNickname nameDetail = mContact->detail<QContactNickname>();
    return nameDetail.nickname();
}

void SeasidePerson::setNickname(const QString &name)
{
    QContactNickname nameDetail = mContact->detail<QContactNickname>();
    nameDetail.setNickname(name);
    mContact->saveDetail(&nameDetail);
    emit nicknameChanged();
    recalculateDisplayLabel();
}

QString SeasidePerson::title() const
{
    QContactName nameDetail = mContact->detail<QContactName>();
    return nameDetail.prefix();
}

void SeasidePerson::setTitle(const QString &name)
{
    QContactName nameDetail = mContact->detail<QContactName>();
    nameDetail.setPrefix(name);
    mContact->saveDetail(&nameDetail);
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

template<typename F>
#ifdef USING_QTPIM
int fieldIdentifier(F field) { return static_cast<int>(field); }
#else
QString fieldIdentifier(F field) { return QString::fromLatin1(field.latin1()); }
#endif

template<typename D, typename F>
bool detailHasField(const D &detail, F field)
{
    return detail.hasValue(fieldIdentifier(field));
}

template<typename T, typename D, typename F>
T detailFieldValue(const D &detail, F field)
{
    return detail.template value<T>(fieldIdentifier(field));
}

template<typename D, typename F>
QStringList listPropertyFromDetailField(const QContact &contact, F field)
{
    QStringList rv;
    foreach (const D &detail, contact.details<D>()) {
        if (detailHasField(detail, field))
            rv.append(detailFieldValue<QString>(detail, field));
    }
    return rv;
}

template<typename T, typename F, typename V>
void setPropertyFieldFromList(QContact &contact, F field, V newValueList)
{
    const QList<T> oldDetailList = contact.details<T>();

    if (oldDetailList.count() != newValueList.count()) {
        bool removeAndReadd = true;
        if (oldDetailList.count() < newValueList.count()) {
            /* Check to see if existing details were modified at all */
            bool modification = false;
            for (int i = 0; i < oldDetailList.count(); ++i) {
                if (detailFieldValue<typename V::value_type>(oldDetailList.at(i), field) != newValueList.at(i)) {
                    modification = true;
                    break;
                }
            }

            if (!modification) {
                /* If the only changes are new additions, just add them. */
                for (int i = oldDetailList.count(); i < newValueList.count(); ++i) {
                    T detail;
                    detail.setValue(fieldIdentifier(field), newValueList.at(i));
                    contact.saveDetail(&detail);
                }
                removeAndReadd = false;
            } else {
                removeAndReadd = true;
            }
        }

        if (removeAndReadd) {
            foreach (T detail, oldDetailList)
                contact.removeDetail(&detail);

            foreach (typename V::value_type const &value, newValueList) {
                T detail;
                detail.setValue(fieldIdentifier(field), value);
                contact.saveDetail(&detail);
            }
        }
    } else {
        /* assign new numbers to the existing details. */
        for (int i = 0; i != newValueList.count(); ++i) {
            T detail = oldDetailList.at(i);
            detail.setValue(fieldIdentifier(field), newValueList.at(i));
            contact.saveDetail(&detail);
        }
    }
}

int phoneNumberType(const QContactPhoneNumber &number)
{
    int type = SeasidePerson::PhoneHomeType;

    if (number.subTypes().contains(QContactPhoneNumber::SubTypeMobile)) {
        type = SeasidePerson::PhoneMobileType;
    } else if (number.subTypes().contains(QContactPhoneNumber::SubTypeFax)) {
        type = SeasidePerson::PhoneFaxType;
    } else if (number.subTypes().contains(QContactPhoneNumber::SubTypePager)) {
        type = SeasidePerson::PhonePagerType;
    } else if (number.contexts().contains(QContactDetail::ContextWork)) {
        type = SeasidePerson::PhoneWorkType;
    }

    return type;
}

#ifdef USING_QTPIM
void setPhoneNumberType(QContactPhoneNumber &phoneNumber, QContactPhoneNumber::SubType type) { phoneNumber.setSubTypes(QList<int>() << static_cast<int>(type)); }
#else
void setPhoneNumberType(QContactPhoneNumber &phoneNumber, const QString &type) { phoneNumber.setSubTypes(type); }
#endif

void setPhoneNumberType(QContactPhoneNumber &number, SeasidePerson::DetailType type)
{
    if (type == SeasidePerson::PhoneHomeType) {
        ::setPhoneNumberType(number, QContactPhoneNumber::SubTypeLandline);
        number.setContexts(QContactDetail::ContextHome);
    }  else if (type == SeasidePerson::PhoneWorkType) {
        ::setPhoneNumberType(number, QContactPhoneNumber::SubTypeLandline);
        number.setContexts(QContactDetail::ContextWork);
    } else if (type == SeasidePerson::PhoneMobileType) {
        ::setPhoneNumberType(number, QContactPhoneNumber::SubTypeMobile);
        number.setContexts(QContactDetail::ContextHome);
    } else if (type == SeasidePerson::PhoneFaxType) {
        ::setPhoneNumberType(number, QContactPhoneNumber::SubTypeFax);
        number.setContexts(QContactDetail::ContextHome);
    } else if (type == SeasidePerson::PhonePagerType) {
        ::setPhoneNumberType(number, QContactPhoneNumber::SubTypePager);
        number.setContexts(QContactDetail::ContextHome);
    } else {
        qWarning() << "Warning: Could not save phone type '" << type << "'";
    }
}

}

QStringList SeasidePerson::phoneNumbers() const
{
    return listPropertyFromDetailField<QContactPhoneNumber>(*mContact, QContactPhoneNumber::FieldNumber);
}

void SeasidePerson::setPhoneNumbers(const QStringList &phoneNumbers)
{
    setPropertyFieldFromList<QContactPhoneNumber>(*mContact, QContactPhoneNumber::FieldNumber, phoneNumbers);
    emit phoneNumbersChanged();
    recalculateDisplayLabel();
}

QList<int> SeasidePerson::phoneNumberTypes() const
{
    const QList<QContactPhoneNumber> &numbers = mContact->details<QContactPhoneNumber>();
    QList<int> types;
    types.reserve((numbers.length()));

    foreach (const QContactPhoneNumber &number, numbers) {
        types.push_back(::phoneNumberType(number));
    }

    return types;
}

void SeasidePerson::setPhoneNumberType(int which, SeasidePerson::DetailType type)
{
    const QList<QContactPhoneNumber> &numbers = mContact->details<QContactPhoneNumber>();
    if (which >= numbers.length()) {
        qWarning() << "Unable to set type for phone number: invalid index specified. Aborting.";
        return;
    }

    QContactPhoneNumber number = numbers.at(which);
    ::setPhoneNumberType(number, type);

    mContact->saveDetail(&number);
    emit phoneNumberTypesChanged();
}

namespace {

const QString detailReadOnly(QString::fromLatin1("readOnly"));
const QString detailOriginId(QString::fromLatin1("originId"));
const QString detailSyncTarget(QString::fromLatin1("syncTarget"));
const QString detailType(QString::fromLatin1("type"));
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

const QString phoneDetailNumber(QString::fromLatin1("number"));
const QString phoneDetailNormalizedNumber(QString::fromLatin1("normalizedNumber"));
const QString phoneDetailMinimizedNumber(QString::fromLatin1("minimizedNumber"));

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
        item.insert(detailType, ::phoneNumberType(detail));
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
        const QVariant indexValue = detail[detailIndex];
        const QVariant typeValue = detail[detailType];
        const QVariant numberValue = detail[phoneDetailNumber];

        QContactPhoneNumber updated;
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < numbers.count()) {
            // Modify the existing detail
            updated = numbers.at(index);
            Q_ASSERT(!validIndices.contains(index));
            validIndices.insert(index);
        } else if (index != -1) {
            qWarning() << "Invalid index value for phone details:" << index;
        }

        const int type = typeValue.isValid() ? typeValue.value<int>() : -1;
        ::setPhoneNumberType(updated, static_cast<SeasidePerson::DetailType>(type));

        // Ignore normalized/minized number variants
        updated.setValue(QContactPhoneNumber::FieldNumber, numberValue.value<QString>());
        updatedNumbers.append(updated);
    }

    updateDetails(numbers, updatedNumbers, mContact, validIndices);

    emit phoneNumbersChanged();
    emit phoneNumberTypesChanged();
    emit phoneDetailsChanged();
}

namespace {

int emailAddressType(const QContactEmailAddress &email)
{
    int type = SeasidePerson::EmailHomeType;

    if (email.contexts().contains(QContactDetail::ContextWork)) {
        type = SeasidePerson::EmailWorkType;
    } else if (email.contexts().contains(QContactDetail::ContextOther)) {
        type = SeasidePerson::EmailOtherType;
    }

    return type;
}

void setEmailAddressType(QContactEmailAddress &email, SeasidePerson::DetailType type)
{
    if (type == SeasidePerson::EmailHomeType) {
        email.setContexts(QContactDetail::ContextHome);
    }  else if (type == SeasidePerson::EmailWorkType) {
        email.setContexts(QContactDetail::ContextWork);
    } else if (type == SeasidePerson::EmailOtherType) {
        email.setContexts(QContactDetail::ContextOther);
    } else {
        qWarning() << "Warning: Could not save email type '" << type << "'";
    }
}

}

QStringList SeasidePerson::emailAddresses() const
{
    return listPropertyFromDetailField<QContactEmailAddress>(*mContact, QContactEmailAddress::FieldEmailAddress);
}

void SeasidePerson::setEmailAddresses(const QStringList &emailAddresses)
{
    setPropertyFieldFromList<QContactEmailAddress>(*mContact, QContactEmailAddress::FieldEmailAddress, emailAddresses);
    emit emailAddressesChanged();
}

QList<int> SeasidePerson::emailAddressTypes() const
{
    const QList<QContactEmailAddress> &emails = mContact->details<QContactEmailAddress>();
    QList<int> types;
    types.reserve((emails.length()));

    foreach (const QContactEmailAddress &email, emails) {
        types.push_back(::emailAddressType(email));
    }

    return types;
}

void SeasidePerson::setEmailAddressType(int which, SeasidePerson::DetailType type)
{
    const QList<QContactEmailAddress> &emails = mContact->details<QContactEmailAddress>();

    if (which >= emails.length()) {
        qWarning() << "Unable to set type for email address: invalid index specified. Aborting.";
        return;
    }

    QContactEmailAddress email = emails.at(which);
    ::setEmailAddressType(email, type);

    mContact->saveDetail(&email);
    emit emailAddressTypesChanged();
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
        item.insert(detailType, ::emailAddressType(detail));
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
        const QVariant indexValue = detail[detailIndex];
        const QVariant typeValue = detail[detailType];
        const QVariant addressValue = detail[emailDetailAddress];

        QContactEmailAddress updated;
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < addresses.count()) {
            // Modify the existing detail
            updated = addresses.at(index);
            Q_ASSERT(!validIndices.contains(index));
            validIndices.insert(index);
        } else if (index != -1) {
            qWarning() << "Invalid index value for email details:" << index;
        }

        const int type = typeValue.isValid() ? typeValue.value<int>() : -1;
        ::setEmailAddressType(updated, static_cast<SeasidePerson::DetailType>(type));

        updated.setValue(QContactEmailAddress::FieldEmailAddress, addressValue.value<QString>());
        updatedAddresses.append(updated);
    }

    updateDetails(addresses, updatedAddresses, mContact, validIndices);

    emit emailAddressesChanged();
    emit emailAddressTypesChanged();
    emit emailDetailsChanged();
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

int addressType(const QContactAddress &address)
{
    int type = SeasidePerson::AddressHomeType;

    if (address.contexts().contains(QContactDetail::ContextWork)) {
        type = SeasidePerson::AddressWorkType;
    } else if (address.contexts().contains(QContactDetail::ContextOther)) {
        type = SeasidePerson::AddressOtherType;
    }

    return type;
}

void setAddressType(QContactAddress &address, SeasidePerson::DetailType type)
{
    if (type == SeasidePerson::AddressHomeType) {
        address.setContexts(QContactDetail::ContextHome);
    }  else if (type == SeasidePerson::AddressWorkType) {
        address.setContexts(QContactDetail::ContextWork);
    } else if (type == SeasidePerson::AddressOtherType) {
        address.setContexts(QContactDetail::ContextOther);
    } else {
        qWarning() << "Warning: Could not save address type '" << type << "'";
    }
}

}

// Fields are separated by \n characters
QStringList SeasidePerson::addresses() const
{
    QStringList rv;
    foreach (const QContactAddress &address, mContact->details<QContactAddress>()) {
        rv.append(::addressString(address));
    }
    return rv;
}

void SeasidePerson::setAddresses(const QStringList &addresses)
{
    QList<QStringList> splitStrings;
    foreach (const QString &currAddressStr, addresses) {
        QStringList split = currAddressStr.split("\n");
        if (split.count() != 6) {
            qWarning() << "Warning: Could not save addresses - invalid format for address:" << currAddressStr;
            return;
        } else {
            splitStrings.append(split);
        }
    }

    const QList<QContactAddress> &oldDetailList = mContact->details<QContactAddress>();
    if (oldDetailList.count() != splitStrings.count()) {
        /* remove all current details, recreate new ones */
        foreach (QContactAddress oldAddress, oldDetailList)
            mContact->removeDetail(&oldAddress);
        foreach (const QStringList &split, splitStrings) {
            QContactAddress newAddress;
            ::setAddress(newAddress, split);
            mContact->saveDetail(&newAddress);
        }
    } else {
        /* overwrite existing details */
        for (int i = 0; i < splitStrings.count(); ++i) {
            const QStringList &split = splitStrings.at(i);
            QContactAddress oldAddress = oldDetailList.at(i);
            ::setAddress(oldAddress, split);
            mContact->saveDetail(&oldAddress);
        }
    }

    emit addressesChanged();
}

QList<int> SeasidePerson::addressTypes() const
{
    const QList<QContactAddress> &addresses = mContact->details<QContactAddress>();
    QList<int> types;
    types.reserve((addresses.length()));

    foreach (const QContactAddress &address, addresses) {
        types.push_back(::addressType(address));
    }

    return types;
}

void SeasidePerson::setAddressType(int which, SeasidePerson::DetailType type)
{
    const QList<QContactAddress> &addresses = mContact->details<QContactAddress>();

    if (which >= addresses.length()) {
        qWarning() << "Unable to set type for address: invalid index specified. Aborting.";
        return;
    }

    QContactAddress address = addresses.at(which);
    ::setAddressType(address, type);

    mContact->saveDetail(&address);
    emit addressTypesChanged();
}

namespace {

const QString addressDetailAddress(QString::fromLatin1("address"));

}

QVariantList SeasidePerson::addressDetails(const QContact &contact)
{
    QVariantList rv;

    int index = 0;
    foreach (const QContactAddress &detail, contact.details<QContactAddress>()) {
        QVariantMap item(detailProperties(detail));
        item.insert(addressDetailAddress, ::addressString(detail));
        item.insert(detailType, ::addressType(detail));
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
        const QVariant indexValue = detail[detailIndex];
        const QVariant typeValue = detail[detailType];
        const QVariant addressValue = detail[addressDetailAddress];

        QContactAddress updated;
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < addresses.count()) {
            // Modify the existing detail
            updated = addresses.at(index);
            Q_ASSERT(!validIndices.contains(index));
            validIndices.insert(index);
        } else if (index != -1) {
            qWarning() << "Invalid index value for address details:" << index;
        }

        const int type = typeValue.isValid() ? typeValue.value<int>() : -1;
        ::setAddressType(updated, static_cast<SeasidePerson::DetailType>(type));

        const QString addressString(addressValue.value<QString>());
        QStringList split = addressString.split("\n");
        if (split.count() != 6) {
            qWarning() << "Warning: Could not save addresses - invalid format for address:" << addressString;
            continue;
        }

        ::setAddress(updated, split);
        updatedAddresses.append(updated);
    }

    updateDetails(addresses, updatedAddresses, mContact, validIndices);

    emit addressesChanged();
    emit addressTypesChanged();
    emit addressDetailsChanged();
}

namespace {

int websiteType(const QContactUrl &url)
{
    int type = SeasidePerson::WebsiteHomeType;

    if (url.contexts().contains(QContactDetail::ContextWork)) {
        type = SeasidePerson::WebsiteWorkType;
    } else if (url.contexts().contains(QContactDetail::ContextOther)) {
        type = SeasidePerson::WebsiteOtherType;
    }

    return type;
}

void setWebsiteType(QContactUrl &url, SeasidePerson::DetailType type)
{
    if (type == SeasidePerson::WebsiteHomeType) {
        url.setContexts(QContactDetail::ContextHome);
    }  else if (type == SeasidePerson::WebsiteWorkType) {
        url.setContexts(QContactDetail::ContextWork);
    } else if (type == SeasidePerson::WebsiteOtherType) {
        url.setContexts(QContactDetail::ContextOther);
    } else {
        qWarning() << "Warning: Could not save website type '" << type << "'";
    }
}

}

QStringList SeasidePerson::websites() const
{
    return listPropertyFromDetailField<QContactUrl>(*mContact, QContactUrl::FieldUrl);
}

void SeasidePerson::setWebsites(const QStringList &websites)
{
    setPropertyFieldFromList<QContactUrl>(*mContact, QContactUrl::FieldUrl, websites);
    emit websitesChanged();
}

QList<int> SeasidePerson::websiteTypes() const
{
    const QList<QContactUrl> &urls = mContact->details<QContactUrl>();
    QList<int> types;
    types.reserve((urls.length()));

    foreach (const QContactUrl &url, urls) {
        types.push_back(::websiteType(url));
    }

    return types;
}

void SeasidePerson::setWebsiteType(int which, SeasidePerson::DetailType type)
{
    const QList<QContactUrl> &urls = mContact->details<QContactUrl>();

    if (which >= urls.length()) {
        qWarning() << "Unable to set type for website: invalid index specified. Aborting.";
        return;
    }

    QContactUrl url = urls.at(which);
    ::setWebsiteType(url, type);

    mContact->saveDetail(&url);
    emit websiteTypesChanged();
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
        item.insert(detailType, ::websiteType(detail));
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
        const QVariant indexValue = detail[detailIndex];
        const QVariant typeValue = detail[detailType];
        const QVariant urlValue = detail[websiteDetailUrl];

        QContactUrl updated;
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < urls.count()) {
            // Modify the existing detail
            updated = urls.at(index);
            Q_ASSERT(!validIndices.contains(index));
            validIndices.insert(index);
        } else if (index != -1) {
            qWarning() << "Invalid index value for website details:" << index;
        }

        const int type = typeValue.isValid() ? typeValue.value<int>() : -1;
        ::setWebsiteType(updated, static_cast<SeasidePerson::DetailType>(type));

        updated.setValue(QContactUrl::FieldUrl, QUrl(urlValue.value<QString>()));
        updatedUrls.append(updated);
    }

    updateDetails(urls, updatedUrls, mContact, validIndices);

    emit websitesChanged();
    emit websiteTypesChanged();
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

QDateTime SeasidePerson::anniversary() const
{
    return mContact->detail<QContactAnniversary>().originalDateTime();
}

void SeasidePerson::setAnniversary(const QDateTime &av)
{
    QContactAnniversary anniv = mContact->detail<QContactAnniversary>();
    anniv.setOriginalDateTime(av);
    mContact->saveDetail(&anniv);
    emit anniversaryChanged();
}

void SeasidePerson::resetAnniversary()
{
    setAnniversary(QDateTime());
}

SeasidePerson::PresenceState SeasidePerson::globalPresenceState() const
{
    return static_cast<SeasidePerson::PresenceState>(mContact->detail<QContactGlobalPresence>().presenceState());
}

namespace { // Helper functions

int someElementPresentAtIndex(const QStringList &needleList, const QStringList &haystack)
{
    QStringList::const_iterator it = haystack.constBegin(), end = haystack.constEnd();
    for ( ; it != end; ++it) {
        if (needleList.contains(*it)) {
            return (it - haystack.constBegin());
        }
    }

    return -1;
}

template<class ListType, class AccountListType>
ListType inAccountOrder(ListType list, AccountListType accountList)
{
    ListType rv;

    // Find the detailUri for each reportable account in the order they're yielded
    QStringList uriList;
    foreach (const typename AccountListType::value_type &account, accountList) {
        if (account.hasValue(QContactOnlineAccount__FieldAccountPath)) {
            uriList.append(account.detailUri());
        }
    }

    rv.reserve(uriList.count());

    // For each value, insert in the order that the linked account is present
    foreach (const typename ListType::value_type &item, list) {
        int index = someElementPresentAtIndex(item.linkedDetailUris(), uriList);
        if (index != -1) {
            if (rv.count() > index) {
                rv[index] = item;
            } else {
                while (rv.count() < index) {
                    rv.append(typename ListType::value_type());
                }
                rv.append(item);
            }
        }
    }

    return rv;
}

}

QStringList SeasidePerson::presenceAccountProviders() const
{
    return accountProviders();
}

QList<int> SeasidePerson::presenceStates() const
{
    QList<int> rv;

    foreach (const QContactPresence &presence, inAccountOrder(mContact->details<QContactPresence>(), mContact->details<QContactOnlineAccount>())) {
        if (!presence.isEmpty()) {
            rv.append(static_cast<int>(presence.presenceState()));
        } else {
            rv.append(QContactPresence::PresenceUnknown);
        }
    }

    return rv;
}

QStringList SeasidePerson::presenceMessages() const
{
    QStringList rv;

    foreach (const QContactPresence &presence, inAccountOrder(mContact->details<QContactPresence>(), mContact->details<QContactOnlineAccount>())) {
        if (!presence.isEmpty()) {
            rv.append(presence.customMessage());
        } else {
            rv.append(QString());
        }
    }

    return rv;
}

QStringList SeasidePerson::accountUris() const
{
    return listPropertyFromDetailField<QContactOnlineAccount>(*mContact, QContactOnlineAccount::FieldAccountUri);
}

QStringList SeasidePerson::accountPaths() const
{
    return listPropertyFromDetailField<QContactOnlineAccount>(*mContact, QContactOnlineAccount__FieldAccountPath);
}

QStringList SeasidePerson::accountProviders() const
{
    QStringList rv;

    foreach (const QContactOnlineAccount &account, mContact->details<QContactOnlineAccount>()) {
        // Include the provider value for each account returned by accountPaths
        if (account.hasValue(QContactOnlineAccount__FieldAccountPath)) {
            rv.append(account.serviceProvider());
        }
    }

    return rv;
}

QStringList SeasidePerson::accountIconPaths() const
{
    QStringList rv;

    foreach (const QContactOnlineAccount &account, mContact->details<QContactOnlineAccount>()) {
        // Include the icon path value for each account returned by accountPaths
        if (account.hasValue(QContactOnlineAccount__FieldAccountPath)) {
            rv.append(account.value<QString>(QContactOnlineAccount__FieldAccountIconPath));
        }
    }

    return rv;
}

namespace {

const QString accountDetailUri(QString::fromLatin1("accountUri"));
const QString accountDetailPath(QString::fromLatin1("accountPath"));
const QString accountDetailDisplayName(QString::fromLatin1("accountDisplayName"));
const QString accountDetailIconPath(QString::fromLatin1("iconPath"));
const QString accountDetailServiceProvider(QString::fromLatin1("serviceProvider"));
const QString accountDetailServiceProviderDisplayName(QString::fromLatin1("serviceProviderDisplayName"));

}

QVariantList SeasidePerson::accountDetails(const QContact &contact)
{
    QVariantList rv;

    int index = 0;
    foreach (const QContactOnlineAccount &detail, contact.details<QContactOnlineAccount>()) {
        QVariantMap item(detailProperties(detail));
        item.insert(accountDetailUri, detail.value(QContactOnlineAccount::FieldAccountUri).toString());
        item.insert(accountDetailPath, detail.value(QContactOnlineAccount__FieldAccountPath).toString());
        item.insert(accountDetailDisplayName, detail.value(QContactOnlineAccount__FieldAccountDisplayName).toString());
        item.insert(accountDetailIconPath, detail.value(QContactOnlineAccount__FieldAccountIconPath).toString());
        item.insert(accountDetailServiceProvider, detail.value(QContactOnlineAccount::FieldServiceProvider).toString());
        item.insert(accountDetailServiceProviderDisplayName, detail.value(QContactOnlineAccount__FieldServiceProviderDisplayName).toString());
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
        const QVariant indexValue = detail[detailIndex];
        const QVariant accountUriValue = detail[accountDetailUri];
        const QVariant accountPathValue = detail[accountDetailPath];
        const QVariant accountDisplayNameValue = detail[accountDetailDisplayName];
        const QVariant iconPathValue = detail[accountDetailIconPath];
        const QVariant serviceProviderValue = detail[accountDetailServiceProvider];
        const QVariant serviceProviderDisplayNameValue = detail[accountDetailServiceProviderDisplayName];

        QContactOnlineAccount updated;
        const int index = indexValue.isValid() ? indexValue.value<int>() : -1;
        if (index >= 0 && index < accounts.count()) {
            // Modify the existing detail
            updated = accounts.at(index);
            Q_ASSERT(!validIndices.contains(index));
            validIndices.insert(index);
        } else if (index != -1) {
            qWarning() << "Invalid index value for account details:" << index;
        } else {
            qWarning() << "Adding new accounts is not supported by setAccountDetails!";
        }

        updated.setAccountUri(accountUriValue.value<QString>());
        updated.setValue(QContactOnlineAccount__FieldAccountPath, accountPathValue.value<QString>());
        updated.setValue(QContactOnlineAccount__FieldAccountDisplayName, accountDisplayNameValue.value<QString>());
        updated.setValue(QContactOnlineAccount__FieldAccountIconPath, iconPathValue.value<QString>());
        updated.setServiceProvider(serviceProviderValue.value<QString>());
        updated.setValue(QContactOnlineAccount__FieldServiceProviderDisplayName, serviceProviderDisplayNameValue.value<QString>());

        updatedAccounts.append(updated);
    }

    updateDetails(accounts, updatedAccounts, mContact, validIndices);

    emit accountDetailsChanged();
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

    QContactOrganization oldCompany = oldContact.detail<QContactOrganization>();
    QContactOrganization newCompany = mContact->detail<QContactOrganization>();

    if (oldCompany.name() != newCompany.name())
        emitChangeSignal(&SeasidePerson::companyNameChanged);

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

    {
        bool statesChanged = false;
        bool messagesChanged = false;
        bool urisChanged = false;

        if (oldPresences.count() != newPresences.count()) {
            statesChanged = messagesChanged = urisChanged = true;
        } else {
            QList<QContactPresence>::const_iterator oldIt = oldPresences.constBegin();
            QList<QContactPresence>::const_iterator newIt = newPresences.constBegin();

            for ( ; oldIt != oldPresences.constEnd(); ++oldIt, ++newIt) {
                if (oldIt->detailUri() != newIt->detailUri()) {
                    // Assume the entire account has been changed
                    statesChanged = messagesChanged = urisChanged = true;
                    break;
                } else if (oldIt->presenceState() != newIt->presenceState()) {
                    statesChanged = true;
                } else if (oldIt->customMessage() != newIt->customMessage()) {
                    messagesChanged = true;
                }
            }
        }

        if (statesChanged) {
            emitChangeSignal(&SeasidePerson::presenceStatesChanged);
        }
        if (messagesChanged) {
            emitChangeSignal(&SeasidePerson::presenceMessagesChanged);
        }
        if (urisChanged) {
            emitChangeSignal(&SeasidePerson::presenceAccountProvidersChanged);
        }
    }

    if (oldContact.details<QContactPhoneNumber>() != mContact->details<QContactPhoneNumber>()) {
        emitChangeSignal(&SeasidePerson::phoneNumbersChanged);
    }
    if (oldContact.details<QContactEmailAddress>() != mContact->details<QContactEmailAddress>()) {
        emitChangeSignal(&SeasidePerson::emailAddressesChanged);
    }
    if (oldContact.details<QContactOnlineAccount>() != mContact->details<QContactOnlineAccount>()) {
        emitChangeSignal(&SeasidePerson::accountUrisChanged);
        emitChangeSignal(&SeasidePerson::accountPathsChanged);
        emitChangeSignal(&SeasidePerson::accountProvidersChanged);
        emitChangeSignal(&SeasidePerson::accountIconPathsChanged);
    }

    if (m_changesReported) {
        emit dataChanged();
    }

    recalculateDisplayLabel();
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

                const QChar plus(QChar::fromLatin1('+'));
                if ((normalized[0] == plus && priorNormalized[0] != plus) ||
                    ((priorNormalized[0] != plus) &&
                     ((normalized.length() > priorNormalized.length()) ||
                      (normalized.length() == priorNormalized.length() &&
                       number.length() > priorNumber.length())))) {
                    // Prefer this form of the number to the shorter form already present
                    *it = detail;
                }
                break;
            }
        }
        if (it == end) {
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
    updateContactDetails(QContact());
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

