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

#include <QtGlobal>

#ifdef QT_VERSION_5
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#define QDeclarativeEngine QQmlEngine
#define QDeclarativeExtensionPlugin QQmlExtensionPlugin
#else
#include <QDeclarativeEngine>
#include <QDeclarativeExtensionPlugin>
#endif

#include "seasideperson.h"
#include "seasidefilteredmodel.h"
#include "seasidenamegroupmodel.h"

class Q_DECL_EXPORT NemoContactsPlugin : public QDeclarativeExtensionPlugin
{
    Q_OBJECT
#ifdef QT_VERSION_5
    Q_PLUGIN_METADATA(IID "org.nemomobile.contacts")
#endif

public:
    virtual ~NemoContactsPlugin() { }

    void initializeEngine(QDeclarativeEngine *, const char *uri)
    {
        Q_ASSERT(uri == QLatin1String("org.nemomobile.contacts"));
    }

    void registerTypes(const char *uri)
    {
        Q_ASSERT(uri == QLatin1String("org.nemomobile.contacts"));

        qmlRegisterType<SeasideFilteredModel>(uri, 1, 0, "PeopleModel");
        qmlRegisterType<SeasideFilteredModel>(uri, 1, 0, "PeopleProxyModel");
        qmlRegisterType<SeasideNameGroupModel>(uri, 1, 0, "PeopleNameGroupModel");
        qmlRegisterType<SeasidePersonAttached>();
        qmlRegisterType<SeasidePerson>(uri, 1, 0, "Person");
    }
};

#ifndef QT_VERSION_5
Q_EXPORT_PLUGIN2(nemocontacts, NemoContactsPlugin);
#endif

#include "plugin.moc"
