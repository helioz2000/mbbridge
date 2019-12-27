/**
 * @file mbbridge.cpp
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

#include "mqtt.h"
#include "datatag.h"
#include "modbustag.h"

using namespace std;
using namespace libconfig;
//using namespace Modbus;

#define CFG_FILENAME_EXT ".cfg"
#define CFG_DEFAULT_FILEPATH "/etc/"

#define VAR_PROCESS_INTERVAL 5      // seconds
//#define PROCESS_LOOP_INTERVAL 100	// milli seconds
#define MAIN_LOOP_INTERVAL_MINIMUM 50     // milli seconds
#define MAIN_LOOP_INTERVAL_MAXIMUM 2000   // milli seconds

#define MQTT_CONNECT_TIMEOUT 5      // seconds
#define MQTT_BROKER_DEFAULT "127.0.0.1"
#define CPU_TEMP_TOPIC "ham/vk2ray/site/raylog/cpu/temp"
//#define ENV_TEMP_TOPIC "binder/home/screen1/env/temp"

const char *build_date_str = __DATE__ " " __TIME__;
static string cfgFileName;
static string execName;
bool exitSignal = false;
bool debugEnabled = false;
bool runningAsDaemon = false;
time_t var_process_time = time(NULL) + VAR_PROCESS_INTERVAL;
time_t mqtt_connection_timeout = 0;
time_t mqtt_connect_time = 0;   // time the connection was initiated
bool mqtt_connection_in_progress = false;
std::string processName;
char *info_label_text;
useconds_t mainloopinterval = 250;   // milli seconds
//extern void cpuTempUpdate(int x, Tag* t);
//extern void roomTempUpdate(int x, Tag* t);
modbus_t *mb_ctx = NULL;


#pragma mark Proto types
void subscribe_tags(void);
void mqtt_connection_status(bool status);
void mqtt_topic_update(const char *topic, const char *value);
void setMainLoopInterval(int newValue);

TagStore ts;
MQTT mqtt;
Config cfg;		// config file name


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

#pragma mark Config File functions

/** Read configuration file.
 * @param
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

#pragma mark Processing

/**
 * Process variables
 * Local variables are processed at a fixed time interval
 * The processing involves reading value from hardware and
 * publishing the value to MQTT broker
 * @returns true if one or more variable were processed
 */
bool var_process(void) {
    bool retval = false;
    time_t now = time(NULL);
//    if (now > var_process_time) {
//        var_process_time = now + VAR_PROCESS_INTERVAL;

    // update CPU temperature
    Tag *tag = ts.getTag((char*) CPU_TEMP_TOPIC);
    if (tag != NULL) {
        // read time due ?
        if (tag->nextReadTime <= now) {
            //tag->setValue(hw.read_cpu_temp());
            tag->nextReadTime = now + tag->readInterval;
	    retval = true;
        }
        // publish time due ?
        if (tag->nextPublishTime <= now) {
            if (mqtt.isConnected()) {
                mqtt.publish(CPU_TEMP_TOPIC, "%.1f", tag->floatValue() );
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

/** Process all  variables
 * @return true if at least one variable was processed
 */
bool process() {
	return var_process();
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

/** Initialise the tag database (tagstore)
 * @return false on failure
 */
bool init_tags(void)
{
/*
	Tag* tp = NULL;
	std::string path;

    // CPU temperature
    try {
        tp = ts.addTag(cfg.lookup("cputemp.topic"));
    } catch (const SettingNotFoundException &excp) {
        ;
    } catch (const SettingTypeException &excp) {
        std::cerr << "Error in config file <" << excp.getPath() << "> is not a string" << std::endl;
        return false;
    }

    if (tp) {
	tp->publishInterval = 0;
	if (!cfg_get_int("cputemp.readinterval", tp->readInterval)) return false;
	if (!cfg_get_int("cputemp.publishinterval", tp->publishInterval)) return false;
        time_t now = time(NULL);
        tp->nextReadTime = now + tp->readInterval;
	// enable publish if interval is present
	if (tp->publishInterval > 0) {
		tp->setPublish();
        	tp->nextPublishTime = now + tp->publishInterval;
	}
    }
*/
    return true;
}

void mqtt_connect(void) {
    printf("%s - attempting to connect to mqtt broker %s.\n", __func__, mqtt.broker());
    mqtt.connect();
    mqtt_connection_timeout = time(NULL) + MQTT_CONNECT_TIMEOUT;
    mqtt_connection_in_progress = true;
    mqtt_connect_time = time(NULL);
    //printf("%s - Done\n", __func__);
}

/**
 * Initialise the MQTT broker and register callbacks
 */
bool mqtt_init(void) {
    if (debugEnabled) {
        mqtt.setConsoleLog(true);
    }
    mqtt.registerConnectionCallback(mqtt_connection_status);
    mqtt.registerTopicUpdateCallback(mqtt_topic_update);
    mqtt_connect();
    return true;
}

/**
 * Subscribe tags to MQTT broker
 * Iterate over tag store and process every "subscribe" tag
 */
void subscribe_tags(void) {
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
        subscribe_tags();
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
 *
 * Note: do not store the pointers "topic" & "value", they will be
 * destroyed after this function returns
 */
void mqtt_topic_update(const char *topic, const char *value) {
	//printf("%s - %s %s\n", __func__, topic, value);
	Tag *tp = ts.getTag(topic);
	if (tp == NULL) {
		fprintf(stderr, "%s: <%s> not  in ts\n", __func__, topic);
		return;
	}
	tp->setValue(value);
}

#pragma mark Modbus

/**
 * read tag configuration for one slave from config file
 */

bool modbus_config_tags(Setting& mbTagsSettings) {
	int numTags = mbTagsSettings.getLength();
	if (numTags < 1) {
		cout << "No tags Found " << endl;
		return true;		// permissible condition
	}
	cout << "Found " << numTags << " tags" << endl;
	return true;
}

/**
 * read slave configuration from config file
 */

bool modbus_config_slaves(Setting& mbSlavesSettings) {
	int slaveId;
	
	// we need at least one slave in config file
	int numSlaves = mbSlavesSettings.getLength();
	if (numSlaves < 1) {
		log(LOG_ERR, "Error in config file, no Modbus slaves found");
		return false;
	}
	// iterate through slaves
	for (int slavesIdx = 0; slavesIdx < numSlaves; slavesIdx++) {
		if (mbSlavesSettings[slavesIdx].lookupValue("id", slaveId)) {
			cout << "Modbus Slave: " << slaveId << endl;
		} else {
			log(LOG_ERR, "Config error - modbus slave ID missing in entry %d", slaveId+1);
			return false;
		}
		
		// get list of tags
		if (mbSlavesSettings[slavesIdx].exists("tags")) {
			Setting& mbTagsSettings = mbSlavesSettings[slavesIdx].lookup("tags");
			if (!modbus_config_tags(mbTagsSettings)) {
				return false; }
		} else {
			log(LOG_NOTICE, "No tags defined for Modbus %d", slaveId);
			// this is a permissible condition
		}
	}
	return true;
}

/**
 * read modbus configuration from config file
 */

bool modbus_config() {
	
	try {
		Setting& mbSlavesSettings = cfg.lookup("mbslaves");
		if (!modbus_config_slaves(mbSlavesSettings)) {
			log(LOG_ERR, "modbus_config_slaves failed");
			return false;
		}
	} catch (const SettingNotFoundException &excp) {
		log(LOG_ERR, "Error in config file <%s> not found", excp.getPath());
		return false;
	} catch (const SettingTypeException &excp) {
		log(LOG_ERR, "Error in config file <%s> is not a string", excp.getPath());
		return false;
	} catch (...) {
		log(LOG_ERR, "Error in config file (exception)");
		return false;
	}
	return true;
}

bool modbus_test() {
	uint16_t registers[4];
	int rc;
	
	cout << "Running Modbus Test" << endl;
	
	modbus_set_slave(mb_ctx, 30);
	
	//nb_points = 10;
	//tab_rp_registers = (uint16_t *) malloc(nb_points * sizeof(uint16_t));
	//memset(registers, 0, nb_points * sizeof(uint16_t));
	// read holding register
	rc = modbus_read_registers(mb_ctx, 100, 1, registers);
	if (rc!=1) {
		log(LOG_ERR, "Holding Register Read failed: %s\n", modbus_strerror(errno));
		return false;
	} else {
		printf("Register Value: %d\n", registers[0]);
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
	int port_baud = 9600;
	uint32_t old_response_to_sec;
	uint32_t old_response_to_usec;
	
	// check if mobus serial device is configured
	if (!cfg_get_str("modbusrtu.device", rtu_port)) {
		return true;
	}
	// get configuration serial device
	if (!cfg_get_int("modbusrtu.baudrate", port_baud)) {
			log(LOG_ERR, "Modbus RTU missing \"baudrate\" parameter for <%s>", rtu_port.c_str());
		return false;
	}
	
	mb_ctx = modbus_new_rtu(rtu_port.c_str(), port_baud, 'N', 8, 1);
	
	if (mb_ctx == NULL) {
		log(LOG_ERR, "Unable to allocate libmodbus context");
		return false;
	}
	
	
	modbus_set_debug(mb_ctx, TRUE);
	
	modbus_set_error_recovery(mb_ctx, modbus_error_recovery_mode(MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL));
	
	// Attempt to open serial device
	//modbus_set_slave(mb_ctx, 1);
	modbus_get_response_timeout(mb_ctx, &old_response_to_sec, &old_response_to_usec);
	if (modbus_connect(mb_ctx) == -1) {
		log(LOG_ERR, "Connection failed: %s\n", modbus_strerror(errno));
		modbus_free(mb_ctx);
		mb_ctx = NULL;
		return false;
	}
	
	log(LOG_INFO, "Modbus RTU opened on port %s at %d", rtu_port.c_str(), port_baud);
	
	if (!modbus_config()) {
		cerr << "modbus_config failed" << endl;
		return false; }
	
	//return modbus_test();
	
	return true;
}


#pragma mark Loops

/** set main loop interval to a valid setting
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
	// close modbus device
	if (mb_ctx != NULL) {
		modbus_close(mb_ctx);
		modbus_free(mb_ctx);
		mb_ctx = NULL;
		cout << "Modbus closed" << endl;
	}
}

/** Main program loop
 */
void main_loop()
{
	bool processing_success = false;
	clock_t start, end;
	useconds_t sleep_usec;
	double delta_time;
	useconds_t processing_time;
	useconds_t min_time = 99999999, max_time = 0;
	useconds_t interval = mainloopinterval * 1000;	// convert ms to us

	// first call takes a long time (10ms)
	while (!exitSignal) {
	// run processing and record start/stop time
		start = clock();
		processing_success = process();
		end = clock();
		// calculate cpu time used [ms]
		delta_time = (((double) (end - start)) / CLOCKS_PER_SEC) * 1000.0;
		processing_time = (useconds_t) (delta_time * 1000);

		// store min/max times if any processing was done
		if (processing_success) {
			if (processing_time > max_time) {
				max_time = processing_time;
			}
			if (processing_time < min_time) {
				min_time = processing_time;
			}
		}
		// enter loop delay if needed
		// if cpu_time_used exceeds the mainLoopInterval
		// then bypass the loop delay
		if (mainloopinterval > processing_time) {
			sleep_usec = interval - processing_time;  // sleep time in us
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
                }
                ;
            } // if
        }  // for (i)
    }  //if (argc >1)

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
