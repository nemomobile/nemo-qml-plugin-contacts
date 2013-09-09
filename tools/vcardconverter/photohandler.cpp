/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Chris Adams <chris.adams@jollamobile.com>
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
 *   * Neither the name of Nemo Mobile nor Jolla Ltd nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "photohandler.h"

#ifndef QT_VERSION_5
#include <QDesktopServices>
#include <QContactThumbnail>
#endif
#include <QContactAvatar>
#include <QCryptographicHash>
#include <QDir>
#include <QImage>

USE_CONTACTS_NAMESPACE

PhotoHandler::PhotoHandler()
{
}

PhotoHandler::~PhotoHandler()
{
}

void PhotoHandler::documentProcessed(const QVersitDocument &, QContact *)
{
    // do nothing, have no state to clean.
}

void PhotoHandler::propertyProcessed(const QVersitDocument &, const QVersitProperty &property, const QContact &, bool *alreadyProcessed, QList<QContactDetail> * updatedDetails)
{
    // if the property is a PHOTO property, store the data to disk
    // and then create an avatar detail which points to it.
    if (property.name().toLower() != QLatin1String("photo"))
        return;

#ifndef QT_VERSION_5
    // The Qt4 / QtMobility version has QContactThumbnail support.
    // We need to remove any such thumbnail detail from the output,
    // as some backends (such as qtcontacts-sqlite) do not support
    // that detail type.
    for (int i = 0; i < updatedDetails->size(); ++i) {
        if (updatedDetails->at(i).definitionName() == QContactThumbnail::DefinitionName) {
            updatedDetails->removeAt(i);
            --i;
        }
    }
#endif

    // The data might be either a URL, a file path, or encoded image data
    bool encodedData = false;
    foreach (const QString &parameter, property.parameters().keys()) {
        // If there is an 'encoding=' or 'type=' parameter, assume encoded data
        if ((parameter.compare(QString::fromLatin1("encoding"), Qt::CaseInsensitive) == 0) ||
            (parameter.compare(QString::fromLatin1("type"), Qt::CaseInsensitive) == 0)) {
            encodedData = true;
            break;
        }
    }

    QUrl url;

    if (!encodedData) {
        // Assume the data is a URL
        QString path(property.variantValue().toString());
        url = QUrl(path);

        // Treat remote URL as a true URL; local file should be copied into our cache
        if (url.isValid() && !url.scheme().isEmpty() && !url.isLocalFile()) {
            QContactAvatar newAvatar;
            newAvatar.setImageUrl(url);
            updatedDetails->append(newAvatar);

            // we have successfully processed this PHOTO property.
            *alreadyProcessed = true;
            return;
        } else {
            // See if we can resolve the data as a local file
            url = QUrl::fromLocalFile(path);
        }
    }

    QByteArray photoData;

    if (!encodedData) {
        QFile file(url.toLocalFile());
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Unable to process photo data as file:" << property.variantValue().toString();
            return;
        } else {
            photoData = file.readAll();
        }
    } else {
        photoData = property.variantValue().toByteArray();
    }

    if (photoData.isEmpty()) {
        qWarning() << "Failed to extract avatar data from vCard PHOTO property";
        return;
    }

    QImage img;
    bool loaded = img.loadFromData(photoData);
    if (!loaded) {
        qWarning() << "Failed to load avatar image from vCard PHOTO data";
        return;
    }

    // We will save the avatar image to disk in the system's data location
    // Since we're importing user data, it should not require privileged access
    const QString subdirectory(QString::fromLatin1(".local/share/system/Contacts/avatars"));
    const QString photoDirPath(QDir::home().filePath(subdirectory));

    // create the photo file dir if it doesn't exist.
    QDir photoDir;
    if (!photoDir.mkpath(photoDirPath)) {
        qWarning() << "Failed to create avatar image directory when loading avatar image from vCard PHOTO data";
        return;
    }

    // construct the filename of the new avatar image.
    QString photoFilePath = QString::fromLatin1(QCryptographicHash::hash(photoData, QCryptographicHash::Md5).toHex());
    photoFilePath = photoDirPath + QDir::separator() + photoFilePath + QString::fromLatin1(".jpg");

    // save the file to disk
    bool saved = img.save(photoFilePath);
    if (!saved) {
        qWarning() << "Failed to save avatar image from vCard PHOTO data to" << photoFilePath;
        return;
    }

    qWarning() << "Successfully saved avatar image from vCard PHOTO data to" << photoFilePath;

    // save the avatar detail - TODO: mark the avatar as "owned by the contact" (remove on delete)
    QContactAvatar newAvatar;
    newAvatar.setImageUrl(QUrl::fromLocalFile(photoFilePath));
    updatedDetails->append(newAvatar);

    // we have successfully processed this PHOTO property.
    *alreadyProcessed = true;
}

