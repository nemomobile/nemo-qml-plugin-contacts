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

    struct ItemData
    {
        virtual ~ItemData() {}

        virtual QString getDisplayLabel() const;
        virtual void displayLabelOrderChanged(DisplayLabelOrder order) = 0;

        virtual void updateContact(const QContact &newContact, QContact *oldContact) = 0;

        virtual void constituentsFetched(const QList<int> &ids) = 0;
        virtual void mergeCandidatesFetched(const QList<int> &ids) = 0;
    };

    struct ModelData
    {
        virtual ~ModelData() {}

        virtual void contactChanged(const QContact &) = 0;
    };

    struct CacheItem
    {
        CacheItem() : itemData(0), modelData(0) {}
        CacheItem(const QContact &contact) : contact(contact), itemData(0), modelData(0) {}

        QContact contact;
        ItemData *itemData;
        ModelData *modelData;
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

    static SeasideCache *instance();

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

    static CacheItem *existingItem(const ContactIdType &id);
    static CacheItem *itemById(const ContactIdType &id);
#ifdef USING_QTPIM
    static CacheItem *itemById(int id);
#endif
    static ContactIdType selfContactId();
    static QContact contactById(const ContactIdType &id);
    static QChar nameGroupForCacheItem(CacheItem *cacheItem);
    static QList<QChar> allNameGroups();

    static CacheItem *itemByPhoneNumber(const QString &msisdn);
    static CacheItem *itemByEmailAddress(const QString &email);
    static bool saveContact(const QContact &contact);
    static void removeContact(const QContact &contact);

    static void fetchConstituents(const QContact &contact);

    static void fetchMergeCandidates(const QContact &contact);

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

    static SeasideCache *instancePtr;
    static QList<QChar> allContactNameGroups;

    ContactIdType idAt(int index) const;
};


#endif
