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
#include <QContactSaveRequest>
#include <QContactSyncTarget>

// Versit
#include <QVersitContactExporter>
#include <QVersitContactImporter>
#include <QVersitReader>
#include <QVersitWriter>

// Custom Photo Handler
#include "photohandler.h"

USE_CONTACTS_NAMESPACE
USE_VERSIT_NAMESPACE

class SaveRequestHandler : public QObject
{
    Q_OBJECT
public:
    SaveRequestHandler(QObject *object)
        : QObject(object) { }

    void startRequest(QContactSaveRequest *request)
    {
        this->request = request;
        QTimer::singleShot(0, this, SLOT(internalStartRequest()));
    }

private slots:

    void internalStartRequest()
    {
        connect(request, SIGNAL(stateChanged(QContactAbstractRequest::State)),
                SLOT(onStateChanged(QContactAbstractRequest::State)));
        request->start();
    }

    void onStateChanged(QContactAbstractRequest::State state)
    {
        if (state != QContactAbstractRequest::FinishedState)
            return;

        QContactSaveRequest *request = static_cast<QContactSaveRequest *>(sender());
        qDebug("Wrote %d contacts", request->contacts().count());
        QCoreApplication::instance()->exit();
    }

private:
    QContactSaveRequest *request;
};

void invalidUsage(const QString &app)
{
    qWarning("Usage: %s [-e | --export] <filename>", qPrintable(app));
    ::exit(1);
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
        ::exit(1);
    }

    if (import) {
        PhotoHandler photoHandler;
        QVersitContactImporter importer;
        importer.setPropertyHandler(&photoHandler);

        QVersitReader reader(&vcf);
        reader.startReading();
        reader.waitForFinished();

        importer.importDocuments(reader.results());
        qDebug("Importing %d contacts", importer.contacts().count());

        SaveRequestHandler *rq = new SaveRequestHandler(QCoreApplication::instance());

        QContactSaveRequest *save = new QContactSaveRequest(rq);
        save->setManager(new QContactManager(rq));
        save->setContacts(importer.contacts());

        rq->startRequest(save);
        return qca.exec();
    } else {
        QContactDetailFilter filter;
#ifdef USING_QTPIM
        filter.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
#else
        filter.setDetailDefinitionName(QContactSyncTarget::DefinitionName, QContactSyncTarget::FieldSyncTarget);
#endif
        filter.setValue(QString::fromLatin1("local"));

        QContactManager mgr;
        QList<QContact> localContacts(mgr.contacts(filter));

        QVersitContactExporter exporter;
        exporter.exportContacts(localContacts);
        qDebug("Exporting %d contacts", exporter.documents().count());

        QVersitWriter writer(&vcf);
        writer.startWriting(exporter.documents());
        writer.waitForFinished();
        qDebug("Wrote %d contacts", exporter.documents().count());
        return 0;
    }
}

#include "main.moc"

