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
#include <QContactGuid>
#include <QContactManager>
#include <QContactSortOrder>
#include <QContactSyncTarget>

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

QContactDetailFilter localContactFilter()
{
    QContactDetailFilter filter;
#ifdef USING_QTPIM
    filter.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
#else
    filter.setDetailDefinitionName(QContactSyncTarget::DefinitionName, QContactSyncTarget::FieldSyncTarget);
#endif
    filter.setValue(QString::fromLatin1("local"));
    return filter;
}

QContactDetailFilter guidFilter()
{
    QContactDetailFilter filter;
#ifdef USING_QTPIM
    filter.setDetailType(QContactGuid::Type, QContactGuid::FieldGuid);
#else
    filter.setDetailDefinitionName(QContactGuid::DefinitionName, QContactGuid::FieldGuid);
#endif
    return filter;
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

        QVersitReader reader(&vcf);
        reader.startReading();
        reader.waitForFinished();

        importer.importDocuments(reader.results());
        QList<QContact> importedContacts(importer.contacts());

        // Find all GUIDs for local contacts
        QContactFetchHint fetchHint;
#ifdef USING_QTPIM
        fetchHint.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactGuid::Type);
#else
        fetchHint.setDetailDefinitionsHint(QStringList() << QContactGuid::DefinitionName);
#endif

        QSet<QString> existingGuids;
        QContactFilter filter(localContactFilter() & guidFilter());
        foreach (const QContact &contact, mgr.contacts(filter, QList<QContactSortOrder>(), fetchHint)) {
            existingGuids.insert(contact.detail<QContactGuid>().guid());
        }

        int existingCount = 0;

        // Ignore any contacts we already have (by GUID)
        QList<QContact>::iterator it = importedContacts.begin();
        while (it != importedContacts.end()) {
            const QString guid = (*it).detail<QContactGuid>().guid();
            if (!guid.isEmpty() && existingGuids.contains(guid)) {
                it = importedContacts.erase(it);
                ++existingCount;
            } else {
                ++it;
            }
        }

        QString existingDesc(existingCount ? QString::fromLatin1(" (%1 already existing)").arg(existingCount) : QString());
        qDebug("Importing %d contacts%s", importedContacts.count(), qPrintable(existingDesc));

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

