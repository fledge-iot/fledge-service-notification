#ifndef _NOTIFICATION_API_H
#define _NOTIFICATION_API_H
/*
 * Fledge Notification service.
 *
 * Copyright (c) 2018 Massimiliano Pinto
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */

#include "logger.h"
#include <server_http.hpp>

using namespace std;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

/*
 * URL for each API entry point
 */
#define ESCAPE_SPECIAL_CHARS		"\\{\\}\\\"\\(\\)\\!\\[\\]\\^\\$\\.\\|\\?\\*\\+\\-"
#define RECEIVE_NOTIFICATION		"^/notification/reading/asset/([A-Za-z0-9][a-zA-Z0-9_%\\-\\.]*)$"
#define RECEIVE_AUDIT_NOTIFICATION	"^/notification/reading/audit/([A-Za-z][a-zA-Z0-9_%\\-\\.]*)$"
#define RECEIVE_STATS_NOTIFICATION	"^/notification/reading/stat/([A-Za-z][a-zA-Z0-9_%\\-\\.]*)$"
#define RECEIVE_STATS_RATE_NOTIFICATION	"^/notification/reading/rate/([A-Za-z][a-zA-Z0-9_%\\-\\.]*)$"
#define GET_NOTIFICATION_INSTANCES	"^/notification$"
#define GET_NOTIFICATION_DELIVERY	"^/notification/delivery$"
#define GET_NOTIFICATION_RULES		"^/notification/rules$"
#define POST_NOTIFICATION_NAME		"^/notification/([A-Za-z][a-zA-Z0-9_%'~" ESCAPE_SPECIAL_CHARS "]*)$"
#define POST_NOTIFICATION_RULE_NAME	"^/notification/([A-Za-z][a-zA-Z0-9_%'~" ESCAPE_SPECIAL_CHARS "]*)/rule" \
					"/([A-Za-z][a-zA-Z0-9_%'~" ESCAPE_SPECIAL_CHARS "]*)$"
#define POST_NOTIFICATION_DELIVERY_NAME	"^/notification/([A-Za-z][a-zA-Z0-9_%'~" ESCAPE_SPECIAL_CHARS "]*)/delivery" \
					"/([A-Za-z][a-zA-Z0-9_%'~" ESCAPE_SPECIAL_CHARS "]*)$"
#define ASSET_NAME_COMPONENT		1
#define AUDIT_CODE_COMPONENT		1
#define STATS_NAME_COMPOENNT		1
#define NOTIFICATION_NAME_COMPONENT	1
#define RULE_NAME_COMPONENT		2
#define DELIVERY_NAME_COMPONENT		2

/**
 * NotificationApi is the entry point for:
 * - Service API
 * - Administration API
 * - notifications received from storage service
 */
class NotificationApi
{
	public:
		typedef enum
		{
			ObjNone,
			ObjGetRulesAll,
			ObjGetDeliveryAll,
			ObjGetNotificationsAll,
			ObjGetNotificationName,
			ObjCreateNotification,
			ObjCreateNotificationRule,
			ObjCreateNotificationDelivery,
			ObjDeleteNotificationDelivery,
			ObjDeleteNotification
		} NOTIFICATION_OBJECT;

		NotificationApi(const unsigned short port,
				const unsigned int threads);
		~NotificationApi();
		static		NotificationApi *getInstance();
		void		initResources();
		void		start();
		void		startServer();
		void		wait();
		void		stop();
		void		stopServer();
		unsigned short	getListenerPort();
		void		processCallback(shared_ptr<HttpServer::Response> response,
						shared_ptr<HttpServer::Request> request);
		void		processAuditCallback(shared_ptr<HttpServer::Response> response,
						shared_ptr<HttpServer::Request> request);
		void		processStatsCallback(shared_ptr<HttpServer::Response> response,
						shared_ptr<HttpServer::Request> request);
		void		processStatsRateCallback(shared_ptr<HttpServer::Response> response,
						shared_ptr<HttpServer::Request> request);
		void		getNotificationObject(NOTIFICATION_OBJECT object,
						      shared_ptr<HttpServer::Response> response,
						      shared_ptr<HttpServer::Request> request);
		bool		createNotification(const string& notificationName);
		bool		createNotificationRule(const string& name,
						       const string& rule);
		bool		createNotificationDelivery(const string& name,const string& rule);
		bool		deleteNotificationDelivery(const string& name,const string& rule);
		const std::string&
				getCallBackURL() const { return m_callBackURL; };
		const std::string&
				getAuditCallbackURL() const { return m_auditCallbackURL; };
		const std::string&
				getStatsCallbackURL() const { return m_statsCallbackURL; };
		const std::string&
				getStatsRateCallbackURL() const { return m_statsRateCallbackURL; };
		void		setCallBackURL();
		bool		removeNotification(const std::string& notificationName);
		// Add asset name and data to the Readings process queue
		bool		queueNotification(const string& assetName,
						  const string& payload);
		bool		queueAuditNotification(const string& auditCode,
						  const string& payload);
		bool		queueStatsNotification(const string& auditCode,
						  const string& payload);
		bool		queueStatsRateNotification(const string& auditCode,
						  const string& payload);

		void		defaultResource(shared_ptr<HttpServer::Response> response,
                                        shared_ptr<HttpServer::Request> request);
		std::string	decodeName(const std::string& name);

	private:
		void		internalError(shared_ptr<HttpServer::Response>,
					      const exception&);
		void		respond(shared_ptr<HttpServer::Response>,
					const string&);
		void		respond(shared_ptr<HttpServer::Response>,
					SimpleWeb::StatusCode,
				const string&);
		bool		ishex(const char c);

	private:
		static NotificationApi*		m_instance;
		HttpServer*			m_server;
		unsigned short			m_port;
		unsigned int			m_threads;
		thread*				m_thread;
		std::string			m_callBackURL;
		std::string			m_auditCallbackURL;
		std::string			m_statsCallbackURL;
		std::string			m_statsRateCallbackURL;
		Logger*				m_logger;
};

#endif
