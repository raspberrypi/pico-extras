# wifi\_settings\_connect

This is a library to manage WiFi connections. It provides Flash storage
for WiFi passwords and hotspot names, and a background async\_context
service to automatically connect to them. You can store details for
up to 100 hotspots and update them using `picotool` or a setup
application. This avoids any need to
specify build-time flags such as `WIFI_SSID` and `WIFI_PASSWORD`.

WiFi hotspot details are stored in a Flash sector that isn't normally used by programs,
normally located near the end of Flash memory. This 
[wifi-settings file](doc/SETTINGS_FILE.md) is a simple text file which
can be updated by USB or by installing a setup app from the
[pico-wifi-settings home page](https://github.com/jwhitham/pico-wifi-settings)
on Github.

## Requirements

 - Raspberry Pi Pico W or Pico 2 W hardware
 - a "bare metal" C/C++ application for Pico W (not FreeRTOS)
   using the `cyw43` driver and `lwip` network stack
   which are provided with the [Pico SDK](https://github.com/raspberrypi/pico-sdk/).
 - between 2kb and 13kb of code space depending on options used
 - WiFi network(s) with a DHCP server and WPA authentication

## How to use it

First, you need to configure the WiFi settings file
in Flash. See the [wifi-settings file documentation](doc/SETTINGS_FILE.md).

Next, you need to modify your application to use wifi\_settings\_connect.
This involves adding a few lines of C code.
There is an [integration guide which explains what you need to do
to add wifi\_settings\_connect to your application](doc/INTEGRATION.md).

## Enabling remote updates

wifi\_settings\_connect is a subset of a larger library (wifi\_settings) which
also has support for remote updates of WiFi settings and over-the-air (OTA)
firmware updates. Visit the
[pico-wifi-settings home page](https://github.com/jwhitham/pico-wifi-settings)
for more information.
