#pragma once

/**
 * @file network_config.h
 * @brief Safe default Wi-Fi and MQTT configuration macros.
 *
 * Create `network_config.local.h` next to this file to override credentials and
 * local broker settings. The local file is ignored by Git and is excluded from
 * generated Doxygen output.
 */

// Safe defaults for the committed repository. Create
// network_config.local.h next to this file to enable Wi-Fi/MQTT locally.

#ifndef AQ_NETWORK_ENABLED
#define AQ_NETWORK_ENABLED 1
#endif

#ifndef AQ_WIFI_SSID
#define AQ_WIFI_SSID ""
#endif

#ifndef AQ_WIFI_PASSWORD
#define AQ_WIFI_PASSWORD ""
#endif

#ifndef AQ_DEVICE_HOSTNAME
#define AQ_DEVICE_HOSTNAME "aq-cooling"
#endif

#ifndef AQ_MQTT_HOST
#define AQ_MQTT_HOST ""
#endif

#ifndef AQ_MQTT_PORT
#define AQ_MQTT_PORT 1883
#endif

#ifndef AQ_MQTT_USERNAME
#define AQ_MQTT_USERNAME ""
#endif

#ifndef AQ_MQTT_PASSWORD
#define AQ_MQTT_PASSWORD ""
#endif

#ifndef AQ_MQTT_CLIENT_ID
#define AQ_MQTT_CLIENT_ID "aq-cooling-controller"
#endif

#ifndef AQ_MQTT_ROOT_TOPIC
#define AQ_MQTT_ROOT_TOPIC "aquarium/cooling"
#endif

#ifndef AQ_WIFI_RECONNECT_INTERVAL_MS
#define AQ_WIFI_RECONNECT_INTERVAL_MS 10000UL
#endif

#ifndef AQ_MQTT_RECONNECT_INTERVAL_MS
#define AQ_MQTT_RECONNECT_INTERVAL_MS 10000UL
#endif

#ifndef AQ_MQTT_PUBLISH_INTERVAL_MS
#define AQ_MQTT_PUBLISH_INTERVAL_MS 10000UL
#endif

#if __has_include("network_config.local.h")
#include "network_config.local.h"
#endif
