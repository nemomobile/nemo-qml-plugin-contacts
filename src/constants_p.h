/*
 * Copyright (C) 2013 Jolla Ltd. <matthew.vogt@jollamobile.com>
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

#ifndef QTCONTACTSSQLITE_CONSTANTS_P_H
#define QTCONTACTSSQLITE_CONSTANTS_P_H

#include <QContactDetail>

#ifdef USING_QTPIM
#include <QContactOnlineAccount>
#include <QContactPhoneNumber>
#include <QContactAvatar>
#include <QContactName>
#endif

BEGIN_CONTACTS_NAMESPACE

#ifdef USING_QTPIM
static const int QContactName__FieldCustomLabel = (QContactName::FieldSuffix+1);

static const int QContactOnlineAccount__FieldAccountPath = (QContactOnlineAccount::FieldSubTypes+1);
static const int QContactOnlineAccount__FieldAccountIconPath = (QContactOnlineAccount::FieldSubTypes+2);
static const int QContactOnlineAccount__FieldEnabled = (QContactOnlineAccount::FieldSubTypes+3);

static const int QContactPhoneNumber__FieldNormalizedNumber = (QContactPhoneNumber::FieldSubTypes+1);

static const int QContactAvatar__FieldAvatarMetadata = (QContactAvatar::FieldVideoUrl+1);

static const int QContactTpMetadata__FieldContactId = 0;
static const int QContactTpMetadata__FieldAccountId = 1;
static const int QContactTpMetadata__FieldAccountEnabled = 2;
#else
// Declared as static:
Q_DECLARE_LATIN1_CONSTANT(QContactOnlineAccount__FieldAccountPath, "AccountPath") = { "AccountPath" };
Q_DECLARE_LATIN1_CONSTANT(QContactOnlineAccount__FieldAccountIconPath, "AccountIconPath") = { "AccountIconPath" };
Q_DECLARE_LATIN1_CONSTANT(QContactOnlineAccount__FieldEnabled, "Enabled") = { "Enabled" };

Q_DECLARE_LATIN1_CONSTANT(QContactPhoneNumber__FieldNormalizedNumber, "NormalizedNumber") = { "NormalizedNumber" };

Q_DECLARE_LATIN1_CONSTANT(QContactAvatar__FieldAvatarMetadata, "AvatarMetadata") = { "AvatarMetadata" };

Q_DECLARE_LATIN1_CONSTANT(QContactTpMetadata__FieldContactId, "ContactId") = { "ContactId" };
Q_DECLARE_LATIN1_CONSTANT(QContactTpMetadata__FieldAccountId, "AccountId") = { "AccountId" };
Q_DECLARE_LATIN1_CONSTANT(QContactTpMetadata__FieldAccountEnabled, "AccountEnabled") = { "AccountEnabled" };
#endif

END_CONTACTS_NAMESPACE

#endif
