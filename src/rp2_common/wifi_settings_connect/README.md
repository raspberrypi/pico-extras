# wifi\_settings\_connect

This is a library to manage WiFi connections. It provides Flash storage
for WiFi passwords and hotspot names, and a background async\_context
service to automatically connect to them. You can store details for
up to 16 hotspots and update them using `picotool` or a setup
application. This avoids any need to
specify build-time flags such as `WIFI_SSID` and `WIFI_PASSWORD`.

The Flash storage location for hotspot details is specified in 
`include/wifi_settings/wifi_settings_configuration.h`. It is at
`0x101ff000` (for Pico W) and `0x103fe000` (for Pico 2 W). To add your
WiFi details at this location, please [see these
instructions](https://github.com/jwhitham/pico-wifi-settings/blob/master/doc/SETTINGS_FILE.md).
You can edit the settings as a text file and transfer it with `picotool`,
or install a [setup application](https://github.com/jwhitham/pico-wifi-settings/blob/master/doc/SETUP_APP.md)
to add or update WiFi details.

This wifi\_settings\_connect library is a subset of a larger
library, [wifi\_settings](https://github.com/jwhitham/pico-wifi-settings/),
which adds remote update functions for both the WiFi settings
and (optionally) your Pico application too. 
