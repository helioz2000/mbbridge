# mbbridge
Modbus RTU to MQTT bridge for Raspberry Pi.

A data bridge between Modbus RTU devices and MQTT broker.

Reads data from Modbus RTU devices on a time based cycle and publishes the data to an MQTT broker.

Subscribes to an MQTT broker and writes received data to Modbus RTU slave.

**mbbridge** read and write data is configured in a text based *cfg* file. A description of each config parameter is provided in project cfg file.

### History:
This project was born out of necessity. My home automation system relies on wireless (HC12) modbus communication to collect data from devices in various locations of the house and control some of the devices.

The main control logic is based on Node-Red (Raspberry Pi) but *node-red-contrib-modbus* is not well suited to co-ordinate the traffic between multiple modbus RTU [wireless] nodes and continually produces errors.

### Required Libraries:
* modbus (libmodbus-dev)
* config (libconfig++-dev)
* mosquitto (libmosquitto-dev)

#### Library Documentation
* [modbus](https://libmodbus.org/documentation/)
* [config](https://hyperrealm.github.io/libconfig/libconfig_manual.html)
* [mosquitto](https://mosquitto.org/api/files/mosquitto-h.html)
