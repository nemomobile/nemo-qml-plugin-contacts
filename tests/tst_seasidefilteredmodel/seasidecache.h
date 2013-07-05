#ifndef SEASIDECACHE_H
#define SEASIDECACHE_H

#include <QContact>
#include <QContactId>

#include <QAbstractListModel>
#include <QVector>

USE_CONTACTS_NAMESPACE

class SeasidePerson;

class SeasideCache : public QObject
{
    Q_OBJECT
public:
#ifdef USING_QTPIM
    typedef QContactId ContactIdType;
#else
    typedef QContactLocalId ContactIdType;
#endif

    enum FilterType {
        FilterNone,
        FilterAll,
        FilterFavorites,
        FilterOnline,
        FilterTypesCount
    };

    enum DisplayLabelOrder {
        FirstNameFirst,
        LastNameFirst
    };

    struct CacheItem
    {
        CacheItem() : person(0) {}
        CacheItem(const QContact &contact) : contact(contact), person(0) {}

        QContact contact;
        SeasidePerson *person;
        QStringList filterKey;
    };

    class ListModel : public QAbstractListModel
    {
    public:
        ListModel(QObject *parent = 0) : QAbstractListModel(parent) {}
        virtual ~ListModel() {}

        virtual void sourceAboutToRemoveItems(int begin, int end) = 0;
        virtual void sourceItemsRemoved() = 0;

        virtual void sourceAboutToInsertItems(int begin, int end) = 0;
        virtual void sourceItemsInserted(int begin, int end) = 0;

        virtual void sourceDataChanged(int begin, int end) = 0;

        virtual void makePopulated() = 0;
        virtual void updateDisplayLabelOrder() = 0;
    };

    static ContactIdType apiId(const QContact &contact);
    static ContactIdType apiId(quint32 iid);

    static bool validId(const ContactIdType &id);

    static quint32 internalId(const QContact &contact);
    static quint32 internalId(const QContactId &id);
#ifndef USING_QTPIM
    static quint32 internalId(QContactLocalId id);
#endif
    SeasideCache();
    ~SeasideCache();

    static void registerModel(ListModel *model, FilterType type);
    static void unregisterModel(ListModel *model);

    static void registerUser(QObject *user);
    static void unregisterUser(QObject *user);

    static DisplayLabelOrder displayLabelOrder();

    static int contactId(const QContact &contact);

    static CacheItem *cacheItemById(const ContactIdType &id);
    static SeasidePerson *personById(const ContactIdType &id);
#ifdef USING_QTPIM
    static SeasidePerson *personById(int id);
#endif
    static SeasidePerson *selfPerson();
    static QContact contactById(const ContactIdType &id);
    static QChar nameGroupForCacheItem(CacheItem *cacheItem);
    static QList<QChar> allNameGroups();

    static SeasidePerson *person(CacheItem *item);

    static SeasidePerson *personByPhoneNumber(const QString &msisdn);
    static SeasidePerson *personByEmailAddress(const QString &email);
    static bool savePerson(SeasidePerson *person);
    static void removePerson(SeasidePerson *person);

    static void fetchConstituents(SeasidePerson *person);

    static void fetchMergeCandidates(SeasidePerson *person);

    static const QVector<ContactIdType> *contacts(FilterType filterType);
    static bool isPopulated(FilterType filterType);

    static QString generateDisplayLabel(const QContact &contact, DisplayLabelOrder order = FirstNameFirst);
    static QString generateDisplayLabelFromNonNameDetails(const QContact &contact);

    void populate(FilterType filterType);
    void insert(FilterType filterType, int index, const QVector<ContactIdType> &ids);
    void remove(FilterType filterType, int index, int count);

    static int importContacts(const QString &path);
    static QString exportContacts();

    void setFirstName(FilterType filterType, int index, const QString &name);

    void reset();

    static QVector<ContactIdType> getContactsForFilterType(FilterType filterType);

    QVector<ContactIdType> m_contacts[FilterTypesCount];
    ListModel *m_models[FilterTypesCount];
    bool m_populated[FilterTypesCount];

    QVector<CacheItem> m_cache;
#ifdef USING_QTPIM
    QHash<ContactIdType, int> m_cacheIndices;
#endif

    static SeasideCache *instance;
    static QList<QChar> allContactNameGroups;

    ContactIdType idAt(int index) const;
};


#endif
