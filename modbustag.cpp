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
	this->address = 0;
	this->topic = "";
	this->slaveId = 0;
	this->rawValue = 0;
	this->multiplier = 1.0;
	this->offset = 0.0;
	this->format = "%f";
	this->_noread = 0.0;
	this->_writePending = false;
	//printf("%s constructor\n", __func__);
	//throw runtime_error("Class Tag - forbidden constructor");
}

ModbusTag::ModbusTag(const uint16_t addr) {
	this->address = addr;
	this->rawValue = 0;
}

ModbusTag::~ModbusTag() {
	//cout << "Topic: <" << topic << ">" << endl;
	//printf("%s - destructor %d\n", __func__, address);
}


void ModbusTag::setSlaveId(int newId) {
	slaveId = newId;
}

uint8_t ModbusTag::getSlaveId(void) {
	return slaveId;
}

void ModbusTag::setAddress(int newAddress) {
	address = newAddress;
}

uint16_t ModbusTag::getAddress(void) {
	return address;
}

void ModbusTag::setTopic(const char *topicStr) {
	if (topicStr != NULL) {
		topic = topicStr;
	}
}

const char* ModbusTag::getTopic(void) {
	return topic.c_str();
}

std::string ModbusTag::getTopicString(void) {
	return topic;
}

void ModbusTag::setFormat(const char *formatStr) {
	if (formatStr != NULL) {
		format = formatStr;
	}
}

const char* ModbusTag::getFormat(void) {
	return format.c_str();
}

void ModbusTag::setRawValue(uint16_t uintValue) {
	rawValue = uintValue;
}

uint16_t ModbusTag::getRawValue(void) {
	return rawValue;
}

float ModbusTag::getScaledValue(void) {
	float fValue = (float) rawValue;
	fValue *= multiplier;
	return fValue + offset;
}

void ModbusTag::setMultiplier(float newMultiplier) {
	multiplier = newMultiplier;
}

void ModbusTag::setOffset(float newOffset) {
	offset = newOffset;
}

void ModbusTag::setUpdateCycleId(int ident) {
	updatecycle_id = ident;
}

int ModbusTag::updateCycleId(void) {
	return updatecycle_id;
}

void ModbusTag::setNoreadValue(float newValue) {
	_noread = newValue;
}

float ModbusTag::getNoreadValue(void) {
	return _noread;
}

void ModbusTag::setWritePending(bool newValue) {
	_writePending = newValue;
}
	
bool ModbusTag::getWritePending(void) {
	return _writePending;
}
