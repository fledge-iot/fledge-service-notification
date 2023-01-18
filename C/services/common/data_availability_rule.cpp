/**
 * Fledge DataAvailability builtin notification rule
 *
 * Copyright (c) 2023 Dianomic Systems, Inc.
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Devki Nandan Ghildiyal
 */

#include <data_availability_rule.h>
#include <logger.h>

#define RULE_NAME "DataAvailability"
#define DEFAULT_TIME_INTERVAL "30"

/**
 * Rule specific default configuration
 */
static const char *default_config = QUOTE({
	"plugin" : {
		"description" : "Generate a notification when internal event occurs",
		"type" : "string",
		"default" : RULE_NAME,
		"displayName" : "Plugin",
		"readonly" : "true"
	},
	"description" : {
		"description" : "Generate a notification when internal event occurs",
		"type" : "string",
		"default" : "Generate a notification if the value of a configured audit log code within an audit log occurs",
		"displayName" : "Rule",
		"readonly" : "true"
	},
	"condition" : {
		"description" : "Audit log codes to consider",
		"type" : "enumeration",
		"options" : [ "ALL", "IN" ],
		"default" : "ALL",
		"displayName" : "Audit Log Code",
		"order" : "1"
	},
	"auditCode" : {
		"description" : "Comma separated list of audit log codes (e.g. CONCH,CONAD)",
		"type" : "string",
		"default" : "ALL",
		"displayName" : "Codes",
		"order" : "2"
	}
});

using namespace std;

/**
 * The C API rule information structure
 */
static PLUGIN_INFORMATION ruleInfo = {
	RULE_NAME,					   // Name
	"1.0.0",					   // Version
	SP_BUILTIN,					   // Flags
	PLUGIN_TYPE_NOTIFICATION_RULE, // Type
	"1.0.0",					   // Interface version
	default_config				   // Configuration
};

/**
 * DataAvailabilityRule builtin rule constructor
 *
 * Call parent class RulePlugin constructor
 * passing a NULL plugin handle
 *
 * @param    name	The builtin rule name
 */
DataAvailabilityRule::DataAvailabilityRule(const std::string &name) : RulePlugin(name, NULL)
{
}

/**
 * DataAvailabilityRule builtin rule destructor
 */
DataAvailabilityRule::~DataAvailabilityRule()
{
}

/**
 * Return rule info
 */
PLUGIN_INFORMATION *DataAvailabilityRule::getInfo()
{
	return &ruleInfo;
}

/**
 * Initialise rule objects based in configuration
 *
 * @param    config	The rule configuration category data.
 * @return		The rule handle.
 */
PLUGIN_HANDLE DataAvailabilityRule::init(const ConfigCategory &config)
{
	BuiltinRule *builtinRule = new BuiltinRule();
	m_instance = (PLUGIN_HANDLE)builtinRule;

	// Configure plugin
	this->configure(config);

	return (m_instance ? &m_instance : NULL);
}

/**
 * Free rule resources
 */
void DataAvailabilityRule::shutdown()
{
	BuiltinRule *handle = (BuiltinRule *)m_instance;
	// Delete plugin handle
	delete handle;
}

/**
 * Return triggers JSON document
 *
 * @return	JSON string
 */
string DataAvailabilityRule::triggers()
{
	string ret;
	BuiltinRule *handle = (BuiltinRule *)m_instance;
	// Configuration fetch is protected by a lock
	lock_guard<mutex> guard(m_configMutex);

	if (!handle->hasTriggers())
	{
		ret = "{\"triggers\" : []}";
		return ret;
	}
	ret = "{\"triggers\" : [ ";
	std::map<std::string, RuleTrigger *> triggers = handle->getTriggers();
	for (auto it = triggers.begin();
		 it != triggers.end();
		 ++it)
	{
		ret += "{ \"asset\"  : \"" + (*it).first + "\"";
		if (!(*it).second->getEvaluation().empty())
		{
			ret += ", \"" + (*it).second->getEvaluation() + "\" : " +
				   to_string((*it).second->getInterval()) + " }";
		}
		else
		{
			ret += " }";
		}

		if (std::next(it, 1) != triggers.end())
		{
			ret += ", ";
		}
	}

	ret += " ] }";
	return ret;
}

/**
 * Evaluate notification data received
 *
 * @param    auditCodeValues	JSON string document
 *				with notification data.
 * @return			True if the rule was triggered,
 *				false otherwise.
 */
bool DataAvailabilityRule::eval(const string &auditCodeValues)
{
	Document doc;
	doc.Parse(auditCodeValues.c_str());
	if (doc.HasParseError())
	{
		return false;
	}

	bool eval = false;
	
	BuiltinRule* handle = (BuiltinRule *)m_instance;
	// Configuration fetch is protected by a lock
	lock_guard<mutex> guard(m_configMutex);

	map<std::string, RuleTrigger *>& triggers = handle->getTriggers();

	// Iterate throgh all configured assets
	for (auto t = triggers.begin();
		  t != triggers.end();
		  ++t)
	{
		string auditCodeName = (*t).first;
		string auditCodeTimestamp = "timestamp_" + auditCodeName;

		if (!doc.HasMember(auditCodeName.c_str()))
		{
			eval = false;
		}
		else
		{
			// Get all datapoints for assetName
			const Value& assetValue = doc[auditCodeName.c_str()];

			// Set evaluation
			eval = this->evalAuditCode(auditCodeValues, (*t).second);

			// Add evalution timestamp
			if (doc.HasMember(auditCodeTimestamp.c_str()))
			{
				const Value& auditCodeTime = doc[auditCodeTimestamp.c_str()];
				double timestamp = auditCodeTime.GetDouble();
				handle->setEvalTimestamp(timestamp);
			}
		}
	}

	// Set final state: true is all calls to evalAsset() returned true
	handle->setState(eval);
	
	return eval;
}

/**
 * Return rule trigger reason: trigger or clear the notification.
 *
 * @return	 A JSON string
 */
string DataAvailabilityRule::reason() const
{
	BuiltinRule *handle = (BuiltinRule *)m_instance;
	// Get state, assets and timestamp
	BuiltinRule::TriggerInfo info;
	handle->getFullState(info);

	string ret = "{ \"reason\": \"";
	ret += info.getState() == BuiltinRule::StateTriggered ? "triggered" : "cleared";
	ret += "\"";
	ret += ", \"auditCode\": " + info.getAssets();
	if (handle->getEvalTimestamp())
	{
		ret += ", \"timestamp\": \"" + info.getUTCTimestamp() + "\"";
	}

	ret += " }";
	return ret;
}

/**
 * Call the reconfigure method in the plugin
 *
 * @param    newConfig		The new configuration for the plugin
 */
void DataAvailabilityRule::reconfigure(const string &newConfig)
{
	ConfigCategory config("dataAvailability", newConfig);
	this->configure(config);
}

/**
 * Evaluate datapoints values for the given asset name
 *
 * @param    auditCodeValue		JSON object with auditCodeValue
 * @param    rule		Current configured rule trigger.
 *
 * @return			True if evalution succeded,
 *				false otherwise.
 */
bool DataAvailabilityRule::evalAuditCode(const std::string &auditCodeValue,
										 RuleTrigger *rule)
{
	bool auditLogEval = false;
	//vector<std::string> auditCodes = rule->getAuditLogCodes();
	BuiltinRule *handle = (BuiltinRule *)m_instance;
	std::map<std::string, RuleTrigger *> triggers = handle->getTriggers();
	if (m_condition == AUDIT_CODE_ALL)
	{
		auditLogEval = true;
	}
	else if (m_condition == AUDIT_CODE_IN)
	{
		for (auto it = triggers.begin(); it != triggers.end(); ++it)
		{
			string auditLogName = (*it).first;
			if (auditLogName == auditCodeValue)
			{
				auditLogEval = true;
			}
		}
	}

	// Return evaluation for current auditLogCode
	return auditLogEval;
}

/**
 * Configure the builtin rule plugin
 *
 * @param    config	The configuration object to process
 */
void DataAvailabilityRule::configure(const ConfigCategory &config)
{
	BuiltinRule *handle = (BuiltinRule *)m_instance;
	string auditCode = config.getValue("auditCode");
	
	string evaluation_data = {};
	unsigned int timeInterval = 0;
	
	
	// Configuration change is protected by a lock
	lock_guard<mutex> guard(m_configMutex);
	if (handle->hasTriggers())
	{
		handle->removeTriggers();
	}

	char filter = ',';
	string::size_type i = 0;
	string::size_type j = auditCode.find(filter);
	vector<std::string> auditCodeList;
	if (j == string::npos && auditCode.length() > 0)
	{
		auditCodeList.push_back(auditCode);
	}
	while (j != string::npos) 
	{
		auditCodeList.push_back(auditCode.substr(i, j-i));
		i = ++j;
		j = auditCode.find(filter, j);

		if (j == string::npos)
			auditCodeList.push_back(auditCode.substr(i, auditCode.length()));
	}

	for (int i = 0; i < auditCodeList.size(); ++i)
	{
		DatapointValue value (auditCodeList[i]);
		handle->addTrigger(auditCodeList[i], new RuleTrigger(auditCodeList[i], new Datapoint(auditCodeList[i], value)));
	}
	
	string condition = config.getValue("condition");
	if (condition.compare("IN") == 0)
		m_condition = AUDIT_CODE_IN;
	else if (condition.compare("ALL") == 0)
		m_condition = AUDIT_CODE_ALL;

}
