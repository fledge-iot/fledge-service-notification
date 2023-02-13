/*
 * Fledge notification subscription.
 *
 * Copyright (c) 2018 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */
#include <notification_service.h>
#include <management_api.h>
#include <management_client.h>
#include <service_record.h>
#include <plugin_api.h>
#include <plugin.h>
#include <logger.h>
#include <iostream>
#include <string>
#include <string_utils.h>
#include <notification_subscription.h>
#include <notification_api.h>
#include <notification_queue.h>
#include <rule_plugin.h>
#include <delivery_plugin.h>
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

NotificationSubscription* NotificationSubscription::m_instance = 0;

/**
 * SubscriptionElement class constructor
 */
SubscriptionElement::SubscriptionElement(const std::string& notificationName,
					 NotificationInstance* notification) :
					 m_name(notificationName),
					 m_notification(notification)
{
}

/**
 * SubscriptionElement class destructor
 */
SubscriptionElement::~SubscriptionElement()
{
}

/**
 * Constructor for asset subscription elements
 */
AssetSubscriptionElement::AssetSubscriptionElement(const std::string& assetName,
                                    const std::string& notificationName,
                                    NotificationInstance* notification) :
					m_asset(assetName),
					SubscriptionElement(notificationName, notification)
{
}

/**
 * SubscriptionElement class destructor
 */
AssetSubscriptionElement::~AssetSubscriptionElement()
{
}

/**
 * Constructor for audit subscription elements
 */
AuditSubscriptionElement::AuditSubscriptionElement(const std::string& auditCode,
                                    const std::string& notificationName,
                                    NotificationInstance* notification) :
					m_code(auditCode),
					SubscriptionElement(notificationName, notification)
{
}

/**
 * SubscriptionElement class destructor
 */
AuditSubscriptionElement::~AuditSubscriptionElement()
{
}

/**
 * Constructor for statistics subscription elements
 */
StatsSubscriptionElement::StatsSubscriptionElement(const std::string& stat,
                                    const std::string& notificationName,
                                    NotificationInstance* notification) :
					m_stat(stat),
					SubscriptionElement(notificationName, notification)
{
}

/**
 * SubscriptionElement class destructor
 */
StatsSubscriptionElement::~StatsSubscriptionElement()
{
}

/**
 * Constructor for statistics subscription elements
 */
StatsRateSubscriptionElement::StatsRateSubscriptionElement(const std::string& stat,
                                    const std::string& notificationName,
                                    NotificationInstance* notification) :
					m_stat(stat),
					SubscriptionElement(notificationName, notification)
{
}

/**
 * SubscriptionElement class destructor
 */
StatsRateSubscriptionElement::~StatsRateSubscriptionElement()
{
}

/**
 * Constructor for the NotificationSubscription class
 */
NotificationSubscription::NotificationSubscription(const string& notificationName,
						   StorageClient& storageClient) :
						   m_name(notificationName),
						   m_storage(storageClient)
{
	// Set instance
	m_instance = this;

	// get logger
	m_logger = Logger::getLogger();
}

/*
 * Destructor for the NotificationSubscription class
 */
NotificationSubscription::~NotificationSubscription()
{
	this->getAllSubscriptions().clear();
}

/**
 * Unregister subscriptions to storage server:
 * NOTE: subscriptions object are not deleted right now.
 */
void NotificationSubscription::unregisterSubscriptions()
{
	// Get NotificationAPI instance
	NotificationApi* api = NotificationApi::getInstance();
	// Get callback URL
	string callBackURL = api->getCallBackURL();

	// Get all NotificationSubscriptions
	m_subscriptionMutex.lock();
	std:map<std::string, std::vector<SubscriptionElement>>&
		subscriptions = this->getAllSubscriptions();

	for (auto it = subscriptions.begin();
		  it != subscriptions.end();
		  ++it)
	{
		// Unregister interest
		m_storage.unregisterAssetNotification((*it).first,
						      callBackURL + urlEncode((*it).first));
		m_logger->info("Unregistering asset '" + \
			       (*it).first + "' for notification " + \
			       this->getNotificationName());
	}
	m_subscriptionMutex.unlock();
}

/**
 * Populate the Subscriptions map given the asset name
 * in "plugin_triggers" call of all rule plugins belonging to
 * registered Notification rules in NotificationManager intances.
 * Register also interest to Storage server for asset names.
 */
void NotificationSubscription::registerSubscriptions()
{
	// Get NotificationManager instance
	NotificationManager* manager = NotificationManager::getInstance();
	// Get all Notification instances
	manager->lockInstances();
	std::map<std::string, NotificationInstance *>& instances = manager->getInstances();

	for (auto it = instances.begin();
		  it != instances.end();
		  ++it)
	{
		// Get asset names from plugin_triggers call
		NotificationInstance* instance = (*it).second;
		if (!instance)
		{
			m_logger->error("Notification instance %s is NULL",
					(*it).first.c_str());
			continue;
		}

		if (!instance->isEnabled())
		{
			m_logger->info("Notification instance %s is not enabled.",
				       (*it).first.c_str());
			continue;
		}

		// Create a new subscription
		bool ret = this->createSubscription(instance);
	}
	// Unlock instances
	manager->unlockInstances();
}

/**
 * Add a subscription object to Subscriptions
 * and register with the storage layer for notification
 * of the appropriate data
 *
 * Different subscription objects can be added to
 * to existing ones per assetName. 
 *
 * @param    element		The Subscription object to add
 *				to current subscriptions.
 * @return			True on succes, false otherwise.
 */
bool NotificationSubscription::addSubscription(const SubscriptionElement& element)
{

	// Get NotificationAPI instance
	NotificationApi* api = NotificationApi::getInstance();
	// Get callback URL
	string callBackURL = api->getCallBackURL();
	string auditCallbackURL = api->getAuditCallbackURL();
	string statsCallbackURL = api->getStatsCallbackURL();

	if (callBackURL.empty())
	{
		m_logger->error("Unable to add subscription for notification, URL is empty");
		return false;
	}

	if (typeid(element) == typeid(AssetSubscriptionElement))
	{
		const AssetSubscriptionElement *elem = (AssetSubscriptionElement *)&element;
		const string& assetName = elem->getAssetName();
		/**
		 * We can have different Subscriptions for each asset:
		 * add new one into the vector
		 */
		m_subscriptions[assetName].push_back(element);

		// Register once per asset Notification interest to Storage server
		if (m_subscriptions[assetName].size() == 1)
		{
			m_storage.registerAssetNotification(assetName,
							    (callBackURL + urlEncode(assetName)));

			m_logger->info("Registering asset '" + \
				       assetName + "' for notification " + \
				       element.getNotificationName());
		}

		m_logger->info("Subscription for asset '" + assetName + \
			       "' has # " + to_string(m_subscriptions[assetName].size()) + " rules");
	}
	else if (typeid(element) == typeid(AuditSubscriptionElement))
	{
		const AuditSubscriptionElement *elem = (AuditSubscriptionElement *)&element;
		const string& code = elem->getAuditCode();
		/**
		 * We can have different Subscriptions for each asset:
		 * add new one into the vector
		 */
		m_subscriptions[code].push_back(element);

		// Register once per asset Notification interest to Storage server
		if (m_subscriptions[code].size() == 1)
		{
			string operation = "insert";
			string tableName = "log";
			string key = "code";
			vector<string> keyValues;
			keyValues.push_back(code);
			m_storage.registerTableNotification(tableName, key, keyValues, operation, (auditCallbackURL + urlEncode(code)));

			m_logger->info("Registering key '" + \
				       code + "' for notification " + \
				       element.getNotificationName());
		}
	}
	else if (typeid(element) == typeid(StatsSubscriptionElement))
	{
		const StatsSubscriptionElement *elem = (StatsSubscriptionElement *)&element;
		const string& stat = elem->getStatistic();
		/**
		 * We can have different Subscriptions for each asset:
		 * add new one into the vector
		 */
		m_subscriptions[stat].push_back(element);

		// Register once per asset Notification interest to Storage server
		if (m_subscriptions[stat].size() == 1)
		{
			string operation = "update";
			string tableName = "statistics";
			string key = "key";
			vector<string> keyValues;
			keyValues.push_back(stat);
			m_storage.registerTableNotification(tableName, key, keyValues, operation, (statsCallbackURL + urlEncode(stat)));

			m_logger->info("Registering key '" + \
				       stat + "' for notification " + \
				       element.getNotificationName());
		}
	}
	else if (typeid(element) == typeid(StatsRateSubscriptionElement))
	{
		const StatsRateSubscriptionElement *elem = (StatsRateSubscriptionElement *)&element;
		const string& stat = elem->getStatistic();
		/**
		 * We can have different Subscriptions for each asset:
		 * add new one into the vector
		 */
		m_subscriptions[stat].push_back(element);

		// Register once per asset Notification interest to Storage server
		if (m_subscriptions[stat].size() == 1)
		{
			string operation = "insert";
			string tableName = "statistics_history";
			string key = "key";
			vector<string> keyValues;
			keyValues.push_back(stat);
			m_storage.registerTableNotification(tableName, key, keyValues, operation, (statsCallbackURL + urlEncode(stat)));

			m_logger->info("Registering key '" + \
				       stat + "' for notification " + \
				       element.getNotificationName());
		}
	}

	return true;
}

/**
 * Check for notification evaluation type in the input JSON object
 *
 * @param    value	The input JSON object 
 * @return		NotificationType object 
 */
EvaluationType NotificationSubscription::getEvalType(const Value& value)
{
	// Default is SingleItem, so set time = 0
	time_t interval = 0;
	EvaluationType::EVAL_TYPE evaluation = EvaluationType::SingleItem;

	if (value.HasMember("All"))
	{
		interval = value["All"].GetUint();
		evaluation = EvaluationType::All;
	}
	else if (value.HasMember("Average"))
	{
		interval = value["Average"].GetUint();
		evaluation = EvaluationType::Average;
	}
	else if (value.HasMember("Minimum"))
	{
		interval = value["Minimum"].GetUint();
		evaluation = EvaluationType::Minimum;
	}
	else if (value.HasMember("Maximum"))
	{
		interval = value["Maximum"].GetUint();
		evaluation = EvaluationType::Maximum;
	}

	return EvaluationType(evaluation, interval);
}

/**
 * Unregister a single subscription from storage layer
 *
 * The caller of this routine must hold the subscriptions lock
 *
 * @param    element		The subscription element to unregister
 */
void NotificationSubscription::unregisterSubscription(const SubscriptionElement& element)
{
	// Get NotificationAPI instance
	NotificationApi* api = NotificationApi::getInstance();
	// Get callback URL
	string callBackURL = api->getCallBackURL();

	// Get all NotificationSubscriptions
	string name;
	if (typeid(element) == typeid(AssetSubscriptionElement))
	{
		const AssetSubscriptionElement *elem = static_cast<const AssetSubscriptionElement *>(&element);
		name = elem->getAssetName();
	}
	else if (typeid(element) == typeid(AuditSubscriptionElement))
	{
		const AuditSubscriptionElement *elem = static_cast<const AuditSubscriptionElement *>(&element);
		name = elem->getAuditCode();
	}
	else if (typeid(element) == typeid(StatsSubscriptionElement))
	{
		const StatsSubscriptionElement *elem = static_cast<const StatsSubscriptionElement *>(&element);
		name = elem->getStatistic();
	}
	else if (typeid(element) == typeid(StatsRateSubscriptionElement))
	{
		const StatsRateSubscriptionElement *elem = static_cast<const StatsRateSubscriptionElement *>(&element);
		name = elem->getStatistic();
	}
	else
	{
		Logger::getLogger()->error("Internal error, attempting to unregister un unsupported subscription type");
		return;
	}
	std:map<std::string, std::vector<SubscriptionElement>>&
		subscriptions = this->getAllSubscriptions();
	auto it = subscriptions.find(name);

	if (it != subscriptions.end())
	{
		// Unregister interest
		if (typeid(element) == typeid(AssetSubscriptionElement))
		{
			m_storage.unregisterAssetNotification((*it).first,
						      callBackURL + urlEncode(name));
		}
		else if (typeid(element) == typeid(AuditSubscriptionElement))
		{
			const AuditSubscriptionElement *elem = static_cast<const AuditSubscriptionElement *>(&element);
			vector<std::string> keyValues;
			keyValues.push_back(elem->getAuditCode());
			m_storage.unregisterTableNotification("log", "code", keyValues, "insert", callBackURL);
		}
		else if (typeid(element) == typeid(StatsSubscriptionElement))
		{
			const StatsSubscriptionElement *elem = static_cast<const StatsSubscriptionElement *>(&element);
			vector<std::string> keyValues;
			keyValues.push_back(elem->getStatistic());
			m_storage.unregisterTableNotification("statistics", "key", keyValues, "update", callBackURL);
		}
		else if (typeid(element) == typeid(StatsRateSubscriptionElement))
		{
			const StatsRateSubscriptionElement *elem = static_cast<const StatsRateSubscriptionElement *>(&element);
			vector<std::string> keyValues;
			keyValues.push_back(elem->getStatistic());
			m_storage.unregisterTableNotification("statistics_history", "key", keyValues, "insert", callBackURL);
		}

		m_logger->info("Unregistering asset '" + \
				name + "' for notification " + \
				this->getNotificationName());
	}
}

/**
 * Create a SubscriptionElement object and register interest for asset names
 *
 * @param    instance		The notification instance
 *				with already set rule and delivery plugins
 * @return			True on success, false on errors
 */
bool NotificationSubscription::createSubscription(NotificationInstance* instance)
{
	bool ret = false;
	// Get RulePlugin
	RulePlugin* rulePluginInstance = instance->getRulePlugin();
	
	bool registerTableNotification = false;
	if (rulePluginInstance->getName() == "DataAvailability") 
	{
		registerTableNotification = true;
	}
	// Get DeliveryPlugin
	DeliveryPlugin* deliveryPluginInstance = instance->getDeliveryPlugin();

	if (rulePluginInstance)
	{
		// Call "plugin_triggers"
		string document = rulePluginInstance->triggers();

		Document JSONData;
		JSONData.Parse(document.c_str());
		if (JSONData.HasParseError() ||
		    !JSONData.HasMember("triggers") ||
		    !JSONData["triggers"].IsArray())
		{
			m_logger->error("Failed to parse %s plugin_triggers JSON data %s",
					rulePluginInstance->getName().c_str(),
					document.c_str());
			return false;
		}

		const Value& triggers = JSONData["triggers"];
		if (!triggers.Size())
		{
			m_logger->info("No triggers set for %s plugin",
				       rulePluginInstance->getName().c_str());
			return false;
		}
		m_logger->info("Triggers set for %s rule plugin: %s",
				       rulePluginInstance->getName().c_str(),
				       document.c_str());

		string ruleName = instance->getRule()->getName();
		NotificationRule* theRule = instance->getRule();
		uint64_t timeBasedInterval = 0;

		// Get "interval" parameter first
		if (JSONData.HasMember("interval"))
		{
			timeBasedInterval = JSONData["interval"].GetUint64();
			if (timeBasedInterval > 0)
			{
				theRule->setTimeBased(timeBasedInterval);
				m_logger->debug("Setting time based rule %s with interval %ld",
					ruleName.c_str(),
					timeBasedInterval);
			}
		}

		// Get "evaluate" parameter for Multiple Trigger Evaluation Control
		if (JSONData.HasMember("evaluate"))
		{
			string value = JSONData["evaluate"].GetString();
			if (value == "any")
			{
				theRule->setMultipleEvaluation(NotificationRule::MultipleEvaluation::M_ANY);
			}
			if (value == "interval" && timeBasedInterval > 0)
			{
				theRule->setMultipleEvaluation(NotificationRule::MultipleEvaluation::M_INTERVAL);
			}
		}

		// Get "asset", "audit", "statistic" or "statisticRate"  objects
		for (Value::ConstValueIterator itr = triggers.Begin();
					       itr != triggers.End();
					       ++itr)
		{
			if (itr->HasMember("asset"))
			{
				// Get asset name
				string asset = (*itr)["asset"].GetString();

				// Get optional evaluation type and time period for asset:
				// (All :30, Minimum: 10, Maximum: 10, Average: 10)
				// If time based rule is set then
				// set EvaluationType::Interval for data buffer operation
				EvaluationType type = theRule->isTimeBased() ?
					EvaluationType(EvaluationType::Interval, timeBasedInterval) :
					this->getEvalType(*itr);

				// Create NotificationDetail object
				NotificationDetail assetInfo(asset,
							     ruleName,
							     type);

				// Add assetInfo to its rule
				theRule->addAsset(assetInfo);

				// Create subscription object
				AssetSubscriptionElement subscription(asset,
								 instance->getName(),
								 instance);

				lock_guard<mutex> guard(m_subscriptionMutex);
				ret = this->addSubscription(subscription);
			}
			else if (itr->HasMember("audit"))
			{
				string code = (*itr)["audit"].GetString();

				// Get optional evaluation type and time period for asset:
				// (All :30, Minimum: 10, Maximum: 10, Average: 10)
				// If time based rule is set then
				// set EvaluationType::Interval for data buffer operation
				EvaluationType type = theRule->isTimeBased() ?
					EvaluationType(EvaluationType::Interval, timeBasedInterval) :
					this->getEvalType(*itr);

				// Create NotificationDetail object
				NotificationDetail auditInfo(code,
							     ruleName,
							     type);

				// Add assetInfo to its rule
				theRule->addAsset(auditInfo);

				AuditSubscriptionElement subscription(code,
								 instance->getName(),
								 instance);
				lock_guard<mutex> guard(m_subscriptionMutex);
				ret = this->addSubscription(subscription);

			}
			else if (itr->HasMember("statistic"))
			{
				string stat = (*itr)["statistic"].GetString();

				// Get optional evaluation type and time period for asset:
				// (All :30, Minimum: 10, Maximum: 10, Average: 10)
				// If time based rule is set then
				// set EvaluationType::Interval for data buffer operation
				EvaluationType type = theRule->isTimeBased() ?
					EvaluationType(EvaluationType::Interval, timeBasedInterval) :
					this->getEvalType(*itr);

				// Create NotificationDetail object
				NotificationDetail statsInfo(stat,
							     ruleName,
							     type);

				// Add assetInfo to its rule
				theRule->addAsset(statsInfo);

				StatsSubscriptionElement subscription(stat,
								 instance->getName(),
								 instance);
				lock_guard<mutex> guard(m_subscriptionMutex);
				ret = this->addSubscription(subscription);

			}
			else if (itr->HasMember("statisticRate"))
			{
				string rate = (*itr)["statisticRate"].GetString();

				// Get optional evaluation type and time period for asset:
				// (All :30, Minimum: 10, Maximum: 10, Average: 10)
				// If time based rule is set then
				// set EvaluationType::Interval for data buffer operation
				EvaluationType type = theRule->isTimeBased() ?
					EvaluationType(EvaluationType::Interval, timeBasedInterval) :
					this->getEvalType(*itr);

				// Create NotificationDetail object
				NotificationDetail rateInfo(rate,
							     ruleName,
							     type);

				// Add assetInfo to its rule
				theRule->addAsset(rateInfo);

				StatsRateSubscriptionElement subscription(rate,
								 instance->getName(),
								 instance);
				lock_guard<mutex> guard(m_subscriptionMutex);
				ret = this->addSubscription(subscription);

			}
			else
			{
				Logger::getLogger()->error("Internal error %s has not valid trigger data", instance->getName());
			}
		}
	}
	return ret;
}

/**
 * Remove a given subscription
 *
 * @param    assetName		The register assetName for notifications
 * @param    ruleName		The associated ruleName
 */
void NotificationSubscription::removeSubscription(const string& assetName,
						  const string& ruleName)
{
	// Get all instances
	NotificationManager* manager = NotificationManager::getInstance();

	// Get subscriptions for assetName
	this->lockSubscriptions();
	map<string, vector<SubscriptionElement>>&
		allSubscriptions = this->getAllSubscriptions();
	auto it = allSubscriptions.find(assetName);
	bool ret = it != allSubscriptions.end();
	
	// For the found assetName subscriptions
	// 1- Unsubscribe notification interest for assetNamme
	// 2- Remove data in buffer[ruleName][assetName]
	// 3- Remove ruleName object fot assetName
	// 4- Remove Subscription
	if (ret)
	{
		vector<SubscriptionElement>& elems = (*it).second;
		if (elems.size() == 1)
		{
		        // 1- We have only one subscription for current asset
		        // call unregister interest
			NotificationInstance* instance = elems[0].getInstance();
			// Get RulePlugin
			RulePlugin* rulePluginInstance = instance->getRulePlugin();

			this->unregisterSubscription(elems[0]);
		        
		}

		// Get Notification queue instance
		NotificationQueue* queue =  NotificationQueue::getInstance();
		// 2- Remove all data in buffer[ruleName][assetName]
		queue->clearBufferData(ruleName, assetName);

		// 3- Check all subscriptions rules for given assetName
		for (auto e = elems.begin();
			  e != elems.end(); )
		{
			// Get notification rule object 
			string notificationName = (*e).getNotificationName();
			NotificationInstance* instance = manager->getNotificationInstance(notificationName);

			if (instance &&
			    !instance->isZombie())
			{
				string currentRule = instance->getRule()->getName();
				if (currentRule.compare(ruleName) == 0)
				{
					// 3- Remove this ruleName from array
					Logger::getLogger()->debug("Notification instance %s: removed subscription %s for asset %s",
								   notificationName.c_str(),
								   currentRule.c_str(),
								   assetName.c_str());
					e = elems.erase(e);
				}
				else
				{
					Logger::getLogger()->debug("Notification instance %s: Not removing subscription %s for asset %s",
								   notificationName.c_str(),
								   currentRule.c_str(),
								   assetName.c_str());
					++e;
				}
			}
			else
			{
				if (!instance)
				{
					Logger::getLogger()->debug("Notification instance %s has not been found, for asset %s",
								   notificationName.c_str(),
								   assetName.c_str());
				}

				++e;
			}
		}

		// 4- Remove subscription if array is empty
		if (!elems.size())
		{
			allSubscriptions.erase(it);
		}
	}
	this->unlockSubscriptions();
}
