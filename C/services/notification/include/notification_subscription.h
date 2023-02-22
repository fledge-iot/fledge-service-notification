#ifndef _NOTIFICATION_SUBSCRIPTION_H
#define _NOTIFICATION_SUBSCRIPTION_H
/*
 * Fledge notification subscription manager.
 *
 * Copyright (c) 2018 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */

#include <logger.h>
#include <management_client.h>
#include <storage_client.h>
#include <notification_manager.h>

/**
 * The SubscriptionElement class handles the notification registration to
 * storage server based on asset name and its notification name.
 */
class SubscriptionElement
{
	public:
		SubscriptionElement(const std::string& notificationName,
				    NotificationInstance* notification);

		virtual ~SubscriptionElement();

		const std::string&	getNotificationName() const { return m_name; };
		NotificationRule*	getRule()
					{
						if (m_notification)
							return m_notification->getRule();
						else
							return NULL;
					};
		NotificationDelivery*	getDelivery()
					{
						if (m_notification)
							return m_notification->getDelivery();
						else
							return NULL;
					};
		NotificationInstance*	getInstance() { return m_notification; };
		virtual bool		registerSubscription(StorageClient& storage) const = 0;
		virtual bool		unregister(StorageClient& storage) const = 0;
		virtual string		getKey() const = 0;

	protected:
		std::string		m_name;
		NotificationInstance*	m_notification;
};

/**
 * The SubscriptionElement class handles the notification registration to
 * storage server based on asset name and its notification name.
 */
class AssetSubscriptionElement : public SubscriptionElement
{
	public:
		AssetSubscriptionElement(const std::string& assetName,
				    const std::string& notificationName,
				    NotificationInstance* notification);

		~AssetSubscriptionElement();

		std::string	getAssetName() const { return m_asset; };
		bool		registerSubscription(StorageClient& storage) const;
		bool		unregister(StorageClient& storage) const;
		string		getKey() const { return string("asset::" + m_asset); };
	private:
		std::string	m_asset;
};

/**
 * The SubscriptionElement class handles the notification registration to
 * storage server based on audit code and its notification name.
 */
class AuditSubscriptionElement : public SubscriptionElement
{
	public:
		AuditSubscriptionElement(const std::string& code,
				    const std::string& notificationName,
				    NotificationInstance* notification);

		~AuditSubscriptionElement();

		std::string	getAuditCode() const { return m_code; };
		bool		registerSubscription(StorageClient& storage) const;
		bool		unregister(StorageClient& storage) const;
		string		getKey() const { return string("audit::" + m_code); };
	private:
		std::string	m_code;
};

/**
 * The SubscriptionElement class handles the notification registration to
 * storage server based on statisitic valuesand its notification name.
 */
class StatsSubscriptionElement : public SubscriptionElement
{
	public:
		StatsSubscriptionElement(const std::string& stat,
				    const std::string& notificationName,
				    NotificationInstance* notification);

		~StatsSubscriptionElement();

		std::string	getStatistic() const { return m_stat; };
		bool		registerSubscription(StorageClient& storage) const;
		bool		unregister(StorageClient& storage) const;
		string		getKey() const { return string("stat::" + m_stat); };
	private:
		std::string	m_stat;
};

/**
 * The SubscriptionElement class handles the notification registration to
 * storage server based on statisitic rate values and its notification name.
 */
class StatsRateSubscriptionElement : public SubscriptionElement
{
	public:
		StatsRateSubscriptionElement(const std::string& stat,
				    const std::string& notificationName,
				    NotificationInstance* notification);

		~StatsRateSubscriptionElement();

		std::string	getStatistic() const { return m_stat; };
		bool		registerSubscription(StorageClient& storage) const;
		bool		unregister(StorageClient& storage) const;
		string		getKey() const { return string("rate::" + m_stat); };
	private:
		std::string	m_stat;
};

/**
 * The NotificationSubscription class handles all notification registrations to
 * storage server.
 * Registrations are done per asset name and one asset name might have different
 * notification rules.
 */
class NotificationSubscription
{
	public:
		NotificationSubscription(const std::string& notificationName,
					 StorageClient& storageClient);
		~NotificationSubscription();

		static	NotificationSubscription*
					getInstance() { return m_instance; };
		void			registerSubscriptions();
		void			unregisterSubscriptions();
		const std::string&	getNotificationName() { return m_name; };
		std::map<std::string, std::vector<SubscriptionElement *>>&
					getAllSubscriptions() { return m_subscriptions; };
		std::vector<SubscriptionElement *>&
					getSubscription(const std::string& assetName)
					{
						return m_subscriptions[assetName];
					};
		bool 			addSubscription(SubscriptionElement *element);
		void			unregisterSubscription(SubscriptionElement *element);
		bool			createSubscription(NotificationInstance* instance);
		void			lockSubscriptions() { m_subscriptionMutex.lock(); };
		void			unlockSubscriptions() { m_subscriptionMutex.unlock(); };
		void			removeSubscription(const string& source,
							   const string& assetName,
							   const string& ruleName);

	private:
		EvaluationType		getEvalType(const Value& value);

	private:
		const std::string	m_name;
		static NotificationSubscription*
					m_instance;
		StorageClient&		m_storage;
		// There can be different subscriptions for the same assetName
		std::map<std::string, std::vector<SubscriptionElement *>>
					m_subscriptions;
		Logger*			m_logger;
		std::mutex		m_subscriptionMutex;
};

#endif
