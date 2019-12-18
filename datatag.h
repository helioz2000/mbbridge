/**
 * @file datatag.h

-----------------------------------------------------------------------------
 Two classes provide encapsulation for typical use of a data tag in an
 automation oriented user interface.
 This implementation is targeted data which is based on the MQTT protocol
 and stores the data access information as a topic path (see MQTT details)

 Class "Tag" encapsulates a single data unit
 Class "TagList" provides a facility to manage a list of tags

 The Tag class provides for a callback interface which is intended to update
 a user interface element (e.g. display value) only when data changes

 The "publish" member defines if a tag's value is published (written)
 to an mqtt broker or if it is subscribed (read from MQTT broker).
 This information is used outside this class.
-----------------------------------------------------------------------------
*/

#ifndef _DATATAG_H_
#define _DATATAG_H_

#include <stdint.h>

#include <iostream>
#include <string>

#define MAX_TAG_NUM 100         // The mximum number of tags which can be stored in TagList


class Tag {
public:
    /**
     * Invalid - Empty constructor throws runtime error
     */
    Tag();

    /**
     * Constructor
     * @param topic: tag topic
     */
    Tag(const char* topicStr);

    /**
     * Destructor
     */
    ~Tag();

    /**
     * Get topic CRC
     * @return the CRC for the topic string
     */
    uint16_t getTopicCrc(void);

    /**
     * Get the topic string
     * @return the topic string
     */
    const char* getTopic(void);

    /**
     * Register a callback function to notify value changes
     * @param updatePtr a pointer to the upadate function
     */
    void registerCallback(void (*updateCallback) (int,Tag*), int callBackID );

    /**
     * Set the value
     * @return the callback ID set with registerCallback
     */
    int valueUpdateID();

    void testCallback();

    /**
     * Set the value
     * @param doubleValue: the new value
     */
    void setValue(double doubleValue);

    /**
     * Set the value
     * @param floatValue: the new value
     */
    void setValue(float floatValue);

    /**
     * Set the value
     * @param intValue: the new value
     */
    void setValue(int intValue);

    /**
     * Set the value
     * @param strValue: new value as a string
     * @returns true on success
     */
    bool setValue(const char* strValue);

    /**
     * Get value
     * @return value as double
     */
    double doubleValue(void);

    /**
     * Get value
     * @return value as float
     */
    float floatValue(void);

    /**
     * Get value
     * @return value as int
     */
    int intValue(void);

    /**
     * is tag "publish"
     * @return true if publish or false if subscribe
     */
    bool isPublish();

    /**
     * is tag "subscribe"
     * @return false if publish or true if subscribe
     */
    bool isSubscribe();

    /**
     * Mark tag as "publish"
     */
    void setPublish(void);

    /**
     * Mark tag as "subscribe" (NOT publish)
     */
    void setSubscribe(void);

    // public members used to store data which is not used inside this class
    int readInterval;                   // seconds between reads
    time_t nextReadTime;                // next scheduled read
    int publishInterval;                // seconds between publish
    time_t nextPublishTime;             // next publish time

private:
    // All properties of this class are private
    // Use setters & getters to access these values
    std::string topic;                  // storage for topic path
    uint16_t topicCRC;                  // CRC on topic path
    double topicDoubleValue;            // storage numeric value
    time_t lastUpdateTime;              // last update time (change of value)
    void (*valueUpdate) (int,Tag*);     // callback for value update
    int _valueUpdateID;                 // ID for value update
    bool publish;                       // true = we publish, false = we subscribe
};

class TagStore {
public:
    TagStore();
    ~TagStore();

    /**
     * Add a tag
     * @param tagTopic: the topic as a string
     * @return reference to new tag or NULL on failure
     */
    Tag* addTag(const char* tagTopic);

    /**
     * Delete all tags from tag list
     */
    void deleteAll(void);

    /**
     * Find tag in the list and return reference
     * @param tagTopic: the topic as a string
     * @return reference to tag or NULL is if not found
     */
    Tag* getTag(const char* tagTopic);

    /**
     * Get first tag from store
     * use in conjunction with getNextTag to iterate over all tags
     * @return reference to first tag or NULL if tagList is empty
     */
    Tag* getFirstTag(void);

    /**
     * Get next tag from store
     * use in conjunction with getFirstTag to iterate over all tags
     * @return reference to next tag or NULL if end of tagList
     */
     Tag* getNextTag(void);

private:
    Tag *tagList[MAX_TAG_NUM];     // An array references to Tags
    int iterateIndex;              // to interate over all tags in store
};

#endif /* _DATATAG_H_ */
