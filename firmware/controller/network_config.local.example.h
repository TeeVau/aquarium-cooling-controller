#pragma once

/**
 * @file network_config.local.example.h
 * @brief Example local network and MQTT override configuration.
 */

// Copy this file to network_config.local.h and fill in local credentials.
// network_config.local.h is ignored by Git and must not be committed.

#undef AQ_WIFI_SSID
#define AQ_WIFI_SSID "your-wifi-ssid"

#undef AQ_WIFI_PASSWORD
#define AQ_WIFI_PASSWORD "your-wifi-password"

#undef AQ_MQTT_HOST
#define AQ_MQTT_HOST "192.168.1.10"

#undef AQ_MQTT_PORT
#define AQ_MQTT_PORT 1883

#undef AQ_MQTT_USERNAME
#define AQ_MQTT_USERNAME ""

#undef AQ_MQTT_PASSWORD
#define AQ_MQTT_PASSWORD ""

#undef AQ_MQTT_ROOT_TOPIC
#define AQ_MQTT_ROOT_TOPIC "aquarium/cooling"
