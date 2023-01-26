#ifndef _DATA_AVAILABILITY_RULE_H
#define _DATA_AVAILABILITY_RULE_H
/*
 * Fledge DataAvailability builtin notification rule.
 *
 * Copyright (c) 2023 Dianomic Systems, Inc.
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Devki Nandan Ghildiyal
 */
#include <plugin.h>
#include <plugin_manager.h>
#include <config_category.h>
#include <rule_plugin.h>
#include <builtin_rule.h>

typedef enum
{
	AUDIT_CODE_ALL,
	AUDIT_CODE_IN
} DataAvailabilityCondition;

/**
 * ThresholdRule, derived from RulePlugin, is a builtin rule object
 */
class DataAvailabilityRule : public RulePlugin
{
public:
	DataAvailabilityRule(const std::string &name);
	~DataAvailabilityRule();

	PLUGIN_HANDLE	init(const ConfigCategory &config);
	void		shutdown();
	bool		persistData() { return info->options & SP_PERSIST_DATA; };
	std::string	triggers();
	bool		eval(const std::string &assetValues);
	std::string	reason() const;
	PLUGIN_INFORMATION *getInfo();
	bool		isBuiltin() const { return true; };
	void		configure(const ConfigCategory &config);
	void		reconfigure(const std::string &newConfig);
	bool		evalAuditCode(const std::string &auditCodeValue,RuleTrigger *rule);

private:
	DataAvailabilityCondition m_condition;
};

#endif
