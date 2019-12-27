/**
 * @file modbustag.h

-----------------------------------------------------------------------------
 Class "ModbusTag" encapsulates a single Modbus Tag unit
 
-----------------------------------------------------------------------------
*/

#ifndef _MODBUSTAG_H_
#define _MODBUSTAG_H_

#include <stdint.h>

#include <iostream>
#include <string>

class ModbusTag {
public:
    /**
     * Invalid - Empty constructor throws runtime error
     */
    ModbusTag();

    /**
     * Constructor
     * @param topic: tag topic
     */
    ModbusTag(const uint16_t addr);

    /**
     * Destructor
     */
    ~ModbusTag();

    /**
     * Get the tag address string
     * @return the tag address
     */
    const uint16_t getAddress(void);

    /**
     * Set the value
     * @param uintValue: the new value
     */
    void setValue(uint16_t uintValue);

    /**
     * Set the value
     * @param intValue: the new value
     */
    void setValue(int intValue);

    /**
     * Get value
     * @return value as int
     */
	int intValue(void);

	/**
	* Get value
	* @return value as uint
	*/
	uint16_t uintValue(void);

    // public members used to store data which is not used inside this class
    //int readInterval;                   // seconds between reads
    //time_t nextReadTime;                // next scheduled read
    //int publishInterval;                // seconds between publish
    //time_t nextPublishTime;             // next publish time

private:
    // All properties of this class are private
    // Use setters & getters to access these values
	uint16_t address;				// the address of the modbus tag in the slave
    uint16_t value;                  // the value of this modbus tag
    time_t lastUpdateTime;              // last update time (change of value)
};



#endif /* _MODBUSTAG_H_ */
