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
	* Set the modbus slave id
	* @param newId: the new slave ID
	*/
	void setSlaveId(int newId);

	/**
	* Get the slave ID
	* @return the slave ID
	*/
	uint8_t getSlaveId(void);

	/**
	* Set the value
	* @param uintValue: the new value
	*/
	void setRawValue(uint16_t uintValue);

	/**
	* Get value
	* @return value as uint
	*/
	uint16_t getRawValue(void);

	/**
	* Get value as bool
	* @return false if _rawValue is 0, otherwise true
	*/
	uint16_t getBoolValue(void);

	/**
	* Get scaled value
	* @return scaled value as float
	*/
	float getScaledValue(void);

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
	* Get the topic string
	* @return the topic string as std:strig
	*/
	std::string getTopicString(void);

	/**
	 * Set topic string
	 */
	void setTopic(const char*);

	/**
	* Get the format string
	* @return the format string
	*/
	const char* getFormat(void);

	/**
	 * Set format string
	 */
	void setFormat(const char*);

	/**
	* Set multiplier
	*/
	void setMultiplier(float);
	
	/**
	* Set offset value
	*/
	void setOffset(float);
	
	/**
	* Set noread
	*/
	void setNoreadValue(float);
	
	/**
	 * Get noread
	*/
	float getNoreadValue(void);
	
	/**
	 * Set data type
	 */
	bool setDataType(char);
	
	/**
	 * Get data type
	 */
	char getDataType(void);
	
	/**
	 * Set group
	 */
	void setGroup(int);
	
	/**
	 * Get group
	 */
	int getGroup(void);

	/**
	 * Set reference time
	 */
	void setReferenceTime(time_t);
	
	/**
	 * Get reference time
	 */
	time_t getReferenceTime(void);
	
	/**
	 * Set write pending
	 * to indicate that value needs to be written to the slave
	 */
	void setWritePending(bool);
	
	/**
	 * Get write pending
	 */
	bool getWritePending(void);
	
	// public members used to store data which is not used inside this class
	//int readInterval;                   // seconds between reads
	//time_t nextReadTime;                // next scheduled read
	//int publishInterval;                // seconds between publish
	//time_t nextPublishTime;             // next publish time

private:
	// All properties of this class are private
	// Use setters & getters to access these values
	std::string _topic;				// storage for topic path
	std::string _format;				// storage for publish format
	bool _writePending;				// value needs to be written to slave
	float _multiplier;				// multiplier for scaled value
	float _offset;					// offset for scaled value
	float _noread;					// value to publish when read fails
	uint8_t	_slaveId;				// modbus address of slave
	uint16_t _address;				// the address of the modbus tag in the slave
	int	_group;						// group tags for single read
	uint16_t _rawValue;				// the value of this modbus tag
	int _updatecycle_id;			// update cycle identifier
	time_t _lastUpdateTime;			// last update time (change of value)
	char _dataType;					// i = input, q = output, r = register
	time_t _referenceTime;			// time to be used externally only
};



#endif /* _MODBUSTAG_H_ */
