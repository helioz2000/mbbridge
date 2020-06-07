/**
 * @file modbustag.cpp
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include <sys/utsname.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "modbustag.h"

#include <stdexcept>
#include <iostream>

/*********************
 *      DEFINES
 *********************/
using namespace std;

/*********************
 * GLOBAL FUNCTIONS
 *********************/


/*********************
 * MEMBER FUNCTIONS
 *********************/

//
// Class Tag
//

ModbusTag::ModbusTag() {
	this->_address = 0;
	this->_group = 0;
	this->_topic = "";
	this->_slaveId = 0;
	this->_rawValue = 0;
	this->_multiplier = 1.0;
	this->_offset = 0.0;
	this->_format = "%f";
	this->_noreadvalue = 0.0;
	this->_noreadaction = -1;	// do nothing
	this->_noreadignore = 0;
	this->_noreadcount = 0;
	this->_writePending = false;
	this->_ignoreRetained = false;
	this->_dataType = 'r';
	this->_referenceTime = 0;
	//printf("%s - constructor %d %s\\", __func__, this->_slaveId, this->_topic.c_str());
	//throw runtime_error("Class Tag - forbidden constructor");
}

ModbusTag::ModbusTag(const uint16_t addr) {
	this->_address = addr;
	this->_rawValue = 0;
}

ModbusTag::~ModbusTag() {
	//cout << "Topic: <" << topic << ">" << endl;
	//printf("%s - destructor %d\n", __func__, address);
}

void ModbusTag::noreadNotify(void) {
	if (_noreadcount <= _noreadignore)	// if noreadignore is 0 noreadcount will still increment to 1
		_noreadcount++;					// a noreadcount > 0 indicates the tag is in noread state
}

bool ModbusTag::isNoread(void) {
	if (_noreadcount > 0) return true;
	else return false;
}

bool ModbusTag::noReadIgnoreExceeded(void) {
	if (_noreadcount > _noreadignore) return true;
	else return false;
}

void ModbusTag::setSlaveId(int newId) {
	_slaveId = newId;
}

uint8_t ModbusTag::getSlaveId(void) {
	return _slaveId;
}

void ModbusTag::setAddress(int newAddress) {
	_address = newAddress;
}

uint16_t ModbusTag::getAddress(void) {
	return _address;
}

void ModbusTag::setTopic(const char *topicStr) {
	if (topicStr != NULL) {
		_topic = topicStr;
	}
}

const char* ModbusTag::getTopic(void) {
	return _topic.c_str();
}

std::string ModbusTag::getTopicString(void) {
	return _topic;
}

void ModbusTag::setPublishRetain(bool newRetain) {
    _publish_retain = newRetain;
}

bool ModbusTag::getPublishRetain(void) {
    return _publish_retain;
}

void ModbusTag::setIgnoreRetained(bool newValue) {
	_ignoreRetained = newValue;
}

bool ModbusTag::getIgnoreRetained(void) {
	return _ignoreRetained;
}


void ModbusTag::setFormat(const char *formatStr) {
	if (formatStr != NULL) {
		_format = formatStr;
	}
}

const char* ModbusTag::getFormat(void) {
	return _format.c_str();
}

void ModbusTag::setRawValue(uint16_t uintValue) {
	switch(_dataType) {
		case 'r':
			_rawValue = uintValue;
			break;
		case 'i':
		case 'q':
			if (uintValue > 0) _rawValue = 1;
			else _rawValue = 0;
			break;
	}
	_lastUpdateTime = time(NULL);
	_noreadcount = 0;
}

uint16_t ModbusTag::getRawValue(void) {
	return _rawValue;
}

uint16_t ModbusTag::getBoolValue(void) {
	if (_rawValue == 0)
		return false;
	else
		return true;
}

float ModbusTag::getScaledValue(void) {
	float fValue = (float) _rawValue;
	fValue *= _multiplier;
	return fValue + _offset;
}

void ModbusTag::setMultiplier(float newMultiplier) {
	_multiplier = newMultiplier;
}

void ModbusTag::setOffset(float newOffset) {
	_offset = newOffset;
}

void ModbusTag::setUpdateCycleId(int ident) {
	_updatecycle_id = ident;
}

int ModbusTag::updateCycleId(void) {
	return _updatecycle_id;
}

void ModbusTag::setNoreadValue(float newValue) {
	_noreadvalue = newValue;
}

float ModbusTag::getNoreadValue(void) {
	return _noreadvalue;
}

void ModbusTag::setNoreadAction(int newValue) {
	_noreadaction = newValue;
}
	
int ModbusTag::getNoreadAction(void) {
	return _noreadaction;
}
	
void ModbusTag::setNoreadIgnore(int newValue) {
	_noreadignore = newValue;
}
	
int ModbusTag::getNoreadIgnore(void) {
	return _noreadignore;
}

bool ModbusTag::setDataType(char newType) {
	switch (newType) {
	case 'i':
	case 'I':
		_dataType = 'i';
		break;
	case 'q':
	case 'Q':
		_dataType = 'q';
		break;
	case 'r':
	case 'R':
		_dataType = 'r';
		break;
	default:
		return false;
	}
	return true;
}

char ModbusTag::getDataType(void) {
	return _dataType;
}

void ModbusTag::setGroup(int newValue) {
	_group = newValue;
}
	
int ModbusTag::getGroup(void) {
	return _group;
}

void ModbusTag::setReferenceTime(time_t newValue) {
	_referenceTime = newValue;
}
	
time_t ModbusTag::getReferenceTime(void) {
	return _referenceTime;
}

void ModbusTag::setWritePending(bool newValue) {
	_writePending = newValue;
}
	
bool ModbusTag::getWritePending(void) {
	return _writePending;
}
