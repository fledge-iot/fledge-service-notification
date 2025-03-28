/*
 * Fledge notification manager.
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
#include <plugin_api.h>
#include <plugin.h>
#include <iostream>
#include <string>

#include <notification_manager.h>
#include <rule_plugin.h>
#include <delivery_plugin.h>
#include <string.h>
#include "plugin_api.h"
#include <threshold_rule.h>
#include <data_availability_rule.h>
#include <notification_subscription.h>
#include <notification_queue.h>
#include <reading.h>
#include <delivery_queue.h>


using namespace std;

struct AssetTrackInfo
{
	NotificationService* service;
	std::string notificationInstanceName;
	std::string pluginName;
};

extern "C" {
void ingestCB(AssetTrackInfo *info, Reading *reading)
{
	info->service->ingestReading(*reading, info->notificationInstanceName.c_str(), info->pluginName.c_str());
}
NotificationService *getService(NotificationService *service)
{
	return service;
}
};

NotificationManager* NotificationManager::m_instance = 0;

/**
 * NotificationDetail class constructor
 *
 * @param    asset	The asset name
 * @param    rule	The rule name the asset belongs to
 * @param    type	The notification evaluation type
 *	
 */
NotificationDetail::NotificationDetail(const string& source,
				       const string& asset,
				       const string& rule,
				       EvaluationType& type) :
				       m_asset(asset),
				       m_rule(rule),
				       m_value(type),
				       m_source(source)
{
}

/*
 * NotificationDetail class destructor
 */
NotificationDetail::~NotificationDetail()
{}

/**
 * NotificationElement constructor
 *
 * @param    name		Element name
 * @param    notification	The notification name
 *				this element belongs to.
 */
NotificationElement::NotificationElement(const std::string& name,
					 const std::string& notification) :
					 m_name(name),
					 m_notification(notification)
{
}

/**
 * NotificationElement destructor
 */
NotificationElement::~NotificationElement()
{
}

/**
 * NotificationRule constructor
 *
 * @param   name		The notification rule name
 * @param   notification	The notification instance name
 *				for the rule name
 * @param   plugin		The loaded rule plugin
 */
NotificationRule::NotificationRule(const std::string& name,
				   const std::string& notification,
				   RulePlugin* plugin) :
				   NotificationElement(name, notification),
				   m_plugin(plugin)
{
	m_timeBased = 0;
	m_multiple_evaluaion = MultipleEvaluation::M_ALL;
}

/**
 * NotificationRule destructor
 */
NotificationRule::~NotificationRule()
{
	// Free plugin resources
	m_plugin->shutdown();
	delete m_plugin;
}

/**
 * NotificationDelivery constructor
 *
 * @param   name		The notification delivery name
 * @param   notification	The notification instance name
 *				for the delivery name
 * @param   plugin		The loaded delivery plugin
 */
NotificationDelivery::NotificationDelivery(const std::string& name,
					   const std::string& notification,
					   DeliveryPlugin* plugin,
					   const std::string& customText) :
					   NotificationElement(name, notification),
					   m_plugin(plugin),
					   m_text(customText)
{
}

/*
 * NotificationDelivery destructor
 */
NotificationDelivery::~NotificationDelivery()
{
	// Get delivery queue object
	DeliveryQueue* dQueue = DeliveryQueue::getInstance();

	// Create data object for delivery queue
	// with no reason, no message and notifcation instance set to NULL
	// This element added to delivery queue will signal the need of shutting down
	// the DeliveryPlugin after processing all data for this Delivery
	DeliveryDataElement* deliveryData =
		new DeliveryDataElement(
					this->getPlugin(),
					this->getName(),
					this->getNotificationName(),
					"",
					"",
					NULL);

	// Add data object to the queue
	DeliveryQueueElement* queueElement = new DeliveryQueueElement(deliveryData);
	dQueue->addElement(queueElement);

	// We don't call plugin resources removal here.
}

/**
 * NotificationInstance constructor
 *
 * @param    name	The Notification instance name
 * @param    enable	The notification is enabled or not
 * @param    type	Notification type:
 *			"one shot", "retriggered", "toggled"
 * @param    rule	The NotificationRule for this instance
 * @param    delivery	The NotificationDelivery for this instance
 */
NotificationInstance::NotificationInstance(const string& name,
					   bool enable,
					   NotificationType type,
					   NotificationRule* rule,	
					   NotificationDelivery* delivery) :
					   m_name(name),
					   m_enable(enable),
					   m_type(type),
					   m_rule(rule),
					   m_delivery(delivery),
					   m_zombie(false)
{
	// Set initial state for notification delivery
	m_lastSentTv.tv_sec = 0;
	m_lastSentTv.tv_usec = 0;
	m_state = NotificationInstance::StateCleared;
}

/**
 * Add an extra delivery channel
 *
 * @param    type	Notification type:
 *			"one shot", "retriggered", "toggled"
 * @param    delivery	The NotificationDelivery for this instance
 */
void NotificationInstance::addDeliveryExtra(
					   NotificationType type,
					   NotificationDelivery* delivery)
{

	m_deliveryExtra.push_back(std::make_pair(delivery->getName(), delivery));
}


/**
 * Delete an extra  delivery channel
 *
 */
void NotificationInstance::deleteDeliveryExtra(const std::string &deliveryName)
{
	for (auto item = m_deliveryExtra.begin();
		  item != m_deliveryExtra.end();
		  ++item)
	{
		if (item->first == deliveryName)
		{
			Logger::getLogger()->debug("%s - deliveryName :%s: ", __FUNCTION__,  item->first.c_str());
			m_deliveryExtra.erase(item);
			break;
		}
	}
}


/**
 * NotificationInstance destructor
 */
NotificationInstance::~NotificationInstance()
{
	delete m_rule;
	delete m_delivery;
}

/**
 * Return JSON string of NotificationInstance object
 *
 * @return	A JSON string representation of the instance
 */
string NotificationInstance::toJSON(bool showAll)
{
	ostringstream ret;

	ret << "{\"name\": \"" << this->getName() << "\", \"enable\": ";
	ret << (this->isEnabled() ? "true" : "false") << ", ";
	if (showAll)
	{
		ret << "\"active\": ";
		ret << (!this->isZombie() ? "true" : "false") << ", ";
	}
	ret << "\"type\": \"" << this->getTypeString(this->getType()) << "\", ";
	ret << "\"rule\": \"";
	ret << (this->getRulePlugin() ? this->getRulePlugin()->getName() : "");
	ret << "\", \"delivery\": \"";
	ret << (this->getDeliveryPlugin() ? this->getDeliveryPlugin()->getName() : "") << "\"";

	// Fetch extra channels
	std::vector<std::pair<std::string, NotificationDelivery *>>& deliveryExtra = this->getDeliveryExtra();
	int s = deliveryExtra.size();
	if (s > 0)
	{
		// Add any extra delivery channel
		ret << ", \"extra_delivery_channels\": [";
		int i = 0;
		for(auto &delivery : deliveryExtra)
		{
			DeliveryPlugin* plugin = delivery.second->getPlugin();
			if (plugin)
			{
				ret << "{\"name\": \"" << delivery.first <<
					"\", \"plugin\" : \"" << plugin->getName() << "\"}";
				if (i < (s -1))

				{
					ret << ", ";
				}
				i++;
			}
		}
		ret << "]";
	}

	ret << "}";

	return ret.str();
}

/**
 * Constructor for the NotificationManager class
 *
 * @param    serviceName	Notification service name
 * @param    managerClient	Pointer to ManagementClient
 * @param    service		Pointer to Notification service
 */
NotificationManager::NotificationManager(const std::string& serviceName,
					 ManagementClient* managerClient,
					 NotificationService* service) :
					 m_name(serviceName),
					 m_managerClient(managerClient),
					 m_service(service)
{
	NotificationManager::m_instance = this;

	// Get logger
	m_logger = Logger::getLogger();

	/**
	 * Add here all the builtin rules we want to make available:
	 */
	this->registerBuiltinRule<ThresholdRule>("Threshold");

	this->registerBuiltinRule<DataAvailabilityRule>("DataAvailability");

	// Register statistics
	ManagementApi *management = ManagementApi::getInstance();
	if (management)
	{
		management->registerStats(&m_stats);
	}
}

/**
 * NotificationManager destructor
 */
NotificationManager::~NotificationManager()
{
	lock_guard<mutex> guard(m_instancesMutex);
	// Mark this instance as zombie
        for (auto it = m_instances.begin();
		  it != m_instances.end();
		  ++it)
	{
		(*it).second->markAsZombie();
	}
	
	// Delete each element in m_instances
	for (auto it = m_instances.begin();
		  it != m_instances.end();
		  ++it)
	{
		delete (*it).second;
	}
}

/**
 * Get the NotificationManager instance
 *
 * @return	Pointer to NotificationManager instance
 */
NotificationManager* NotificationManager::getInstance()
{
	return m_instance;
}

/**
 * Load all notification instances found in "Notifications" Fledge category.
 *
 */
void NotificationManager::loadInstances()
{
	try
	{
		// Get child categories of "Notifications"
		ConfigCategories instances = m_managerClient->getChildCategories("Notifications");
		for (int i = 0; i < instances.length(); i++)
		{
			// Fetch instance configuration category
			ConfigCategory config = m_managerClient->getCategory(instances[i]->getName());

			// Create the NotificationInstance object
			if (this->setupInstance(instances[i]->getName(), config))
			{
				m_stats.loaded++;
				m_stats.total++;
			}
		}
	}
	catch (...)
	{
		// Non blocking error
		return;
	}
}

/**
 * Add an instance to the current instances
 *
 * @param    instanceName	The instance name
 * @param    enabled		Is enabled or not
 * @param    rule		Pointer to the associated NotificationRule
 * @param    delivery		Pointer to the ssociated NotificationDelivery
 */
void NotificationManager::addInstance(const string& instanceName,
				      bool enabled,
				      NOTIFICATION_TYPE type,
				      NotificationRule* rule,
				      NotificationDelivery* delivery)
{
	bool createInstance = true;

	// Protect changes to m_instances
	lock_guard<mutex> guard(m_instancesMutex);
	auto instance = m_instances.find(instanceName);
	if (instance != m_instances.end())
	{
		if (!instance->second->isZombie())
		{
			// Already set
			Logger::getLogger()->debug("Instance %s already set", instanceName.c_str());

			// Don't create a new instance
			createInstance = false;
		}
		else
		{
			// Zombie instance: delete it now
			Logger::getLogger()->debug("Zombie instance %s detected, deleting it ...", instanceName.c_str());

			delete instance->second;
			instance->second = NULL;
			m_instances.erase(instance);
		}
	}

	if (createInstance)
	{
		// Add a new instance
		NotificationInstance* instance = new NotificationInstance(instanceName,
									  enabled,
									  type,
									  rule,
									  delivery);
		if (instance)
		{
			m_instances[instanceName] = instance;
		}
		else
		{
			 Logger::getLogger()->error("Cannot setup new instance for key %s",
						    instanceName.c_str());
		}
	}
}


/**
 * Add an extra delivery channel
 *
 * @param    instanceName	The instance name
 * @param    delivery		Pointer to the associated NotificationDelivery
 */
void NotificationManager::addDeliveryExtra(
	const string& instanceName,
	NOTIFICATION_TYPE type,
	NotificationDelivery* delivery)
{
	bool createInstance = true;

	// Protect changes to m_instances
	lock_guard<mutex> guard(m_instancesMutex);
	auto instance = m_instances.find(instanceName);
	if (instance != m_instances.end())
	{
		instance->second->addDeliveryExtra(type, delivery);
	}
	else
	{
		 Logger::getLogger()->error("Cannot setup new delivery for key %s",
						instanceName.c_str());
	}

}


/**
 * Return a JSON string with current loaded notification instances
 *
 * @return	JSON string with all loaded instances
 */
string NotificationManager::getJSONInstances(bool showAll)
{
	// Protect changes to m_instances
	lock_guard<mutex> guard(m_instancesMutex);
	string ret = "";
	for (auto it = m_instances.begin();
		  it != m_instances.end();
		  ++it)
	{
		// Do not show Zombie instance
		if (showAll == true || !it->second->isZombie())
		{
			// Get instance JSON string
			ret += (it->second)->toJSON(showAll);
		}

		// Add ', ' separator
		if (ret[0] != '\0' &&
		   std::next(it) != m_instances.end())
		{
			if (showAll == true || !std::next(it)->second->isZombie())
			{
				ret += ", ";
			}
		}
	}
	return ret;
}

/**
 * Return a notification instance, given its name
 *
 * @param instanceName		The instance name to fetch
 * @return			Pointer of the found instance or
 *				NULL if not found.
 */
NotificationInstance*
NotificationManager::getNotificationInstance(const std::string& instanceName) const
{
	auto instance = m_instances.find(instanceName);
	if (instance != m_instances.end())
	{
		return (*instance).second;
	}
	else
	{
		return NULL;
	}
}

/**
 * Load a rule plugin
 * 
 * @param    rulePluginName	The rule plugin to load
 * @return			Plugin handle on success, NULL otherwise 
 *
 */
PLUGIN_HANDLE NotificationManager::loadRulePlugin(const string& rulePluginName)
{
	if (rulePluginName.empty())
	{
		m_logger->error("Unable to fetch rule plugin '%s' from configuration.",
				rulePluginName.c_str());
		// Failure
		return NULL;
	}

	m_logger->info("Loading rule plugin '%s'.",
		       rulePluginName.c_str());

	PluginManager* manager = PluginManager::getInstance();
	PLUGIN_HANDLE handle;
	if ((handle = manager->loadPlugin(rulePluginName,
					  PLUGIN_TYPE_NOTIFICATION_RULE)) != NULL)
	{
		// Success
		m_logger->info("Loaded rule plugin '%s'.",
			       rulePluginName.c_str());
	}
	return handle;
}

/**
 * Load a delivery plugin
 * 
 * @param    deliveryPluginName		The delivery plugin to load
 * @return				Plugin handle on success, NULL otherwise 
 *
 */
PLUGIN_HANDLE NotificationManager::loadDeliveryPlugin(const string& loadDeliveryPlugin)
{
	if (loadDeliveryPlugin.empty())
	{
		m_logger->error("Unable to fetch delivery plugin "
				"'%s' from configuration.",
				loadDeliveryPlugin.c_str());
		// Failure
		return NULL;
	}

	m_logger->info("Loading delivery plugin '%s'.",
		       loadDeliveryPlugin.c_str());

	PluginManager* manager = PluginManager::getInstance();
	PLUGIN_HANDLE handle;
	if ((handle = manager->loadPlugin(loadDeliveryPlugin,
					  PLUGIN_TYPE_NOTIFICATION_DELIVERY)) != NULL)
        {
		// Success
		m_logger->info("Loaded delivery plugin '%s'.",
			       loadDeliveryPlugin.c_str());
	}
	return handle;
}

/**
 * Parse the Notification type string
 *
 * @param    type	The notification type:
 *			"one shot", "retriggered", "toggled"
 * @return		The NotificationType value
 */
E_NOTIFICATION_TYPE NotificationManager::parseType(const string& type)
{
	E_NOTIFICATION_TYPE ret;
	const char* ptrType = type.c_str();

	if (strcasecmp(ptrType, "one shot") == 0 ||
	    strcasecmp(ptrType, "oneshot") == 0)
	{
		ret = E_NOTIFICATION_TYPE::OneShot;
	}
	else if (strcasecmp(ptrType, "toggled") == 0)
	{
		ret = E_NOTIFICATION_TYPE::Toggled;
	}
	else if (strcasecmp(ptrType, "retriggered") == 0)
	{
		ret = E_NOTIFICATION_TYPE::Retriggered;
	}
	else
	{
		ret = E_NOTIFICATION_TYPE::None;
	}
	return ret;
}

/**
 * Return string value of NotificationType enum
 *
 * @param    nType	The NotificationType value
 * @return		String value of NotificationType value
 */
string NotificationInstance::getTypeString(NOTIFICATION_TYPE nType)
{
	string ret = "";
	switch (nType.type)
	{
		case E_NOTIFICATION_TYPE::OneShot:
			ret = "One Shot";	
			break;
		case E_NOTIFICATION_TYPE::Toggled:
			ret = "Toggled";
			break;
		case E_NOTIFICATION_TYPE::Retriggered:
			ret = "Retriggered";
			break;
		default:
			break;
	}
	return ret;
}

/**
 * Wraps the loading of a rule plugin and return the RulePlugin class
 *
 * @param    rulePluginName		The rule plugin to load.
 * @return   The RulePlugin class new instance or NULL on errors.
 */
RulePlugin* NotificationManager::createRulePlugin(const string& rulePluginName)
{
	RulePlugin* rule = NULL;
	PLUGIN_HANDLE handle = NULL;

	// Check for builtin rule first
	RulePlugin* isBuiltin = this->findBuiltinRule(rulePluginName);
	if (isBuiltin)
	{
		return isBuiltin;
	}
	else
	{
		handle = this->loadRulePlugin(rulePluginName);
		if (handle)
		{
			rule = new RulePlugin(rulePluginName, handle);
		}
	}

	// Return pointer to RulePlugin class instance
	return rule;
}

/**
 * Wraps the loading of a delivery plugin and return the DeliveryPlugin class
 *
 * @param    deliveryPluginName		The delivery plugin to load.
 * @return   The DeliveryPlugin class new instance or NULL on errors.
 */
DeliveryPlugin* NotificationManager::createDeliveryPlugin(const string& deliveryPluginName)
{
	DeliveryPlugin* delivery = NULL;
	PLUGIN_HANDLE handle = NULL;

	// Load the delivery plugin
	handle = this->loadDeliveryPlugin(deliveryPluginName);
	if (handle)
	{
		// Create DeliveryPlugin class instance
		delivery = new DeliveryPlugin(deliveryPluginName, handle);
	}
	// Return pointer to DeliveryPlugin class instance
	return delivery;
}

/**
 * Find the builtin rule
 *
 * @param    ruleName
 * @return   True if the ruleName is a builtin one, false otherwise
 */
RulePlugin* NotificationManager::findBuiltinRule(const string& ruleName)
{
	if (!m_builtinRules.size())
	{
		// No builtin rules
		return NULL;
	}

	auto it = m_builtinRules.find(ruleName);
	if (it !=  m_builtinRules.end())
	{
		// Return the class instance for ruleName
		return it->second(ruleName);
	}
	else
	{
		// ruleName not found
		return NULL;
	}
}

/**
 * Register a builtin rule class, derived form RulePlugin class
 *
 * Call this routine with the class name T and its "string" name:
 * registerBuiltinRule<ThresholdRule>("Threshold");
 *
 * @param   ruleName	The built in rule name
 */
template<typename T> void
NotificationManager::registerBuiltinRule(const std::string& ruleName)
{
	m_builtinRules[ruleName] = [](const std::string& ruleName)
				   {
					return new T(ruleName);
				   };
}

/**
 * Check whether a notification can be sent
 *
 * A notification is sent accordingly to the notification type,
 * value of "plugin_eval" call and the maximum repeat frequency.
 *
 * @param    evalRet	Notification data evaluation
 *			via rule "plugin_eval" call
 * @return	True if notification has to be sent or false
 */
bool NotificationInstance::handleState(bool evalRet)
{
	bool setTriggered = false;
	bool ret = false;
	NOTIFICATION_TYPE nType = this->getType();

	// Get now with seconds and microseconds
	struct timeval now_tv, diffTimeTv;
	gettimeofday(&now_tv, NULL);

	// Calculate time diff
	timersub(&now_tv, &m_lastSentTv, &diffTimeTv);

	switch(nType.type)
	{
	case NotificationInstance::OneShot:
	case NotificationInstance::Toggled:
		if (m_state == NotificationState::StateTriggered)
		{
			// Set state depends on evalRet
			setTriggered = evalRet;
			// Try sending "cleared" when evaluation is false (Toggled only)
			ret = !evalRet && (nType.type == E_NOTIFICATION_TYPE::Toggled);
		}
		else
		{
			// Try sending "triggered" when evaluation is true
			ret = evalRet && timercmp(&diffTimeTv, &nType.retriggerTimeTv, >=);
			// Here state change depends on sending value
			setTriggered = ret;
		}
		break;

	case NotificationInstance::Retriggered:
		// Set state depends on evalRet
		setTriggered = evalRet;
		// Try sending "triggered" when evaluation is true
		ret = evalRet && timercmp(&diffTimeTv, &nType.retriggerTimeTv, >=);
		break;

	default:
		break;
	}

	// Update state
	NotificationState newState = setTriggered ?
		  NotificationState::StateTriggered :
		  NotificationState::StateCleared;

	if (m_state != newState)
	{
		Logger::getLogger()->info("Notification %s has %s", m_name.c_str(), newState == NotificationState::StateTriggered ? "triggered" : "cleared");
		m_state = newState;
	}

	if (ret)
	{
		// Update last sent time
		m_lastSentTv = now_tv;
		char dateStr[80];
		struct tm tm;
		time_t tim = now_tv.tv_sec + nType.retriggerTimeTv.tv_usec;
		asctime_r(localtime_r(&tim, &tm), dateStr);
		Logger::getLogger()->info("Notification %s will not be sent again until after %s", m_name.c_str(), dateStr);
	}

	return ret;
}

/**
 * Return JSON string of a notification rule object
 *
 * @return      A JSON string representation of the rule
 */
string NotificationRule::toJSON()
{
	ostringstream ret;

	ret << "{\"" << this->getPlugin()->getName() << "\": ";
	ret << this->getPlugin()->getInfo()->config;
	ret << " }";

	return ret.str();
}

/**
 * Return JSON string of a notification delivery object
 *
 * @return      A JSON string representation of the delivery
 */
string NotificationDelivery::toJSON()
{
	ostringstream ret;

	ret << "{\"" << this->getPlugin()->getName() << "\": ";
	ret << this->getPlugin()->getInfo()->config;
	ret << " }";

	return ret.str();
}

/**
 * Return a JSON string with current loaded notification rules
 *
 * @return	JSON string with all loaded rules
 */
string NotificationManager::getJSONRules()
{
	string ret;
	PluginManager* plugins = PluginManager::getInstance();
	list<std::string> pList;
	plugins->getInstalledPlugins("notificationRule", pList);

	bool foundPlugin = false;
	ret = "[";

	for (auto it = pList.begin();
		  it != pList.end();
		  ++it)
	{
		PLUGIN_HANDLE pHandle = plugins->findPluginByName(*it);
		if (pHandle)
		{
			foundPlugin = true;
			ret += this->getPluginInfo(plugins->getInfo(pHandle));
			if (std::next(it) != pList.end())
			{
				ret += ", ";
			}
		}
	}

	// Add ", " if at least one loaded plugin and one builtin rule
	if (foundPlugin && m_builtinRules.size())
	{
		ret += ", ";
	}
	
	for (auto it = m_builtinRules.begin();
		  it != m_builtinRules.end();
		  ++it)
	{
		RulePlugin* builtinRule = this->findBuiltinRule((*it).first);
		if (builtinRule)
		{
			ret += this->getPluginInfo(builtinRule->getInfo());

			if (std::next(it) != m_builtinRules.end())
			{
				ret += ", ";
			}
		}
		delete builtinRule;
	}

	ret += "]";

	return ret;
}

/**
 * Return a JSON string with current loaded notification delivery objects
 *
 * @return	JSON string with all loaded delivery objects
 */
string NotificationManager::getJSONDelivery()
{

	string ret;
	PluginManager* plugins = PluginManager::getInstance();
	list<std::string> pList;
	plugins->getInstalledPlugins("notificationDelivery", pList);

	if (!pList.size())
	{
		return "[]";
	}

	ret = "[";
	for (auto it = pList.begin();
		  it != pList.end();
		  ++it)
	{
		PLUGIN_HANDLE pHandle = plugins->findPluginByName(*it);
		if (pHandle)
		{
			ret += this->getPluginInfo(plugins->getInfo(pHandle));
			if (std::next(it) != pList.end())
			{
				ret += ", ";
			}
		}
	}

	ret += "]";

	return ret;
}

/**
 * Creates an empty, disabled notification category
 * within the Notifications parent, via API call
 *
 * @param    name	The notification instance to create
 * @return		True on success, false otherwise
 */
bool NotificationManager::APIcreateEmptyInstance(const string& name)
{
	bool ret = false;

	// Create an empty Notification category
	string payload = "{\"name\" : {\"description\" : \"The name of this notification\", "
			 "\"readonly\": \"true\", "
			 "\"type\" : \"string\", \"default\": \"" + JSONescape(name) + "\"}, ";
	payload += "\"description\" :{\"description\" : \"Description of this notification\", "
			 "\"displayName\" : \"Description\", \"order\" : \"1\","
			 "\"type\": \"string\", \"default\": \"\"}, "
		   "\"rule\" : {\"description\": \"Rule to evaluate\", "
			 "\"displayName\" : \"Rule\", \"order\" : \"2\","
			 "\"type\": \"string\", \"default\": \"\"}, "
		   "\"channel\": {\"description\": \"Channel to send alert on\", "
			 "\"displayName\" : \"Channel\", \"order\" : \"3\","
			 "\"type\": \"string\", \"default\": \"\"}, "
		   "\"notification_type\": {\"description\": \"Type of notification\", \"type\": "
			 "\"enumeration\", \"options\": [ \"one shot\", \"retriggered\", \"toggled\" ], "
			 "\"displayName\" : \"Type\", \"order\" : \"4\","
			 "\"default\" : \"one shot\"}, "
		   "\"enable\": {\"description\" : \"Enabled\", "
			 "\"displayName\" : \"Enabled\", \"order\" : \"5\","
			 "\"type\": \"boolean\", \"default\": \"false\"}, " 
		   "\"retrigger_time\": {\"description\" : \"Retrigger time in seconds for sending a new notification.\", "
			 "\"displayName\" : \"Retrigger Time\", \"order\" : \"6\", "
			 "\"type\": \"float\",  \"default\": \"" + to_string(DEFAULT_RETRIGGER_TIME) + "\", \"minimum\" : \"0.0\"} }";

	DefaultConfigCategory notificationConfig(name, payload);
	notificationConfig.setDescription("Notification " + name);

	// Don't update any existing configuration, just replace all 
	if (m_managerClient->addCategory(notificationConfig, false))
	{
		NOTIFICATION_TYPE type;
		type.retriggerTimeTv.tv_usec = DEFAULT_RETRIGGER_TIME;
		type.type = E_NOTIFICATION_TYPE::OneShot;
		// Create the empty Notification instance
		this->addInstance(name,
				  false,
				  type,
				  NULL,
				  NULL);

		try
		{
			// Add the category name under "Notifications" parent category
			vector<string> children;
			children.push_back(name);
			m_managerClient->addChildCategories("Notifications",
							    children);
			// Register category for configuration updates
			m_service->registerCategory(name);

			m_stats.created++;
			m_stats.total++;

			// Success
			ret = true;
		}
		catch (std::exception* ex)
		{
			delete ex;
		}
	}	
	return ret;
}

/**
 * Create a rule subcategory for the notification
 * with the template content for the given rule.
 *
 * @param    name	The notification name 
 * @param    rule	The notification rule to create
 * @return		RulePlugin object pointer on success,
 *			NULL otherwise
 */
RulePlugin* NotificationManager::createRuleCategory(const string& name,
						    const string& rule)
{
	RulePlugin* rulePlugin = this->createRulePlugin(rule);
	if (!rulePlugin)
	{
		string errMsg("Cannot load rule plugin '" + rule + "'");
		m_logger->error(errMsg.c_str());
		return NULL;
	}

	// Create category names for plugins under instanceName
	// with names: "rule" + instanceName
	string ruleCategoryName = "rule" + name;

	// Get plugins default configuration
	string rulePluginConfig = rulePlugin->getInfo()->config;

	DefaultConfigCategory ruleDefConfig(ruleCategoryName,
					    rulePluginConfig);

	// Unregister configuration changes	
	// NOTE:
	// currently unregisterCategory is not called
	// as we don't change at run time the rule plugin
	//m_managerClient->unregisterCategory(ruleCategoryName);

	// Create category, don't merge existing values
	if (!m_managerClient->addCategory(ruleDefConfig, false))
	{
		string errMsg("Cannot create/update '" + \
			      ruleCategoryName + "' rule plugin category");
		m_logger->error(errMsg.c_str());

		delete rulePlugin;
		return NULL;
	}

	try
	{
		// Set new rule plugin name in "value"
		m_managerClient->setCategoryItemValue(ruleCategoryName,
						      "plugin",
						      rule);

		// Add ruleCategoryName as child of Notification name
		vector<string> children;
		children.push_back(ruleCategoryName);
		m_managerClient->addChildCategories(name, children);

		// Register category for configuration updates
		m_service->registerCategory(ruleCategoryName);
	}
	catch (std::exception* ex)
	{
		string errMsg("Cannot create/update/register '" + \
			      ruleCategoryName + "' rule plugin category: " + ex->what());
		m_logger->error(errMsg.c_str());
		delete ex;
		delete rulePlugin;
		return NULL;
	}

	// Return plugin object
	return rulePlugin;
}

/**
 * Generate the name of the delivery for the first or the extra ones
 *
 * @param    NotificationName	The instance name
 * @param    delivery		    Name of the delivery
 * @param    extraDelivery		True = it is an extra delivery channel
 * @param    prefixOnly		    For extra delivery only, True = without the delivery name
 */
string NotificationManager::getDeliveryCategoryName(const string& NotificationName, const string& delivery, bool extraDelivery, bool prefixOnly) {

	string deliveryCategoryName;

	if (extraDelivery) {
		if (prefixOnly)
		{
			deliveryCategoryName = NotificationName + CATEGORY_DELIVERY_EXTRA;
		} else {
			deliveryCategoryName = NotificationName + CATEGORY_DELIVERY_EXTRA + delivery;
		}
	} else {
		deliveryCategoryName = CATEGORY_DELIVERY_PREFIX + NotificationName;
	}

	return  (deliveryCategoryName);
}

/**
 * Create a delivery subcategory for the notification
 * with the template content for the given delivery plugin.
 *
 * @param    name	The notification name 
 * @param    delivery	The notification delivery to create
 * @return		DeliveryPlugin object pointer on success,
 *			NULL otherwise
 */
DeliveryPlugin* NotificationManager::createDeliveryCategory(const string& name, const string& delivery, bool extraDelivery)
{
	DeliveryPlugin* deliveryPlugin = this->createDeliveryPlugin(delivery);

	if (!deliveryPlugin)
	{
		string errMsg("Cannot load delivery plugin '" + delivery + "'");
		m_logger->error(errMsg.c_str());
		return NULL;
	}

	if ( ! extraDelivery) {

		// Create category names for plugins under instanceName
		string deliveryCategoryName = getDeliveryCategoryName(name, delivery, extraDelivery, false);

		// Get plugins default configuration
		string deliveryPluginConfig = deliveryPlugin->getInfo()->config;

		DefaultConfigCategory deliveryDefConfig(deliveryCategoryName,
							deliveryPluginConfig);

		// Unregister configuration changes
		// NOTE:
		// currently unregisterCategory is not called
		// as we don't change at run time the delivery plugin
		//m_managerClient->unregisterCategory(deliveryCategoryName);

		// Create category, don't merge existing values
		if (!m_managerClient->addCategory(deliveryDefConfig, false))
		{
			string errMsg("Cannot create/update '" + \
					  deliveryCategoryName + "' delivery plugin category");
			m_logger->error(errMsg.c_str());

			delete deliveryPlugin;
			return NULL;
		}

		try
		{
			// Set new delivery plugin name in "value"
			m_managerClient->setCategoryItemValue(deliveryCategoryName,
								  "plugin",
								  delivery);

			// Add ruleCategoryName as child of Notification name
			vector<string> children;
			children.push_back(deliveryCategoryName);
			m_managerClient->addChildCategories(name, children);

			// Register category for configuration updates
			m_service->registerCategory(deliveryCategoryName);
		}
		catch (std::exception* ex)
		{
			string errMsg("Cannot create/update/register '" + \
					  deliveryCategoryName + "' rule delivery category: " + ex->what());
			delete ex;
			delete deliveryPlugin;
			return NULL;
		}

	}

	// Return plugin object
	return deliveryPlugin;
}

/**
 * Delete an extra delivery
 */
DeliveryPlugin* NotificationManager::deleteDeliveryCategory(const string& instanceName, const string& deliveryName, bool extraDelivery)
{
	// Protect changes to m_instances
	lock_guard<mutex> guard(m_instancesMutex);
	auto instance = m_instances.find(instanceName);
	if (instance != m_instances.end())
	{
		string fullName = getDeliveryCategoryName(instanceName, deliveryName, true, false);

		instance->second->deleteDeliveryExtra(fullName);
	}
	else
	{
		 Logger::getLogger()->error("Cannot setup new delivery for key %s",  instanceName.c_str());
	}


	// Return plugin object
	return NULL;
}


/**
 * Reconfigure a notification instance
 *
 * NOTE: not yet implemented
 *
 * @param    name		The notification to reconfigure
 * @param    category		The JSON string with new configuration
 * @return			True on success, false otherwise.
 */
bool NotificationInstance::reconfigure(const string& name,
					const string& category)
{
	ConfigCategory newConfig(name, category);

	return this->updateInstance(name, newConfig);
}

/**
 * Return JSON string with pluginInfo data
 *
 * @param    info	The plugin info C API 
 * @return		The JSON info string
 */
string NotificationManager::getPluginInfo(PLUGIN_INFORMATION* info)
{
	string ret;
	if (!info)
	{
		ret = "{}";
	}
	else
	{
		string plugin_type = string(info->type);
		// Installed_directory: i.e "notificationRule/Average"
		string installed_directory = plugin_type + "/" + info->name;

		// Return "rule" or "notify" for plugin type
		if (plugin_type == "notificationRule")
		{
			plugin_type = "rule";
		}
		if (plugin_type == "notificationDelivery")
		{
			plugin_type = "notify";
		}

		string package_name;

		// Check for SP_BUILTIN flag
		if (info->options & SP_BUILTIN)
		{
			// Set empty installation directory
			installed_directory = "";
		} else {
			// Set package name
			package_name = "fledge-" + plugin_type + "-" + info->name;
			// Transform to lowercase
			std::transform(package_name.begin(), package_name.end(), package_name.begin(),
					[](unsigned char c){
					return std::tolower(c);
			});
		}

		// Build JSON object
		ret += "{\"name\": \"" + string(info->name) + "\", \"version\": \"" + \
			string(info->version) + "\", \"type\": \"" + plugin_type + \
			"\", \"installedDirectory\": \"" + installed_directory + \
			"\", \"packageName\": \"" + package_name + \
			"\", \"interface\": \"" + string(info->interface) + \
			"\", \"config\": " + string(info->config) + "}";
	}
	return ret;
}

/**
 * Create a notification instance
 *
 * @param    name		The notification to create
 * @param    category		The JSON string with configuration
 * @return                      True on success, false otherwise.
 */
bool NotificationManager::createInstance(const string& name,
					 const string& category)
{
	ConfigCategory config(name, category);

	return this->setupInstance(name, config);
}

/**
 * Create and add a new Notification instance to instances map.
 * Register also interest for configuration changes.
 *
 * @param    name		The instance name to create.
 * @param    config		The configuration for the new instance.
 * @return			True on success, false otherwise.
 */
bool NotificationManager::setupRuleDeliveryFirst(const string& name, const ConfigCategory& config) {

	bool success;

	success = true;

	bool enabled;
	string rulePluginName;
	string deliveryPluginName;
	NOTIFICATION_TYPE type;
	string customText;
	if (!this->getConfigurationItems(config,
					 enabled,
					 rulePluginName,
					 deliveryPluginName,
					 type,
					 customText))
	{
		return false;
	}

	string notificationName = config.getName();


	std::map<std::string, NotificationInstance *>& instances = this->getInstances();

	// Load plugins and update categories and register configuration change interest
	RulePlugin* rule = this->createRuleCategory(notificationName,
						    rulePluginName);

	DeliveryPlugin* deliver = this->createDeliveryCategory(notificationName, deliveryPluginName, false);

	if (rule && deliver && enabled)
	{
		// Create category names for plugins under instanceName
		// Register category interest as well
		string ruleCategoryName = "rule" + notificationName;
		string deliveryCategoryName = getDeliveryCategoryName(notificationName, deliveryPluginName, false, false);

		// Initialise plugins
		// Get up-to-date plugin configurations
		ConfigCategory ruleConfig = m_managerClient->getCategory(ruleCategoryName);
		ConfigCategory deliveryConfig = m_managerClient->getCategory(deliveryCategoryName);

		NotificationRule* theRule = NULL;
		NotificationDelivery* theDelivery = NULL;

		// Call rule "plugin_init" with configuration
		// and instantiate NotificationRule class
		if (rule->init(ruleConfig))
		{
			theRule = new NotificationRule(ruleCategoryName,
						       notificationName,
						       rule);
		}

		// Call delivery "plugin_init" with configuration
		// and instantiate  NotificationDelivery class
		if (deliver->init(deliveryConfig))
		{
			// Check and set registerIngest
			if (deliver->ingestData())
			{
				
				AssetTrackInfo* trackingInfo = new AssetTrackInfo;
				trackingInfo->service = m_service;
				trackingInfo->notificationInstanceName = notificationName;
				trackingInfo->pluginName = deliver->getName();
				std::vector<AssetTrackingTuple*>& vec = m_managerClient->getAssetTrackingTuples(notificationName);
				
				for (AssetTrackingTuple* &rec : vec)
				{
					if (rec->m_eventName == "Notify")
					{
						m_service->updateAssetTrackerCache(*rec);
					}
				}
				deliver->registerIngest((void *)ingestCB, (void *)trackingInfo);
				
				
			}

			// Check and set the NotificationService class
			if (deliver->getService())
			{
				deliver->registerService((void *)getService, (void *)m_service);

				// Call Plugin start
				deliver->start();
			}

			// Now create NotificationDelivery object
			theDelivery = new NotificationDelivery(deliveryCategoryName,
								notificationName,
								deliver,
								customText);
		}

		// Add plugin category name under service/process config name
		vector<string> children;
		children.push_back(ruleCategoryName);
		children.push_back(deliveryCategoryName);
		m_managerClient->addChildCategories(notificationName,
						    children);

		// Add the new instance
		this->addInstance(notificationName,
				  enabled,
				  type,
				  theRule,
				  theDelivery);
	}
	else
	{
		// Add a new instance without plugins
		delete deliver;
		delete rule;
		this->addInstance(notificationName,
				  enabled,
				  type,
				  NULL,
				  NULL);
	}

	// Register category for configuration updates
	m_service->registerCategory(notificationName);
	m_service->registerCategoryChild(notificationName);

	return success;
}


/**
 * Add a delivery to a notification instance
 *
 * @param    config		          The configuration for the new instance.
 * @param    deliveryCategoryName The delivery name
 * @param    config	              The configuration of the delivery.
 * @return			True on success, false otherwise.
 */
bool NotificationManager::addDelivery(const ConfigCategory& config, const string &deliveryCategoryName, ConfigCategory &deliveryConfig)
{

	bool success;

	success = true;

	bool enabled;
	string rulePluginName;
	string deliveryPluginName;
	string deliveryPluginNameFirst;

	NOTIFICATION_TYPE type;
	string customText;
	if (!this->getConfigurationItems(config,
					 enabled,
					 rulePluginName,
					 deliveryPluginNameFirst,
					 type,
					 customText))
	{
		return false;
	}

	deliveryPluginName = deliveryConfig.getValue("plugin");

	string const notificationName = config.getName();

	DeliveryPlugin* deliver = this->createDeliveryCategory(notificationName, deliveryPluginName, true);

	if (deliver)
	{

		NotificationDelivery* theDelivery = NULL;

		// Call delivery "plugin_init" with configuration
		// and instantiate NotificationDelivery class
		if (deliver->init(deliveryConfig))
		{
			// Check and set registerIngest
			if (deliver->ingestData())
			{
				AssetTrackInfo* trackingInfo  = new AssetTrackInfo;
				trackingInfo->service = m_service;
				trackingInfo->notificationInstanceName = notificationName;
				trackingInfo->pluginName = deliver->getName();
				deliver->registerIngest((void *)ingestCB, (void *)trackingInfo);
			}

			// Check and set the NotificationService class
			if (deliver->getService())
			{
				deliver->registerService((void *)getService, (void *)m_service);

				// Call Plugin start
				deliver->start();
			}

			// Now create NotificationDelivery object
			theDelivery = new NotificationDelivery(deliveryCategoryName,
								notificationName,
								deliver,
								customText);
		}

		addDeliveryExtra(notificationName, type, theDelivery);
	}
	else
	{
		delete deliver;
		success = false;
	}

	return success;

}

/**
 * Identifies and add extra deliveries channels for a notification istance
 *
 * @param    name		The instance name to manage.
 * @param    config		The configuration for the new instance.
 * @return			True on success, false otherwise.
 */
bool NotificationManager::setupDeliveryExtra(const string& name, const ConfigCategory& config) {
	bool success = true;
	string notificationName = config.getName();
	string prefix = getDeliveryCategoryName(notificationName, "", true, true);
	ConfigCategories categories = m_managerClient->getChildCategories(notificationName);

	for (unsigned int idx = 0; idx < categories.length(); idx++)
	{
		string categoryName = categories[idx]->getName();

		if (categoryName.compare(0, prefix.size(), prefix) == 0)
		{

			ConfigCategory deliveryConfig = m_managerClient->getCategory(categoryName);

			success = addDelivery(config, categoryName, deliveryConfig);
			if (success)
			{
				// Register category for configuration updates
				m_service->registerCategory(categoryName);
			}
		}
	}

	return success;
}


/**
 * Create and add a new Notification instance.
 * Creates the first delivery and rule and extra deliveries if needed
 *
 * @param    name		The instance name to create.
 * @param    config		The configuration for the new instance.
 * @return			True on success, false otherwise.
 */
bool NotificationManager::setupInstance(const string& name,
					const ConfigCategory& config)
{
	 bool success;

	success = setupRuleDeliveryFirst (name, config);

	if (success) {

		success = setupDeliveryExtra (name, config);
	}

	return success;
}

/**
 * Update an existing notification instance
 *
 * @param    name		The  notification instance name
 * @param    newConfig		The new configuration to apply
 */
bool NotificationInstance::updateInstance(const string& name,
					  const ConfigCategory& newConfig)
{
	bool ret = false;
	bool enabled;
	string rulePluginName;
	string deliveryPluginName;
	NOTIFICATION_TYPE type;
	string customText;
	NotificationManager* instances =  NotificationManager::getInstance();
	// Parse new configuration object
	if (!instances->getConfigurationItems(newConfig,
					      enabled,
					      rulePluginName,
					      deliveryPluginName,
					      type,
					      customText))
	{
		return false;
	}

	NotificationSubscription* subscriptions = NotificationSubscription::getInstance();

	// Current instance is not enabled, new config has enable = true 
	if (enabled && !this->isEnabled())
	{
		bool enabled = false;
		Logger::getLogger()->info("Enabling notification instance '%s'",
					  name.c_str());

		// Remove current instance
		instances->removeInstance(name);

		// Create a new one with new configuration
		if (instances->setupInstance(name, newConfig))
		{
			// Get new instance
			// Protect access to m_instances
			{
				lock_guard<mutex> guard(instances->m_instancesMutex);
				bool ret;
				auto i = instances->getInstances().find(name);
				ret = i != instances->getInstances().end();
				if (ret)
				{
					// Create a new subscription
					subscriptions->createSubscription((*i).second);

					Logger::getLogger()->info("Successfully enabled notification instance '%s'",
								  name.c_str());
					enabled = true;
				}
			}
		}
		else
		{
			Logger::getLogger()->error("Errors found while enabling notification instance '%s' the notification will not be enabled",
						   name.c_str());
		}
		return enabled;
	}

	// Current notification is enabled, new configuration is disabling it.
	if (!enabled && this->isEnabled())
	{
		// Set disable flag
		this->disable();
		// Get rule name
		if (!this->getRule())
		{
			return false;
		}
		string ruleName = this->getRule()->getName();
		// Get all assets for this rule
		std::vector<NotificationDetail>& assets = this->getRule()->getAssets();

		// Unregister current subscriptions for this rule and
		// clean all current rule/asset buffers
		// remove all assets from the rule
		for (auto a = assets.begin();
			  a != assets.end(); )
		{
			lock_guard<mutex> guard(instances->m_instancesMutex);
			subscriptions->removeSubscription(a->getSource(),
							  a->getAssetName(),
							  ruleName);
			// Remove asset
			a = assets.erase(a);
		}

		// Just remove current instance
		instances->removeInstance(name);

		// Create a new one with new config
		bool ret = instances->setupInstance(name, newConfig);
		if (ret)
		{
			Logger::getLogger()->info("Successfully disabled notification instance '%s'",
						   name.c_str());
		}
		else
		{
			Logger::getLogger()->error("Errors found while disabling notification instance '%s'",
						   name.c_str());
		}

		// Just create a new one, not enabled, replacing current one
		return ret;
	}

	// Current instance is not enabled and
	// in the new configuration it's still not enabled
	if (!enabled && !this->isEnabled())
	{
		// Just remove current instance
		instances->removeInstance(name);

		// Just create a new one with new config
		return instances->setupInstance(name, newConfig);
	}

	/**
	 * This is an update with plugins, type etc:
	 *
	 * 1- Check rule/delivery plugin change:
	 *    remove instance & create a new one
	 * 2- Notification type change: update current instance
	 * 3- Custom text: it only affects delivery plugin:
	 *	easy way: remove instance & create a new one
	 * 4- ....
	 */

	if (!this->getRulePlugin() ||
	    !this->getDeliveryPlugin() ||
	    rulePluginName.compare(this->getRulePlugin()->getName()) != 0 ||
	    deliveryPluginName.compare(this->getDeliveryPlugin()->getName()) != 0)
	{
		bool retCode = false;

		// Set disable flag
		this->disable();
		// Get rule name
		if (this->getRule())
		{
			string ruleName = this->getRule()->getName();
			// Get all assets for this rule
			std::vector<NotificationDetail>& assets = this->getRule()->getAssets();

			// Unregister current subscriptions for this rule and
			// clean all current rule/asset buffers
			// remove all assets from the rule
			for (auto a = assets.begin();
			          a != assets.end(); )
			{
				lock_guard<mutex> guard(instances->m_instancesMutex);
				subscriptions->removeSubscription(a->getSource(),
						       		  a->getAssetName(),
								  ruleName);
				// Remove asset
				a = assets.erase(a);
			}
		}

		// Remove current instance
		instances->removeInstance(name);

		// Create a new one with new config
		if (instances->setupInstance(name, newConfig))
		{
			// Get new instance
			// Protect access to m_instances
			{
				lock_guard<mutex> guard(instances->m_instancesMutex);
				//instances->lockInstances();
				auto i = instances->getInstances().find(name);
				bool ret = i != instances->getInstances().end();
				//instances->unlockInstances();
				if (ret)
				{
					// Create a new subscription
					subscriptions->createSubscription((*i).second);
					retCode = true;
				}
			}
		}

		return retCode;
	}

	/**
	 * We can easily update some instance objects here
	 */

	// Update type
	this->setType(type);

	// Update custom text
	if (this->getDelivery() && !customText.empty())
	{
		this->getDelivery()->setText(customText);
	}

	return true;
}

/**
 * Remove an instance from instances map
 *
 * Rather than actually delete them we mark them as zombies
 * so that they will be deleted when we are sure the system is not
 * processing the notification.
 *
 * @param    instanceName	The instance name to remove.
 * @return			True for found instance removed,
 *				false otherwise.
 */
bool NotificationManager::removeInstance(const string& instanceName)
{
	bool ret = false;

	// Protect access to m_instances
	lock_guard<mutex> guard(m_instancesMutex);

	auto r = m_instances.find(instanceName);
	if (r != m_instances.end())
	{
		(*r).second->markAsZombie();
		ret = true;
		Logger::getLogger()->debug("Instance %s marked as Zombie",
					   instanceName.c_str());
	}
	return ret;
}

/**
 * Traverse all the instances and remove the zombies
 */
void NotificationManager::collectZombies()
{
	lock_guard<mutex> guard(m_instancesMutex);
	for (auto r = m_instances.begin();
		  r != m_instances.end(); )
	{
		if (!r->second)
		{
			Logger::getLogger()->debug("Instance has NULL object, size %lu",
						   m_instances.size());
		}

		if (r->second &&
		    r->second->isZombie())
		{
			Logger::getLogger()->debug("Instance %s removed from m_instances",
					   r->second->getName().c_str());
			// Free memory
			delete r->second;
			r->second = NULL;
			// Remove element
			r = m_instances.erase(r);
		}
		else
		{
			++r;
		}
	}
}

/**
 * Get instance configuration items.
 *
 * @param    config			The instance configuration object.
 * @param    enabled			Enable output parameter.
 * @param    rulePluginName		The rule plugin output parameter.
 * @param    deliveryPluginName		The delivery plugin output parameter.
 * @param    nType			The notification type output parameter.
 * @param    customText			The custom text output parameter.
 * @return				True is configuration parsing succeded,
 *					false otherwise.
 */
bool NotificationManager::getConfigurationItems(const ConfigCategory& config,
						bool& enabled,
						string& rulePluginName,
						string& deliveryPluginName,
						NOTIFICATION_TYPE& nType,
						string& customText)
{
	long retriggerTime = DEFAULT_RETRIGGER_TIME;
	struct timeval retriggerTimeTv;
        retriggerTimeTv.tv_sec = DEFAULT_RETRIGGER_TIME;
	string notificationName = config.getName();
	// The rule plugin to use
	rulePluginName = config.getValue("rule");
	// The delivery plugin to use
	deliveryPluginName = config.getValue("channel");
	// Is it enabled?
	enabled = config.getValue("enable").compare("true") == 0 ||
		  config.getValue("enable").compare("True") == 0;

	// Re-trigger time
	if (config.itemExists("retrigger_time") &&
	    !config.getValue("retrigger_time").empty())
	{
		double new_value = atof(config.getValue("retrigger_time").c_str());
		if (new_value >= 0)
		{
			retriggerTimeTv.tv_sec = (int)new_value;
			double intPart;
			double fractPart;
			fractPart = modf(new_value, &intPart);
			retriggerTimeTv.tv_usec = 1000000 * fractPart;
		}
	}
	nType.retriggerTimeTv = retriggerTimeTv;

	// Get notification type
	string notification_type;
	if (config.itemExists("notification_type") &&
	    !config.getValue("notification_type").empty())
	{
		notification_type = config.getValue("notification_type");
	}
	else
	{
		m_logger->error("Unable to fetch Notification type "
				"in Notification instance '" + \
				notificationName + "' configuration.");
		return false;
	}
	nType.type = this->parseType(notification_type);
	if (nType.type == E_NOTIFICATION_TYPE::None)
	{
		m_logger->error("Found unsupported Notification type '" + \
				notification_type + \
				"' in Notification instance '" + \
				notificationName + "' configuration.");
		return false;
	}

	// Get custom text message for delivery
	if (config.itemExists("text"))
	{
		customText = config.getValue("text");
	}

	if (enabled && rulePluginName.empty())
	{
		m_logger->error("Unable to fetch Notification Rule "
				"plugin name from Notification instance '" + \
				notificationName + "' configuration.");
		return false;
	}
	if (enabled && deliveryPluginName.empty())
	{
		m_logger->error("Unable to fetch Notification Delivery "
				"plugin name from Notification instance '" + \
				notificationName + "' configuration");
		return false;
	}

	return true;
}

/**
 * Audit log entry for sent notification
 *
 * @param       notificationName	The notification just delivered
 * @param       reason			The trigger reason as a JSON string
 * @return				True on success, false otherwise
 */
bool NotificationManager::auditNotification(const string& notificationName,
					    const string& reason)
{
	Document JSONData;
	JSONData.Parse(reason.c_str());
	if (JSONData.HasParseError() ||
	    !JSONData.HasMember("reason") ||
	    !JSONData["reason"].IsString())
	{
		m_logger->error("Failed to parse trigger_reason JSON '%s' "
				"for notification '%s'",
				reason.c_str(),
				notificationName.c_str());
		return false;
	}
	string audit = JSONData["reason"].GetString();
	return m_managerClient->addAuditEntry((audit.compare("cleared") == 0 ?
					       "NTFCL" :
					       "NTFSN"),
					      "INFORMATION",
					      "{\"name\": \"" + notificationName + "\"}");
}

/**
 * Remove an instance via API call
 *
 * Update notification statistics
 *
 * @param    instanceName	The instance name to remove.
 * @return			True for found instance removed,
 *				false otherwise.
 */
bool NotificationManager::APIdeleteInstance(const string& instanceName)
{
	NotificationManager* notifications = NotificationManager::getInstance();
	NotificationInstance* instance = NULL;

	{
	lock_guard<mutex> guard(m_instancesMutex);

	instance = notifications->getNotificationInstance(instanceName);

	if (instance)
	{
		NotificationSubscription* subscriptions = NotificationSubscription::getInstance();
		NotificationRule *rule = instance->getRule();
		if (rule)
		{
			string ruleName = rule->getName();
			// Get all assets for this rule
			std::vector<NotificationDetail>& assets = rule->getAssets();

			// Unregister current subscriptions for this rule and
			// clean all current rule/asset buffers
			// remove all assets from the rule
			for (auto a = assets.begin();
			     a != assets.end(); )
			{
				subscriptions->removeSubscription(a->getSource(),
								  a->getAssetName(),
								  ruleName);
				// Remove asset
				a = assets.erase(a);
			}
		}
	}
	}

	bool ret = this->removeInstance(instanceName);

	if (ret)
	{
		m_stats.removed++;
		m_stats.total--;
	}

	return ret;
}
