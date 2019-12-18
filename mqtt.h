/**
 * @file mqtt.h
 *
 -----------------------------------------------------------------------------
  The MQTT class encapsulates the mosquitto connection used for publishing
  and receiving data via the MQTT protocol from a broker.

 -----------------------------------------------------------------------------
 */

#ifndef MQTT_H
#define MQTT_H

//#include <time.h>

#include <mosquitto.h>

#include <string>

class MQTT {
public:
    // Constructor
    MQTT ();

    /**
     * Constructor
     * @param clientID: client ID for broker connections
     */
    MQTT (const char* clientID);

    // Destructor
    ~MQTT();

    /**
     * Connect to the MQTT broker
     */
    void connect(void);

    /**
     * Disconnect from the MQTT broker
     */
    void disconnect(void);

    /**
     * enable / disable console logging
     */
    void setConsoleLog(bool enable);

    /**
     * register callback for connection status change
     */
    void registerConnectionCallback(void (*callback) (bool));

    /**
     * register callback for topic update
     */
    void registerTopicUpdateCallback(void (*callback) (const char*, const char*));

    /**
     * callback function for async connect
     * @param mosq: pointer to mosquitto structure
     * @param result: connection result
     */
    void connect_callback(struct mosquitto *mosq, int result);

    /**
     * callback function for disconnect
     * @param mosq: pointer to mosquitto structure
     * @param rc: result code
     */
    void disconnect_callback(struct mosquitto *mosq, int rc);

    /**
     * callback function for publish
     * @param mosq: pointer to mosquitto structure
     * @param mid: message id
     */
    void publish_callback(struct mosquitto *mosq, int mid);

    /**
     * callback function for message receive
     * called by mosquitto when a subscribed topic is updated
     * @param mosq: pointer to mosquitto structure
     * @param message: the received mosquite message
     */
    void message_callback(struct mosquitto *m, const struct mosquitto_message *message);

     /**
      * callback function for logging
      * @param mosq: pointer to mosquitto structure
      * @param level: log level
      * @param str: log message
      */
    void log_callback(struct mosquitto *m, int level, const char *str);

    /**
     * callback function for subscribe
     * @param mosq: pointer to mosquitto structure
     * @param mid: message ID
     * @param qos_count: the number of granted subscriptions (size of granted_qos)
     * @param granted_qos: array of integers indicating the granted QoS for each of the subscriptions
     */
    void subscribe_callback(struct mosquitto *m, int mid, int qos_count, const int *granted_qos);

    /**
     * publish topic
     * @param topic: the topic name to be published
     * @param format: printf style format string
     * @param value: the numeric value to publish
     * @return: message ID, can be used for further tracking
     */
    int publish(const char* topic, const char* format, float value);

    /**
     * subscribe to a topic
     * @param topic: topic string
     * @return: message ID, can be used for further tracking
     */
    int subscribe(const char *topic);

    /**
     * unsubscribe from a topic
     * @param topic: topic string
     * @return: message ID, can be used for further tracking
     */
    int unsubscribe(const char *topic);

    /**
     * get MQTT broker
     * @return: 0 on success, negative number for error
     */
    int setBroker(const char *newBroker);

    /**
     * get MQTT broker
     * @return: mqtt broker
     */
    const char* broker(void);

    /**
     * get MQTT server port
     * @return: mqtt server port
     */
    unsigned int port(void);

    /**
     * check connection status
     * @param buffer: text storage buffer
     * @returns: true if connection is established
     */
    bool isConnected(void);


private:
    void (*connectionStatusCallback) (bool);     // callback for connection status change
    void (*topicUpdateCallback) (const char *topic, const char *value);     // callback for topic update
    void _construct (const char* clientID);

    struct mosquitto *_mosq;
    bool _connected;
    char _pub_buf[100];
    std::string _mqttBroker;
    unsigned int _mqttPort;
    int _mqttKeepalive;

    bool _console_log_enable;    // for mosqitto logging

    int _qos;        // quality of service [0..2]
    bool _retain;    // reained message (last known value)
};

#endif /* HARDWARE_H */
