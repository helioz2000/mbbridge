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
	printf("%s\n", __func__);
	throw runtime_error("Class Tag - forbidden constructor");
}

ModbusTag::ModbusTag(const uint16_t addr) {
	this->address = addr;
	this->value = 0;
}

ModbusTag::~ModbusTag() {
	//printf("%s - %d\n", __func__, address);
}


void ModbusTag::setValue(int intValue) {
	setValue( (uint16_t) intValue );
}

void ModbusTag::setValue(uint16_t uintValue) {
	value = uintValue;
}

int ModbusTag::intValue(void) {
	return (int) value;
}

uint16_t ModbusTag::uintValue(void) {
	return value;
}

