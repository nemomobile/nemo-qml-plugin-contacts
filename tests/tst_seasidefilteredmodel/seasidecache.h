#ifndef SEASIDECACHE_H
#define SEASIDECACHE_H

#include <qtcontacts-extensions.h>
#include <QContactStatusFlags>

#include <QContact>
#include <QContactId>

#include <QAbstractListModel>
#include <QVector>

// Provide enough of SeasideCache's interface to support SeasideFilteredModel

USE_CONTACTS_NAMESPACE

class SeasidePerson;

class SeasideCache : public QObject
{
    Q_OBJECT
public:
    typedef QtContactsSqliteExtensions::ApiContactIdType ContactIdType;

    enum FilterType {
        FilterNone,
        FilterAll,
        FilterFavorites,
        FilterOnline,
        FilterTypesCount
    };

    enum FetchDataType {
        FetchNone = 0,
        FetchAccountUri = (1 << 0),
        FetchPhoneNumber = (1 << 1),
        FetchEmailAddress = (1 << 2)
    };

    enum DisplayLabelOrder {
        FirstNameFirst = 0,
        LastNameFirst
    };

    enum ContactState {
        ContactAbsent,
        ContactPartial,
        ContactRequested,
        ContactComplete
    };

    struct ItemData
    {
        virtual ~ItemData() {}

        virtual void displayLabelOrderChanged(DisplayLabelOrder order) = 0;

        virtual void updateContact(const QContact &newContact, QContact *oldContact, ContactState state) = 0;

        virtual void constituentsFetched(const QList<int> &ids) = 0;
        virtual void mergeCandidatesFetched(const QList<int> &ids) = 0;
    };

    struct CacheItem;
    struct ItemListener
    {
        virtual ~ItemListener() {}

        virtual void itemUpdated(CacheItem *) {};
        virtual void itemAboutToBeRemoved(CacheItem *) {};

        ItemListener *next;
    };

    struct CacheItem
    {
        CacheItem() : itemData(0), iid(0), statusFlags(0), contactState(ContactAbsent), listeners(0) {}
        CacheItem(const QContact &contact)
            : contact(contact), itemData(0), iid(internalId(contact)),
              statusFlags(contact.detail<QContactStatusFlags>().flagsValue()), contactState(ContactComplete), listeners(0) {}

        ItemListener *listener(void *) { return 0; }

        ItemListener *appendListener(ItemListener *listener, void *) { listeners = listener; listener->next = 0; return listener; }
        bool removeListener(ItemListener *) { listeners = 0; return true; }

        QContact contact;
        ItemData *itemData;
        quint32 iid;
        quint64 statusFlags;
        ContactState contactState;
        ItemListener *listeners;
        QString nameGroup;
        QString displayLabel;
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

        virtual void sourceItemsChanged() = 0;

        virtual void makePopulated() = 0;
        virtual void updateDisplayLabelOrder() = 0;
        virtual void updateSortProperty() = 0;
        virtual void updateGroupProperty() = 0;
    };

    struct ResolveListener
    {
        virtual ~ResolveListener() {}

        virtual void addressResolved(const QString &, const QString &, CacheItem *item) = 0;
    };

    struct ChangeListener
    {
        virtual ~ChangeListener() {}

        virtual void itemUpdated(CacheItem *item) = 0;
        virtual void itemAboutToBeRemoved(CacheItem *item) = 0;
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

    static void registerModel(ListModel *model, FilterType type, FetchDataType extraData = FetchNone);
    static void unregisterModel(ListModel *model);

    static void registerUser(QObject *user);
    static void unregisterUser(QObject *user);

    static void registerChangeListener(ChangeListener *listener);
    static void unregisterChangeListener(ChangeListener *listener);

    static void unregisterResolveListener(ResolveListener *listener);

    static DisplayLabelOrder displayLabelOrder();
    static QString sortProperty();
    static QString groupProperty();

    static int contactId(const QContact &contact);

    static CacheItem *existingItem(const ContactIdType &id);
    static CacheItem *itemById(const ContactIdType &id, bool requireComplete = true);
#ifdef USING_QTPIM
    static CacheItem *itemById(int id, bool requireComplete = true);
#endif
    static ContactIdType selfContactId();
    static QContact contactById(const ContactIdType &id);
    static QString nameGroup(const CacheItem *cacheItem);
    static QString determineNameGroup(const CacheItem *cacheItem);
    static QStringList allNameGroups();

    static void ensureCompletion(CacheItem *cacheItem);
    static void refreshContact(CacheItem *cacheItem);

    static CacheItem *itemByPhoneNumber(const QString &number, bool requireComplete = true);
    static CacheItem *itemByEmailAddress(const QString &email, bool requireComplete = true);
    static CacheItem *itemByOnlineAccount(const QString &localUid, const QString &remoteUid, bool requireComplete = true);

    static CacheItem *resolvePhoneNumber(ResolveListener *listener, const QString &msisdn, bool requireComplete = true);
    static CacheItem *resolveEmailAddress(ResolveListener *listener, const QString &email, bool requireComplete = true);
    static CacheItem *resolveOnlineAccount(ResolveListener *listener, const QString &localUid, const QString &remoteUid, bool requireComplete = true);

    static bool saveContact(const QContact &contact);
    static void removeContact(const QContact &contact);

    static void aggregateContacts(const QContact &contact1, const QContact &contact2);
    static void disaggregateContacts(const QContact &contact1, const QContact &contact2);

    static void fetchConstituents(const QContact &contact);

    static void fetchMergeCandidates(const QContact &contact);

    static const QVector<ContactIdType> *contacts(FilterType filterType);
    static bool isPopulated(FilterType filterType);

    static QString generateDisplayLabel(const QContact &contact, DisplayLabelOrder order = FirstNameFirst);
    static QString generateDisplayLabelFromNonNameDetails(const QContact &contact);
    static QUrl filteredAvatarUrl(const QContact &contact, const QStringList &metadataFragments = QStringList());

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
    static QStringList allContactNameGroups;

    ContactIdType idAt(int index) const;
};


#endif
