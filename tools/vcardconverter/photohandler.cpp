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

#ifdef QT_VERSION_5
#include <QStandardPaths>
#else
#include <QDesktopServices>
#include <QContactThumbnail>
#endif
#include <QContactAvatar>
#include <QUuid>
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

    // we will save the avatar image to disk at some generic data services location.
    QString photoDirPath;

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

    photoDirPath = QDesktopServices::storageLocation(QDesktopServices::HomeLocation)
                  + QLatin1String("/.local/share/data/avatars/");
#else
    photoDirPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#endif

    // create the photo file dir if it doesn't exist.
    QDir photoDir;
    if (!photoDir.mkpath(photoDirPath)) {
        qWarning() << "Failed to create avatar image directory when loading avatar image from vCard PHOTO data";
        return;
    }

    // construct the filename of the new avatar image.
    QString photoFilePath = QUuid::createUuid().toString();
    photoFilePath = photoFilePath.mid(1, photoFilePath.length() - 2) + QLatin1String(".jpg");
    photoFilePath = photoDirPath + photoFilePath;

    // save the file to disk
    QImage img;
    bool loaded = img.loadFromData(property.variantValue().toByteArray());
    if (!loaded) {
        qWarning() << "Failed to load avatar image from vCard PHOTO data";
        return;
    }

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

