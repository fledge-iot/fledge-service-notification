#ifndef _NOTIFICATION_SERVICE_H
#define _NOTIFICATION_SERVICE_H
/*
 * Fledge notification service.
 *
 * Copyright (c) 2018 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */

#include <service_handler.h>
#include <management_client.h>
#include <management_api.h>
#include <notification_api.h>
#include <reading.h>
#include <storage_client.h>
#include <logger.h>
#include <asset_tracking.h>
#include <unordered_set>

#define CATEGORY_DELIVERY_PREFIX "delivery"
#define CATEGORY_DELIVERY_EXTRA  "_channel_"

#define SERVICE_NAME		"Fledge Notification"
#define SERVICE_TYPE		"Notification"
#define DEFAULT_DELIVERY_WORKER_THREADS 2
/**
 * The NotificationService class.
 */
class NotificationService : public ServiceAuthHandler
{
	public:
		NotificationService(const std::string& name,
				const std::string& token = "");
		~NotificationService();
		const std::string&	getName() { return m_name; };
		bool 			start(std::string& coreAddress,
					      unsigned short corePort);
		void 			stop(bool removeFromCore=true);
		void			shutdown();
		void			restart();
		bool			isRunning() { return !m_shutdown; };
		void			cleanupResources();
		void			configChange(const std::string&,const std::string&);
		void			configChildCreate(const std::string& parent_category,
							const std::string&,
							const std::string&);
		void			configChildDelete(const std::string& parent_category,
							const std::string&);

		void			registerCategory(const std::string& categoryName);
		void   			registerCategoryChild(const std::string& categoryName);
		void   			updateAssetTrackerCache(AssetTrackingTuple tuple)
						{
							std::string track = tuple.m_assetName + tuple.m_eventName + tuple.m_serviceName + tuple.m_pluginName;
							m_AssetTrackerCache.insert(track);
						}

		void			ingestReading(Reading& reading, const std::string& notificationInstanceName, const std::string& notificationDeliveryPluginName)
					{
						m_storage->readingAppend(reading);
						//Do Asset Tracking
						const std::string serviceInstance = notificationInstanceName; // Notification Service Instance
						const std::string plugin = notificationDeliveryPluginName; // Delivery Plugin Name
						const std::string asset = reading.getAssetName();
						const std::string event = "Notify";
						const bool deprecated = false;
						
						AssetTrackingTuple tuple (serviceInstance,plugin,asset,event,deprecated);
						std::string track = asset + event + serviceInstance + plugin;
						
						if (m_AssetTrackerCache.find(track) == m_AssetTrackerCache.end())
						{
							m_logger->debug("Adding AssetTracker tuple for Notificatiion %s: %s:Ingest, deprecated state is %d",serviceInstance.c_str(),asset.c_str(),(deprecated ? 1:0));
							m_assetTracker->addAssetTrackingTuple(tuple);
							m_AssetTrackerCache.insert(track);
						}

					};
		StorageClient*		getStorageClient() { return m_storage; };
		void			setDryRun() { m_dryRun = true; };
		bool			sendToDispatcher(const string& path,
							const string& payload);

	private:
		Logger*			m_logger;
		bool			m_shutdown;
		NotificationApi*	m_api;
		ManagementApi*		m_managementApi;
		StorageClient*		m_storage;
		std::map<std::string, bool>
					m_registerCategories;
		std::map<std::string, bool>
					m_registerCategoriesChild;

		unsigned long		m_delivery_threads;
		const std::string	m_token;
		std::string			m_notificationInstanceName;;
		std::string			m_notificationDeliveryPluginName;;
		bool			m_dryRun;
		bool			m_restartRequest;
		bool			m_removeFromCore;
		AssetTracker*	m_assetTracker;
		std::unordered_set<std::string> m_AssetTrackerCache;
};
#endif
