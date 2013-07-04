#ifndef SEASIDECACHE_H
#define SEASIDECACHE_H

#include <QContact>

#include "seasidefilteredmodel.h"

struct SeasideCacheItem
{
    SeasideCacheItem() : person(0) {}
    SeasideCacheItem(const QContact &contact) : contact(contact), person(0) {}

    QContact contact;
    SeasidePerson *person;
    QStringList filterKey;
};

class SeasideCache : public QObject
{
    Q_OBJECT
public:
    typedef SeasideFilteredModel::ContactIdType ContactIdType;

    SeasideCache();
    ~SeasideCache();

    static void registerModel(SeasideFilteredModel *model, SeasideFilteredModel::FilterType type);
    static void unregisterModel(SeasideFilteredModel *model);

    static void registerUser(QObject *user);
    static void unregisterUser(QObject *user);

    static SeasideFilteredModel::DisplayLabelOrder displayLabelOrder();

    static int contactId(const QContact &contact);

    static SeasideCacheItem *cacheItemById(const ContactIdType &id);
    static SeasidePerson *personById(const ContactIdType &id);
#ifdef USING_QTPIM
    static SeasidePerson *personById(int id);
#endif
    static SeasidePerson *selfPerson();
    static QContact contactById(const ContactIdType &id);
    static QChar nameGroupForCacheItem(SeasideCacheItem *cacheItem);
    static QList<QChar> allNameGroups();

    static SeasidePerson *person(SeasideCacheItem *item);

    static SeasidePerson *personByPhoneNumber(const QString &msisdn);
    static SeasidePerson *personByEmailAddress(const QString &email);
    static bool savePerson(SeasidePerson *person);
    static void removePerson(SeasidePerson *person);

    static void fetchConstituents(SeasidePerson *person);

    static void fetchMergeCandidates(SeasidePerson *person);

    static const QVector<ContactIdType> *contacts(SeasideFilteredModel::FilterType filterType);
    static bool isPopulated(SeasideFilteredModel::FilterType filterType);

    void populate(SeasideFilteredModel::FilterType filterType);
    void insert(SeasideFilteredModel::FilterType filterType, int index, const QVector<ContactIdType> &ids);
    void remove(SeasideFilteredModel::FilterType filterType, int index, int count);

    static int importContacts(const QString &path);
    static QString exportContacts();

    void setDisplayName(SeasideFilteredModel::FilterType filterType, int index, const QString &name);

    void reset();

    static QVector<ContactIdType> getContactsForFilterType(SeasideFilteredModel::FilterType filterType);

    QVector<ContactIdType> m_contacts[SeasideFilteredModel::FilterTypesCount];
    SeasideFilteredModel *m_models[SeasideFilteredModel::FilterTypesCount];
    bool m_populated[SeasideFilteredModel::FilterTypesCount];

    QVector<SeasideCacheItem> m_cache;
#ifdef USING_QTPIM
    QHash<ContactIdType, int> m_cacheIndices;
#endif

    static SeasideCache *instance;
    static QList<QChar> allContactNameGroups;

    ContactIdType idAt(int index) const;
};


#endif
