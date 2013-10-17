/*
 * Copyright (C) 2012 Robin Burchell <robin+mer@viroteck.net>
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

// Qt
#include <QCoreApplication>
#include <QFile>
#include <QTimer>

// Contacts
#include <QContactDetailFilter>
#include <QContactFetchHint>
#include <QContactManager>
#include <QContactSortOrder>
#include <QContactSyncTarget>

#include <QContactAddress>
#include <QContactAnniversary>
#include <QContactAvatar>
#include <QContactBirthday>
#include <QContactEmailAddress>
#include <QContactGuid>
#include <QContactHobby>
#include <QContactName>
#include <QContactNickname>
#include <QContactNote>
#include <QContactOnlineAccount>
#include <QContactOrganization>
#include <QContactPhoneNumber>
#include <QContactRingtone>
#include <QContactTag>
#include <QContactUrl>

#ifdef USING_QTPIM
#include <QContactIdFilter>
#include <QContactExtendedDetail>
#else
#include <QContactLocalIdFilter>
#endif

// Versit
#include <QVersitContactExporter>
#include <QVersitContactImporter>
#include <QVersitReader>
#include <QVersitWriter>

// Custom Photo Handler
#include <seasidephotohandler.h>

USE_CONTACTS_NAMESPACE
USE_VERSIT_NAMESPACE

namespace {

void invalidUsage(const QString &app)
{
    qWarning("Usage: %s [-e | --export] <filename>", qPrintable(app));
    ::exit(1);
}

QContactFetchHint basicFetchHint()
{
    QContactFetchHint fetchHint;

    fetchHint.setOptimizationHints(QContactFetchHint::NoRelationships |
                                   QContactFetchHint::NoActionPreferences |
                                   QContactFetchHint::NoBinaryBlobs);

    return fetchHint;
}

QContactFilter localContactFilter()
{
    // Contacts that are local to the device have sync target 'local' or 'was_local'
    QContactDetailFilter filterLocal, filterWasLocal;
#ifdef USING_QTPIM
    filterLocal.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
    filterWasLocal.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
#else
    filterLocal.setDetailDefinitionName(QContactSyncTarget::DefinitionName, QContactSyncTarget::FieldSyncTarget);
    filterWasLocal.setDetailDefinitionName(QContactSyncTarget::DefinitionName, QContactSyncTarget::FieldSyncTarget);
#endif
    filterLocal.setValue(QString::fromLatin1("local"));
    filterWasLocal.setValue(QString::fromLatin1("was_local"));

    return filterLocal | filterWasLocal;
}

QString contactNameString(const QContact &contact)
{
    QStringList details;
    QContactName name(contact.detail<QContactName>());
    details.append(name.prefix());
    details.append(name.firstName());
    details.append(name.middleName());
    details.append(name.lastName());
    details.append(name.suffix());
    return details.join(QChar::fromLatin1('|'));
}


template<typename T, typename F>
QVariant detailValue(const T &detail, F field)
{
#ifdef USING_QTPIM
    return detail.value(field);
#else
    return detail.variantValue(field);
#endif
}

#ifdef USING_QTPIM
typedef QMap<int, QVariant> DetailMap;
#else
typedef QVariantMap DetailMap;
#endif

DetailMap detailValues(const QContactDetail &detail)
{
#ifdef USING_QTPIM
    DetailMap rv(detail.values());
#else
    DetailMap rv(detail.variantValues());
#endif
    return rv;
}

static bool variantEqual(const QVariant &lhs, const QVariant &rhs)
{
#ifdef USING_QTPIM
    // Work around incorrect result from QVariant::operator== when variants contain QList<int>
    static const int QListIntType = QMetaType::type("QList<int>");

    const int lhsType = lhs.userType();
    if (lhsType != rhs.userType()) {
        return false;
    }

    if (lhsType == QListIntType) {
        return (lhs.value<QList<int> >() == rhs.value<QList<int> >());
    }
#endif
    return (lhs == rhs);
}

static bool detailValuesSuperset(const QContactDetail &lhs, const QContactDetail &rhs)
{
    // True if all values in rhs are present in lhs
    const DetailMap lhsValues(detailValues(lhs));
    const DetailMap rhsValues(detailValues(rhs));

    if (lhsValues.count() < rhsValues.count()) {
        return false;
    }

    foreach (const DetailMap::key_type &key, rhsValues.keys()) {
        if (!variantEqual(lhsValues[key], rhsValues[key])) {
            return false;
        }
    }

    return true;
}

// Override for QContactUrl because importer produces incorrectly typed URL field
static bool detailValuesSuperset(const QContactUrl &lhs, const QContactUrl &rhs)
{
    const DetailMap lhsValues(detailValues(lhs));
    const DetailMap rhsValues(detailValues(rhs));

    if (lhsValues.count() < rhsValues.count()) {
        return false;
    }

    foreach (const DetailMap::key_type &key, rhsValues.keys()) {
        if (key == QContactUrl::FieldUrl) {
            QVariant lhsVar(lhsValues[key]);
            QVariant rhsVar(rhsValues[key]);
            QUrl lhsUrl = (lhsVar.type() == QMetaType::QUrl) ? lhsVar.value<QUrl>() : QUrl(lhsVar.value<QString>());
            QUrl rhsUrl = (rhsVar.type() == QMetaType::QUrl) ? rhsVar.value<QUrl>() : QUrl(rhsVar.value<QString>());
            if (lhsUrl != rhsUrl) {
                return false;
            }
        } else {
            if (!variantEqual(lhsValues[key], rhsValues[key])) {
                return false;
            }
        }
    }

    return true;
}

template<typename T>
void updateExistingDetails(QContact *updateContact, const QContact &importedContact, bool singular = false)
{
    QList<T> existingDetails(updateContact->details<T>());
    if (singular && !existingDetails.isEmpty())
        return;

    foreach (T detail, importedContact.details<T>()) {
        // See if the contact already has a detail which is a superset of this one
        bool found = false;
        foreach (const T &existing, existingDetails) {
            if (detailValuesSuperset(existing, detail)) {
                found = true;
                break;
            }
        }
        if (!found) {
            updateContact->saveDetail(&detail);
        }
    }
}

void mergeIntoExistingContact(QContact *updateContact, const QContact &importedContact)
{
    // Update the existing contact with any details in the new import
    updateExistingDetails<QContactAddress>(updateContact, importedContact);
    updateExistingDetails<QContactAnniversary>(updateContact, importedContact);
    updateExistingDetails<QContactAvatar>(updateContact, importedContact);
    updateExistingDetails<QContactBirthday>(updateContact, importedContact, true);
    updateExistingDetails<QContactEmailAddress>(updateContact, importedContact);
    updateExistingDetails<QContactGuid>(updateContact, importedContact);
    updateExistingDetails<QContactHobby>(updateContact, importedContact);
    updateExistingDetails<QContactNickname>(updateContact, importedContact);
    updateExistingDetails<QContactNote>(updateContact, importedContact);
    updateExistingDetails<QContactOnlineAccount>(updateContact, importedContact);
    updateExistingDetails<QContactOrganization>(updateContact, importedContact);
    updateExistingDetails<QContactPhoneNumber>(updateContact, importedContact);
    updateExistingDetails<QContactRingtone>(updateContact, importedContact);
    updateExistingDetails<QContactTag>(updateContact, importedContact);
    updateExistingDetails<QContactUrl>(updateContact, importedContact);
#ifdef USING_QTPIM
    updateExistingDetails<QContactExtendedDetail>(updateContact, importedContact);
#endif
}

void updateExistingContact(QContact *updateContact, const QContact &contact)
{
    // Replace the imported contact with the existing version
    QContact importedContact(*updateContact);
    *updateContact = contact;

    mergeIntoExistingContact(updateContact, importedContact);
}

}

int main(int argc, char **argv)
{
    QCoreApplication qca(argc, argv);

    bool import = true;
    QString filename;

    const QString app(QString::fromLatin1(argv[0]));

    for (int i = 1; i < argc; ++i) {
        const QString arg(QString::fromLatin1(argv[i]));
        if (arg.startsWith('-')) {
            if (!filename.isNull()) {
                invalidUsage(app);
            } else if (arg == QString::fromLatin1("-e") || arg == QString::fromLatin1("--export")) {
                import = false;
            } else {
                qWarning("%s: unknown option: '%s'", qPrintable(app), qPrintable(arg));
                invalidUsage(app);
            }
        } else {
            filename = arg;
        }
    }

    if (filename.isNull()) {
        qWarning("%s: filename must be specified", qPrintable(app));
        invalidUsage(app);
    }

    QFile vcf(filename);
    QIODevice::OpenMode mode(import ? QIODevice::ReadOnly : QIODevice::WriteOnly | QIODevice::Truncate);
    if (!vcf.open(mode)) {
        qWarning("%s: file cannot be opened: '%s'", qPrintable(app), qPrintable(filename));
        ::exit(2);
    }

    QContactManager mgr;

    if (import) {
        SeasidePhotoHandler photoHandler;
        QVersitContactImporter importer;
        importer.setPropertyHandler(&photoHandler);

        // Read the contacts from the VCF
        QVersitReader reader(&vcf);
        reader.startReading();
        reader.waitForFinished();

        importer.importDocuments(reader.results());
        QList<QContact> importedContacts(importer.contacts());

        QHash<QString, QList<QContact>::iterator> importGuids;
        QHash<QString, QList<QContact>::iterator> importNames;

        // Merge any duplicates in the import list
        QList<QContact>::iterator it = importedContacts.begin();
        while (it != importedContacts.end()) {
            const QString guid = (*it).detail<QContactGuid>().guid();
            const QString name = contactNameString((*it));

            QContact *previous = 0;
            QHash<QString, QList<QContact>::iterator>::const_iterator git = importGuids.find(guid);
            if (git != importGuids.end()) {
                QContact &contact(*(git.value()));
                previous = &contact;
            } else {
                QHash<QString, QList<QContact>::iterator>::const_iterator nit = importNames.find(name);
                if (nit != importNames.end()) {
                    QContact &contact(*(nit.value()));
                    previous = &contact;
                }
            }

            if (previous) {
                // Combine these dupliacte contacts
                mergeIntoExistingContact(previous, *it);
                it = importedContacts.erase(it);
            } else {
                if (!guid.isEmpty()) {
                    importGuids.insert(guid, it);
                }
                if (!name.isEmpty()) {
                    importNames.insert(name, it);
                }

                ++it;
            }
        }

        // Find all names and GUIDs for local contacts that might match these contacts
        QContactFetchHint fetchHint(basicFetchHint());
#ifdef USING_QTPIM
        fetchHint.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactName::Type << QContactGuid::Type);
#else
        fetchHint.setDetailDefinitionsHint(QStringList() << QContactName::DefinitionName << QContactGuid::DefinitionName);
#endif

        QHash<QString, QContactId> existingGuids;
        QHash<QString, QContactId> existingNames;

        foreach (const QContact &contact, mgr.contacts(localContactFilter(), QList<QContactSortOrder>(), fetchHint)) {
            const QString guid = contact.detail<QContactGuid>().guid();
            const QString name = contactNameString(contact);

            if (!guid.isEmpty()) {
                existingGuids.insert(guid, contact.id());
            }
            if (!name.isEmpty()) {
                existingNames.insert(name, contact.id());
            }
        }

        // Find any imported contacts that match contacts we already have
        int newCount = 0;
        QMap<QContactId, QList<QContact>::iterator> existingIds;
        QList<QList<QContact>::iterator> duplicates;

        QList<QContact>::iterator end = importedContacts.end();
        for (it = importedContacts.begin(); it != end; ++it) {
            const QString guid = (*it).detail<QContactGuid>().guid();

            QContactId existingId;

            bool existing = true;
            QHash<QString, QContactId>::const_iterator git = existingGuids.find(guid);
            if (git != existingGuids.end()) {
                existingId = *git;
            } else {
                const QString name = contactNameString(*it);

                QHash<QString, QContactId>::const_iterator nit = existingNames.find(name);
                if (nit != existingNames.end()) {
                    existingId = *nit;
                } else {
                    existing = false;
                }
            }

            if (existing) {
                QMap<QContactId, QList<QContact>::iterator>::iterator eit = existingIds.find(existingId);
                if (eit == existingIds.end()) {
                    existingIds.insert(existingId, it);
                } else {
                    // Combine these contacts with matching names
                    QList<QContact>::iterator cit(*eit);
                    mergeIntoExistingContact(&*cit, *it);

                    duplicates.append(it);
                }
            } else {
                ++newCount;
            }
        }

        // Remove any duplicates we identified
        while (!duplicates.isEmpty())
            importedContacts.erase(duplicates.takeLast());

        const int existingCount(existingIds.count());
        if (existingCount > 0) {
            // Retrieve all the contacts that we have matches for
#ifdef USING_QTPIM
            QContactIdFilter idFilter;
            idFilter.setIds(existingIds.keys());
#else
            QContactLocalIdFilter idFilter;
            QList<QContactLocalId> localIds;
            foreach (const QContactId &id, existingIds.keys()) {
                localids.append(id.toLocal());
            }
#endif
            foreach (const QContact &contact, mgr.contacts(idFilter & localContactFilter(), QList<QContactSortOrder>(), basicFetchHint())) {
                QMap<QContactId, QList<QContact>::iterator>::const_iterator it = existingIds.find(contact.id());
                if (it != existingIds.end()) {
                    // Update the existing version of the contact with any new details
                    QList<QContact>::iterator cit(*it);
                    QContact &importContact(*cit);
                    updateExistingContact(&importContact, contact);
                } else {
                    qWarning() << "unable to update existing contact:" << contact.id();
                }
            }
        }

        QString existingDesc(existingCount ? QString::fromLatin1(" (%1 already existing)").arg(existingCount) : QString());
        qDebug("Importing %d new contacts%s", newCount, qPrintable(existingDesc));

        QMap<int, QContactManager::Error> errors;
        mgr.saveContacts(&importedContacts, &errors);

        int importedCount = importedContacts.count();
        QMap<int, QContactManager::Error>::const_iterator eit = errors.constBegin(), eend = errors.constEnd();
        for ( ; eit != eend; ++eit) {
            const QContact &failed(importedContacts.at(eit.key()));
            qDebug() << "  Unable to import contact" << failed.detail<QContactDisplayLabel>().label() << "error:" << eit.value();
            --importedCount;
        }
        qDebug("Wrote %d contacts", importedCount);
    } else {
        QList<QContact> localContacts(mgr.contacts(localContactFilter()));

        QVersitContactExporter exporter;
        exporter.exportContacts(localContacts);
        qDebug("Exporting %d contacts", exporter.documents().count());

        QVersitWriter writer(&vcf);
        writer.startWriting(exporter.documents());
        writer.waitForFinished();
        qDebug("Wrote %d contacts", exporter.documents().count());
    }

    return 0;
}

