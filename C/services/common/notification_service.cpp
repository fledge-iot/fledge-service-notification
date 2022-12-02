/*
 * Fledge notification service class
 *
 * Copyright (c) 2018 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */
#include <management_api.h>
#include <management_client.h>
#include <service_record.h>
#include <plugin_manager.h>
#include <plugin_api.h>
#include <plugin.h>
#include <logger.h>
#include <iostream>
#include <string>
#include <service_handler.h>
#include <storage_client.h>
#include <config_handler.h>
#include <notification_service.h>
#include <notification_manager.h>
#include <notification_queue.h>
#include <notification_subscription.h>
#include <delivery_queue.h>

using namespace std;

/**
 * Constructor for the NotificationService class
 *
 * This class handles all Notification server components.
 *
 * @param    myName	The notification server name
 * @param    token	The satrtup token passed at startup time by core server
 */
NotificationService::NotificationService(const string& myName,
					 const string& token) :
					 m_shutdown(false),
					 m_token(token),
					 m_dryRun(false),
					 m_restartRequest(false),
					 m_storageServiceRestartPending(false)
{
	// Set name
	m_name = myName;

	// Set type
	m_type = SERVICE_TYPE;

	// Default to a dynamic port
	unsigned short servicePort = 0;

	// Create new logger instance
	m_logger = new Logger(myName);
	//m_logger->setMinLevel("warning");

	m_logger->info("Starting %s notification server", myName.c_str());

	// One thread
	unsigned int threads = 1;

	// Instantiate the NotificationApi class
	m_api = new NotificationApi(servicePort, threads);

	// Set NULL for other resources
	m_mgtClient = NULL;
	m_managementApi = NULL;
}

/**
 * NotificationService destructor
 */
NotificationService::~NotificationService()
{
	delete m_api;
	delete m_mgtClient;
	delete m_managementApi;
	delete m_logger;
}

bool NotificationService::connectToStorage(std::map<std::thread::id, std::atomic<int>>* m)
{
	// Get Storage service
	ServiceRecord storageInfo("Fledge Storage");
	if (!m_mgtClient->getService(storageInfo))
	{
		m_logger->fatal("Unable to find Fledge storage "
				"connection info for service '" + m_name + "'");

		this->cleanupResources();

		if (m_restartRequest)
		{
			PRINT_FUNC;
			// Request the Fledge core to restart the service
			m_mgtClient->restartService();
		}
		else if (!getStorageServiceRestartPendingFlag())
		{
			PRINT_FUNC;
			// Unregister from Fledge
			m_mgtClient->unregisterService();
		}

		return false;
	}
	m_logger->info("Connect to storage on %s:%d",
		       storageInfo.getAddress().c_str(),
		       storageInfo.getPort());

	// Setup StorageClient
	m_storage = new StorageClient(storageInfo.getAddress(), storageInfo.getPort(), m);
	
	return true;
}


/**
 * Start the notification service
 * by connecting to Fledge core service.
 *
 * @param coreAddress	The Fledge core address
 * @param corePort	The Fledge core port
 * @return		True for success, false otherwise
 */
bool NotificationService::start(string& coreAddress,
				unsigned short corePort)
{
	// Dynamic port
	unsigned short managementPort = (unsigned short)0;

	// Instantiate ManagementApi class
	PRINT_FUNC;
	m_managementApi = new ManagementApi(SERVICE_NAME, managementPort);
	PRINT_FUNC;
	m_managementApi->registerService(this);
	PRINT_FUNC;
	m_managementApi->start();
	PRINT_FUNC;

	// Allow time for the listeners to start before we register
	while(m_managementApi->getListenerPort() == 0)
	{
		PRINT_FUNC;
		sleep(1);
	}

    // Enable http API methods
    PRINT_FUNC;
    m_api->initResources();

    // Start the NotificationApi on service port
    PRINT_FUNC;
	m_api->start();
	PRINT_FUNC;
	sleep(1); // this delay is required only when new storage service instance has been re-started, but seems no way to differentiate that case presently
	PRINT_FUNC;

	// Allow time for the listeners to start before we continue
	try
	{
		while(m_api->getListenerPort() == 0)
		{
			PRINT_FUNC;
			sleep(1);
		}
	}
	catch(std::exception const& ex)
	{
		m_logger->fatal("Error during m_api->getListenerPort(): error=%s", ex.what());
	}

	// Set Notification callback url prefix
	m_api->setCallBackURL();

	// Get management client
	m_mgtClient = new ManagementClient(coreAddress, corePort);
	if (!m_mgtClient)
	{
		m_logger->fatal("Notification service '" + m_name + \
				"' can not connect to Fledge at " + \
				string(coreAddress + ":" + to_string(corePort)));

		this->cleanupResources();
		return false;
	}

	// Create an empty Notification category if one doesn't exist
	DefaultConfigCategory notificationConfig(string("Notifications"), string("{}"));
	notificationConfig.setDescription("Notification services");
	if (!m_mgtClient->addCategory(notificationConfig, true))
	{
		m_logger->fatal("Notification service '" + m_name + \
				"' can not connect to Fledge ConfigurationManager at " + \
				string(coreAddress + ":" + to_string(corePort)));

		this->cleanupResources();
		return false;
	}

	// Create a category with given Notification server m_name
	DefaultConfigCategory notificationServerConfig(m_name, string("{}"));
	notificationServerConfig.setDescription("Notification server " + m_name);
	vector<string>  logLevels = { "error", "warning", "info", "debug" };
	notificationServerConfig.addItem("logLevel", "Minimum logging level reported",
                        "warning", "warning", logLevels);
	notificationServerConfig.setItemDisplayName("logLevel", "Minimum Log Level");

	notificationServerConfig.addItem("deliveryThreads",
					 "Maximum number of notification delivery threads",
					 "integer", "2", "2");
	notificationServerConfig.setItemDisplayName("deliveryThreads",
						    "Maximum number of delivery threads");
	
	if (!m_mgtClient->addCategory(notificationServerConfig, true))
	{
		m_logger->fatal("Notification service '" + m_name + \
				"' can not connect to Fledge ConfigurationManager at " + \
				string(coreAddress + ":" + to_string(corePort)));

		this->cleanupResources();
		return false;
	}

	// Register this notification service with Fledge core
	unsigned short listenerPort = m_api->getListenerPort();
	unsigned short managementListener = m_managementApi->getListenerPort();
	ServiceRecord record(m_name,
			     SERVICE_TYPE,		// Service type
			     "http",			// Protocol
			     "localhost",		// Listening address
			     listenerPort,		// Service port
			     managementListener,	// Management port
			     m_token);			// Startup token

	if (m_dryRun)
	{

		this->createSecurityCategories(m_mgtClient, true);

		m_logger->info("Dry run invocation - shutting down");
		this->cleanupResources();
		return true;
	}

	if (!m_mgtClient->registerService(record))
	{
		m_logger->fatal("Unable to register service "
				"\"Notification\" for service '" + m_name + "'");

		this->cleanupResources();
		return false;
	}
	// Register 'm_name' category name to Fledge Core
	// for configuration changes update
	this->registerCategory(m_name);
	this->registerCategory("storage_service_restart");
	

	// Get 'm_name' category name to Fledge Core
	ConfigCategory category = m_mgtClient->getCategory(m_name);
	if (category.itemExists("logLevel"))
	{
		m_logger->setMinLevel(category.getValue("logLevel"));
	}

	if (category.itemExists("deliveryThreads"))
	{
		m_delivery_threads = atoi(category.getValue("deliveryThreads").c_str());
	}
	if (!m_delivery_threads)
	{
		m_delivery_threads = DEFAULT_DELIVERY_WORKER_THREADS;
	}

	m_logger->info("Starting Notification service '" + m_name +  "' ...");

	bool rv = connectToStorage(NULL);
	m_logger->info("connectToStorage() 1 returned %s", rv?"true":"false");
	if (rv == false)
		return false;
	
	// Setup NotificationManager class
	NotificationManager instances(m_name, m_mgtClient, this);
	// Get all notification instances under Notifications
	// and load plugins defined in all notifications 
	instances.loadInstances();

	m_mgtClient->addAuditEntry("NTFST",
					"INFORMATION",
					"{\"name\": \"" + m_name + "\"}");

	// Create default security category 
	// note we do not get here if m_dryRun is true
	this->createSecurityCategories(m_mgtClient, m_dryRun);

	// We have notification instances loaded
	// (1.1) Start the NotificationQueue
	// (1.2) Start the DeliveryQueue
	NotificationQueue *queue = new NotificationQueue(m_name);
	DeliveryQueue *dQueue = new DeliveryQueue(m_name, m_delivery_threads);

	// (2) Register notification interest, per assetName:
	// by call Storage layer Notification API.
	NotificationSubscription *subscriptions = new NotificationSubscription(m_name, *m_storage);
	subscriptions->registerSubscriptions();

	// Notification data will be now received via NotificationApi server
	// and added into the queue for processing.

	// .... wait until shutdown ...

	// Wait for all the API threads to complete
	// m_api->wait();
	while (1)
	{
		if(m_api->serverDown())
		{
			m_logger->info("m_api->serverDown() returned TRUE");
			m_api->wait(); // let the HTTP server thread join
			m_logger->info("m_api->wait() returned here");
			
			m_logger->info("HTTP server down, thread joined, getStorageServiceRestartPendingFlag()=%s", 
							getStorageServiceRestartPendingFlag()?"true":"false");
			
			if (getStorageServiceRestartPendingFlag())
			{
				PRINT_FUNC;
#if 1
				// NotificationSubscription* subscriptions = NotificationSubscription::getInstance();
				subscriptions->removeAllSubscriptions(getStorageServiceRestartPendingFlag());
				delete subscriptions;
#endif
				PRINT_FUNC;

#if 0
				// Flush all data in the queues
				queue->stop(); delete queue;
				dQueue->stop(); delete dQueue;
#endif

				PRINT_FUNC;
				m_api->start();
				
				PRINT_FUNC;
				sleep(1); 	// this delay is required only when new storage service instance has been started
							// but seems no way to differentiate that case
				PRINT_FUNC;

				// Allow time for the listeners to start before we continue
				try
				{
					while(m_api->getListenerPort() == 0)
					{
						PRINT_FUNC;
						sleep(1);
					}
				}
				catch(std::exception const& ex)
				{
					m_logger->fatal("Error during m_api->getListenerPort(): error=%s", ex.what());
				}

				PRINT_FUNC;

				m_logger->info("%s:%d: BEFORE: m_storage->getUrlBase().str().c_str()=%s", __FUNCTION__, __LINE__, m_storage->getUrlBase().str().c_str());

				m_logger->info("calling connectToStorage(): prev m_storage seqMap has %d entries", m_storage->getSeqNumMap()?0:m_storage->getSeqNumMap()->size());
				StorageClient* prev_storage_client = m_storage;
				bool rv = connectToStorage(m_storage->getSeqNumMap());
				m_logger->info("connectToStorage() 2 returned %s", rv?"true":"false");
				
				delete prev_storage_client;
				// previous storage service is now gone, so can't/needn't unregister subscriptions done to it

				m_logger->info("%s:%d: AFTER: m_storage->getUrlBase().str().c_str()=%s", __FUNCTION__, __LINE__, m_storage->getUrlBase().str().c_str());
				
				if (rv == false)
					return false;

				// Set Notification callback url prefix
				m_api->setCallBackURL();
				
				m_mgtClient->addAuditEntry("NTFST",
								"INFORMATION",
								"{\"name\": \"" + m_name + "\"}");

				PRINT_FUNC;

#if 0

				// We have notification instances loaded
				// (1.1) Start the NotificationQueue
				// (1.2) Start the DeliveryQueue
				queue = new NotificationQueue(m_name);
				dQueue = new DeliveryQueue(m_name, m_delivery_threads);

				PRINT_FUNC;
#endif

#if 1
				// (2) Register notification interest, per assetName:
				// by calling Storage layer Notification API.
				NotificationSubscription *subscriptions = new NotificationSubscription(m_name, *m_storage);
				subscriptions->registerSubscriptions();
#endif
				PRINT_FUNC;
				resetStorageServiceRestartPendingFlag();
			}
			else
			{
				PRINT_FUNC;
				break;
			}
		}
		sleep(1);
	}
	
	// Shutdown is starting ...
	// NOTE:
	// - Notification API listener is already down.
	// - all subscriptions already unregistered

	// Unregister from storage service
	m_mgtClient->unregisterService();

	// Stop management API
	m_managementApi->stop();

	// Flush all data in the queues
	queue->stop(); delete queue;
	dQueue->stop(); delete dQueue;

	m_logger->info("Notification service '" + m_name + "' shutdown completed.");

	m_mgtClient->addAuditEntry("NTFSD",
					"INFORMATION",
					"{\"name\": \"" + m_name + "\"}");

	return true;
}

/**
 * Unregister notification subscriptions and
 * stop NotificationAPi listener
 */
void NotificationService::stop(bool unregisterSubscriptions)
{
	m_logger->info("Stopping Notification service '" + m_name + "' ...");

	if (unregisterSubscriptions)
	{
		// Unregister notifications to storage service
		NotificationSubscription* subscriptions = NotificationSubscription::getInstance();
		if (subscriptions)
		{
			subscriptions->unregisterSubscriptions();
		}
	}

	// Stop the NotificationApi
	m_logger->info("1. Calling m_api->stop()");
	m_api->stop();
}

/**
 * Shutdown request
 */
void NotificationService::shutdown()
{
	m_shutdown = true;
	m_logger->info("Notification service '" + m_name + "' shutdown in progress ...");

	this->stop();
}

/**
 * Restart request. Shut down the service and then request the core to restart the service.
 */
void NotificationService::restart()
{
	m_restartRequest = true;
	m_shutdown = true;
	m_logger->info("Notification service '" + m_name + "' restart in progress ...");

	this->stop();
}

/**
 * Cleanup resources and stop services
 */
void NotificationService::cleanupResources()
{
	this->stop();
	m_api->wait();

	m_managementApi->stop();
}

/**
 * Create an extra delivery
 */
void NotificationService::configChildCreate(const std::string& parent_category, const string& categoryName, const string& category)
{

	NotificationManager* notifications = NotificationManager::getInstance();
	NotificationInstance* instance = NULL;
	string notificationName;

	notificationName = parent_category;

		// It's a notification category
		notifications->lockInstances();
		instance = notifications->getNotificationInstance(notificationName);
		notifications->unlockInstances();

		if (instance)
		{
			ConfigCategory config(categoryName, category);

			ConfigCategory notificationConfig = m_mgtClient->getCategory(notificationName);

			notifications->addDelivery(notificationConfig, categoryName, config);
		}

	if (instance == NULL)
	{
		// Log message
	}

}


/**
 * Delete an extra delivery
 */
void NotificationService::configChildDelete(const std::string& parent_category, const string& categoryName)
{
	NotificationManager* notifications = NotificationManager::getInstance();
	NotificationInstance* instance = NULL;
	string notificationName;

	notificationName = parent_category;

	// It's a notification category
	notifications->lockInstances();
	instance = notifications->getNotificationInstance(notificationName);
	notifications->unlockInstances();

	if (instance)
	{
		bool ret = false;
		string deliveryName;
		std::size_t found = categoryName.find(CATEGORY_DELIVERY_EXTRA);
		if (found != std::string::npos)
		{
			deliveryName = categoryName.substr(found + strlen(CATEGORY_DELIVERY_EXTRA));
			NotificationManager* manager = NotificationManager::getInstance();

			DeliveryPlugin* deliveryPlugin = manager->deleteDeliveryCategory(notificationName,
											deliveryName);

			ret = deliveryPlugin != NULL;
			// Delete plugin object
			delete deliveryPlugin;
		}
	}	
	if (instance == NULL)
	{
		// Log message
	}
}

/**
 * Configuration change notification
 *
 * @param    categoryName	The category name which configuration has been changed
 * @param    category		The JSON string with new configuration
 */
void NotificationService::configChange(const string& categoryName,
				       const string& category)
{

	NotificationManager* notifications = NotificationManager::getInstance();
	NotificationInstance* instance = NULL;

	m_logger->info("NotificationService::configChange(): categoryName=%s, category=%s", categoryName.c_str(), category.c_str());

	if (categoryName == m_name)
	{
		ConfigCategory config(categoryName, category);
		if (config.itemExists("logLevel"))
		{
			m_logger->setMinLevel(config.getValue("logLevel"));
			m_logger->warn("Set log level to %s", config.getValue("logLevel").c_str());
		}
		return;
	}

	// Update the  Security category
	if (categoryName.compare(m_name+"Security") == 0)
	{
		this->updateSecurityCategory(category);
		return;
	}

	// Update the  Security category
	if (categoryName.compare("storage_service_restart") == 0)
	{
		m_logger->info("Notification service received storage_service_restart in configChange");
		m_logger->info("2. Calling m_api->stop()");
		m_api->stop();
		setStorageServiceRestartPendingFlag();

		return;
	}

	std::size_t found;
	std::size_t foundRule = categoryName.find("rule");
	std::size_t foundDelivery = categoryName.find(CATEGORY_DELIVERY_PREFIX);
	std::size_t foundExtraDelivery = categoryName.find(CATEGORY_DELIVERY_EXTRA);

	if (foundRule == std::string::npos &&
	    foundDelivery == std::string::npos &&
	    foundExtraDelivery == std::string::npos)
	{
		// It's a notification category
		notifications->lockInstances();
		instance = notifications->getNotificationInstance(categoryName);
		notifications->unlockInstances();
		if (instance)
		{
			instance->reconfigure(categoryName, category);
		}
		else
		{
			notifications->createInstance(categoryName, category);
		}
		return;
	}
	else
	{
		// Check it's a rule category
		if (foundRule != std::string::npos)
		{
			// Get related notification instance object
			notifications->lockInstances();
			instance = notifications->getNotificationInstance(categoryName.substr(4));
			notifications->unlockInstances();
			if (!instance ||
			    !instance->getRulePlugin())
			{
				return;
			}

			// Call plugin reconfigure
			instance->getRulePlugin()->reconfigure(category);

			// Instance not enabled, just return
			if (!instance->isEnabled())
			{
				return;
			}

			// Get instance rule
			string ruleName = instance->getRule()->getName();
			// Get all asset names
			std::vector<NotificationDetail>& allAssets = instance->getRule()->getAssets();

			// Get Notification subscripption inastance
			NotificationSubscription* subscriptions = NotificationSubscription::getInstance();

			if (!allAssets.size())
			{
				// No subscriptions, just create a new one
				// by calling "plugin_triggers"
				subscriptions->createSubscription(instance);
			}
			else
			{
				for (auto a = allAssets.begin();
					  a != allAssets.end(); )
				{
					// Remove assetName/ruleName from subscriptions
					subscriptions->removeSubscription((*a).getAssetName(),
									  ruleName);
					// Remove asseet
					a = allAssets.erase(a);
				}

				// Create a new subscription by calling "plugin_triggers"
				subscriptions->createSubscription(instance);
			}

			return;
		}

		// Check it's a delivery category
		if (foundDelivery != std::string::npos)
		{
			// Get related notification instance
			notifications->lockInstances();

			string NotificationName = categoryName.substr(strlen(CATEGORY_DELIVERY_PREFIX));

			instance = notifications->getNotificationInstance(NotificationName);
			notifications->unlockInstances();
			if (instance && instance->getDeliveryPlugin())
			{
				// Call plugin reconfigure
				instance->getDeliveryPlugin()->reconfigure(category);
				return;
			}
		}

		// Check it's an extra delivery channel category
		if (foundExtraDelivery != std::string::npos)
		{
			m_logger->debug("Configuration change for extra delivery channel %s",
			categoryName.c_str());

			// Get related notification instance
			notifications->lockInstances();

			// Get notification name as first part of category name
			string NotificationName = categoryName.substr(0, foundExtraDelivery);

			instance = notifications->getNotificationInstance(NotificationName);
			notifications->unlockInstances();

			if (instance)
			{
				// Fetch all extra delivery channels for this nitification
				std::vector<std::pair<std::string, NotificationDelivery *>>& extra = instance->getDeliveryExtra();
				for (auto item = extra.begin();
					  item != extra.end();
					  ++item)
				{
					if (item->first == categoryName  && item->second->getPlugin())
					{
						// Call plugin reconfigure for the found extra delivery channel
						item->second->getPlugin()->reconfigure(category);
						return;
					}
				}
			}
		}
	}

	if (instance == NULL)
	{
		// Log message
	}

}


/**
 * Register a configuration category for updates
 *
 * @param    categoryName	The category to register
 */
void NotificationService::registerCategory(const string& categoryName)
{
	ConfigHandler* configHandler = ConfigHandler::getInstance(m_mgtClient);
	// Call registerCategory only once
	if (configHandler &&
	    m_registerCategories.find(categoryName) == m_registerCategories.end())
	{
		configHandler->registerCategory(this, categoryName);

		m_registerCategories[categoryName] = true;
	}
}

/**
 * Register the notification for all the child categories of the requested parent one
 *
 * @param    categoryName	Parent category for which registation is requested
 */
void NotificationService::registerCategoryChild(const string& categoryName)
{
	ConfigHandler* configHandler = ConfigHandler::getInstance(m_mgtClient);
	// Call registerCategory only once
	if (configHandler &&
	    m_registerCategoriesChild.find(categoryName) == m_registerCategoriesChild.end())
	{
		configHandler->registerCategoryChild(this, categoryName);

		m_registerCategoriesChild[categoryName] = true;
	}
}

/**
 * Send to the control dispatcher service
 *
 * @param path		The path component of the URL to send
 * @param payload	The JSON paylaod
 * @return bool		Return true if the paylaod was sent
 */
bool NotificationService::sendToDispatcher(const string& path, const string& payload)
{
	// Send the control message to the south service
	try {
		if (!m_mgtClient)
		{
			Logger::getLogger()->error("Missing connection to management client, "
					"unable to deliver control message");
			return false;
		}

		ServiceRecord service("dispatcher");
		if (!m_mgtClient->getService(service))
		{
			Logger::getLogger()->error("Unable to find dispatcher service 'Dispatcher'");
			return false;
		}
		string address = service.getAddress();
		unsigned short port = service.getPort();
		char addressAndPort[80];
		snprintf(addressAndPort, sizeof(addressAndPort), "%s:%d", address.c_str(), port);
		SimpleWeb::Client<SimpleWeb::HTTP> http(addressAndPort);

		try {
			SimpleWeb::CaseInsensitiveMultimap headers = {{"Content-Type", "application/json"}};
			// Pass Notification service bearer token to dispatcher
			string regToken = m_mgtClient->getRegistrationBearerToken();
			if (regToken != "")
			{
				headers.emplace("Authorization", "Bearer " + regToken);
			}

			auto res = http.request("POST", path, payload, headers);
			if (res->status_code.compare("202 Accepted"))
			{
				Logger::getLogger()->error("Failed to send control request to dispatcher service, %s",
						res->status_code.c_str());
				Logger::getLogger()->error("Failed Path %s, %s", path.c_str(), payload.c_str());
				return false;
			}
		} catch (exception& e) {
			Logger::getLogger()->error("Failed to send control operation to dispatcher service, %s",
						e.what());
			Logger::getLogger()->error("Failed Path %s, %s", path.c_str(), payload.c_str());
			return false;
		}

		return true;
	}
	catch (exception &e) {
		Logger::getLogger()->error("Failed to send control operation to dispatcher service, %s", e.what());
		return false;
	}
}
