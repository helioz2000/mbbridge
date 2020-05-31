/**
 * @file datatag.cpp
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
#include "datatag.h"

#include <stdexcept>
#include <iostream>

/*********************
 *      DEFINES
 *********************/
#define CRC16 0x8005
using namespace std;

/*********************
 * GLOBAL FUNCTIONS
 *********************/

/**
 * Generate CRC16 for data
 * @param data: sequence of bytes
 * @param size: number of bytes
 */
static uint16_t gen_crc16(const char *data, uint16_t size)
{
    uint16_t out = 0;
    int bits_read = 0, bit_flag;

    /* Sanity check: */
    if(data == NULL)
        return 0;

    while(size > 0)
    {
        bit_flag = out >> 15;

        /* Get next bit: */
        out <<= 1;
        out |= (*data >> bits_read) & 1; // item a) work from the least significant bits

        /* Increment bit counter: */
        bits_read++;
        if(bits_read > 7)
        {
            bits_read = 0;
            data++;
            size--;
        }

        /* Cycle check: */
        if(bit_flag)
            out ^= CRC16;

    }

    // item b) "push out" the last 16 bits
    int i;
    for (i = 0; i < 16; ++i) {
        bit_flag = out >> 15;
        out <<= 1;
        if(bit_flag)
            out ^= CRC16;
    }

    // item c) reverse the bits
    uint16_t crc = 0;
    i = 0x8000;
    int j = 0x0001;
    for (; i != 0; i >>=1, j <<= 1) {
        if (i & out) crc |= j;
    }

    return crc;
}

/*********************
 * MEMBER FUNCTIONS
 *********************/

//
// Class Tag
//

Tag::Tag() {
    printf("%s\n", __func__);
    throw runtime_error("Class Tag - forbidden constructor");
}

Tag::Tag(const char *topicStr) {

    if (topicStr == NULL) {
        throw invalid_argument("Class Tag - topic is NULL");
    }
    this->_topic = topicStr;
    _valueUpdate = NULL;
    _valueUpdateID = -1;
    _publish = false;        // subscribe tag
    _publish_retain = false;
    //cout << topic << endl;
    _topicCRC = gen_crc16(_topic.data(), _topic.length());
    //cout << topicCRC << endl;
}

Tag::~Tag() {
    //printf("%s - %s\n", __func__, topic.c_str());
}

const char* Tag::getTopic(void) {
    return _topic.c_str();
}

uint16_t Tag::getTopicCrc(void) {
    return _topicCRC;
}

void Tag::registerCallback(void (*updateCallback) (int, Tag*), int callBackID) {
    //printf("%s - 1\n", __func__);
    _valueUpdate = updateCallback;
    _valueUpdateID = callBackID;
    //printf("%s - 2\n", __func__);
}

int Tag::valueUpdateID(void) {
    return _valueUpdateID;
}

void Tag::testCallback() {
    if (_valueUpdate != NULL) {
        (*_valueUpdate) (_valueUpdateID, this);
    }
}

void Tag::setValue(double doubleValue) {
    _topicDoubleValue = doubleValue;
    _lastUpdateTime = time(NULL);
    // call valueUpdate callback if it exists
    if (_valueUpdate != NULL) {
        (*_valueUpdate) (_valueUpdateID, this);
    }
}

void Tag::setValue(float floatValue) {
    setValue( (double) floatValue );
}

void Tag::setValue(int intValue) {
    setValue( (double) intValue );
}

bool Tag::setValue(const char* strValue) {
    float newValue;
    int result = sscanf(strValue, "%f", &newValue);
    if (result != 1) {
        fprintf(stderr, "%s - failed to convert <%s> for topic %s\n", __func__, strValue, _topic.c_str());
        return false;
    }
    setValue(newValue);
    return true;
}

double Tag::doubleValue(void) {
    return _topicDoubleValue;
}

float Tag::floatValue(void) {
    return (float) _topicDoubleValue;
}

int Tag::intValue(void) {
    return (int) _topicDoubleValue;
}

bool Tag::isPublish() {
    return _publish;
}

bool Tag::isSubscribe() {
    return !_publish;
}

void Tag::setPublish(void) {
    _publish = true;
}

void Tag::setSubscribe(void) {
    _publish = false;
}

void Tag::setRetain(bool newRetain) {
    _publish_retain = newRetain;
}

bool Tag::getRetain(void) {
    return _publish_retain;
}

//
// Class TagStore
//

TagStore::TagStore() {
    // mark tag list entries as empty
    for (int i = 0; i < MAX_TAG_NUM; i++) {
        _tagList[i] = NULL;
    }
    _iterateIndex = -1;
}

TagStore::~TagStore() {
    deleteAll();
}

void TagStore::deleteAll(void) {
    // delete every tag
    for (int i = 0; i < MAX_TAG_NUM; i++) {
        delete(_tagList[i]);
        _tagList[i] = NULL;
    }
}

Tag *TagStore::getTag(const char* tagTopic) {

    string topic (tagTopic);
    uint16_t tagCrc = gen_crc16(topic.data(), topic.length());
    Tag *retTag =  NULL;

    for (int index = 0; index < MAX_TAG_NUM; index++) {
        if (_tagList[index] == NULL) continue;
        if (tagCrc == _tagList[index]->getTopicCrc()) {
            retTag = _tagList[index];
            break;
        }
    }
    return retTag;
}

Tag* TagStore::getFirstTag(void) {
    int index;
    // find first free entry in tag list
    for (index = 0; index < MAX_TAG_NUM; index++) {
        if (_tagList[index] != NULL) {
            _iterateIndex = index;
            return _tagList[index];
        }
    }
    return NULL;    // no tags found
}

Tag* TagStore::getNextTag(void) {
    int index;
    // check if getFirstTag has been called
    if (_iterateIndex < 0) return NULL;
    // find first free entry in tag list
    for (index = _iterateIndex+1; index < MAX_TAG_NUM; index++) {
        if (_tagList[index] != NULL) {
            _iterateIndex = index;
            return _tagList[index];
        }
    }
    // No more tags found
    _iterateIndex = -1; // reset iterateIndex
    return NULL;
}

Tag* TagStore::addTag(const char* tagTopic) {
    int index, freeIndex = -1;
    // find first free entry in tag list
    for (index = 0; index < MAX_TAG_NUM; index++) {
        if (_tagList[index] == NULL) {
            freeIndex = index;
            break;
        }
    }
    // abort if tagList is full
    if (freeIndex < 0) return NULL;
    // create new tag and store in list
    Tag *tPtr = new Tag(tagTopic);
    _tagList[index] = tPtr;
    //printf("%s - [%d] - %s\n", __func__, index, tPtr->getTopic());
    return tPtr;
}
