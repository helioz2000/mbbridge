/**
 * @file mqtt.cpp
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
#include <syslog.h>

#include <stdexcept>
#include <iostream>

#include "mqtt.h"

/*********************
 *      DEFINES
 *********************/

#define DEFAULT_CLIENT_ID "mqtt_client"        // our ID for broker connection
#define MQTT_BROKER_DEFAULT "127.0.0.1"
#define MQTT_BROKER_DEFAULT_PORT 1883
#define MQTT_BROKER_DEFAULT_KEEPALIVE 60
#define MQTT_RETAIN_DEFAULT false

using namespace std;

/*********************
 *  MOSQUITTO INFORMATION
 *********************
 *
struct mosquitto_message{
	int mid;
	char *topic;
	void *payload;
	int payloadlen;
	int qos;
	bool retain;
};
 */


/*********************
 *  GLOBAL FUNCTIONS
 *********************
 *
 * These functions are used for mosquitto callback implementation
 * the parameter obj contains a pointer to a MQTT class instance
 */

// Callback function for mosquitto connect async
static void on_connect(struct mosquitto *mosq, void *obj, int result) {
    // callback function of the relevant instance
    ((MQTT*)obj)->connect_callback(mosq, result);
}

// Callback function for mosquitto disconnect async
static void on_disconnect(struct mosquitto *mosq, void *obj, int rc) {
    // callback function of the relevant instance
    ((MQTT*)obj)->disconnect_callback(mosq, rc);
}

// Callback function for mosquitto publish
static void on_publish(struct mosquitto *mosq, void *obj, int mid) {
    ((MQTT*)obj)->publish_callback(mosq, mid);
}

// Callback function for mosquitto message receive
static void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    ((MQTT*)obj)->message_callback(mosq, message);
}

// Callback function for mosquitto logging
static void on_log(struct mosquitto *mosq, void *obj, int level, const char *str) {
	((MQTT*)obj)->log_callback(mosq, level, str);
}

// Callback function for mosquitto subscribe
static void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos) {
    ((MQTT*)obj)->subscribe_callback(mosq, mid, qos_count, granted_qos);
}

/*********************
 * GLOBAL FUNCTIONS
 *********************/

 /*********************
  * MEMBER FUNCTIONS
  *********************/

 //
 // Class MQTT
 //

 MQTT::MQTT() {
     _construct(DEFAULT_CLIENT_ID);
 }

 MQTT::MQTT (const char* clientID) {
     _construct(clientID);
}

 void MQTT::_construct (const char* clientID) {
     _connected = false;
     _console_log_enable = false;
     _qos = 0;
     _retain = MQTT_RETAIN_DEFAULT;
     connectionStatusCallback = NULL;
     topicUpdateCallback = NULL;
     _mqttBroker.assign( MQTT_BROKER_DEFAULT );
     _mqttPort = MQTT_BROKER_DEFAULT_PORT;
     _mqttKeepalive = MQTT_BROKER_DEFAULT_KEEPALIVE;

     // initialise library
     mosquitto_lib_init();
     int major, minor, revision, result;
     result = mosquitto_lib_version(&major, &minor, &revision);
     printf("mosquitto library V%d.%d.%d (%d)\n", major, minor, revision, result);
     syslog(LOG_INFO, "mosquitto library V%d.%d.%d (%d)", major, minor, revision, result);

     // create new mqtt
     _mosq = mosquitto_new(clientID, false, this);  // "this" provides a link from calllback to class instance
     if (_mosq == NULL) {
         syslog(LOG_ERR,"Class MQTT - mosquitto_new returned NULL");
         throw runtime_error("Class MQTT - mosquitto_new returned NULL");
     }

     // start mqtt processing loop in own thread
     result = mosquitto_loop_start(_mosq);
     if (result != MOSQ_ERR_SUCCESS) {
         syslog(LOG_ERR, "Class MQTT - mosquitto_loop_start failed");
         throw runtime_error("Class MQTT - mosquitto_loop_start failed");
     }

     // set callback functions
     mosquitto_connect_callback_set(_mosq, on_connect);
     mosquitto_disconnect_callback_set(_mosq, on_disconnect);
     mosquitto_publish_callback_set(_mosq, on_publish);
     mosquitto_message_callback_set(_mosq, on_message);
     mosquitto_log_callback_set(_mosq, on_log);
     mosquitto_subscribe_callback_set(_mosq, on_subscribe);
 }

 MQTT::~MQTT() {
     //printf("%s - Connected: %d\n", __func__, connected);
     if (_connected) mosquitto_disconnect(_mosq) ;
     //mosquitto_loop_stop(_mosq, false);
     mosquitto_loop_stop(_mosq, true); // Note: must be true or this will block
     if (_mosq != NULL) {
         mosquitto_destroy(_mosq);
         _mosq = NULL;
     }
     mosquitto_lib_cleanup();
 }

#pragma mark Connecting

void MQTT::connect(void) {
    char strbuf[255];
    // connect to mqtt server
    int result = mosquitto_connect_async(_mosq, _mqttBroker.c_str(), _mqttPort, _mqttKeepalive);
    if (result != MOSQ_ERR_SUCCESS) {
        sprintf(strbuf, "%s - mosquitto_connect failed: %s [%d]\n", __func__, strerror(result), result);
        syslog(LOG_ERR, "mosquitto_connect failed: %s [%d]", strerror(result), result);
        throw runtime_error(strbuf);
    }
    //printf ("%s\n", __func__);
}

void MQTT::disconnect(void) {
    if (_connected) mosquitto_disconnect(_mosq) ;
}

#pragma mark Operation

void MQTT::registerConnectionCallback(void (*callback) (bool)) {
    connectionStatusCallback = callback;
}

void MQTT::registerTopicUpdateCallback(void (*callback) (const struct mosquitto_message*)) {
    topicUpdateCallback = callback;
}

int MQTT::publish(const char* topic, const char* format, float value, bool pubRetain) {
    int messageid = 0;
    if (!_connected) {
        fprintf(stderr, "%s: Not Connected!\n", __func__);
        return -1;
    } else {
        //printf ("%s: %s\n", __func__, topic);
    }
    sprintf(_pub_buf, format, value);
    //printf ("%s: %s %s\n", __func__, topic, _pub_buf);
    int result = mosquitto_publish(_mosq, &messageid, topic, strlen(_pub_buf), (const char *) _pub_buf, _qos, pubRetain);
    if (result != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "%s: %s [%s]\n", __func__, mosquitto_strerror(result), topic);
    }
    return messageid;
}

int MQTT::clear_retained_message(const char* topic) {
    int messageid = 0;
    if (!_connected) {
        fprintf(stderr, "%s: Not Connected!\n", __func__);
        return -1;
    } else {
        //printf ("%s: %s\n", __func__, topic);
    }
	// publishing an empty message with retain on will clear the message from 
	// mosquitto's persistance store
    int result = mosquitto_publish(_mosq, &messageid, topic, 0, "", _qos, true);
    if (result != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "%s: %s [%s]\n", __func__, mosquitto_strerror(result), topic);
    }
    return messageid;
}

int MQTT::subscribe(const char *topic) {
    int messageid = 0;
    int result = mosquitto_subscribe(_mosq, &messageid, topic, _qos);
    if (result != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "%s: %s [%s]\n", __func__, mosquitto_strerror(result), topic);
    } else {
        //printf ("%s: %s\n", __func__, topic);
    }
    return messageid;
}

int MQTT::unsubscribe(const char *topic) {
    int messageid = 0;
    int result = mosquitto_unsubscribe(_mosq, &messageid, topic);
    if (result != MOSQ_ERR_SUCCESS) {
        syslog(LOG_ERR, "%s [%s]", mosquitto_strerror(result), topic);
        fprintf(stderr, "%s: %s [%s]\n", __func__, mosquitto_strerror(result), topic);
    }
    return messageid;
}

#pragma mark Setters and Getters

void MQTT::setConsoleLog(bool enable) {
    _console_log_enable = enable;
}

int MQTT::setBroker(const char *newBroker) {
    _mqttBroker = newBroker;
    return 0;
}

const char* MQTT::broker(void) {
    return _mqttBroker.c_str();
}

unsigned int MQTT::port(void) {
    return _mqttPort;
}

bool MQTT::isConnected(void) {
    return _connected;
}

int MQTT::setRetain(bool newRetain) {
	_retain = newRetain;
	return 0;
}

bool MQTT::getRetain(void) {
	return _retain;
}

#pragma mark Callbacks

void MQTT::message_callback(struct mosquitto *m, const struct mosquitto_message *message) {
	//fprintf(stderr, "%s:\n", __func__);
	/*
	if(message->payloadlen){
		fprintf(stderr, "%s %s\n", message->topic, (const char *)message->payload);
	}else{
		fprintf(stderr, "%s (null)\n", message->topic);
	}
	*/
	if (topicUpdateCallback != NULL) {
		(*topicUpdateCallback) (message);
		//fprintf(stderr, "%s - topicUpdateCallback done %s\n", __func__, message->topic);
	}
//	else {
//		fprintf(stderr, "%s - topicUpdateCallback is NULL\n", __func__);
//	}
}

void MQTT::log_callback(struct mosquitto *m, int level, const char *str) {
    /* Print all log messages regardless of level. */
    if (_console_log_enable) {
        fprintf(stderr, "%s [%d]: %s\n",__func__ , level, str);
    }
}

void MQTT::subscribe_callback(struct mosquitto *m, int mid, int qos_count, const int *granted_qos) {
    //printf("%s: mid:%d qos_count:%d\n", __func__, mid, qos_count);
}

void MQTT::publish_callback(struct mosquitto *m, int mid) {
    //fprintf(stderr, "%s: %d\n", __func__, mid );
}

void MQTT::connect_callback(struct mosquitto *m, int result) {
     //printf("%s: %s\n", __func__ , mosquitto_connack_string(result) );
     if (result == MOSQ_ERR_SUCCESS) {
         _connected = true;
         if (_console_log_enable) {
             printf("%s: connection success\n", __func__);
         }
     } else {
         syslog(LOG_ERR, "%s", mosquitto_connack_string(result));
         fprintf(stderr, "%s: %s\n", __func__ , mosquitto_connack_string(result) );
     }
     if (connectionStatusCallback != NULL) {
         (*connectionStatusCallback) (_connected);
     }
}

void MQTT::disconnect_callback(struct mosquitto *m, int rc) {
     //fprintf(stderr, "%s: %s\n", __func__, mosquitto_strerror(rc) );
     _connected = false;
     if (connectionStatusCallback != NULL) {
         (*connectionStatusCallback) (_connected);
     }
 }

 /*********************
  * PRIVATE FUNCTIONS
  *********************/
