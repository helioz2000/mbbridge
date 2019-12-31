/**
 * @file mbbridge.cpp
 *
 * https://github.com/helioz2000/mbbridge
 *
 * Author: Erwin Bejsta
 * December 2019
 */

/*********************
 *      INCLUDES
 *********************/

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include <libconfig.h++>
#include <mosquitto.h>
#include <modbus.h>

#include "hardware.h"
#include "mqtt.h"
#include "datatag.h"
#include "modbustag.h"
#include "mbbridge.h"

using namespace std;
using namespace libconfig;

const char *build_date_str = __DATE__ " " __TIME__;
const int version_major = 1;
const int version_minor = 0;

#define CFG_FILENAME_EXT ".cfg"
#define CFG_DEFAULT_FILEPATH "/etc/"

#define MAIN_LOOP_INTERVAL_MINIMUM 50     // milli seconds
#define MAIN_LOOP_INTERVAL_MAXIMUM 2000   // milli seconds

#define MQTT_BROKER_DEFAULT "127.0.0.1"

static string cpu_temp_topic = "";
static string cfgFileName;
static string execName;
static string mbSlaveStatusTopic;
bool exitSignal = false;
bool debugEnabled = false;
bool modbusDebugEnabled = false;
bool mqttDebugEnabled = false;
bool runningAsDaemon = false;
time_t mqtt_connect_time = 0;   // time the connection was initiated
bool mqtt_connection_in_progress = false;
bool mqtt_retain = false;
std::string processName;
char *info_label_text;
useconds_t mainloopinterval = 250;   // milli seconds
//extern void cpuTempUpdate(int x, Tag* t);
//extern void roomTempUpdate(int x, Tag* t);
modbus_t *mb_ctx = NULL;
updatecycle *updateCycles = NULL;	// array of update cycle definitions
ModbusTag *mbReadTags = NULL;			// array of all modbus read tags
ModbusTag *mbWriteTags = NULL;			// array of all modbus write tags
int mbTagCount = -1;
#define MODBUS_SLAVE_MAX 254		// highest permitted slave ID
#define MODBUS_SLAVE_MIN 1			// lowest permitted slave ID
bool mbSlaveOnline[MODBUS_SLAVE_MAX+1];			// array to store online/offline status

#pragma mark Proto types
void subscribe_tags(void);
void mqtt_connection_status(bool status);
void mqtt_topic_update(const char* topic, const char* value);
void mqtt_subscribe_tags(void);
void setMainLoopInterval(int newValue);
bool modbus_read_tag(ModbusTag tag);
bool modbus_write_tag(ModbusTag tag);
void modbus_write_request(int callbackId, Tag *tag);
bool mqtt_publish_tag(ModbusTag tag, bool noread = false);

TagStore ts;
MQTT mqtt;
Config cfg;			// config file
Hardware hw(false);	// no screen

/**
 * log to console and syslog for daemon
 */
template<typename... Args> void log(int priority, const char * f, Args... args) {
	if (runningAsDaemon) {
		syslog(priority, f, args...);
	} else {
		fprintf(stderr, f, args...);
		fprintf(stderr, "\n");
	}
}

/** Handle OS signals
 */
void sigHandler(int signum)
{
	char signame[10];
	switch (signum) {
		case SIGTERM:
			strcpy(signame, "SIGTERM");
			break;
		case SIGHUP:
			strcpy(signame, "SIGHUP");
			break;
		case SIGINT:
			strcpy(signame, "SIGINT");
			break;

		default:
			break;
	}

	log(LOG_INFO, "Received %s", signame);
	exitSignal = true;
}

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result) {
	if ((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
	result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}
	return;
}

#pragma mark -- Config File functions

/** Read configuration file.
 * @return true if success
 */
bool readConfig (void)
{
	//int ival;
	// Read the file. If there is an error, report it and exit.

	try
	{
		cfg.readFile(cfgFileName.c_str());
	}
	catch(const FileIOException &fioex)
	{
		std::cerr << "I/O error while reading file <" << cfgFileName << ">." << std::endl;
		return false;
	}
	catch(const ParseException &pex)
	{
		std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
				<< " - " << pex.getError() << std::endl;
		return false;
	}

	//log (LOG_INFO, "CFG file read OK");
	//std::cerr << cfgFileName << " read OK" <<endl;

	try {
		setMainLoopInterval(cfg.lookup("mainloopinterval"));
	} catch (const SettingNotFoundException &excp) {
	;
	} catch (const SettingTypeException &excp) {
		std::cerr << "Error in config file <" << excp.getPath() << "> is not an integer" << std::endl;
		return false;
	}

	// Read MQTT broker from config
	try {
		mqtt.setBroker(cfg.lookup("mqtt.broker"));
	} catch (const SettingNotFoundException &excp) {
		mqtt.setBroker(MQTT_BROKER_DEFAULT);
	} catch (const SettingTypeException &excp) {
		std::cerr << "Error in config file <" << excp.getPath() << "> is not a string" << std::endl;
		return false;
	}
	return true;
}

/**
 * Get integer value from config file
 */
bool cfg_get_int(const std::string &path, int &value) {
	if (!cfg.lookupValue(path, value)) {
		std::cerr << "Error in config file <" << path << ">" << std::endl;
		return false;
	}
	return true;
}

/**
 * Get string value from config file
 */
bool cfg_get_str(const std::string &path, std::string &value) {
	if (!cfg.lookupValue(path, value)) {
		std::cerr << "Error in config file <" << path << ">" << std::endl;
		return false;
	}
	return true;
}

#pragma mark -- Processing

/**
 * Process internal variables
 * Local variables are processed at a fixed time interval
 * The processing involves reading value from hardware and
 * publishing the value to MQTT broker
 * @returns true if one or more variable(s) were processed
 */
bool var_process(void) {
    bool retval = false;
    time_t now = time(NULL);
//    if (now > var_process_time) {
//        var_process_time = now + VAR_PROCESS_INTERVAL;

	// update CPU temperature
	Tag *tag = ts.getTag(cpu_temp_topic.c_str());
	if (tag != NULL) {
		// read time due ?
		if (tag->nextReadTime <= now) {
			tag->setValue(hw.read_cpu_temp());
			tag->nextReadTime = now + tag->readInterval;
		retval = true;
		}
		// publish time due ?
		if (tag->nextPublishTime <= now) {
			if (mqtt.isConnected()) {
				mqtt.publish(tag->getTopic(), "%.1f", tag->floatValue() );
				retval = true;
			}
			tag->nextPublishTime = now + tag->publishInterval;
		}
	}
    // reconnect mqtt if required
    /*if (!mqtt.isConnected() && !mqtt_connection_in_progress) {
        mqtt_connect();
    }*/
    return retval;
}

/**
 * process modbus write
 * only one write function is processed per call
 * @return false is there was nothing to process, otherwise true
 */
bool modbus_write_process() {
	int idx = 0;
	uint8_t slaveId = mbWriteTags[idx].getSlaveId();
	while ((slaveId >= MODBUS_SLAVE_MIN) && (slaveId <= MODBUS_SLAVE_MAX)) {
		if (mbWriteTags[idx].getWritePending()) {
			//printf ("%s - writing %d to Slave %d Addr %d idx %d\n", __func__, mbWriteTags[idx].getRawValue(),slaveId, mbWriteTags[idx].getAddress(), idx);
			modbus_write_tag(mbWriteTags[idx]);
			mbWriteTags[idx].setWritePending(false);	// mark as write done
			return true;			// end here, we only do one write
			}
		idx++;
		slaveId = mbWriteTags[idx].getSlaveId();
	}
	return false;
}

/**
 * modbus read group of tags
 * reads tags in the same group (and same slave) in one operation.
 * if called with a tag which has already been read in this cycle
 * use the previously read result and just publish the value.
 * @param tagArray: an array tag indexes which are part 
 * @param arrayIndex: index into current tag array
 * @param refTime: time reference to identify already read tags
 * @returns true if group of tags have been read, false if tag is not in a group
 */
bool modbus_read_multi_tags(int *tagArray, int arrayIndex, time_t refTime) {
	ModbusTag t = mbReadTags[tagArray[arrayIndex]];
	int group = t.getGroup();
	// if tag is not part of a group then return false
	if (group < 1) return false;
	// if tag has already been read in this cycle the reference time will match
	if (t.getReferenceTime() == refTime) {
		//all we need to do is publish the tag
		mqtt_publish_tag(t);
		return true;
		}
	// if get to here the tag is part of a group which has not been read in this cycle
	
	// assemble tag indexes which belong to same group and slave into one array
	for () {
		
	}
	
	// read all tags in this group (multi read)
	
	// publish only the one tag referenced in parameters
	
	return true;	// indicate that tag has been processed
}

/**
 * process modbus cyclic read update
 * @return false is there was nothing to process, otherwise true
 */
bool modbus_read_process() {
	int index = 0;
	int tagIndex = 0;
	int *tagArray;
	bool retval = false;
	time_t now = time(NULL);
	time_t refTime;
	while (updateCycles[index].ident >= 0) {
		// ignore if cycle has no tags to process
		if (updateCycles[index].tagArray == NULL) {
			index++; continue;
		}
		// new reference time for each read cycle
		refTime = time(NULL);		// used for group reads
		if (now >= updateCycles[index].nextUpdateTime) {
			// set next update cycle time
			updateCycles[index].nextUpdateTime = now + updateCycles[index].interval;
			// get array for tags
			tagArray = updateCycles[index].tagArray;
			// read each tag in the array
			tagIndex = 0;
			while (tagArray[tagIndex] >= 0) {
				// if tag is not part of a group ....
				if (!modbus_read_multi_tags(tagArray, tagIndex, refTime))
					// perform single tag read
					modbus_read_tag(mbReadTags[tagArray[tagIndex]]);
				tagIndex++;
			}
			retval = true;
			//cout << now << " Update Cycle: " << updateCycles[index].ident << " - " << updateCycles[index].tagArraySize << " tags" << endl;
		}
		index++;
	}
	
	return retval;
}

/** Process all  variables
 * @return true if at least one variable was processed
 * Note: the return value from this function is used 
 * to measure processing time
 */
bool process() {
	bool retval = false;
	if (mqtt.isConnected()) {
		if (modbus_read_process()) retval = true;
		if (modbus_write_process()) retval = true;
	}
	var_process();	// don't want it in time measuring, doesn't take up much time
	return retval;
}

bool init_values(void)
{
	//char info1[80], info2[80], info3[80], info4[80];

/*
    // get hardware info
    hw.get_model_name(info1, sizeof(info1));
    hw.get_os_name(info2, sizeof(info2));
    hw.get_kernel_name(info3, sizeof(info3));
    hw.get_ip_address(info4, sizeof(info4));
    info_label_text = (char *)malloc(strlen(info1) +strlen(info2) +strlen(info3) +strlen(info4) +5);
    sprintf(info_label_text, "%s\n%s\n%s\n%s", info1, info2, info3, info4);
    if (!runningAsDaemon) {
	    printf(info_label_text);
        }
    //printf(info_label_text);
    */
    return true;
}

#pragma mark MQTT



/** Initialise hardware tags
 * @return false on failure
 */
bool init_hwtags(void)
{

	Tag* tp = NULL;
	std::string topicPath;

	// CPU temperature (optional, may not exist)
	if (cfg.lookupValue("cputemp.topic", topicPath)) {
		cpu_temp_topic = topicPath;
		tp = ts.addTag(topicPath.c_str());
		tp->publishInterval = 0;
		if (!cfg_get_int("cputemp.readinterval", tp->readInterval)) return false;
		if (!cfg_get_int("cputemp.publishinterval", tp->publishInterval)) return false;
		tp->nextReadTime = time(NULL) + tp->readInterval;
		// enable publish if interval is present
		if (tp->publishInterval > 0) {
			tp->setPublish();
			tp->nextPublishTime = time(NULL) + tp->publishInterval;
		}
	}
	return true;
}

/** Initialise the tag database (tagstore)
 * @return false on failure
 */
bool init_tags(void) {
	Tag* tp = NULL;
	std::string strValue;
	int numTags, iVal, i;

	if (!init_hwtags()) return false;
	if (!cfg.exists("mqtt_tags")) {	// optional
		log(LOG_NOTICE,"configuration - parameter \"mqtt_tags\" does not exist");
		return true;
		}
	Setting& mqttTagsSettings = cfg.lookup("mqtt_tags");
	numTags = mqttTagsSettings.getLength();
	mbWriteTags = new ModbusTag[numTags+1];
	//printf("%s - %d mqtt tags found\n", __func__, numTags);
	for (i=0; i < numTags; i++) {
		if (mqttTagsSettings[i].lookupValue("topic", strValue)) {
			tp = ts.addTag(strValue.c_str());
			tp->setSubscribe();
			tp->registerCallback(modbus_write_request, i);
			mbWriteTags[i].setTopic(strValue.c_str());
			if (mqttTagsSettings[i].lookupValue("slaveid", iVal))
				mbWriteTags[i].setSlaveId(iVal);
			if (mqttTagsSettings[i].lookupValue("address", iVal))
				mbWriteTags[i].setAddress(iVal);
			if (mqttTagsSettings[i].exists("datatype")) {
				mqttTagsSettings[i].lookupValue("datatype", strValue);
				//printf("%s - %s\n", __func__, strValue.c_str());
				mbWriteTags[i].setDataType(strValue[0]);
			}
		}
	}
	// Mark end of list
	mbWriteTags[i].setSlaveId(MODBUS_SLAVE_MAX +1);
	mbWriteTags[i].setUpdateCycleId(-1);
	//printf("%s - allocated %d tags, last is index %d\n", __func__, i, i-1);
	return true;
}

void mqtt_connect(void) {
	if (mqttDebugEnabled)
		printf("%s - attempting to connect to mqtt broker %s.\n", __func__, mqtt.broker());
	mqtt.connect();
	mqtt_connection_in_progress = true;
	mqtt_connect_time = time(NULL);
	//printf("%s - Done\n", __func__);
}

/**
 * Initialise the MQTT broker and register callbacks
 */
bool mqtt_init(void) {
	bool bValue;
	if (!runningAsDaemon) {
		if (cfg.lookupValue("mqtt.debug", bValue)) {
			mqttDebugEnabled = bValue;
			mqtt.setConsoleLog(mqttDebugEnabled);
			if (mqttDebugEnabled) printf("%s - mqtt debug enabled\n", __func__);
		}
	}
	if (cfg.lookupValue("mqtt.retain", bValue))
		mqtt_retain = bValue;
	mqtt.registerConnectionCallback(mqtt_connection_status);
	mqtt.registerTopicUpdateCallback(mqtt_topic_update);
	mqtt_connect();
	return true;
}

/**
 * Subscribe tags to MQTT broker
 * Iterate over tag store and process every "subscribe" tag
 */
void mqtt_subscribe_tags(void) {
	//printf("%s - Start\n", __func__);
	Tag* tp = ts.getFirstTag();
	while (tp != NULL) {
		if (tp->isSubscribe()) {
			//printf("%s: %s\n", __func__, tp->getTopic());
			mqtt.subscribe(tp->getTopic());
		}
		tp = ts.getNextTag();
	}
	//printf("%s - Done\n", __func__);
}

/**
 * callback function for MQTT
 * MQTT notifies a change in connection status by calling this function
 * This function is registered with MQTT during initialisation
 */
void mqtt_connection_status(bool status) {
	//printf("%s - %d\n", __func__, status);
	// subscribe tags when connection is online
	if (status) {
		log(LOG_INFO, "Connected to MQTT broker [%s]", mqtt.broker());
		mqtt_connection_in_progress = false;
		mqtt.setRetain(mqtt_retain);
		mqtt_subscribe_tags();
	} else {
		if (mqtt_connection_in_progress) {
			mqtt.disconnect();
			// Note: the timeout is determined by OS network stack
			unsigned long timeout = time(NULL) - mqtt_connect_time;
			log(LOG_INFO, "mqtt connection timeout after %lds", timeout);
			mqtt_connection_in_progress = false;
		} else {
			log(LOG_WARNING, "Disconnected from MQTT broker [%s]", mqtt.broker());
		}
	}
	//printf("%s - done\n", __func__);
}

/**
 * callback function for MQTT
 * MQTT notifies when a subscribed topic has received an update
 * @param topic: topic string
 * @param value: value as a string
 * Note: do not store the pointers "topic" & "value", they will be
 * destroyed after this function returns
 */
void mqtt_topic_update(const char* topic, const char* value) {
	//printf("%s - %s %s\n", __func__, topic, value);
	Tag *tp = ts.getTag(topic);
	if (tp == NULL) {
		fprintf(stderr, "%s: <%s> not  in ts\n", __func__, topic);
		return;
	}
	tp->setValue(value);	// This will trigger a callback to modbus_write_request
}

/**
 * Publish tag to MQTT
 * @param tag: ModbusTag to publish
 * @param noread: publish the "noread" value (for failed read)
 */
bool mqtt_publish_tag(ModbusTag tag, bool noread = false) {
	if (!mqtt.isConnected()) return false;
	if (tag.getTopicString().empty()) return true;	// don't publish if topic is empty
	if (noread) {
		mqtt.publish(tag.getTopic(), tag.getFormat(), tag.getNoreadValue());
	} else {
		mqtt.publish(tag.getTopic(), tag.getFormat(), tag.getScaledValue());
	}
	return true;
}

/**
 * Publish noread value to all tags (normally done on program exit)
 * @param publish_noread: publish the "noread" value of the tag
 * @param clear_retain: clear retained value from broker's persistance store
 */
void mqtt_clear_tags(bool publish_noread = true, bool clear_retain = true) {
	int index = 0, tagIndex = 0;
	int *tagArray;
	ModbusTag mbTag;
	//printf("%s", __func__);
	
	//mqtt.setRetain(false);
	while (updateCycles[index].ident >= 0) {
		// ignore if cycle has no tags to process
		if (updateCycles[index].tagArray == NULL) {
			index++; continue;
		}
		// get array for tags
		tagArray = updateCycles[index].tagArray;
		// read each tag in the array
		tagIndex = 0;
		while (tagArray[tagIndex] >= 0) {
			mbTag = mbReadTags[tagArray[tagIndex]];
			if (publish_noread)
				mqtt_publish_tag(mbTag, true);			// publish noread value
			if (clear_retain)
				mqtt.clear_retained_message(mbTag.getTopic());	// clear retained status
			tagIndex++;
		}
		index++;
	}
}

#pragma mark Modbus

/**
 * Set and report slave online status to mqtt broker
 * @param slaveId: slave id to be changed
 * @param newStatus: true for online, false for offline
 * @param forceReport: publish report even if the status hasn't changed
 */
void modbus_slave_set_online_status (int slaveId, bool newStatus, bool forceReport = false) {
	string topic = mbSlaveStatusTopic;
	// range check on slaveId
	if ((slaveId > MODBUS_SLAVE_MAX) || (slaveId < MODBUS_SLAVE_MIN)) return;
	//printf("%s - %d: %d old(%d)\n", __func__, slaveId, newStatus, mbSlaveOnline[slaveId]);
	// report only if the status has changed or on forced report
	if ( (mbSlaveOnline[slaveId] != newStatus) || (forceReport)) {
		mbSlaveOnline[slaveId] = newStatus;
		if (!topic.empty()) {
			topic = topic.append(std::to_string(slaveId));
			//printf("%s - topic= %s value=%d\n", __func__, topic.c_str(), mbSlaveOnline[slaveId]);
			if (mbSlaveOnline[slaveId]) {
				mqtt.publish(topic.c_str(), "%.0f", 1);
			} else {
				mqtt.publish(topic.c_str(), "%.0f", 0);
			}
		}
	}
}

/**
 * Modbus write request
 * This a callback function from Tag class
 * it is executed when the value of tag is assigned
 * that is, when it's published by the broker
 * the request is added to the list of write requests
 */
void modbus_write_request(int callbackId, Tag *tag) {
	// update value in tag array
	mbWriteTags[callbackId].setRawValue(tag->intValue());
	// set write request on tag
	mbWriteTags[callbackId].setWritePending(true);			
	//printf("%s - %s is %d (%d)\n", __func__, tag->getTopic(), mbWriteTags[callbackId].getRawValue(),tag->intValue());
}

/**
 * Write tag to modbus device
 */
bool modbus_write_tag(ModbusTag tag) {
	int rc = 0;
	
	uint8_t slaveId = tag.getSlaveId();
	if (modbusDebugEnabled)
		printf ("%s - writing %d to Slave %d Addr %d\n", __func__, tag.getRawValue(),slaveId, tag.getAddress());
	modbus_set_slave(mb_ctx, slaveId);
	if (tag.getDataType() == 'r') {
		rc = modbus_write_register(mb_ctx, tag.getAddress(), tag.getRawValue());
	} else {
		rc = modbus_write_bit(mb_ctx, tag.getAddress(), tag.getBoolValue());
	}
	if (rc != 1) {
		if (errno == 110) {		//timeout
			if (!runningAsDaemon) {
				printf("%s - failed: no response from slave %d addr %d (timeout)\n", __func__, slaveId, tag.getAddress()); 
				}
			modbus_slave_set_online_status(slaveId, false);
		} 
		if (errno == 0x6b24250) {	// Illegal Data Address
			if (!runningAsDaemon)
				printf("%s - failed: illegal data address %d on slave %d\n", __func__, tag.getAddress(), slaveId);
		}
		log(LOG_ERR, "Modbus Write failed (%x): %s", errno, modbus_strerror(errno));
		return false;
	} else {
		// successful read
		modbus_slave_set_online_status(slaveId, true);
		if (modbusDebugEnabled) 
			printf("%s - write success, value = %d [0x%04x]\n", __func__, tag.getRawValue(), tag.getRawValue());
	}
	return true;
}

/**
 * Read tag from modbus device
 */
bool modbus_read_tag(ModbusTag tag) {
	uint16_t registers[4];
	int rc;

	uint8_t slaveId = tag.getSlaveId();
	if (modbusDebugEnabled)
		printf ("%s - reading slave %d HR %d\n", __func__, slaveId, tag.getAddress());

	modbus_set_slave(mb_ctx, slaveId);
	
	rc = modbus_read_registers(mb_ctx, tag.getAddress(), 1, registers);
	if (rc < 1) {
		if (errno == 110) {		//timeout
			if (!runningAsDaemon)
				printf("%s - failed: no response from slave %d (timeout)\n", __func__, slaveId);
			modbus_slave_set_online_status(slaveId, false);
		} 
		if (errno == 0x6b24250) {	// Illegal Data Address
			if (!runningAsDaemon)
				printf("%s - failed: illegal data address %d on slave %d\n", __func__, tag.getAddress(), slaveId);
		}
		log(LOG_ERR, "Modbus Read failed (%x): %s", errno, modbus_strerror(errno));
		mqtt_publish_tag(tag, true);	// publish noread value for failed read
		return false;
	} else {
		// successful read
		modbus_slave_set_online_status(slaveId, true);
		tag.setRawValue(registers[0]);
		if (modbusDebugEnabled) 
			printf("%s - reading success, value = %d [0x%04x]\n", __func__, registers[0], registers[0]);
		mqtt_publish_tag(tag);
	}
	return true;
}

/**
 * assign tags to update cycles
 * generate arrays of tags assigned ot the same updatecycle
 * 1) iterate over update cycles
 * 2) count tags which refer to update cycle
 * 3) allocate array for those tags
 * 4) fill array with index of tags that match update cycle
 * 5) assign array to update cycle
 * 6) go back to 1) until all update cycles have been matched
 */
bool modbus_assign_updatecycles () {
	int updidx = 0;
	int mbTagIdx = 0;
	int cycleIdent = 0;
	int matchCount = 0;
	int *intArray = NULL;
	int arIndex = 0;
	// iterate over updatecycle array
	while (updateCycles[updidx].ident >= 0) {
		cycleIdent = updateCycles[updidx].ident;
		updateCycles[updidx].tagArray = NULL;
		updateCycles[updidx].tagArraySize = 0;
		// iterate over mbReadTags array
		mbTagIdx = 0;
		matchCount = 0;
		while (mbReadTags[mbTagIdx].updateCycleId() >= 0) {
			// count tags with cycle id match
			if (mbReadTags[mbTagIdx].updateCycleId() == cycleIdent) {
				matchCount++;
				//cout << cycleIdent <<" " << mbReadTags[mbTagIdx].getAddress() << endl;
			}
			mbTagIdx++;
		}
		// skip to next cycle update if we have no matching tags
		if (matchCount < 1) {
			updidx++;
			continue;
		}
		// -- We have some matching tags
		// allocate array for tags in this cycleupdate
		intArray = new int[matchCount+1];			// +1 to allow for end marker
		// fill array with matching tag indexes
		mbTagIdx = 0;
		arIndex = 0;
		while (mbReadTags[mbTagIdx].updateCycleId() >= 0) {
			// count tags with cycle id match
			if (mbReadTags[mbTagIdx].updateCycleId() == cycleIdent) {
				intArray[arIndex] = mbTagIdx;
				arIndex++;
			}
			mbTagIdx++;
		}
		// mark end of array
		intArray[arIndex] = -1;
		// add the array to the update cycles
		updateCycles[updidx].tagArray = intArray;
		updateCycles[updidx].tagArraySize = arIndex;
		// next update index
		updidx++;
	}
	return true;
}

/**
 * read tag configuration for one slave from config file
 */
bool modbus_config_tags(Setting& mbTagsSettings, uint8_t slaveId) {
	int tagIndex;
	int tagAddress;
	int tagUpdateCycle;
	string strValue;
	float fValue;
	int intValue;
	
	int numTags = mbTagsSettings.getLength();
	if (numTags < 1) {
		cout << "No tags Found " << endl;
		return true;		// permissible condition
	}
	
	for (tagIndex = 0; tagIndex < numTags; tagIndex++) {
		if (mbTagsSettings[tagIndex].lookupValue("address", tagAddress)) {
			mbReadTags[mbTagCount].setAddress(tagAddress);
			mbReadTags[mbTagCount].setSlaveId(slaveId);
		} else {
			log(LOG_WARNING, "Error in config file, tag address missing");
			continue;		// skip to next tag
		}
		if (mbTagsSettings[tagIndex].lookupValue("update_cycle", tagUpdateCycle)) {
			mbReadTags[mbTagCount].setUpdateCycleId(tagUpdateCycle);
		}
		if (mbTagsSettings[tagIndex].lookupValue("group", intValue))
				mbReadTags[mbTagCount].setGroup(intValue);
		// is topic present? -> read mqtt related parametrs
		if (mbTagsSettings[tagIndex].lookupValue("topic", strValue)) {
			mbReadTags[mbTagCount].setTopic(strValue.c_str());
			if (mbTagsSettings[tagIndex].lookupValue("format", strValue))
				mbReadTags[mbTagCount].setFormat(strValue.c_str());
			if (mbTagsSettings[tagIndex].lookupValue("multiplier", fValue))
				mbReadTags[mbTagCount].setMultiplier(fValue);
			if (mbTagsSettings[tagIndex].lookupValue("offset", fValue))
				mbReadTags[mbTagCount].setOffset(fValue);
			if (mbTagsSettings[tagIndex].lookupValue("noread", fValue))
				mbReadTags[mbTagCount].setNoreadValue(fValue);
		}
		mbTagCount++;
		//cout << "Tag " << mbTagCount << " addr: " << tagAddress << " cycle: " << tagUpdateCycle << " Topic: " << tagTopic << endl;
	}
	return true;
}

/**
 * read slave configuration from config file
 */

bool modbus_config_slaves(Setting& mbSlavesSettings) {
	int slaveId, numTags;
	string slaveName;
	bool slaveEnabled;
	
	// we need at least one slave in config file
	int numSlaves = mbSlavesSettings.getLength();
	if (numSlaves < 1) {
		log(LOG_ERR, "Error in config file, no Modbus slaves found");
		return false;
	}
	
	// calculate the total number of tags for all configured slaves
	numTags = 0;
	for (int slavesIdx = 0; slavesIdx < numSlaves; slavesIdx++) {
		if (mbSlavesSettings[slavesIdx].exists("tags")) {
			if (!mbSlavesSettings[slavesIdx].lookupValue("enabled", slaveEnabled)) {
				slaveEnabled = true;	// true is assumed if there is no entry in config file
			}
			if (slaveEnabled) {
				Setting& mbTagsSettings = mbSlavesSettings[slavesIdx].lookup("tags");
				numTags += mbTagsSettings.getLength();
			}
		}
	}
	
	mbReadTags = new ModbusTag[numTags+1];
	
	mbTagCount = 0;
	// iterate through slaves
	for (int slavesIdx = 0; slavesIdx < numSlaves; slavesIdx++) {
		mbSlavesSettings[slavesIdx].lookupValue("name", slaveName);
		if (mbSlavesSettings[slavesIdx].lookupValue("id", slaveId)) {
			if (modbusDebugEnabled)
				printf("%s - processing Slave %d (%s)\n", __func__, slaveId, slaveName.c_str());
		} else {
			log(LOG_ERR, "Config error - modbus slave ID missing in entry %d", slaveId+1);
			return false;
		}
		
		// get list of tags
		if (mbSlavesSettings[slavesIdx].exists("tags")) {
			if (!mbSlavesSettings[slavesIdx].lookupValue("enabled", slaveEnabled)) {
				slaveEnabled = true;	// true is assumed if there is no entry in config file
			}
			if (slaveEnabled) {
				Setting& mbTagsSettings = mbSlavesSettings[slavesIdx].lookup("tags");
				if (!modbus_config_tags(mbTagsSettings, slaveId)) {
					return false; }
			} else {
				log(LOG_NOTICE, "Slave %d (%s) disabled in config", slaveId, slaveName.c_str());
			}
		} else {
			log(LOG_NOTICE, "No tags defined for Modbus %d", slaveId);
			// this is a permissible condition
		}
	}
	// mark end of array
	mbReadTags[mbTagCount].setUpdateCycleId(-1);
	mbReadTags[mbTagCount].setSlaveId(MODBUS_SLAVE_MAX +1);
	return true;
}

/**
 * read update cycles from config file
 */
bool modbus_config_updatecycles(Setting& updateCyclesSettings) {
	int idValue, interval, index;
	int numUpdateCycles = updateCyclesSettings.getLength();
	if (numUpdateCycles < 1) {
		log(LOG_ERR, "Error in config file, \"updatecycles\" missing");
		return false;
	}
	
	// allocate array 
	updateCycles = new updatecycle[numUpdateCycles+1];
	
	for (index = 0; index < numUpdateCycles; index++) {
		if (updateCyclesSettings[index].lookupValue("id", idValue)) {
		} else {
			log(LOG_ERR, "Config error - cycleupdate ID missing in entry %d", index+1);
			return false;
		}
		if (updateCyclesSettings[index].lookupValue("interval", interval)) {
		} else {
			log(LOG_ERR, "Config error - cycleupdate interval missing in entry %d", index+1);
			return false;
		}
		updateCycles[index].ident = idValue;
		updateCycles[index].interval = interval;
		updateCycles[index].nextUpdateTime = time(0) + interval;
		//cout << "Update " << index << " ID " << idValue << " Interval: " << interval << " t:" << updateCycles[index].nextUpdateTime << endl;
	}
	// mark end of data
	updateCycles[index].ident = -1;
	updateCycles[index].interval = -1;
	
	return true;
}

/**
 * read modbus configuration from config file
 */

bool modbus_config() {
	// Configure update cycles
	try {
		Setting& updateCyclesSettings = cfg.lookup("updatecycles");
		if (!modbus_config_updatecycles(updateCyclesSettings)) {
			return false; }
	} catch (const SettingNotFoundException &excp) {
		log(LOG_ERR, "Error in config file <%s> not found", excp.getPath());
		return false;
	} catch (const SettingTypeException &excp) {
		log(LOG_ERR, "Error in config file <%s> is wrong type", excp.getPath());
		return false;
	}
	
	// Configure modbus slaves
	try {
		Setting& mbSlavesSettings = cfg.lookup("mbslaves");
		if (!modbus_config_slaves(mbSlavesSettings)) {
			return false; }
	} catch (const SettingNotFoundException &excp) {
		log(LOG_ERR, "Error in config file <%s> not found", excp.getPath());
		return false;
	} catch (const SettingTypeException &excp) {
		log(LOG_ERR, "Error in config file <%s> is not a string", excp.getPath());
		return false;
	} catch (const ParseException &excp) {
		log(LOG_ERR, "Error in config file - Parse Exception");
		return false;
	} catch (...) {
		log(LOG_ERR, "modbus_config <mbslaves> Error in config file (exception)");
		return false;
	}
	return true;
}

/**
 * initialize modbus
 * @returns false for configuration error, otherwise true
 */
bool init_modbus()
{
	//char str[256];
	string rtu_port;
	string strValue;
	int port_baud = 9600;
	uint32_t response_to_sec = 0;
	uint32_t response_to_usec = 0;
	int newValue;
	//bool boolValue;
	
	// check if mobus serial device is configured
	if (!cfg_get_str("modbusrtu.device", rtu_port)) {
		return true;
	}
	// get configuration serial device
	if (!cfg_get_int("modbusrtu.baudrate", port_baud)) {
			log(LOG_ERR, "Modbus RTU missing \"baudrate\" parameter for <%s>", rtu_port.c_str());
		return false;
	}
	
	// Create modbus context
	mb_ctx = modbus_new_rtu(rtu_port.c_str(), port_baud, 'N', 8, 1);
	
	if (mb_ctx == NULL) {
		log(LOG_ERR, "Unable to allocate libmodbus context");
		return false;
	}
	
	if (!runningAsDaemon) {
		cfg.lookupValue("modbusrtu.debug", modbusDebugEnabled);
		if (modbusDebugEnabled)
			printf("%s - Modbus Debug Enabled\n", __func__);
	}
	
	// set slave status reporting topic
	if (cfg.lookupValue("modbusrtu.slavestatustopic", strValue)) {
		mbSlaveStatusTopic = strValue;
		//printf("%s - mbSlaveStatusTopic: %s\n", __func__, mbSlaveStatusTopic.c_str());
	}
	// set new response timeout if configured
	if (cfg_get_int("modbusrtu.responsetimeout_us", newValue)) {
		response_to_usec = newValue;
	}
	if (cfg_get_int("modbusrtu.responsetimeout_s", newValue)) {
		response_to_sec = newValue;
	}
	if ((response_to_usec > 0) || (response_to_sec > 0)) {
		modbus_set_response_timeout(mb_ctx, response_to_sec, response_to_usec);
	}

	if (modbusDebugEnabled) {
		// enable libmodbus debugging
		if (cfg_get_int("modbusrtu.debuglevel", newValue)) {
			if (newValue > 1) modbus_set_debug(mb_ctx, TRUE);
		}
		if (modbus_get_response_timeout(mb_ctx, &response_to_sec, &response_to_usec) >= 0)
			printf("%s response timeout %ds %dus\n", __func__, response_to_sec, response_to_usec);
	}
	
	//modbus_set_error_recovery(mb_ctx, modbus_error_recovery_mode(MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL));
	
	// Attempt to open serial device
	if (modbus_connect(mb_ctx) == -1) {
		log(LOG_ERR, "Connection failed: %s\n", modbus_strerror(errno));
		modbus_free(mb_ctx);
		mb_ctx = NULL;
		return false;
	}
	
	log(LOG_INFO, "Modbus RTU opened on port %s at %d", rtu_port.c_str(), port_baud);
	
	if (!modbus_config()) return false;
	if (!modbus_assign_updatecycles()) return false;
	
	// all slaves start life in offline mode
	for (int i=0; i <= MODBUS_SLAVE_MAX; i++)
		mbSlaveOnline[i] = false;
	
	return true;
}

#pragma mark Loops

/** 
 * set main loop interval to a valid setting
 * @param newValue the new main loop interval in ms
 */
void setMainLoopInterval(int newValue)
{
	int val = newValue;
	if (newValue < MAIN_LOOP_INTERVAL_MINIMUM) {
		val = MAIN_LOOP_INTERVAL_MINIMUM;
	}
	if (newValue > MAIN_LOOP_INTERVAL_MAXIMUM) {
		val = MAIN_LOOP_INTERVAL_MAXIMUM;
	}
	mainloopinterval = val;

	log(LOG_INFO, "Main Loop interval is %dms", mainloopinterval);
}

/** 
 * called on program exit
 */
void exit_loop(void) 
{
	bool bValue, clearonexit = false, noreadonexit = false;
	
	// set all modbus slaves as offline and publish new status
	for (int i=0; i <= MODBUS_SLAVE_MAX; i++) {
		// only if they are online, no change if they are already offline
		if (mbSlaveOnline[i]) {
			modbus_slave_set_online_status(i, false);
		}
	}
	
	// close modbus device
	if (mb_ctx != NULL) {
		modbus_close(mb_ctx);
		modbus_free(mb_ctx);
		mb_ctx = NULL;
		cout << "Modbus closed" << endl;
	}
	
	// how to handle mqtt broker published tags 
	// clear retain status for all tags?
	if (cfg.lookupValue("mqtt.clearonexit", bValue))
		clearonexit = bValue;
	// publish noread value for all tags?
	if (cfg.lookupValue("mqtt.noreadonexit", bValue)) 
		noreadonexit = bValue;
	if (noreadonexit || clearonexit)
		mqtt_clear_tags(noreadonexit, clearonexit);
	// free allocated memory
	// arrays of tags in cycleupdates
	int *ar, idx=0;
	while (updateCycles[idx].ident >= 0) {
		ar = updateCycles[idx].tagArray;
		if (ar != NULL) delete [] ar;		// delete array if one exists
		idx++;
	}
	
	delete [] updateCycles;
	delete [] mbWriteTags;
	delete [] mbReadTags;
}

/** Main program loop
 */
void main_loop()
{
	bool processing_success = false;
	//clock_t start, end;
	struct timespec starttime, endtime, difftime;
	useconds_t sleep_usec;
	//double delta_time;
	useconds_t processing_time;
	useconds_t min_time = 99999999, max_time = 0;
	useconds_t interval = mainloopinterval * 1000;	// convert ms to us
	
	// first call takes a long time (10ms)
	while (!exitSignal) {
	// run processing and record start/stop time
		clock_gettime(CLOCK_MONOTONIC, &starttime);
		processing_success = process();
		clock_gettime(CLOCK_MONOTONIC, &endtime);
		// calculate cpu time used [us]
		timespec_diff(&starttime, &endtime, &difftime);
		processing_time = (difftime.tv_nsec / 1000) + (difftime.tv_sec * 1000000);

		// store min/max times if any processing was done
		if (processing_success) {
			// calculate cpu time used [us]
			if (debugEnabled)
				printf("%s - process() took %dus\n", __func__, processing_time);
			if (processing_time > max_time) {
				max_time = processing_time;
			}
			if (processing_time < min_time) {
				min_time = processing_time;
			}
			//printf("%s - success (%dus)\n", __func__, processing_time);
		}
		// enter loop delay if needed
		// if cpu_time_used exceeds the mainLoopInterval
		// then bypass the loop delay
		if (interval > processing_time) {
			sleep_usec = interval - processing_time;  // sleep time in us
			//printf("%s - sleeping for %dus (%dus)\n", __func__, sleep_usec, processing_time);
			usleep(sleep_usec);
		} 
	}
	if (!runningAsDaemon)
		printf("CPU time for variable processing: %dus - %dus\n", min_time, max_time);
}

/** Display program usage instructions.
 * @param
 * @return
 */
static void showUsage(void) {
	cout << "usage:" << endl;
	cout << execName << "-cCfgFileName -d -h" << endl;
	cout << "c = name of config file (.cfg is added automatically)" << endl;
	cout << "d = enable debug mode" << endl;
	cout << "h = show help" << endl;
}

/** Parse command line arguments.
 * @param argc argument count
 * @param argv array of arguments
 * @return false to indicate program needs to abort
 */
bool parseArguments(int argc, char *argv[]) {
	char buffer[64];
	int i, buflen;
	int retval = true;
	execName = std::string(basename(argv[0]));
	cfgFileName = execName;

	if (argc > 1) {
		for (i = 1; i < argc; i++) {
			strcpy(buffer, argv[i]);
			buflen = strlen(buffer);
			if ((buffer[0] == '-') && (buflen >=2)) {
				switch (buffer[1]) {
				case 'c':
					cfgFileName = std::string(&buffer[1]);
					break;
				case 'd':
					debugEnabled = true;
					printf("Debug enabled\n");
					break;
				case 'h':
					showUsage();
					retval = false;
					break;
				default:
					log(LOG_NOTICE, "unknown parameter: %s", argv[i]);
					showUsage();
					retval = false;
					break;
				} // switch
				;
			} // if
		}  // for (i)
	}  // if (argc >1)
	// add config file extension
	cfgFileName += std::string(CFG_FILENAME_EXT);
	return retval;
}

int main (int argc, char *argv[])
{
	if ( getppid() == 1) runningAsDaemon = true;
	processName =  argv[0];

	if (! parseArguments(argc, argv) ) goto exit_fail;

	log(LOG_INFO,"[%s] PID: %d PPID: %d", argv[0], getpid(), getppid());
	log(LOG_INFO,"Version %d.%02d [%s] ", version_major, version_minor, build_date_str);

	signal (SIGINT, sigHandler);
	//signal (SIGHUP, sigHandler);

	// catch SIGTERM only if running as daemon (started via systemctl)
	// when run from command line SIGTERM provides a last resort method
	// of killing the process regardless of any programming errors.
	if (runningAsDaemon) {
		signal (SIGTERM, sigHandler);
	}

	// read config file
	if (! readConfig()) {
		log(LOG_ERR, "Error reading config file <%s>", cfgFileName.c_str());
		goto exit_fail;
	}

	if (!init_tags()) goto exit_fail;
	if (!mqtt_init()) goto exit_fail;
	if (!init_values()) goto exit_fail;
	if (!init_modbus()) goto exit_fail;
	usleep(100000);
	main_loop();

	exit_loop();
	log(LOG_INFO, "exiting");
	exit(EXIT_SUCCESS);

exit_fail:
	log(LOG_INFO, "exit with error");
	exit(EXIT_FAILURE);
}
