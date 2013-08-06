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

SeasidePerson::SeasidePerson(QObject *parent)
    : QObject(parent)
    , mContact(new QContact)
    , mComplete(true)
    , mDeleteContact(true)
{
}

SeasidePerson::SeasidePerson(const QContact &contact, QObject *parent)
    : QObject(parent)
    , mContact(new QContact(contact))
    , mComplete(true)
    , mDeleteContact(true)
{
}

SeasidePerson::SeasidePerson(QContact *contact, bool complete, QObject *parent)
    : QObject(parent)
    , mContact(contact)
    , mComplete(complete)
    , mDeleteContact(false)
{
}

SeasidePerson::~SeasidePerson()
{
    SeasideCache::unregisterResolveListener(this);

    emit contactRemoved();

    if (mDeleteContact)
        delete mContact;
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

        // TODO: If required, store this to the contact backend to prevent later recalculation
    }
}

QString SeasidePerson::displayLabel() const
{
    if (mDisplayLabel.isEmpty()) {
        recalculateDisplayLabel();
    }

    return mDisplayLabel;
}

QString SeasidePerson::sectionBucket() const
{
    if (displayLabel().isEmpty())
        return QString();

    // TODO: won't be at all correct for localisation
    // for some locales (asian in particular), we may need multiple bytes - not
    // just the first - also, we should use QLocale (or ICU) to uppercase, not
    // QString, as QString uses C locale.
    return displayLabel().at(0).toUpper();

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
    QContactAvatar avatarDetail = mContact->detail<QContactAvatar>();
    return avatarDetail.imageUrl();
}

void SeasidePerson::setAvatarUrl(QUrl avatarUrl)
{
    QContactAvatar avatarDetail = mContact->detail<QContactAvatar>();
    avatarDetail.setImageUrl(avatarUrl);
    mContact->saveDetail(&avatarDetail);
    emit avatarUrlChanged();
    emit avatarPathChanged();
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

}

QStringList SeasidePerson::phoneNumbers() const
{
    return listPropertyFromDetailField<QContactPhoneNumber>(*mContact, QContactPhoneNumber::FieldNumber);
}

void SeasidePerson::setPhoneNumbers(const QStringList &phoneNumbers)
{
    setPropertyFieldFromList<QContactPhoneNumber>(*mContact, QContactPhoneNumber::FieldNumber, phoneNumbers);
    emit phoneNumbersChanged();
}

QList<int> SeasidePerson::phoneNumberTypes() const
{
    const QList<QContactPhoneNumber> &numbers = mContact->details<QContactPhoneNumber>();
    QList<int> types;
    types.reserve((numbers.length()));

    foreach(const QContactPhoneNumber &number, numbers) {
        if (number.contexts().contains(QContactDetail::ContextHome)
            && number.subTypes().contains(QContactPhoneNumber::SubTypeLandline)) {
            types.push_back(SeasidePerson::PhoneHomeType);
        } else if (number.contexts().contains(QContactDetail::ContextWork)
            && number.subTypes().contains(QContactPhoneNumber::SubTypeLandline)) {
            types.push_back(SeasidePerson::PhoneWorkType);
        } else if (number.contexts().contains(QContactDetail::ContextHome)
            && number.subTypes().contains(QContactPhoneNumber::SubTypeMobile)) {
            types.push_back(SeasidePerson::PhoneMobileType);
        } else if (number.contexts().contains(QContactDetail::ContextHome)
            && number.subTypes().contains(QContactPhoneNumber::SubTypeFax)) {
            types.push_back(SeasidePerson::PhoneFaxType);
        } else if (number.contexts().contains(QContactDetail::ContextHome)
            && number.subTypes().contains(QContactPhoneNumber::SubTypePager)) {
            types.push_back(SeasidePerson::PhonePagerType);
        } else {
            qWarning() << "Warning: Could not get phone type for '" << number.contexts() << "'";
        }
    }

    return types;
}

#ifdef USING_QTPIM
void setPhoneNumberType(QContactPhoneNumber &phoneNumber, QContactPhoneNumber::SubType type) { phoneNumber.setSubTypes(QList<int>() << static_cast<int>(type)); }
#else
void setPhoneNumberType(QContactPhoneNumber &phoneNumber, const QString &type) { phoneNumber.setSubTypes(type); }
#endif

void SeasidePerson::setPhoneNumberType(int which, SeasidePerson::DetailType type)
{
    const QList<QContactPhoneNumber> &numbers = mContact->details<QContactPhoneNumber>();
    if (which >= numbers.length()) {
        qWarning() << "Unable to set type for phone number: invalid index specified. Aborting.";
        return;
    }

    QContactPhoneNumber number = numbers.at(which);
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

    mContact->saveDetail(&number);
    emit phoneNumberTypesChanged();
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

    foreach(const QContactEmailAddress &email, emails) {
        if (email.contexts().contains(QContactDetail::ContextHome)) {
            types.push_back(SeasidePerson::EmailHomeType);
        } else if (email.contexts().contains(QContactDetail::ContextWork)) {
            types.push_back(SeasidePerson::EmailWorkType);
        } else if (email.contexts().contains(QContactDetail::ContextOther)) {
            types.push_back(SeasidePerson::EmailOtherType);
        } else {
            qWarning() << "Warning: Could not get email type '" << email.contexts() << "'";
        }
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
    if (type == SeasidePerson::EmailHomeType) {
        email.setContexts(QContactDetail::ContextHome);
    }  else if (type == SeasidePerson::EmailWorkType) {
        email.setContexts(QContactDetail::ContextWork);
    } else if (type == SeasidePerson::EmailOtherType) {
        email.setContexts(QContactDetail::ContextOther);
    } else {
        qWarning() << "Warning: Could not save email type '" << type << "'";
    }

    mContact->saveDetail(&email);
    emit emailAddressTypesChanged();
}

// Fields are separated by \n characters
QStringList SeasidePerson::addresses() const
{
    QStringList retn;
    const QList<QContactAddress> &addresses = mContact->details<QContactAddress>();
    foreach (const QContactAddress &address, addresses) {
        QString currAddressStr;
        currAddressStr.append(address.street());
        currAddressStr.append("\n");
        currAddressStr.append(address.locality());
        currAddressStr.append("\n");
        currAddressStr.append(address.region());
        currAddressStr.append("\n");
        currAddressStr.append(address.postcode());
        currAddressStr.append("\n");
        currAddressStr.append(address.country());
        currAddressStr.append("\n");
        currAddressStr.append(address.postOfficeBox());
        retn.append(currAddressStr);
    }
    return retn;
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
            newAddress.setStreet(split.at(0));
            newAddress.setLocality(split.at(1));
            newAddress.setRegion(split.at(2));
            newAddress.setPostcode(split.at(3));
            newAddress.setCountry(split.at(4));
            newAddress.setPostOfficeBox(split.at(5));
            mContact->saveDetail(&newAddress);
        }
    } else {
        /* overwrite existing details */
        for (int i = 0; i < splitStrings.count(); ++i) {
            const QStringList &split = splitStrings.at(i);
            QContactAddress oldAddress = oldDetailList.at(i);
            oldAddress.setStreet(split.at(0));
            oldAddress.setLocality(split.at(1));
            oldAddress.setRegion(split.at(2));
            oldAddress.setPostcode(split.at(3));
            oldAddress.setCountry(split.at(4));
            oldAddress.setPostOfficeBox(split.at(5));
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

    foreach(const QContactAddress &address, addresses) {
        if (address.contexts().contains(QContactDetail::ContextHome)) {
            types.push_back(SeasidePerson::AddressHomeType);
        } else if (address.contexts().contains(QContactDetail::ContextWork)) {
            types.push_back(SeasidePerson::AddressWorkType);
        } else if (address.contexts().contains(QContactDetail::ContextOther)) {
            types.push_back(SeasidePerson::AddressOtherType);
        } else {
            qWarning() << "Warning: Could not get address type '" << address.contexts() << "'";
        }
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
    if (type == SeasidePerson::AddressHomeType) {
        address.setContexts(QContactDetail::ContextHome);
    }  else if (type == SeasidePerson::AddressWorkType) {
        address.setContexts(QContactDetail::ContextWork);
    } else if (type == SeasidePerson::AddressOtherType) {
        address.setContexts(QContactDetail::ContextOther);
    } else {
        qWarning() << "Warning: Could not save address type '" << type << "'";
    }

    mContact->saveDetail(&address);
    emit addressTypesChanged();
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

    foreach(const QContactUrl &url, urls) {
        if (url.contexts().contains(QContactDetail::ContextHome)) {
            types.push_back(SeasidePerson::WebsiteHomeType);
        } else if (url.contexts().contains(QContactDetail::ContextWork)) {
            types.push_back(SeasidePerson::WebsiteWorkType);
        } else if (url.contexts().contains(QContactDetail::ContextOther)) {
            types.push_back(SeasidePerson::WebsiteOtherType);
        } else {
            qWarning() << "Warning: Could not get website type '" << url.contexts() << "'";
        }
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
    if (type == SeasidePerson::WebsiteHomeType) {
        url.setContexts(QContactDetail::ContextHome);
    }  else if (type == SeasidePerson::WebsiteWorkType) {
        url.setContexts(QContactDetail::ContextWork);
    } else if (type == SeasidePerson::WebsiteOtherType) {
        url.setContexts(QContactDetail::ContextOther);
    } else {
        qWarning() << "Warning: Could not save website type '" << type << "'";
    }

    mContact->saveDetail(&url);
    emit websiteTypesChanged();
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
    if (oldContact.id() != mContact->id())
        emit contactChanged();

    QContactName oldName = oldContact.detail<QContactName>();
    QContactName newName = mContact->detail<QContactName>();

    if (oldName.firstName() != newName.firstName())
        emit firstNameChanged();

    if (oldName.lastName() != newName.lastName())
        emit lastNameChanged();

    QContactOrganization oldCompany = oldContact.detail<QContactOrganization>();
    QContactOrganization newCompany = mContact->detail<QContactOrganization>();

    if (oldCompany.name() != newCompany.name())
        emit companyNameChanged();

    QContactFavorite oldFavorite = oldContact.detail<QContactFavorite>();
    QContactFavorite newFavorite = mContact->detail<QContactFavorite>();

    if (oldFavorite.isFavorite() != newFavorite.isFavorite())
        emit favoriteChanged();

    QContactAvatar oldAvatar = oldContact.detail<QContactAvatar>();
    QContactAvatar newAvatar = mContact->detail<QContactAvatar>();

    if (oldAvatar.imageUrl() != newAvatar.imageUrl()) {
        emit avatarUrlChanged();
        emit avatarPathChanged();
    }

    QContactGlobalPresence oldPresence = oldContact.detail<QContactGlobalPresence>();
    QContactGlobalPresence newPresence = mContact->detail<QContactGlobalPresence>();

    if (oldPresence.presenceState() != newPresence.presenceState())
        emit globalPresenceStateChanged();

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
            emit presenceStatesChanged();
        }
        if (messagesChanged) {
            emit presenceMessagesChanged();
        }
        if (urisChanged) {
            emit presenceAccountProvidersChanged();
        }
    }

    // TODO: differencing of list type details
    emit phoneNumbersChanged();
    emit emailAddressesChanged();
    emit accountUrisChanged();
    emit accountPathsChanged();
    emit accountProvidersChanged();
    emit accountIconPathsChanged();

    recalculateDisplayLabel();
}

void SeasidePerson::ensureComplete()
{
    if (SeasideCache::CacheItem *item = SeasideCache::itemById(SeasideCache::apiId(*mContact))) {
        SeasideCache::ensureCompletion(item);
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

            // TODO: at this point, we are attached to the contact in the cache, but the cache
            // doesn't know about this Person instance, so it won't update us when the
            // contact changes.  We could use a list of attached Person objects in the
            // model to fix this...
        }

        setComplete(item->contactState == SeasideCache::ContactComplete);
    }

    emit addressResolved();
}

QString SeasidePerson::getDisplayLabel() const
{
    return displayLabel();
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

