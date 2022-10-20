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
SubscriptionElement::SubscriptionElement(const std::string& assetName,
					 const std::string& notificationName,
					 NotificationInstance* notification) :
					 m_asset(assetName),
					 m_name(notificationName),
					 m_notification(notification)
{
}

/**
 * SubscriptionElement class destructor
 */
SubscriptionElement::~SubscriptionElement()
{}

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
 * and register the Reading asset notification to storage service.
 *
 * Different subscription objects can be added to
 * to existing ones per assetName. 
 *
 * @param    assetName		The assetName to register for notifications
 * @param    element		The Subscription object to add
 *				to current subscriptions.
 * @return			True on succes, false otherwise.
 */
bool NotificationSubscription::addSubscription(const std::string& assetName,
					       SubscriptionElement& element)
{

	// Get NotificationAPI instance
	NotificationApi* api = NotificationApi::getInstance();
	// Get callback URL
	string callBackURL = api->getCallBackURL();

	if (callBackURL.empty())
	{
		m_logger->fatal(" Error while registering asset '" + \
				assetName + "' for notification " + \
				element.getNotificationName() + \
				" callback URL is not set");
		return false;
	}

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
 * @param    assetName		The asset name to unregister
 */
void NotificationSubscription::unregisterSubscription(const string& assetName)
{
	// Get NotificationAPI instance
	NotificationApi* api = NotificationApi::getInstance();
	// Get callback URL
	string callBackURL = api->getCallBackURL();

	// Get all NotificationSubscriptions
	std:map<std::string, std::vector<SubscriptionElement>>&
		subscriptions = this->getAllSubscriptions();
	auto it = subscriptions.find(assetName);

	if (it != subscriptions.end())
	{
		// Unregister interest
		m_storage.unregisterAssetNotification((*it).first,
						      callBackURL + urlEncode(assetName));

		m_logger->info("Unregistering asset '" + \
				assetName + "' for notification " + \
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

		// Get "asset" objects
		for (Value::ConstValueIterator itr = triggers.Begin();
					       itr != triggers.End();
					       ++itr)
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
			SubscriptionElement subscription(asset,
							 instance->getName(),
							 instance);

			// Add subscription and register asset interest
			lock_guard<mutex> guard(m_subscriptionMutex);
			ret = this->addSubscription(asset, subscription);
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
						  const string& ruleName, bool storageServiceRestartPending)
{
	PRINT_FUNC;
	// Get all instances
	NotificationManager* manager = NotificationManager::getInstance();
	PRINT_FUNC;
	// Get subscriptions for assetName
	this->lockSubscriptions();
	PRINT_FUNC;
	map<string, vector<SubscriptionElement>>&
		allSubscriptions = this->getAllSubscriptions();
	PRINT_FUNC;
	auto it = allSubscriptions.find(assetName);
	bool ret = it != allSubscriptions.end();
	PRINT_FUNC;

	// For the found assetName subscriptions
	// 1- Unsubscribe notification interest for assetNamme
	// 2- Remove data in buffer[ruleName][assetName]
	// 3- Remove ruleName object fot assetName
	// 4- Remove Subscription
	if (ret)
	{
		PRINT_FUNC;
		vector<SubscriptionElement>& elems = (*it).second;
		if (elems.size() == 1)
		{
				PRINT_FUNC;
		        // 1- We have only one subscription for current asset
		        // call unregister interest
		        if (!storageServiceRestartPending)
			        this->unregisterSubscription(assetName);
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
	PRINT_FUNC;
	this->unlockSubscriptions();
	PRINT_FUNC;
}


/**
 * Remove a given subscription
 *
 * @param    assetName		The register assetName for notifications
 * @param    ruleName		The associated ruleName
 */
void NotificationSubscription::removeAllSubscriptions(bool storageServiceRestartPending)
{
	// Get all NotificationSubscriptions
	m_subscriptionMutex.lock();
	PRINT_FUNC;
	// std::map<std::string assetName, std::vector<SubscriptionElement>>
	auto & subscriptions = this->getAllSubscriptions();
	PRINT_FUNC;
	Logger::getLogger()->info("%s:%d: subscriptions.size()=%d", __FUNCTION__, __LINE__, subscriptions.size());
	PRINT_FUNC;
	m_subscriptionMutex.unlock();
	
	int n=0;
	for (auto & it : subscriptions)
	{
		PRINT_FUNC;
		std::string assetName = it.first;
		PRINT_FUNC;
		auto & seVector = it.second;
		PRINT_FUNC;
		Logger::getLogger()->info("%s:%d: assetName=%s, SubscriptionElement vec size=%d", 
									__FUNCTION__, __LINE__, assetName.c_str(), seVector.size());
		PRINT_FUNC;
		for(auto & se : seVector)
		{
			PRINT_FUNC;
			if (!se.getRule())
				break;
			Logger::getLogger()->info("SubscriptionElement: assetName=%s, ruleName=%s", 
										se.getAssetName().c_str(), se.getRule()->getName().c_str());
			removeSubscription(se.getAssetName(), se.getRule()->getName(), storageServiceRestartPending);
		}
		PRINT_FUNC;
		n++;
		Logger::getLogger()->info("%s:%d: n=%d, subscriptions.size()=%d", 
									__FUNCTION__, __LINE__, n, subscriptions.size());
		if (n >= subscriptions.size())
			break;
	}
	PRINT_FUNC;
}

