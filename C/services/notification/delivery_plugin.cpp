/**
 * Fledge delivery plugin class
 *
 * Copyright (c) 2018 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */

#include <delivery_plugin.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

using namespace std;
using namespace rapidjson;


// DeliveryPlugin constructor
DeliveryPlugin::DeliveryPlugin(const std::string& name,
			       PLUGIN_HANDLE handle) :
			       Plugin(handle), m_name(name)
{
	// Setup the function pointers to the plugin
	pluginInit = (PLUGIN_HANDLE (*)(const ConfigCategory *))
					manager->resolveSymbol(handle,
							       "plugin_init");
	pluginShutdownPtr = (void (*)(PLUGIN_HANDLE))
				      manager->resolveSymbol(handle,
							     "plugin_shutdown");

	pluginDeliverPtr = (bool (*)(const PLUGIN_HANDLE,
				     const string& deliveryName,
				     const string& notificationName,
				     const string& triggerReason,
				     const string& message))
				     manager->resolveSymbol(handle,
							    "plugin_deliver");

	pluginReconfigurePtr = (void (*)(PLUGIN_HANDLE, const std::string&))
					 manager->resolveSymbol(handle,
								"plugin_reconfigure");

	pluginStartPtr = (void (*)(PLUGIN_HANDLE))
				   manager->resolveSymbol(handle,
							  "plugin_start");

	// Persist data initialised
	m_plugin_data = NULL;

	// Set disable
	m_enabled = false;
}

//DeliveryPlugin destructor
DeliveryPlugin::~DeliveryPlugin()
{
	delete m_plugin_data;
}

PLUGIN_HANDLE DeliveryPlugin::init(const ConfigCategory& config)
{
	m_instance = this->pluginInit(&config);
	// Set the enable flag
	this->setEnabled(config);
	// Return instance
	return (m_instance ? &m_instance : NULL);
}

/**
 * Register a function that the plugin canm call to ingest a reading
 *
 * @param func	The function to call
 * @param data	First argument to pass t above function
 */
void DeliveryPlugin::registerIngest(void *func, void *data)
{
void (*pluginRegisterPtr)(PLUGIN_HANDLE, void *, void *) =
       			(void (*)(PLUGIN_HANDLE, void *, void *))
			      manager->resolveSymbol(handle, "plugin_registerIngest");
	if (pluginRegisterPtr)
		(*pluginRegisterPtr)(m_instance, func, data);
}

/**
 * Call the loaded plugin "plugin_shutdown" method
 */
void DeliveryPlugin::shutdown()
{
	if (this->pluginShutdownPtr)
	{
		return this->pluginShutdownPtr(m_instance);
	}
}

/**
 * Call the loaded plugin "plugin_deliver" method
 *
 * @param    deliveryName	The category name associated to the plugin.
 * @param    notificationName	The notification name this delivery plugin
 *				instance belongs to.
 * @param    triggerReason	A JSON string with the related
 *				triggered rule reason.
 * @param    message		A custom text message.
 * @return			True on success, false otherwise.
 */
bool DeliveryPlugin::deliver(const std::string& deliveryName,
			     const std::string& notificationName,
			     const std::string& triggerReason,
			     const std::string& message)
{

	bool ret = false;
	time_t	start = time(0);
	if (this->pluginDeliverPtr)
	{
		ret = this->pluginDeliverPtr(m_instance,
					     deliveryName,
					     notificationName,
					     triggerReason,
					     expandMacros(message, triggerReason));
	}
	unsigned int duration = time(0) - start;
	if (duration > 5)
	{
		Logger::getLogger()->warn("Delivery of notification %s was slow, %d seconds",
				notificationName.c_str(), duration);
	}
	return ret;
}

/**
 * Call the reconfigure method in the plugin
 *
 * @param    newConfig		The new configuration for the plugin
 */
void DeliveryPlugin::reconfigure(const string& newConfig)
{
	if (this->pluginReconfigurePtr)
	{
		ConfigCategory reconfig("new_cfg", newConfig);
		this->setEnabled(reconfig);
		return this->pluginReconfigurePtr(m_instance, newConfig);
	}
}

/**
 * Set the enable flag from configuration
 *
 * @param    config	The configuration of the plugin
 */
void DeliveryPlugin::setEnabled(const ConfigCategory& config)
{
	// Set the enable flag
	if (config.itemExists("enable"))
	{
		m_enabled = config.getValue("enable").compare("true") == 0 ||
			    config.getValue("enable").compare("True") == 0;

		Logger::getLogger()->debug("DeliveryPlugin::setEnabled = %d",
					   m_enabled);
	}

}

/**
 * Call plugin_start
 */
void DeliveryPlugin::start()
{
	if (pluginStartPtr != NULL)
	{
		this->pluginStartPtr(m_instance);
	}
}

/**
 * Register service
 *
 * @param func  The function to call
 * @param data  First argument to pass to above function
 */
void DeliveryPlugin::registerService(void *func, void *data)
{
	void (*pluginRegisterService)(PLUGIN_HANDLE, void *, void *) =
		(void (*)(PLUGIN_HANDLE, void *, void *))
		manager->resolveSymbol(handle, "plugin_registerService");

	if (pluginRegisterService != NULL)
	{
		(*pluginRegisterService)(m_instance, func, data);
	}
}


/**
 * Create the information about the macros to substitute in the given string
 *
 * @param str	The string we are substituting
 * @param macros	Vector of macros to build up
 */
void DeliveryPlugin::collectMacroInfo(const string& str, vector<Macro>& macros)
{
	string::size_type start = str.find('$');
	string::size_type end = str.find('$', start + 1);

	while (start != string::npos && end != string::npos) 
	{
		string::size_type bar = str.find('|', start + 1);
		if (bar != string::npos && bar < end && bar > start + 1) 
		{
			string def = str.substr(bar + 1, end - bar - 1);
			macros.emplace_back(Macro(str.substr(start + 1, bar - start - 1), start, def));
		}
		else if (end > start + 1)
		{
			macros.emplace_back(Macro(str.substr(start + 1, end - start - 1), start));
		}
		start = str.find('$', end + 1);
		end = str.find('$', start + 1);
	}
}

/**
 * Substitute values from the reason into the string.
 * Macros are of the form $key$ or 
 * $key|default$. Where key is one of the keys found in
 * the notification data of the reason document. Keys
 * are typical datapoint names that triggered the alert
 * for readings, statistic values for statistics data and
 * messages for alert data.
 *
 * @param message	The string to substitute into
 * @param reason	The notification reason from which to pull data
 * @return string	The substituted string
 */
string  DeliveryPlugin::expandMacros(const string& message, const string& reason)
{
	string rval = message;
	vector<Macro> macros;
	Logger::getLogger()->debug("Expand macros in message %s with reason %s",
			message.c_str(), reason.c_str());
	collectMacroInfo(rval, macros);
	if (macros.size())
	{
		Document doc;
		doc.Parse(reason.c_str());
		if (doc.HasParseError())
		{
			// Failed to parse the reason, ignore macros
			Logger::getLogger()->warn("Unable to parse reason document, macro substitutions within the notification will be ignored. The reason document is:  %s", reason.c_str());
			return rval;
		}
		if (!doc.HasMember("data"))
		{
			Logger::getLogger()->warn("Unable to perform macro substitution in the notification alert. No data element was found in reason document %s", reason.c_str());
			return rval;
		}
		Value& data = doc["data"];
		Value::ConstMemberIterator itr = data.MemberBegin();
		if (itr == data.MemberEnd())
		{
			Logger::getLogger()->warn("Unable to perform macro substitution in the notifcation alert. Data element has no children, reason document %s", reason.c_str());
			return rval;
		}
		const Value& v = itr->value;
		string assetName = itr->name.GetString();


		// Replace Macros by datapoint value
		for (auto it =  macros.rbegin(); it != macros.rend(); ++it)
		{
			if (it->name == "ASSET")
			{
				rval.replace(it->start, it->name.length()+2
							+ (it->def.empty() ? 0 : it->def.length() + 1),
							assetName);
			}
			else if (v.HasMember(it->name.c_str()))
			{
				string val;
				if (v[it->name.c_str()].IsString())
				{
					val = v[it->name.c_str()].GetString();
				}
				else if (v[it->name.c_str()].IsInt64())
				{
					val = to_string(v[it->name.c_str()].GetInt64());
				}
				else if (v[it->name.c_str()].IsDouble())
				{
					val = to_string(v[it->name.c_str()].GetDouble());
					// Trim trailing 0's
					size_t len = val.length();
					while (len > 0 && val[len-1] == '0')
					{
						len--;
					}
					if (len > 0)
					{
						val = val.substr(0, len);
					}
					
				}
				else if (v[it->name.c_str()].IsObject())
				{
					StringBuffer strbuf;
					Writer<rapidjson::StringBuffer> writer(strbuf);
					v[it->name.c_str()].Accept(writer);
			                val = strbuf.GetString();
				}
				else
				{
					Logger::getLogger()->warn("The datapoint %s cannot be used as a macro substitution as it is not a string, numeric value or JSON document",it->name.c_str());
					continue;
				}
				rval.replace(it->start, it->name.length()+2
							+ (it->def.empty() ? 0 : it->def.length() + 1),
							val);
			}
			else if (!it->def.empty())
			{
				rval.replace(it->start, it->name.length() + it->def.length() + 3, it->def);
			}
			else
			{
				rval.replace(it->start, it->name.length()+2, "");
			}
		}
	}
	return rval;
}
