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
#include <QContactSaveRequest>

// Versit
#include <QVersitReader>
#include <QVersitContactImporter>

// Custom Photo Handler
#include "photohandler.h"

USE_CONTACTS_NAMESPACE
USE_VERSIT_NAMESPACE

class RequestHandler : public QObject
{
    Q_OBJECT
public:
    RequestHandler(QObject *object)
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
        qDebug("Saved %d contacts", request->contacts().count());
        QCoreApplication::instance()->exit();
    }

private:
    QContactSaveRequest *request;
};

int main(int argc, char **argv)
{
    QCoreApplication qca(argc, argv);
    QContactSaveRequest req;

    PhotoHandler photoHandler;
    QVersitContactImporter importer;
    importer.setPropertyHandler(&photoHandler);

    for (int i = 1; i < argc; ++i) {
        QFile vcf(argv[i]);
        if (!vcf.open(QIODevice::ReadOnly)) {
            qWarning("vcardconverter: %s cannot be opened", argv[i]);
            exit(1);
        }

        QVersitReader reader(&vcf);
        reader.startReading();
        reader.waitForFinished();

       importer.importDocuments(reader.results());
    }

    qDebug("Saving %d contacts", importer.contacts().count());

    req.setManager(new QContactManager(&req));
    req.setContacts(importer.contacts());
    req.start();

    RequestHandler *rq = new RequestHandler(QCoreApplication::instance());
    rq->startRequest(&req);

    return qca.exec();
}

#include "main.moc"

