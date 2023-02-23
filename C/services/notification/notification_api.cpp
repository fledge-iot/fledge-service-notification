/*
 * Fledge Notification API class for Notification micro service.
 *
 * Copyright (c) 2018 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */
#include "client_http.hpp"
#include "server_http.hpp"
#include "string_utils.h"
#include "notification_api.h"
#include "management_api.h"
#include "notification_manager.h"
#include "notification_subscription.h"
#include "notification_queue.h"


NotificationApi* NotificationApi::m_instance = 0;

using namespace std;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

/**
 * Wrapper function for the notification POST callback API call used for audit events.
 *
 * POST /notification/reading/statisticRate/{statistic}
 *
 * @param response	The response stream to send the response on
 * @param request	The HTTP request
 */
void notificationStatsRateReceiveWrapper(shared_ptr<HttpServer::Response> response,
				shared_ptr<HttpServer::Request> request)
{
	Logger::getLogger()->debug("Statistics rate callback received");
	NotificationApi* api = NotificationApi::getInstance();
	api->processStatsRateCallback(response, request);
}

/**
 * Wrapper function for the notification POST callback API call used for audit events.
 *
 * POST /notification/reading/statistic/{statistic}
 *
 * @param response	The response stream to send the response on
 * @param request	The HTTP request
 */
void notificationStatsReceiveWrapper(shared_ptr<HttpServer::Response> response,
				shared_ptr<HttpServer::Request> request)
{
	Logger::getLogger()->debug("Statistics callback received");
	NotificationApi* api = NotificationApi::getInstance();
	api->processStatsCallback(response, request);
}

/**
 * Wrapper function for the notification POST callback API call used for audit events.
 *
 * POST /notification/reading/audit/{auditCode}
 *
 * @param response	The response stream to send the response on
 * @param request	The HTTP request
 */
void notificationAuditReceiveWrapper(shared_ptr<HttpServer::Response> response,
				shared_ptr<HttpServer::Request> request)
{
	Logger::getLogger()->debug("Audit callback received");
	NotificationApi* api = NotificationApi::getInstance();
	api->processAuditCallback(response, request);
}

/**
 * Wrapper function for the notification POST callback API call.
 *
 * POST /notification/reading/asset/{assetName}
 *
 * @param response	The response stream to send the response on
 * @param request	The HTTP request
 */
void notificationReceiveWrapper(shared_ptr<HttpServer::Response> response,
				shared_ptr<HttpServer::Request> request)
{
	NotificationApi* api = NotificationApi::getInstance();
	api->processCallback(response, request);
}

/**
 * Wrapper for GET /notification
 *
 * Reply to caller with a JSON string of all loaded Notification instances
 *
 * @param response	The response stream to send the response on
 * @param request	The HTTP request
 */
void notificationGetInstances(shared_ptr<HttpServer::Response> response,
			      shared_ptr<HttpServer::Request> request)
{
	NotificationApi* api = NotificationApi::getInstance();
	api->getNotificationObject(NotificationApi::ObjGetNotificationsAll,
				   response,
				   request);
}

/**
 * Wrapper for GET /notification/rules
 *
 * Return a list of all the rules that are available on the notification server.
 * This is a list of all the built in rules and all the
 * currently loaded rules plugins.
*/
void notificationGetRules(shared_ptr<HttpServer::Response> response,
			  shared_ptr<HttpServer::Request> request)
{
	NotificationApi* api = NotificationApi::getInstance();
	api->getNotificationObject(NotificationApi::ObjGetRulesAll,
				   response,
				   request);
}

/**
 * Wrapper for GET /notification/delivery
 *
 * Return the list of delivery mechanisms, i.e. plugins,
 * installed on the notification service.
 */
void notificationGetDelivery(shared_ptr<HttpServer::Response> response,
			     shared_ptr<HttpServer::Request> request)
{
	NotificationApi* api = NotificationApi::getInstance();
	api->getNotificationObject(NotificationApi::ObjGetDeliveryAll,
				   response,
				   request);
}

/**
 * Wrapper for POST /notification/{notificationName}
 */
void notificationCreateNotification(shared_ptr<HttpServer::Response> response,
				    shared_ptr<HttpServer::Request> request)
{
	NotificationApi* api = NotificationApi::getInstance();
	api->getNotificationObject(NotificationApi::ObjCreateNotification,
				   response,
				   request);
}

/**
 * Wrapper for POST /notification/{NotificationName}/rule/{RuleName}
 */
void notificationCreateNotificationRule(shared_ptr<HttpServer::Response> response,
				    shared_ptr<HttpServer::Request> request)
{
	NotificationApi* api = NotificationApi::getInstance();
	api->getNotificationObject(NotificationApi::ObjCreateNotificationRule,
				   response,
				   request);
}

/**
 * Wrapper for POST /notification/{NotificationName}/delivery/{DeliveryName}
 */
void notificationCreateNotificationDelivery(shared_ptr<HttpServer::Response> response,
				    shared_ptr<HttpServer::Request> request)
{
	NotificationApi* api = NotificationApi::getInstance();
	api->getNotificationObject(NotificationApi::ObjCreateNotificationDelivery,
				   response,
				   request);
}

/**
 * Wrapper for DELETE /notification/{NotificationName}/delivery/{DeliveryName}
 */
void notificationDeleteNotificationDelivery(shared_ptr<HttpServer::Response> response,
				    shared_ptr<HttpServer::Request> request)
{
	NotificationApi* api = NotificationApi::getInstance();
	api->getNotificationObject(NotificationApi::ObjDeleteNotificationDelivery,
				   response,
				   request);
}

/**
 * Wrapper for DELETE /notification/{NotificationName}
 */
void notificationDeleteNotification(shared_ptr<HttpServer::Response> response,
				    shared_ptr<HttpServer::Request> request)
{
	NotificationApi* api = NotificationApi::getInstance();
	api->getNotificationObject(NotificationApi::ObjDeleteNotification,
				   response,
				   request);
}

/**
 * Wrapper for not handled URLS
 */
void defaultWrapper(shared_ptr<HttpServer::Response> response,
		    shared_ptr<HttpServer::Request> request)
{
	NotificationApi *api = NotificationApi::getInstance();
	api->defaultResource(response, request);
}

/**
 * Handle a bad URL endpoint call
 */
void NotificationApi::defaultResource(shared_ptr<HttpServer::Response> response,
				      shared_ptr<HttpServer::Request> request)
{
	string payload("{ \"error\" : \"Unsupported URL: " + request->path + "\" }");
	respond(response,
		SimpleWeb::StatusCode::client_error_bad_request,
		payload);
}

/**
 * Construct the singleton Notification API
 *
 * @param    port	Listening port (0 = automatically set)
 * @param    threads	Thread pool size of HTTP server
 */
NotificationApi::NotificationApi(const unsigned short port,
				 const unsigned int threads)
{
	m_port = port;
	m_threads = threads;
	m_server = new HttpServer();
	m_server->config.port = port;
	m_server->config.thread_pool_size = threads;
	m_thread = NULL;
	m_callBackURL = "";
	m_logger = Logger::getLogger();
	NotificationApi::m_instance = this;
}

/**
 * NotificationAPi destructor
 */
NotificationApi::~NotificationApi()
{
	if (m_server)
	{
		delete m_server;
	}
	if (m_thread)
	{
		delete m_thread;
	}
}

/**
 * Return the singleton instance of the NotificationAPI class
 */
NotificationApi* NotificationApi::getInstance()
{
	if (m_instance == NULL)
	{
		m_instance = new NotificationApi(0, 1);
	}
	return m_instance;
}

/**
 * Return the current listener port
 *
 * @return	The current listener port
 */
unsigned short NotificationApi::getListenerPort()
{
	return m_server->getLocalPort();
}

/**
 * Method for HTTP server, called by a dedicated thread
 */
void startService()
{
	NotificationApi::getInstance()->startServer();
}

/**
 * Start the HTTP server
 */
void NotificationApi::start() {
	m_thread = new thread(startService);
}

/**
 * Start method for HTTP server
 */
void NotificationApi::startServer() {
	m_server->start();
}

/**
 * Stop method for HTTP server
 */
void NotificationApi::stopServer() {
	m_server->stop();
}

/**
 * API stop entery point
 */
void NotificationApi::stop()
{
	this->stopServer();
}

/**
 * Wait for the HTTP server to shutdown
 */
void NotificationApi::wait() {
	m_thread->join();
}

/**
 * Initialise the API entry points for the common data resource and
 * the readings resource.
 */
void NotificationApi::initResources()
{       
	m_server->resource[RECEIVE_NOTIFICATION]["POST"] = notificationReceiveWrapper;
	m_server->resource[RECEIVE_AUDIT_NOTIFICATION]["POST"] = notificationAuditReceiveWrapper;
	m_server->resource[RECEIVE_STATS_NOTIFICATION]["POST"] = notificationStatsReceiveWrapper;
	m_server->resource[RECEIVE_STATS_RATE_NOTIFICATION]["POST"] = notificationStatsRateReceiveWrapper;
	m_server->resource[GET_NOTIFICATION_INSTANCES]["GET"] = notificationGetInstances;
	m_server->resource[GET_NOTIFICATION_RULES]["GET"] = notificationGetRules;
	m_server->resource[GET_NOTIFICATION_DELIVERY]["GET"] = notificationGetDelivery;
	m_server->resource[POST_NOTIFICATION_NAME]["POST"] = notificationCreateNotification;
	m_server->resource[POST_NOTIFICATION_RULE_NAME]["POST"] = notificationCreateNotificationRule;
	m_server->resource[POST_NOTIFICATION_DELIVERY_NAME]["POST"] = notificationCreateNotificationDelivery;
	m_server->resource[POST_NOTIFICATION_DELIVERY_NAME]["DELETE"] = notificationDeleteNotificationDelivery;
	m_server->resource[POST_NOTIFICATION_NAME]["DELETE"] = notificationDeleteNotification;


	// Handle errors
	m_server->default_resource["GET"] = defaultWrapper;
	m_server->default_resource["POST"] = defaultWrapper;
	m_server->default_resource["DELETE"] = defaultWrapper;
}

/**
 * Handle a exception by sending back an internal error
 *
  *
 * @param response	The response stream to send the response on.
 * @param ex		The current exception caught.
 */
void NotificationApi::internalError(shared_ptr<HttpServer::Response> response,
				    const exception& ex)
{
	string payload = "{ \"Exception\" : \"";

	payload = payload + string(ex.what());
	payload = payload + "\"";

	m_logger->error("NotificationApi Internal Error: %s\n", ex.what());

	this->respond(response,
		      SimpleWeb::StatusCode::server_error_internal_server_error,
		      payload);
}

/**
 * Construct an HTTP response with the 200 OK return code using the payload
 * provided.
 *
 * @param response	The response stream to send the response on
 * @param payload	The payload to send
 */
void NotificationApi::respond(shared_ptr<HttpServer::Response> response,
			      const string& payload)
{
	*response << "HTTP/1.1 200 OK\r\nContent-Length: "
		  << payload.length() << "\r\n"
		  <<  "Content-type: application/json\r\n\r\n" << payload;
}

/**
 * Construct an HTTP response with the specified return code using the payload
 * provided.
 *
 * @param response	The response stream to send the response on
 * @param code		The HTTP esponse code to send
 * @param payload	The payload to send
 */
void NotificationApi::respond(shared_ptr<HttpServer::Response> response,
			      SimpleWeb::StatusCode code,
			      const string& payload)
{
	*response << "HTTP/1.1 " << status_code(code) << "\r\nContent-Length: "
		  << payload.length() << "\r\n"
		  <<  "Content-type: application/json\r\n\r\n" << payload;
}

/**
 * Add data provided in the payload of callback API call
 * into the notification queue.
 * 
 * This is called by the storage service when new data arrives
 * for an asset in which we have registered an interest.
 *
 * @param response	The response stream to send the response on
 * @param request	The HTTP request
 */
void NotificationApi::processCallback(shared_ptr<HttpServer::Response> response,
				      shared_ptr<HttpServer::Request> request)
{
	try
	{
		// URL decode assetName
		string assetName = urlDecode(request->path_match[ASSET_NAME_COMPONENT]);
		string payload = request->content.string();
		string responsePayload;

		// Add data to the queue
		if (queueNotification(assetName, payload))
		{
			responsePayload = "{ \"response\" : \"processed\", \"";
			responsePayload += assetName;
			responsePayload += "\" : \"data queued\" }";

			this->respond(response, responsePayload);
		}
		else
		{
			responsePayload = "{ \"error\": \"error_message\" }";
			this->respond(response,
				      SimpleWeb::StatusCode::client_error_bad_request,
				      responsePayload);
		}
	}
	catch (exception ex)
	{
		this->internalError(response, ex);
	}
}

/**
 * Add data provided in the audit payload of callback API call
 * into the notification queue.
 * 
 * This is called by the storage service when new data arrives
 * for an asset in which we have registered an interest.
 *
 * @param response	The response stream to send the response on
 * @param request	The HTTP request
 */
void NotificationApi::processAuditCallback(shared_ptr<HttpServer::Response> response,
				      shared_ptr<HttpServer::Request> request)
{
	try
	{
		// URL decode audit code
		string auditCode = urlDecode(request->path_match[AUDIT_CODE_COMPONENT]);
		string payload = request->content.string();
		string responsePayload;

		// Add data to the queue
		if (queueAuditNotification(auditCode, payload))
		{
			responsePayload = "{ \"response\" : \"processed\", \"";
			responsePayload += auditCode;
			responsePayload += "\" : \"data queued\" }";

			this->respond(response, responsePayload);
		}
		else
		{
			responsePayload = "{ \"error\": \"error_message\" }";
			this->respond(response,
				      SimpleWeb::StatusCode::client_error_bad_request,
				      responsePayload);
		}
	}
	catch (exception ex)
	{
		this->internalError(response, ex);
	}
}

/**
 * Add data provided in the statistics payload of callback API call
 * into the notification queue.
 * 
 * This is called by the storage service when new data arrives
 * for an asset in which we have registered an interest.
 *
 * @param response	The response stream to send the response on
 * @param request	The HTTP request
 */
void NotificationApi::processStatsCallback(shared_ptr<HttpServer::Response> response,
				      shared_ptr<HttpServer::Request> request)
{
	try
	{
		// URL decode statistic
		string statistic = urlDecode(request->path_match[AUDIT_CODE_COMPONENT]);
		string payload = request->content.string();
		string responsePayload;
		// Add data to the queue
		if (queueStatsNotification(statistic, payload))
		{
			responsePayload = "{ \"response\" : \"processed\", \"";
			responsePayload += statistic;
			responsePayload += "\" : \"data queued\" }";

			this->respond(response, responsePayload);
		}
		else
		{
			responsePayload = "{ \"error\": \"error_message\" }";
			this->respond(response,
				      SimpleWeb::StatusCode::client_error_bad_request,
				      responsePayload);
		}
	}
	catch (exception ex)
	{
		this->internalError(response, ex);
	}
}

/**
 * Add data provided in the statistics rate payload of callback API call
 * into the notification queue.
 * 
 * This is called by the storage service when new data arrives
 * for an asset in which we have registered an interest.
 *
 * @param response	The response stream to send the response on
 * @param request	The HTTP request
 */
void NotificationApi::processStatsRateCallback(shared_ptr<HttpServer::Response> response,
				      shared_ptr<HttpServer::Request> request)
{
	try
	{
		// URL decode statistic
		string statistic = urlDecode(request->path_match[AUDIT_CODE_COMPONENT]);
		string payload = request->content.string();
		string responsePayload;
		// Add data to the queue
		if (queueStatsRateNotification(statistic, payload))
		{
			responsePayload = "{ \"response\" : \"processed\", \"";
			responsePayload += statistic;
			responsePayload += "\" : \"data queued\" }";

			this->respond(response, responsePayload);
		}
		else
		{
			responsePayload = "{ \"error\": \"error_message\" }";
			this->respond(response,
				      SimpleWeb::StatusCode::client_error_bad_request,
				      responsePayload);
		}
	}
	catch (exception ex)
	{
		this->internalError(response, ex);
	}
}

/**
 * Add readings data of asset name into the process queue
 *
 * @param assetName	The asset name
 * @param payload	Readings data belonging to asset name
 * @return		false error, true on success
 */
bool NotificationApi::queueNotification(const string& assetName,
					const string& payload)
{
	ReadingSet* readings = NULL;
	try
	{
		readings = new ReadingSet(payload);
	}
	catch (exception* ex)
	{
		m_logger->error("Exception '" + string(ex->what()) + \
				"' while parsing readings for asset '" + \
				assetName + "' with payload " + payload);
		delete ex;
		return false;
	}
	catch (...)
	{
		std::exception_ptr p = std::current_exception();
		string name = (p ? p.__cxa_exception_type()->name() : "null");
		m_logger->error("Exception '" + name + \
				"' while parsing readigns for asset '" + \
				assetName  + "'" );
		return false;
	}

	NotificationQueue* queue = NotificationQueue::getInstance();
	NotificationQueueElement* item =  new NotificationQueueElement("asset", assetName, readings);
	// Add element to the queue
	return queue->addElement(item);
}

/**
 * Add audit data of asset name into the process queue
 *
 * @param auditCode	The audit code
 * @param payload	The data for the audit code
 * @return		false error, true on success
 */
bool NotificationApi::queueAuditNotification(const string& auditCode,
					const string& payload)
{
	Reading *reading = new Reading(auditCode, payload);
	vector<Reading *> readingVec;
	readingVec.push_back(reading);
	ReadingSet* readings = NULL;
	try
	{
		readings = new ReadingSet(&readingVec);
	}
	catch (exception* ex)
	{
		m_logger->error("Exception '" + string(ex->what()) + \
				"' while parsing readings for audit code '" + \
				auditCode + "' with payload " + payload);
		delete ex;
		return false;
	}
	catch (...)
	{
		std::exception_ptr p = std::current_exception();
		string name = (p ? p.__cxa_exception_type()->name() : "null");
		m_logger->error("Exception '" + name + \
				"' while parsing readings for audit code '" + \
				auditCode  + "'" );
		return false;
	}

	NotificationQueue* queue = NotificationQueue::getInstance();
	NotificationQueueElement* item =  new NotificationQueueElement("audit", auditCode, readings);

	// Add element to the queue
	return queue->addElement(item);
}

/**
 * Add stats data of asset name into the process queue
 *
 * @param statistic	The statistic name
 * @param payload	The data for the audit code
 * @return		false error, true on success
 */
bool NotificationApi::queueStatsNotification(const string& statistic,
					const string& payload)
{
	Logger::getLogger()->debug("Recieved statisitics notification for statistic %s", statistic.c_str());

	Reading *reading = new Reading(statistic, payload);
	vector<Reading *> readingVec;
	readingVec.push_back(reading);
	ReadingSet* readings = NULL;
	try
	{
		readings = new ReadingSet(&readingVec);
	}
	catch (exception* ex)
	{
		m_logger->error("Exception '" + string(ex->what()) + \
				"' while parsing readings for statistic '" + \
				statistic + "' with payload " + payload);
		delete ex;
		return false;
	}
	catch (...)
	{
		std::exception_ptr p = std::current_exception();
		string name = (p ? p.__cxa_exception_type()->name() : "null");
		m_logger->error("Exception '" + name + \
				"' while parsing readings for audit code '" + \
				statistic  + "'" );
		return false;
	}

	NotificationQueue* queue = NotificationQueue::getInstance();
	NotificationQueueElement* item =  new NotificationQueueElement("stat", statistic, readings);

	// Add element to the queue
	return queue->addElement(item);
}

/**
 * Add stats rate data of asset name into the process queue
 *
 * @param statistic	The statistic name
 * @param payload	The data for the audit code
 * @return		false error, true on success
 */
bool NotificationApi::queueStatsRateNotification(const string& statistic,
					const string& payload)
{
	Logger::getLogger()->debug("Recieved statisitics rate notification for statistic %s", statistic.c_str());

	Reading *reading = new Reading(statistic, payload);
	vector<Reading *> readingVec;
	readingVec.push_back(reading);
	ReadingSet* readings = NULL;
	try
	{
		readings = new ReadingSet(&readingVec);
	}
	catch (exception* ex)
	{
		m_logger->error("Exception '" + string(ex->what()) + \
				"' while parsing readings for statistic '" + \
				statistic + "' with payload " + payload);
		delete ex;
		return false;
	}
	catch (...)
	{
		std::exception_ptr p = std::current_exception();
		string name = (p ? p.__cxa_exception_type()->name() : "null");
		m_logger->error("Exception '" + name + \
				"' while parsing readings for audit code '" + \
				statistic  + "'" );
		return false;
	}

	NotificationQueue* queue = NotificationQueue::getInstance();
	NotificationQueueElement* item =  new NotificationQueueElement("rate", statistic, readings);

	// Add element to the queue
	return queue->addElement(item);
}

/**
 * Return JSON string of a notification object
 *
 * @param object	The requested object type
 * @param response      The response stream to send the response on
 * @param request       The HTTP request
 */
void NotificationApi::getNotificationObject(NOTIFICATION_OBJECT object,
					    shared_ptr<HttpServer::Response> response,
					    shared_ptr<HttpServer::Request> request)
{
	string responsePayload;
	// Get NotificationManager instance
	NotificationManager* manager = NotificationManager::getInstance();
	if (manager)
	{
		switch (object)
		{
		case ObjGetRulesAll:
			responsePayload = manager->getJSONRules();
			// Get all Notification rules
			break;

		case ObjGetDeliveryAll:
			// Get all Notification delivery
			responsePayload = manager->getJSONDelivery();
			break;

		case ObjGetNotificationsAll:
			{
			// Get Notifications
			auto query = request->parse_query_string();
			auto search = query.find("all");

			// Fetch active notifications
			// GET /notification
			//
			// Fetch active notifications plus ones still in the zombie state
			// GET /notification?all
			// false parameter in getJSONInstances means: show only active notifications
			responsePayload = "{ \"notifications\": [" + \
					  manager->getJSONInstances(search != query.end())  + \
					  "] }";
			break;
			}

		case ObjCreateNotification:
			{
				string name = request->path_match[NOTIFICATION_NAME_COMPONENT];
				bool ret = this->createNotification(urlDecode(name));
				responsePayload = ret ?
						  "{\"message\": \"created\"}" :
						  "{\"error\": \"create notification failure\"}";
			}
			break;

		case ObjCreateNotificationRule:
			{
				string name = request->path_match[NOTIFICATION_NAME_COMPONENT];
				string rule = request->path_match[RULE_NAME_COMPONENT];
				bool ret = this->createNotificationRule(urlDecode(name), rule);
				responsePayload = ret ?
						 "{\"message\": \"created\"}" :
						 "{\"error\": \"create rule failure\"}";
			}
			break;

		case ObjCreateNotificationDelivery:
			{
				string name = request->path_match[NOTIFICATION_NAME_COMPONENT];
				string delivery = request->path_match[DELIVERY_NAME_COMPONENT];
				bool ret = this->createNotificationDelivery(urlDecode(name), delivery);
				responsePayload = ret ?
						 "{\"message\": \"created\"}" :
						  "{\"error\": \"create delivery failure\"}";;
			}
			break;

		case ObjDeleteNotificationDelivery:
			{
				string name = urlDecode(request->path_match[NOTIFICATION_NAME_COMPONENT]);
				string delivery = request->path_match[DELIVERY_NAME_COMPONENT];

				bool ret = this->deleteNotificationDelivery(name, delivery);
				responsePayload = ret ?
						 "{\"message\": \"created\"}" :
						  "{\"error\": \"create delivery failure\"}";;
			}
			break;


		case ObjDeleteNotification:
			{
				string name = request->path_match[NOTIFICATION_NAME_COMPONENT];
				bool ret = this->removeNotification(urlDecode(name));
				responsePayload = ret ?
						  "{\"message\": \"deleted\"}" :
						  "{\"error\": \"delete notification failure\"}";
			}
			break;

		default:
			responsePayload = "{ \"error\": \"Unknown Notification object requested.\" }";
			break;
		}

		// Return notification oject to client
		this->respond(response, responsePayload);
	}
        else
	{
			// Return error to client
			responsePayload = "{ \"error\": \"NotificationManager not yet available.\" }";
			this->respond(response,
				      SimpleWeb::StatusCode::server_error_internal_server_error,
				      responsePayload);
	}
}

/**
 * Set the callBack URL prefix for Notification callbacks.
 */
void NotificationApi::setCallBackURL()
{
	unsigned short apiPort =  this->getListenerPort();
	m_callBackURL = "http://127.0.0.1:" + to_string(apiPort) + "/notification/reading/asset/";

	m_logger->debug("Notification service: callBackURL prefix is " + m_callBackURL);
	m_auditCallbackURL = "http://127.0.0.1:" + to_string(apiPort) + "/notification/reading/audit/";
	m_statsCallbackURL = "http://127.0.0.1:" + to_string(apiPort) + "/notification/reading/stat/";
	m_statsRateCallbackURL = "http://127.0.0.1:" + to_string(apiPort) + "/notification/reading/rate/";
}

/**
 * Creates an empty, disabled notification category
 * within the Notifications parent.
 *
 * @param    name		The Notification category to create
 * @return			True on success, false otherwise
 */
bool NotificationApi::createNotification(const string& name)
{

	bool ret = false;

	// Get NotificationManager instance
	NotificationManager* manager = NotificationManager::getInstance();
	if (manager)
	{
		ret = manager->APIcreateEmptyInstance(name);
	}
	return ret;
}

/**
 * Create a rule subcategory for the notification
 * with the template content for the given rule.
 *
 * @param    name		The notification category name
 * @param    rule		The rule subcategory to create
 * @return			True on success, false otherwise
 */
bool NotificationApi::createNotificationRule(const string& name,
					     const string& rule)
{
	bool ret = false;

	// Get NotificationManager instance
	NotificationManager* manager = NotificationManager::getInstance();
	if (manager)
	{
		RulePlugin* rulePlugin = manager->createRuleCategory(name, rule);
		ret = rulePlugin != NULL;
		// Delete plugin object
		delete rulePlugin;
	}

	return ret;
}

/**
 * Add a delivery for the notification
 *
 * @param    name     The notification category name
 * @param    delivery The delivery subcategory to create
 * @return			  True on success, false otherwise
 */
bool NotificationApi::createNotificationDelivery(const string& name,
						 const string& delivery)
{
	bool ret = false;

	// Get NotificationManager instance
	NotificationManager* manager = NotificationManager::getInstance();
	if (manager)
	{
		DeliveryPlugin* deliveryPlugin = manager->createDeliveryCategory(name, delivery);
		ret = deliveryPlugin != NULL;
		// Delete plugin object
		delete deliveryPlugin;
	}

	return ret;
}

/**
 * Delete a delivery for the notification
 *
 * @param    name     The notification category name
 * @param    delivery The delivery subcategory to create
 * @return			  True on success, false otherwise
 */
bool NotificationApi::deleteNotificationDelivery(const string& name,const string& delivery)
{
	bool ret = false;

	// Get NotificationManager instance
	NotificationManager* manager = NotificationManager::getInstance();
	if (manager)
	{
		DeliveryPlugin* deliveryPlugin = manager->deleteDeliveryCategory(name, delivery);
		ret = deliveryPlugin != NULL;
		// Delete plugin object
		delete deliveryPlugin;
	}

	return ret;
}


/**
 * Remove a notification instance
 *
 * @param    name		The Notification category to remove
 * @return			True on success, false otherwise
 */
bool NotificationApi::removeNotification(const string& name)
{

	bool ret = false;

	// Get NotificationManager instance
	NotificationManager* manager = NotificationManager::getInstance();
	if (manager)
	{
		ret = manager->APIdeleteInstance(name);
	}
	return ret;
}

/**
 * Check if a char is an hex value
 *
 * @param c	The input char
 * @return	True with hex value
 * 		false otherwise
 */
bool NotificationApi::ishex (const char c)
{
	if (isdigit(c) ||
	    c=='A' ||
	    c=='B' ||
	    c=='C' ||
	    c=='D' ||
	    c=='E' ||
	    c=='F')
	{
		return true;
	}
	else
	{
		return false;
	}
}
