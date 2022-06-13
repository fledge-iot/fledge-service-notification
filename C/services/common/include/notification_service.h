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

#define CATEGORY_DELIVERY_PREFIX "delivery"
#define CATEGORY_DELIVERY_EXTRA  "_channel_"

#define SERVICE_NAME		"Fledge Notification"
#define SERVICE_TYPE		"Notification"
#define DEFAULT_DELIVERY_WORKER_THREADS 2
/**
 * The NotificationService class.
 */
class NotificationService : public ServiceHandler
{
	public:
		NotificationService(const std::string& name,
				const std::string& token = "");
		~NotificationService();
		bool 			start(std::string& coreAddress,
					      unsigned short corePort);
		void 			stop();
		void			shutdown();
		bool			isRunning() { return !m_shutdown; };
		void			cleanupResources();
		void			configChange(const std::string&,const std::string&);
		void			configChildCreate(const std::string& parent_category,const std::string&, const std::string&);
		void			configChildDelete(const std::string& parent_category,const std::string&);

		void			registerCategory(const std::string& categoryName);
		void   			registerCategoryChild(const std::string& categoryName);

		void			ingestReading(Reading& reading)
					{
						m_storage->readingAppend(reading);
					};
		ManagementClient*	getManagementClient() { return m_managerClient; };
		StorageClient*		getStorageClient() { return m_storage; };
		void			configChildCreate(const std::string& parent_category,
							const std::string&,
							const std::string&) {};
		void			configChildDelete(const std::string& parent_category,
							const std::string&) {};

	private:
		const std::string	m_name;
		Logger*			m_logger;
		bool			m_shutdown;
		NotificationApi*	m_api;
		ManagementClient* 	m_managerClient;
		ManagementApi*		m_managementApi;
		StorageClient*		m_storage;
		std::map<std::string, bool>
					m_registerCategories;
		std::map<std::string, bool>
					m_registerCategoriesChild;

		unsigned long		m_delivery_threads;
		const std::string	m_token;
};
#endif
