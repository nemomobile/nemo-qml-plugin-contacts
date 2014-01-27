/*
 * Copyright (C) 2012 Jolla Mobile <robin.burchell@jollamobile.com>
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

#include <QObject>
#include <QtTest>

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
#include <QContactOrganization>
#include <QContactUrl>
#ifdef USING_QTPIM
#include <QContactManager>
#endif

#include "seasideperson.h"

#include <algorithm>

USE_CONTACTS_NAMESPACE

class tst_SeasidePerson : public QObject
{
    Q_OBJECT

public:
    // Ensure that the cache has been initialized
    tst_SeasidePerson() : m_nameGroups(SeasideCache::allNameGroups()) {}

private slots:
    void firstName();
    void lastName();
    void middleName();
    void displayLabel();
    void sectionBucket();
    void companyName();
    void title();
    void favorite();
    void avatarPath();
    void nicknameDetails();
    void phoneDetails();
    void emailDetails();
    void websiteDetails();
    void accountDetails();
    void birthday();
    void anniversaryDetails();
    void addressDetails();
    void globalPresenceState();
    void complete();
    void marshalling();
    void setContact();
    void vcard();
    void syncTarget();
    void constituents();
    void removeDuplicatePhoneNumbers();
    void removeDuplicateOnlineAccounts();

private:
    QStringList m_nameGroups;
};

void tst_SeasidePerson::firstName()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    QCOMPARE(person->firstName(), QString());
    QSignalSpy spy(person.data(), SIGNAL(firstNameChanged()));
    person->setFirstName("Test");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(person->firstName(), QString::fromLatin1("Test"));
    QCOMPARE(person->property("firstName").toString(), person->firstName());
}

void tst_SeasidePerson::lastName()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    QCOMPARE(person->lastName(), QString());
    QSignalSpy spy(person.data(), SIGNAL(lastNameChanged()));
    person->setLastName("Test");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(person->lastName(), QString::fromLatin1("Test"));
    QCOMPARE(person->property("lastName").toString(), person->lastName());
}

void tst_SeasidePerson::middleName()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    QCOMPARE(person->middleName(), QString());
    QSignalSpy spy(person.data(), SIGNAL(middleNameChanged()));
    person->setMiddleName("Test");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(person->middleName(), QString::fromLatin1("Test"));
    QCOMPARE(person->property("middleName").toString(), person->middleName());
}

void tst_SeasidePerson::displayLabel()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    QSignalSpy spy(person.data(), SIGNAL(displayLabelChanged()));
    person->setFirstName("Test");
    person->setLastName("Last");
    QCOMPARE(spy.count(), 2); // TODO: it would be nicer if this would only emit once per event loop
    QCOMPARE(person->firstName(), QString::fromLatin1("Test"));
    QCOMPARE(person->lastName(), QString::fromLatin1("Last"));
    QCOMPARE(person->displayLabel(), QString::fromLatin1("Test Last"));

    // TODO: test additional cases for displayLabel:
    // - email/IM id
    // - company
    // - other fallbacks?
    // - "(unnamed)"
}

void tst_SeasidePerson::sectionBucket()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    QSignalSpy spy(person.data(), SIGNAL(displayLabelChanged()));
    QCOMPARE(person->displayLabel(), QString("(Unnamed)"));
    QCOMPARE(spy.count(), 0);
    QCOMPARE(person->sectionBucket(), QString());

    // set first
    person->setLastName("Test");
    QCOMPARE(person->displayLabel(), QString::fromLatin1("Test"));
    QCOMPARE(person->sectionBucket(), QString::fromLatin1("T"));
    QCOMPARE(spy.count(), 1);

    // change first
    person->setFirstName("Another");
    const bool firstNameFirst(SeasideCache::displayLabelOrder() == SeasideCache::FirstNameFirst);
    if (firstNameFirst) {
        QCOMPARE(person->displayLabel(), QString::fromLatin1("Another Test"));
        QCOMPARE(person->sectionBucket(), QString::fromLatin1("A"));
    } else {
        QCOMPARE(person->displayLabel(), QString::fromLatin1("Test Another"));
        QCOMPARE(person->sectionBucket(), QString::fromLatin1("T"));
    }
    QCOMPARE(spy.count(), 2);
}

void tst_SeasidePerson::companyName()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    QCOMPARE(person->companyName(), QString());
    QSignalSpy spy(person.data(), SIGNAL(companyNameChanged()));
    person->setCompanyName("Test");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(person->companyName(), QString::fromLatin1("Test"));
    QCOMPARE(person->property("companyName").toString(), person->companyName());
}

void tst_SeasidePerson::title()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    QCOMPARE(person->title(), QString());
    QSignalSpy spy(person.data(), SIGNAL(titleChanged()));
    person->setTitle("Test");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(person->title(), QString::fromLatin1("Test"));
    QCOMPARE(person->property("title").toString(), person->title());
}

void tst_SeasidePerson::favorite()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    QVERIFY(!person->favorite());
    QSignalSpy spy(person.data(), SIGNAL(favoriteChanged()));
    person->setFavorite(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(person->property("favorite").toBool(), person->favorite());
    QVERIFY(person->favorite());
}

void tst_SeasidePerson::avatarPath()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    QCOMPARE(person->avatarPath(), QUrl("image://theme/icon-m-telephony-contact-avatar")); // TODO: should this not really belong in QML level instead of API?
    QSignalSpy spy(person.data(), SIGNAL(avatarPathChanged()));
    person->setAvatarPath(QUrl("http://test.com"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(person->avatarPath(), QUrl("http://test.com"));
    QCOMPARE(person->property("avatarPath").toUrl(), person->avatarPath());
}

QVariantMap makeNickname(const QString &nick, int label = SeasidePerson::NoLabel)
{
    QVariantMap rv;

    rv.insert(QString::fromLatin1("nickname"), nick);
    rv.insert(QString::fromLatin1("type"), static_cast<int>(SeasidePerson::NicknameType));
    rv.insert(QString::fromLatin1("label"), label);
    rv.insert(QString::fromLatin1("index"), -1);

    return rv;
}

void tst_SeasidePerson::nicknameDetails()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);

    QCOMPARE(person->nicknameDetails(), QVariantList());

    QStringList nicks(QStringList() << "Ace" << "Blocker" << "Chopper");

    QSignalSpy spy(person.data(), SIGNAL(nicknameDetailsChanged()));

    QVariantList nicknameDetails;
    for (int i = 0; i < 2; ++i) {
        nicknameDetails.append(makeNickname(nicks.at(i)));
    }

    person->setNicknameDetails(nicknameDetails);
    QCOMPARE(spy.count(), 1);

    QList<SeasidePerson::DetailLabel> nicknameLabels;
    nicknameLabels.append(SeasidePerson::HomeLabel);
    nicknameLabels.append(SeasidePerson::WorkLabel);
    nicknameLabels.append(SeasidePerson::OtherLabel);

    QVariantList nicknames = person->nicknameDetails();
    QCOMPARE(nicknames.count(), 2);

    for (int i=0; i<nicknames.count(); i++) {
        QVariant &var(nicknames[i]);
        QVariantMap nickname(var.value<QVariantMap>());
        QCOMPARE(nickname.value(QString::fromLatin1("nickname")).toString(), nicks.at(i));
        QCOMPARE(nickname.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::NicknameType));
        QCOMPARE(nickname.value(QString::fromLatin1("label")), QVariant());

        // Modify the label of this detail
        nickname.insert(QString::fromLatin1("label"), static_cast<int>(nicknameLabels.at(i)));
        var = nickname;
    }

    // Add another to the list
    nicknames.append(makeNickname(nicks.at(2), nicknameLabels.at(2)));

    person->setNicknameDetails(nicknames);
    QCOMPARE(spy.count(), 2);

    nicknames = person->nicknameDetails();
    QCOMPARE(nicknames.count(), 3);

    for (int i=0; i<nicknames.count(); i++) {
        QVariant &var(nicknames[i]);
        QVariantMap nickname(var.value<QVariantMap>());
        QCOMPARE(nickname.value(QString::fromLatin1("nickname")).toString(), nicks.at(i));
        QCOMPARE(nickname.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::NicknameType));
        QCOMPARE(nickname.value(QString::fromLatin1("label")).toInt(), static_cast<int>(nicknameLabels.at(i)));
    }

    // Remove all but the middle
    person->setNicknameDetails(nicknames.mid(1, 1));
    QCOMPARE(spy.count(), 3);

    nicknames = person->nicknameDetails();
    QCOMPARE(nicknames.count(), 1);
    {
        QVariant &var(nicknames[0]);
        QVariantMap nickname(var.value<QVariantMap>());
        QCOMPARE(nickname.value(QString::fromLatin1("nickname")).toString(), nicks.at(1));
        QCOMPARE(nickname.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::NicknameType));
        QCOMPARE(nickname.value(QString::fromLatin1("label")).toInt(), static_cast<int>(nicknameLabels.at(1)));
    }
}

QVariantMap makePhoneNumber(const QString &number, int label = SeasidePerson::NoLabel, int subType = SeasidePerson::NoSubType)
{
    QVariantMap rv;

    rv.insert(QString::fromLatin1("number"), number);
    rv.insert(QString::fromLatin1("type"), static_cast<int>(SeasidePerson::PhoneNumberType));
    rv.insert(QString::fromLatin1("subTypes"), QVariantList() << subType);
    rv.insert(QString::fromLatin1("label"), label);
    rv.insert(QString::fromLatin1("index"), -1);

    return rv;
}

void tst_SeasidePerson::phoneDetails()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);

    QCOMPARE(person->phoneDetails(), QVariantList());

    QStringList phoneNumbers(QStringList() << "111" << "222"<< "333"<< "444"<< "555");

    QSignalSpy spy(person.data(), SIGNAL(phoneDetailsChanged()));

    QVariantList phoneDetails;
    for (int i = 0; i < 4; ++i) {
        phoneDetails.append(makePhoneNumber(phoneNumbers.at(i)));
    }

    person->setPhoneDetails(phoneDetails);
    QCOMPARE(spy.count(), 1);

    QList<SeasidePerson::DetailSubType> phoneSubTypes;
    phoneSubTypes.append(SeasidePerson::PhoneSubTypeLandline);
    phoneSubTypes.append(SeasidePerson::PhoneSubTypeMobile);
    phoneSubTypes.append(SeasidePerson::PhoneSubTypeFax);
    phoneSubTypes.append(SeasidePerson::PhoneSubTypePager);
    phoneSubTypes.append(SeasidePerson::PhoneSubTypeVoice);

    QVariantList numbers = person->phoneDetails();
    QCOMPARE(numbers.count(), 4);

    for (int i=0; i<numbers.count(); i++) {
        QVariant &var(numbers[i]);
        QVariantMap number(var.value<QVariantMap>());
        QCOMPARE(number.value(QString::fromLatin1("number")).toString(), phoneNumbers.at(i));
        QCOMPARE(number.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::PhoneNumberType));
        QCOMPARE(number.value(QString::fromLatin1("subTypes")).value<QVariantList>(), QVariantList());
        QCOMPARE(number.value(QString::fromLatin1("label")), QVariant());

        // Modify the subType of this detail
        number.insert(QString::fromLatin1("subTypes"), QVariantList() << static_cast<int>(phoneSubTypes.at(i)));

        // Modify the label also
        number.insert(QString::fromLatin1("label"), static_cast<int>(SeasidePerson::OtherLabel));

        var = number;
    }

    // Add another to the list
    numbers.append(makePhoneNumber(phoneNumbers.at(4), SeasidePerson::OtherLabel, phoneSubTypes.at(4)));

    person->setPhoneDetails(numbers);
    QCOMPARE(spy.count(), 2);

    numbers = person->phoneDetails();
    QCOMPARE(numbers.count(), 5);

    for (int i=0; i<numbers.count(); i++) {
        QVariant &var(numbers[i]);
        QVariantMap number(var.value<QVariantMap>());
        QCOMPARE(number.value(QString::fromLatin1("number")).toString(), phoneNumbers.at(i));
        QCOMPARE(number.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::PhoneNumberType));
        QCOMPARE(number.value(QString::fromLatin1("subTypes")).value<QVariantList>(), QVariantList() << static_cast<int>(phoneSubTypes.at(i)));
        QCOMPARE(number.value(QString::fromLatin1("label")).toInt(), static_cast<int>(SeasidePerson::OtherLabel));

        // Now remove the label
        number.remove(QString::fromLatin1("label"));
        var = number;
    }

    // Remove all but the middle
    person->setPhoneDetails(numbers.mid(2, 1));
    QCOMPARE(spy.count(), 3);

    numbers = person->phoneDetails();
    QCOMPARE(numbers.count(), 1);
    {
        QVariant &var(numbers[0]);
        QVariantMap number(var.value<QVariantMap>());
        QCOMPARE(number.value(QString::fromLatin1("number")).toString(), phoneNumbers.at(2));
        QCOMPARE(number.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::PhoneNumberType));
        QCOMPARE(number.value(QString::fromLatin1("subTypes")).value<QVariantList>(), QVariantList() << static_cast<int>(phoneSubTypes.at(2)));
        QCOMPARE(number.value(QString::fromLatin1("label")), QVariant());
    }
}

QVariantMap makeEmailAddress(const QString &address, int label = SeasidePerson::NoLabel)
{
    QVariantMap rv;

    rv.insert(QString::fromLatin1("address"), address);
    rv.insert(QString::fromLatin1("type"), static_cast<int>(SeasidePerson::EmailAddressType));
    rv.insert(QString::fromLatin1("label"), label);
    rv.insert(QString::fromLatin1("index"), -1);

    return rv;
}

void tst_SeasidePerson::emailDetails()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);

    QCOMPARE(person->emailDetails(), QVariantList());

    QStringList emailAddresses(QStringList() << "foo@bar" << "bar@foo"<< "foo@baz");

    QSignalSpy spy(person.data(), SIGNAL(emailDetailsChanged()));

    QVariantList emailDetails;
    for (int i = 0; i < 2; ++i) {
        emailDetails.append(makeEmailAddress(emailAddresses.at(i)));
    }

    person->setEmailDetails(emailDetails);
    QCOMPARE(spy.count(), 1);

    QList<SeasidePerson::DetailLabel> emailLabels;
    emailLabels.append(SeasidePerson::HomeLabel);
    emailLabels.append(SeasidePerson::WorkLabel);
    emailLabels.append(SeasidePerson::OtherLabel);

    QVariantList addresses = person->emailDetails();
    QCOMPARE(addresses.count(), 2);

    for (int i=0; i<addresses.count(); i++) {
        QVariant &var(addresses[i]);
        QVariantMap address(var.value<QVariantMap>());
        QCOMPARE(address.value(QString::fromLatin1("address")).toString(), emailAddresses.at(i));
        QCOMPARE(address.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::EmailAddressType));
        QCOMPARE(address.value(QString::fromLatin1("label")), QVariant());

        // Modify the label of this detail
        address.insert(QString::fromLatin1("label"), static_cast<int>(emailLabels.at(i)));
        var = address;
    }

    // Add another to the list
    addresses.append(makeEmailAddress(emailAddresses.at(2), emailLabels.at(2)));

    person->setEmailDetails(addresses);
    QCOMPARE(spy.count(), 2);

    addresses = person->emailDetails();
    QCOMPARE(addresses.count(), 3);

    for (int i=0; i<addresses.count(); i++) {
        QVariant &var(addresses[i]);
        QVariantMap address(var.value<QVariantMap>());
        QCOMPARE(address.value(QString::fromLatin1("address")).toString(), emailAddresses.at(i));
        QCOMPARE(address.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::EmailAddressType));
        QCOMPARE(address.value(QString::fromLatin1("label")).toInt(), static_cast<int>(emailLabels.at(i)));
    }

    // Remove all but the middle
    person->setEmailDetails(addresses.mid(1, 1));
    QCOMPARE(spy.count(), 3);

    addresses = person->emailDetails();
    QCOMPARE(addresses.count(), 1);
    {
        QVariant &var(addresses[0]);
        QVariantMap address(var.value<QVariantMap>());
        QCOMPARE(address.value(QString::fromLatin1("address")).toString(), emailAddresses.at(1));
        QCOMPARE(address.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::EmailAddressType));
        QCOMPARE(address.value(QString::fromLatin1("label")).toInt(), static_cast<int>(emailLabels.at(1)));
    }
}

QVariantMap makeWebsite(const QString &url, int label = SeasidePerson::NoLabel)
{
    QVariantMap rv;

    rv.insert(QString::fromLatin1("url"), url);
    rv.insert(QString::fromLatin1("type"), static_cast<int>(SeasidePerson::WebsiteType));
    rv.insert(QString::fromLatin1("label"), label);
    rv.insert(QString::fromLatin1("index"), -1);

    return rv;
}

void tst_SeasidePerson::websiteDetails()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);

    QCOMPARE(person->websiteDetails(), QVariantList());

    QStringList urls(QStringList() << "www.example.com" << "www.test.com" << "www.foobar.com");

    QSignalSpy spy(person.data(), SIGNAL(websiteDetailsChanged()));

    QVariantList websiteDetails;
    for (int i = 0; i < 2; ++i) {
        websiteDetails.append(makeWebsite(urls.at(i)));
    }

    person->setWebsiteDetails(websiteDetails);
    QCOMPARE(spy.count(), 1);

    QList<SeasidePerson::DetailLabel> websiteLabels;
    websiteLabels.append(SeasidePerson::HomeLabel);
    websiteLabels.append(SeasidePerson::WorkLabel);
    websiteLabels.append(SeasidePerson::OtherLabel);

    QVariantList websites = person->websiteDetails();
    QCOMPARE(websites.count(), 2);

    for (int i=0; i<websites.count(); i++) {
        QVariant &var(websites[i]);
        QVariantMap website(var.value<QVariantMap>());
        QCOMPARE(website.value(QString::fromLatin1("url")).toString(), urls.at(i));
        QCOMPARE(website.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::WebsiteType));
        QCOMPARE(website.value(QString::fromLatin1("label")), QVariant());

        // Modify the label of this detail
        website.insert(QString::fromLatin1("label"), static_cast<int>(websiteLabels.at(i)));
        var = website;
    }

    // Add another to the list
    websites.append(makeWebsite(urls.at(2), websiteLabels.at(2)));

    person->setWebsiteDetails(websites);
    QCOMPARE(spy.count(), 2);

    websites = person->websiteDetails();
    QCOMPARE(websites.count(), 3);

    for (int i=0; i<websites.count(); i++) {
        QVariant &var(websites[i]);
        QVariantMap website(var.value<QVariantMap>());
        QCOMPARE(website.value(QString::fromLatin1("url")).toString(), urls.at(i));
        QCOMPARE(website.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::WebsiteType));
        QCOMPARE(website.value(QString::fromLatin1("label")).toInt(), static_cast<int>(websiteLabels.at(i)));
    }

    // Remove all but the middle
    person->setWebsiteDetails(websites.mid(1, 1));
    QCOMPARE(spy.count(), 3);

    websites = person->websiteDetails();
    QCOMPARE(websites.count(), 1);
    {
        QVariant &var(websites[0]);
        QVariantMap website(var.value<QVariantMap>());
        QCOMPARE(website.value(QString::fromLatin1("url")).toString(), urls.at(1));
        QCOMPARE(website.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::WebsiteType));
        QCOMPARE(website.value(QString::fromLatin1("label")).toInt(), static_cast<int>(websiteLabels.at(1)));
    }
}

QVariantMap makeAccount(const QString &uri, int label = SeasidePerson::NoLabel)
{
    QVariantMap rv;

    rv.insert(QString::fromLatin1("accountUri"), uri);
    rv.insert(QString::fromLatin1("type"), static_cast<int>(SeasidePerson::OnlineAccountType));
    rv.insert(QString::fromLatin1("label"), label);
    rv.insert(QString::fromLatin1("index"), -1);

    return rv;
}

void tst_SeasidePerson::accountDetails()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);

    QCOMPARE(person->accountDetails(), QVariantList());

    QStringList uris(QStringList() << "fred@foo.org" << "fred@bar.com" << "123456789");

    QSignalSpy spy(person.data(), SIGNAL(accountDetailsChanged()));

    QVariantList accountDetails;
    for (int i = 0; i < 2; ++i) {
        accountDetails.append(makeAccount(uris.at(i)));
    }

    person->setAccountDetails(accountDetails);
    QCOMPARE(spy.count(), 1);

    QList<SeasidePerson::DetailLabel> accountLabels;
    accountLabels.append(SeasidePerson::HomeLabel);
    accountLabels.append(SeasidePerson::WorkLabel);
    accountLabels.append(SeasidePerson::OtherLabel);

    QVariantList accounts = person->accountDetails();
    QCOMPARE(accounts.count(), 2);

    for (int i=0; i<accounts.count(); i++) {
        QVariant &var(accounts[i]);
        QVariantMap account(var.value<QVariantMap>());
        QCOMPARE(account.value(QString::fromLatin1("accountUri")).toString(), uris.at(i));
        QCOMPARE(account.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::OnlineAccountType));
        QCOMPARE(account.value(QString::fromLatin1("label")), QVariant());

        // Modify the label of this detail
        account.insert(QString::fromLatin1("label"), static_cast<int>(accountLabels.at(i)));
        var = account;
    }

    // Add another to the list
    accounts.append(makeAccount(uris.at(2), accountLabels.at(2)));

    person->setAccountDetails(accounts);
    QCOMPARE(spy.count(), 2);

    accounts = person->accountDetails();
    QCOMPARE(accounts.count(), 3);

    for (int i=0; i<accounts.count(); i++) {
        QVariant &var(accounts[i]);
        QVariantMap account(var.value<QVariantMap>());
        QCOMPARE(account.value(QString::fromLatin1("accountUri")).toString(), uris.at(i));
        QCOMPARE(account.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::OnlineAccountType));
        QCOMPARE(account.value(QString::fromLatin1("label")).toInt(), static_cast<int>(accountLabels.at(i)));
    }

    // Remove all but the middle
    person->setAccountDetails(accounts.mid(1, 1));
    QCOMPARE(spy.count(), 3);

    accounts = person->accountDetails();
    QCOMPARE(accounts.count(), 1);
    {
        QVariant &var(accounts[0]);
        QVariantMap account(var.value<QVariantMap>());
        QCOMPARE(account.value(QString::fromLatin1("accountUri")).toString(), uris.at(1));
        QCOMPARE(account.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::OnlineAccountType));
        QCOMPARE(account.value(QString::fromLatin1("label")).toInt(), static_cast<int>(accountLabels.at(1)));
    }
}

void tst_SeasidePerson::birthday()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    QCOMPARE(person->birthday(), QDateTime());
    QSignalSpy spy(person.data(), SIGNAL(birthdayChanged()));
    person->setBirthday(QDateTime::fromString("05/01/1980 15:00:00.000", "dd/MM/yyyy hh:mm:ss.zzz"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(person->birthday(), QDateTime::fromString("05/01/1980 15:00:00.000", "dd/MM/yyyy hh:mm:ss.zzz"));
    QCOMPARE(person->property("birthday").toDateTime(), person->birthday());
    person->resetBirthday();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(person->birthday(), QDateTime());
    QCOMPARE(person->property("birthday").toDateTime(), person->birthday());
}

QVariantMap makeAnniversary(const QDateTime &timestamp, int label = SeasidePerson::NoLabel, int subType = SeasidePerson::NoSubType)
{
    QVariantMap rv;

    rv.insert(QString::fromLatin1("originalDate"), timestamp);
    rv.insert(QString::fromLatin1("type"), static_cast<int>(SeasidePerson::AnniversaryType));
    rv.insert(QString::fromLatin1("subType"), subType);
    rv.insert(QString::fromLatin1("label"), label);
    rv.insert(QString::fromLatin1("index"), -1);

    return rv;
}

void tst_SeasidePerson::anniversaryDetails()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);

    QCOMPARE(person->anniversaryDetails(), QVariantList());

    QList<QDateTime> dates(QList<QDateTime>() << QDateTime::fromString("05/01/1980 15:00:00.000", "dd/MM/yyyy hh:mm:ss.zzz")
                                              << QDateTime::fromString("05/01/1980 23:59:59.999", "dd/MM/yyyy hh:mm:ss.zzz")
                                              << QDateTime::fromString("06/01/1980 00:00:00.000", "dd/MM/yyyy hh:mm:ss.zzz"));

    QSignalSpy spy(person.data(), SIGNAL(anniversaryDetailsChanged()));

    QVariantList anniversaryDetails;
    for (int i = 0; i < 2; ++i) {
        anniversaryDetails.append(makeAnniversary(dates.at(i)));
    }

    person->setAnniversaryDetails(anniversaryDetails);
    QCOMPARE(spy.count(), 1);

    QList<SeasidePerson::DetailLabel> anniversaryLabels;
    anniversaryLabels.append(SeasidePerson::HomeLabel);
    anniversaryLabels.append(SeasidePerson::WorkLabel);
    anniversaryLabels.append(SeasidePerson::OtherLabel);

    QList<SeasidePerson::DetailSubType> anniversarySubTypes;
    anniversarySubTypes.append(SeasidePerson::AnniversarySubTypeEngagement);
    anniversarySubTypes.append(SeasidePerson::AnniversarySubTypeHouse);
    anniversarySubTypes.append(SeasidePerson::AnniversarySubTypeEmployment);

    QVariantList anniversaries = person->anniversaryDetails();
    QCOMPARE(anniversaries.count(), 2);

    for (int i=0; i<anniversaries.count(); i++) {
        QVariant &var(anniversaries[i]);
        QVariantMap anniversary(var.value<QVariantMap>());
        QCOMPARE(anniversary.value(QString::fromLatin1("originalDate")).value<QDateTime>(), dates.at(i));
        QCOMPARE(anniversary.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::AnniversaryType));
        QCOMPARE(anniversary.value(QString::fromLatin1("subType")).toInt(), static_cast<int>(SeasidePerson::AnniversarySubTypeWedding));
        QCOMPARE(anniversary.value(QString::fromLatin1("label")), QVariant());

        // Modify the subType of this detail
        anniversary.insert(QString::fromLatin1("subType"), static_cast<int>(anniversarySubTypes.at(i)));

        // Modify the label of this detail
        anniversary.insert(QString::fromLatin1("label"), static_cast<int>(anniversaryLabels.at(i)));
        var = anniversary;
    }

    // Add another to the list
    anniversaries.append(makeAnniversary(dates.at(2), anniversaryLabels.at(2), anniversarySubTypes.at(2)));

    person->setAnniversaryDetails(anniversaries);
    QCOMPARE(spy.count(), 2);

    anniversaries = person->anniversaryDetails();
    QCOMPARE(anniversaries.count(), 3);

    for (int i=0; i<anniversaries.count(); i++) {
        QVariant &var(anniversaries[i]);
        QVariantMap anniversary(var.value<QVariantMap>());
        QCOMPARE(anniversary.value(QString::fromLatin1("originalDate")).value<QDateTime>(), dates.at(i));
        QCOMPARE(anniversary.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::AnniversaryType));
        QCOMPARE(anniversary.value(QString::fromLatin1("subType")).toInt(), static_cast<int>(anniversarySubTypes.at(i)));
        QCOMPARE(anniversary.value(QString::fromLatin1("label")).toInt(), static_cast<int>(anniversaryLabels.at(i)));
    }

    // Remove all but the middle
    person->setAnniversaryDetails(anniversaries.mid(1, 1));
    QCOMPARE(spy.count(), 3);

    anniversaries = person->anniversaryDetails();
    QCOMPARE(anniversaries.count(), 1);
    {
        QVariant &var(anniversaries[0]);
        QVariantMap anniversary(var.value<QVariantMap>());
        QCOMPARE(anniversary.value(QString::fromLatin1("originalDate")).value<QDateTime>(), dates.at(1));
        QCOMPARE(anniversary.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::AnniversaryType));
        QCOMPARE(anniversary.value(QString::fromLatin1("subType")).toInt(), static_cast<int>(anniversarySubTypes.at(1)));
        QCOMPARE(anniversary.value(QString::fromLatin1("label")).toInt(), static_cast<int>(anniversaryLabels.at(1)));
    }
}

QVariantMap makeAddress(const QString &address, int label = SeasidePerson::NoLabel)
{
    QVariantMap rv;

    rv.insert(QString::fromLatin1("address"), address);
    rv.insert(QString::fromLatin1("type"), static_cast<int>(SeasidePerson::AddressType));
    rv.insert(QString::fromLatin1("label"), label);
    rv.insert(QString::fromLatin1("index"), -1);

    return rv;
}

void tst_SeasidePerson::addressDetails()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);

    QCOMPARE(person->addressDetails(), QVariantList());

    QStringList addressStrings(QStringList() << "Street 1\nLocality 1\nRegion 1\nPostcode 1\nCountry 1\nPoBox 1"
                                             << "Street 2\nLocality 2\nRegion 2\nPostcode 2\nCountry 2\nPoBox 2"
                                             << "Street 3\nLocality 3\nRegion 3\nPostcode 3\nCountry 3\nPoBox 3");

    QSignalSpy spy(person.data(), SIGNAL(addressDetailsChanged()));

    QVariantList addressDetails;
    for (int i = 0; i < 2; ++i) {
        addressDetails.append(makeAddress(addressStrings.at(i)));
    }

    person->setAddressDetails(addressDetails);
    QCOMPARE(spy.count(), 1);

    QList<SeasidePerson::DetailLabel> addressLabels;
    addressLabels.append(SeasidePerson::HomeLabel);
    addressLabels.append(SeasidePerson::WorkLabel);
    addressLabels.append(SeasidePerson::OtherLabel);


    QVariantList addresses = person->addressDetails();
    QCOMPARE(addresses.count(), 2);

    for (int i=0; i<addresses.count(); i++) {
        QVariant &var(addresses[i]);
        QVariantMap address(var.value<QVariantMap>());
        QCOMPARE(address.value(QString::fromLatin1("address")).toString(), addressStrings.at(i));
        QCOMPARE(address.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::AddressType));
        QCOMPARE(address.value(QString::fromLatin1("label")), QVariant());

        // Modify the label of this detail
        address.insert(QString::fromLatin1("label"), static_cast<int>(addressLabels.at(i)));
        var = address;
    }

    // Add another to the list
    addresses.append(makeAddress(addressStrings.at(2), addressLabels.at(2)));

    person->setAddressDetails(addresses);
    QCOMPARE(spy.count(), 2);

    addresses = person->addressDetails();
    QCOMPARE(addresses.count(), 3);

    for (int i=0; i<addresses.count(); i++) {
        QVariant &var(addresses[i]);
        QVariantMap address(var.value<QVariantMap>());
        QCOMPARE(address.value(QString::fromLatin1("address")).toString(), addressStrings.at(i));
        QCOMPARE(address.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::AddressType));
        QCOMPARE(address.value(QString::fromLatin1("label")).toInt(), static_cast<int>(addressLabels.at(i)));
    }

    // Remove all but the middle
    person->setAddressDetails(addresses.mid(1, 1));
    QCOMPARE(spy.count(), 3);

    addresses = person->addressDetails();
    QCOMPARE(addresses.count(), 1);
    {
        QVariant &var(addresses[0]);
        QVariantMap address(var.value<QVariantMap>());
        QCOMPARE(address.value(QString::fromLatin1("address")).toString(), addressStrings.at(1));
        QCOMPARE(address.value(QString::fromLatin1("type")).toInt(), static_cast<int>(SeasidePerson::AddressType));
        QCOMPARE(address.value(QString::fromLatin1("label")).toInt(), static_cast<int>(addressLabels.at(1)));
    }
}

void tst_SeasidePerson::globalPresenceState()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    QCOMPARE(person->globalPresenceState(), SeasidePerson::PresenceUnknown);

    // We can't test any changes, as the change is effected by a DBUS request...
}

void tst_SeasidePerson::complete()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    // An empty person is always complete
    QCOMPARE(person->isComplete(), true);

    QSignalSpy spy(person.data(), SIGNAL(completeChanged()));
    person->setComplete(false);
    person->setComplete(true);
    QCOMPARE(spy.count(), 2);
}

void tst_SeasidePerson::marshalling()
{
    QContact contact;

    {
        QContactName nameDetail;
        nameDetail.setFirstName("Hello");
        nameDetail.setLastName("World");
        contact.saveDetail(&nameDetail);
    }

    {
        QContactFavorite favorite;
        favorite.setFavorite(true);
        contact.saveDetail(&favorite);
    }

    QScopedPointer<SeasidePerson> person(new SeasidePerson(contact));
    QCOMPARE(person->firstName(), QString::fromLatin1("Hello"));
    QCOMPARE(person->lastName(), QString::fromLatin1("World"));
    QCOMPARE(person->displayLabel(), QString::fromLatin1("Hello World"));
    QVERIFY(person->favorite());

    QCOMPARE(person->contact(), contact);
}

void tst_SeasidePerson::setContact()
{
#ifdef USING_QTPIM
    // The contact manager must have previously been loaded for this test to succeed
    QContactManager cm("org.nemomobile.contacts.sqlite");
#endif

    QScopedPointer<SeasidePerson> person(new SeasidePerson);

    QContact contact;

    {   // ### contactChanged is only emitted if the id differs, not any of the members.
#ifdef USING_QTPIM
        QString idStr(QString::fromLatin1("qtcontacts:org.nemomobile.contacts.sqlite::sql-5"));
        contact.setId(QContactId::fromString(idStr));
#else
        QContactId contactId;
        contactId.setLocalId(5);
        contact.setId(contactId);
#endif
    }

    {
        QContactName nameDetail;
        nameDetail.setFirstName("Hello");
        nameDetail.setLastName("World");
        contact.saveDetail(&nameDetail);
    }

    {
        QSignalSpy spy(person.data(), SIGNAL(firstNameChanged()));
        QSignalSpy spyTwo(person.data(), SIGNAL(lastNameChanged()));
        QSignalSpy spyThree(person.data(), SIGNAL(displayLabelChanged()));
        QSignalSpy spyFour(person.data(), SIGNAL(contactChanged()));
        person->setContact(contact);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spyTwo.count(), 1);
        QCOMPARE(spyThree.count(), 1);
        QCOMPARE(spyFour.count(), 1);

        // change them again, nothing should emit
        person->setContact(contact);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spyTwo.count(), 1);
        QCOMPARE(spyThree.count(), 1);
        QCOMPARE(spyFour.count(), 1);
    }
}

void tst_SeasidePerson::vcard()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);
    person->setFirstName("Star");
    person->setLastName("Fish");

    person->setPhoneDetails(QVariantList() << makePhoneNumber("12345678"));

    QString vcard = person->vCard();

    QVERIFY(vcard.indexOf("N:Fish;Star;;;") > 0);
    QVERIFY(vcard.indexOf("TEL:12345678") > 0);

    // If we were exporting version 3.0, FN would also be expected
    //QVERIFY(vcard.indexOf("FN:Star Fish") > 0);
}

void tst_SeasidePerson::syncTarget()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);

    // Until stored to the contacts manager, a person has no syncTarget
    QCOMPARE(person->syncTarget(), QString());
}

void tst_SeasidePerson::constituents()
{
    QScopedPointer<SeasidePerson> person(new SeasidePerson);

    // No constituents in this data
    QCOMPARE(person->constituents(), QList<int>());
}

template<int N>
void prepare(int (&indices)[N]) {
    for (int i = 0; i < N; ++i)
        indices[i] = i;
}

template<int N>
bool permute(int (&indices)[N]) {
    return std::next_permutation(indices, indices + N);
}

template<int N, typename T>
QVariantList reorder(const int (&indices)[N], const T &list)
{
    QVariantList rv;
    for (int i = 0; i < N; ++i) {
        rv.append(list[indices[i]]);
    }
    return rv;
}

void tst_SeasidePerson::removeDuplicatePhoneNumbers()
{
    const QString phoneDetailType(QString::fromLatin1("type"));
    const QString phoneDetailNumber(QString::fromLatin1("number"));
    const QString phoneDetailNormalizedNumber(QString::fromLatin1("normalizedNumber"));
    const QString phoneDetailMinimizedNumber(QString::fromLatin1("minimizedNumber"));

    QList<QVariantMap> numbers;

    QStringList numberStrings;
    numberStrings << QString::fromLatin1("12345678")          // simple
                  << QString::fromLatin1("(12) 345-678")      // punctuation
                  << QString::fromLatin1("00112345678")       // extended
                  << QString::fromLatin1("+0112345678")       // initial plus
                  << QString::fromLatin1("+00112345678")      // longer initial plus
                  << QString::fromLatin1("+0011-2345678");    // longer initial plus with punctuation

    // The last form is the form that we expect to be selected for display
    const int preferredFormIndex = numberStrings.count() - 1;

    numberStrings << QString::fromLatin1("0000112345678")     // longer extended
                  << QString::fromLatin1("12345012345678")    // super extended
                  << QString::fromLatin1("(12345)012345678"); // super extended with punctuation

    // The last form is the extended option that should also be selected
    const int preferredExtendedFormIndex = numberStrings.count() - 1;

    foreach (const QString &num, numberStrings) {
        QVariantMap number;
        number.insert(phoneDetailType, SeasidePerson::PhoneNumberType);
        number.insert(phoneDetailNumber, num);
        number.insert(phoneDetailNormalizedNumber, SeasideCache::normalizePhoneNumber(num));
        number.insert(phoneDetailMinimizedNumber, SeasideCache::minimizePhoneNumber(num));
        numbers.append(number);
    }

    // Whatever order the numbers are inserted in, the two returned numbers should be the same
    QSet<QString> expectedSet;
    expectedSet.insert(numbers[preferredFormIndex].value(phoneDetailNumber).toString());
    expectedSet.insert(numbers[preferredExtendedFormIndex].value(phoneDetailNumber).toString());

    SeasidePerson person;

    // Test all possible orderings of the input set
    int indices[9];
    for (prepare(indices); permute(indices); ) {
        QVariantList numberList(reorder(indices, numbers));
        QVariantList deduplicated(person.removeDuplicatePhoneNumbers(numberList));

        QStringList resultList;
        foreach (const QVariant &item, deduplicated) {
            const QVariantMap detail(item.value<QVariantMap>());
            resultList.append(detail.value(phoneDetailNumber).toString());
        }
        QCOMPARE(resultList.toSet(), expectedSet);
    }
}

void tst_SeasidePerson::removeDuplicateOnlineAccounts()
{
    const QString accountDetailUri(QString::fromLatin1("accountUri"));
    const QString accountDetailPath(QString::fromLatin1("accountPath"));

    QList<QVariantMap> accounts;

    QList<QPair<QString, QString> > accountDetails;
    accountDetails << qMakePair(QString::fromLatin1("Fred"), QString())
                   << qMakePair(QString::fromLatin1("Fred"), QString())
                   << qMakePair(QString::fromLatin1("Fred"), QString::fromLatin1("provider1"))
                   << qMakePair(QString::fromLatin1("fred"), QString::fromLatin1("provider1"))
                   << qMakePair(QString::fromLatin1("fred"), QString::fromLatin1("provider2"))
                   << qMakePair(QString::fromLatin1("barney"), QString());

    QList<QPair<QString, QString> >::const_iterator it = accountDetails.constBegin(), end = accountDetails.constEnd();
    for ( ; it != end; ++it) {
        QVariantMap account;
        account.insert(accountDetailUri, (*it).first);
        account.insert(accountDetailPath, (*it).second);
        accounts.append(account);
    }

    // Whatever order the accounts are inserted in, the returned accounts should be the same (except URI case)
    QSet<QPair<QString, QString> > expected;
    expected.insert(qMakePair(QString::fromLatin1("fred"), QString::fromLatin1("provider1")));
    expected.insert(qMakePair(QString::fromLatin1("fred"), QString::fromLatin1("provider2")));
    expected.insert(qMakePair(QString::fromLatin1("barney"), QString()));

    SeasidePerson person;

    // Test all possible orderings of the input set
    int indices[6];
    for (prepare(indices); permute(indices); ) {
        QVariantList accountList(reorder(indices, accounts));
        QVariantList deduplicated(person.removeDuplicateOnlineAccounts(accountList));

        QList<QPair<QString, QString> > resultList;
        foreach (const QVariant &item, deduplicated) {
            const QVariantMap detail(item.value<QVariantMap>());
            const QString uri(detail.value(accountDetailUri).toString().toLower());
            const QString path(detail.value(accountDetailPath).toString());
            resultList.append(qMakePair(uri, path));
        }
        QCOMPARE(resultList.toSet(), expected);
    }
}

// TODO:
// - account URIs/paths (or let contactsd do that?)

#include "tst_seasideperson.moc"
QTEST_APPLESS_MAIN(tst_SeasidePerson)
