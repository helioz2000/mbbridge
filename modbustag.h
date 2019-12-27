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
     * Empty constructor
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
	* Set the address
	* @param address: the new address
	*/
	void setAddress(int newAddress);


	/**
	* Get the tag address string
	* @return the tag address
	*/
	uint16_t getAddress(void);

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
	* Set the updatecycle_id
	* @param ident: the new value
	*/
	void setUpdateCycleId(int ident);

	/**
	* Get updatecycle_id
	* @return updatecycle_id as int
	*/
	int updateCycleId(void);
	
	/**
	* Get the topic string
	* @return the topic string
	*/
	const char* getTopic(void);

	/**
	 * Set topic string
	 */
	void setTopic(const char*);

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
	std::string topic;				// storage for topic path
	uint16_t address;				// the address of the modbus tag in the slave
	uint16_t value;					// the value of this modbus tag
	int updatecycle_id;				// update cycle identifier
	time_t lastUpdateTime;			// last update time (change of value)
};



#endif /* _MODBUSTAG_H_ */
