/*
 * Copyright (C) 2014 Martin Jones <martin.jones@jolla.com>
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

#include "seasidevcardmodel.h"
#include "seasideperson.h"
#include <QFile>
#include <QQmlEngine>
#include <QVersitContactImporter>
#include <QVersitReader>
#include <QContactDisplayLabel>
#include <QContactPhoneNumber>
#include <QContactEmailAddress>
#include <seasidepropertyhandler.h>

QTVERSIT_USE_NAMESPACE

const QByteArray displayLabelRole("displayLabel");
const QByteArray avatarRole("avatar");
const QByteArray avatarUrlRole("avatarUrl");
const QByteArray phoneNumbersRole("phoneNumbers");
const QByteArray emailAddressesRole("emailAddresses");
const QByteArray primaryNameRole("primaryName");
const QByteArray secondaryNameRole("secondaryName");
const QByteArray nicknameDetailsRole("nicknameDetails");
const QByteArray phoneDetailsRole("phoneDetails");
const QByteArray emailDetailsRole("emailDetails");
const QByteArray personRole("person");

SeasideVCardModel::SeasideVCardModel(QObject *parent)
    : QAbstractListModel(parent) , mComplete(false)
#ifdef HAS_MLITE
    , mDisplayLabelOrderConf(QLatin1String("/org/nemomobile/contacts/display_label_order"))
#endif
{
#ifdef HAS_MLITE
    connect(&mDisplayLabelOrderConf, SIGNAL(valueChanged()), this, SIGNAL(displayLabelOrderChanged()));
#endif
}

SeasideVCardModel::~SeasideVCardModel()
{
    qDeleteAll(mPeople);
}

QUrl SeasideVCardModel::source() const
{
    return mSource;
}

void SeasideVCardModel::setSource(const QUrl &source)
{
    if (source != mSource) {
        mSource = source;
        readContacts();
        emit sourceChanged();
    }
}

int SeasideVCardModel::count() const
{
    return rowCount(QModelIndex());
}

QString SeasideVCardModel::defaultCodec() const
{
    return mDefaultCodec;
}

void SeasideVCardModel::setDefaultCodec(const QString &codec)
{
    if (codec != mDefaultCodec) {
        mDefaultCodec = codec;
        readContacts();
        emit defaultCodecChanged();
    }
}

QHash<int, QByteArray> SeasideVCardModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(Qt::DisplayRole, displayLabelRole);
    roles.insert(AvatarRole, avatarRole);
    roles.insert(AvatarUrlRole, avatarUrlRole);
    roles.insert(PhoneNumbersRole, phoneNumbersRole);
    roles.insert(EmailAddressesRole, emailAddressesRole);
    roles.insert(PrimaryNameRole, primaryNameRole);
    roles.insert(SecondaryNameRole, secondaryNameRole);
    roles.insert(NicknameDetailsRole, nicknameDetailsRole);
    roles.insert(PhoneDetailsRole, phoneDetailsRole);
    roles.insert(EmailDetailsRole, emailDetailsRole);
    roles.insert(PersonRole, personRole);

    return roles;
}

int SeasideVCardModel::rowCount(const QModelIndex &) const
{
    return mContacts.count();
}

QVariant SeasideVCardModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= count())
        return QVariant();

    const QContact &contact = mContacts.at(index.row());

    if (role == PrimaryNameRole || role == SecondaryNameRole) {
        QContactName name = contact.detail<QContactName>();
        const QString firstName(name.firstName());
        const QString lastName(name.lastName());
        const bool firstNameFirst(displayLabelOrder() == FirstNameFirst);

        if (role == SecondaryNameRole) {
            return firstNameFirst ? lastName : firstName;
        }

        if (firstName.isEmpty() && lastName.isEmpty()) {
            // No real name details - fall back to the display label for primary name
            return data(index, Qt::DisplayRole);
        }
        return firstNameFirst ? firstName : lastName;
    } else if (role == AvatarRole || role == AvatarUrlRole) {
        QUrl avatarUrl = SeasideCache::filteredAvatarUrl(contact);
        if (role == AvatarUrlRole || !avatarUrl.isEmpty()) {
            return avatarUrl;
        }
        // Return the default avatar path for when no avatar URL is available
        return QUrl(QLatin1String("image://theme/icon-m-telephony-contact-avatar"));
    } else if (role == NicknameDetailsRole) {
        return SeasidePerson::nicknameDetails(contact);
    } else if (role == PhoneDetailsRole) {
        return SeasidePerson::phoneDetails(contact);
    } else if (role == EmailDetailsRole) {
        return SeasidePerson::emailDetails(contact);
    } else if (role == PhoneNumbersRole || role == EmailAddressesRole) {
        QStringList rv;
        if (role == PhoneNumbersRole) {
            foreach (const QContactPhoneNumber &number, contact.details<QContactPhoneNumber>()) {
                rv.append(number.number());
            }
        } else if (role == EmailAddressesRole) {
            foreach (const QContactEmailAddress &address, contact.details<QContactEmailAddress>()) {
                rv.append(address.emailAddress());
            }
        }
        return rv;
    } else if (role == Qt::DisplayRole) {
        QContactDisplayLabel label = contact.detail<QContactDisplayLabel>();
        return label.label();
    } else if (role == PersonRole) {
        return QVariant::fromValue(getPerson(index.row()));
    } else {
        qWarning() << "Invalid role requested:" << role;
    }

    return QVariant();
}

SeasidePerson *SeasideVCardModel::getPerson(int index) const
{
    if (index < 0 || index >= count())
        return 0;

    const QContact &contact = mContacts.at(index);
    SeasidePerson *person = mPeople.at(index);
    if (!person) {
        person = new SeasidePerson(contact);
        mPeople[index] = person;
        qmlEngine(this)->setObjectOwnership(person, QQmlEngine::CppOwnership);
    }

    return person;
}

SeasideVCardModel::DisplayLabelOrder SeasideVCardModel::displayLabelOrder() const
{
#ifdef HAS_MLITE
    QVariant displayLabelOrder = mDisplayLabelOrderConf.value();
    if (displayLabelOrder.isValid())
         return static_cast<DisplayLabelOrder>(displayLabelOrder.toInt());
#endif
    return FirstNameFirst;
}

void SeasideVCardModel::classBegin()
{
}

void SeasideVCardModel::componentComplete()
{
    mComplete = true;
    if (mSource.isValid())
        readContacts();
}

void SeasideVCardModel::readContacts()
{
    if (!mComplete)
        return;

    int oldCount = count();
    beginResetModel();

    mContacts.clear();
    qDeleteAll(mPeople);
    mPeople.clear();

    QFile vcf(mSource.toLocalFile());
    if (vcf.open(QIODevice::ReadOnly)) {
        // TODO: thread
        QVersitReader reader(&vcf);

        if (!mDefaultCodec.isEmpty()) {
            QTextCodec *codec = QTextCodec::codecForName(mDefaultCodec.toLatin1());
            if (codec)
                reader.setDefaultCodec(codec);
        }

        reader.startReading();
        reader.waitForFinished();

        QVersitContactImporter importer;
        SeasidePropertyHandler propertyHandler;
        importer.setPropertyHandler(&propertyHandler);
        importer.importDocuments(reader.results());

        mContacts = importer.contacts();
        for (int i = 0; i < mContacts.count(); ++i)
            mPeople.append(0);
    } else {
        qWarning() << Q_FUNC_INFO << "Cannot open " << mSource;
    }

    endResetModel();
    if (oldCount != count())
        emit countChanged();
}
