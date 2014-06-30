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

#include <seasideimport.h>
#include <seasideexport.h>

// Qt
#include <QCoreApplication>
#include <QFile>
#include <QTimer>

// Contacts
#include <QContactDetailFilter>
#include <QContactManager>
#include <QContactSyncTarget>

// Versit
#include <QVersitReader>
#include <QVersitWriter>

QTCONTACTS_USE_NAMESPACE
QTVERSIT_USE_NAMESPACE

namespace {

void invalidUsage(const QString &app)
{
    qWarning("Usage: %s [-e | --export] <filename>", qPrintable(app));
    ::exit(1);
}

QContactFilter localContactFilter()
{
    // Contacts that are local to the device have sync target 'local' or 'was_local'
    QContactDetailFilter filterLocal, filterWasLocal, filterBluetooth;
    filterLocal.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
    filterWasLocal.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
    filterBluetooth.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
    filterLocal.setValue(QString::fromLatin1("local"));
    filterWasLocal.setValue(QString::fromLatin1("was_local"));
    filterBluetooth.setValue(QString::fromLatin1("bluetooth"));

    return filterLocal | filterWasLocal | filterBluetooth;
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
        // Read the contacts from the VCF
        QVersitReader reader(&vcf);
        reader.startReading();
        reader.waitForFinished();

        // Get the import list which duplicates coalesced, and updates merged
        int newCount;
        int updatedCount;
        QList<QContact> importedContacts(SeasideImport::buildImportContacts(reader.results(), &newCount, &updatedCount));

        QString existingDesc(updatedCount ? QString::fromLatin1(" (updating %1 existing)").arg(updatedCount) : QString());
        qDebug("Importing %d new contacts%s", newCount, qPrintable(existingDesc));

        int importedCount = 0;

        while (!importedContacts.isEmpty()) {
            QMap<int, QContactManager::Error> errors;
            mgr.saveContacts(&importedContacts, &errors);
            importedCount += (importedContacts.count() - errors.count());

            QList<QContact> retryContacts;
            QMap<int, QContactManager::Error>::const_iterator eit = errors.constBegin(), eend = errors.constEnd();
            for ( ; eit != eend; ++eit) {
                const QContact &failed(importedContacts.at(eit.key()));
                if (eit.value() == QContactManager::LockedError) {
                    // This contact was part of a failed batch - we should retry
                    retryContacts.append(failed);
                } else {
                    qDebug() << "  Unable to import contact" << failed.detail<QContactDisplayLabel>().label() << "error:" << eit.value();
                }
            }

            importedContacts = retryContacts;
        }
        qDebug("Wrote %d contacts", importedCount);
    } else {
        QList<QContact> localContacts(mgr.contacts(localContactFilter()));

        QList<QVersitDocument> documents(SeasideExport::buildExportContacts(localContacts));
        qDebug("Exporting %d contacts", documents.count());

        QVersitWriter writer(&vcf);
        writer.startWriting(documents);
        writer.waitForFinished();
        qDebug("Wrote %d contacts", documents.count());
    }

    return 0;
}

