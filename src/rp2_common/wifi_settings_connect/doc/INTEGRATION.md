# Integrating wifi\_settings\_connect into your own Pico application

You can integrate wifi\_settings\_connect into your own Pico application
with just a few lines of code:
```
    #include "wifi_settings/wifi_settings_connect.h"    // << add this
    int main() {
        stdio_init_all();
        if (wifi_settings_init() != 0) {                // << and add this
            panic(...);
        }
        wifi_settings_connect();                        // << and add this
        // and that's it...
    }
```
The following steps go through the process in more detail for
a [CMake](https://cmake.org) project stored in Git,
similar to all of the official Pico projects.

You may find it useful to look at [the example app for wifi\_settings\_connect in
the pico-playground repository](https://github.com/raspberrypi/pico-playground/tree/master/wifi_settings_connect/example).

## Modify CMakeLists.txt to use the library

The `target_link_libraries` rule for your project should be extended to
add `wifi_settings_connect`.
```
    target_link_libraries(your_app
            wifi_settings_connect
            pico_stdlib
        )
```
If your project does not include the `pico-extras` repository, then this
must also be added.
```
    include(pico_extras_import.cmake)
```
You can copy the `pico_extras_import.cmake` file from [the root of
the pico-playground repository](https://github.com/raspberrypi/pico-playground).

### Additional configuration for LwIP and mbedtls

If your project has not previously used WiFi you will also need
to add one of the WiFi driver targets to `target_link_libraries`, e.g.
`pico_cyw43_arch_lwip_background` or `pico_cyw43_arch_lwip_poll`.

You will also need `lwipopts.h` in the project directory (this configures
LwIP). You can copy an example from
[here](https://github.com/raspberrypi/pico-playground/tree/master/wifi_settings_connect/example).

## Include the header file

Your main C/C++ source file (containing `main()`) should be modified to include
`wifi_settings/wifi_settings_connect.h`:
```
    #include "wifi_settings/wifi_settings_connect.h"
```

## Modify your main function

Your `main()` function should be modified to call `wifi_settings_init()` once on startup.

 - This must *replace* any call to `cyw43` initialisation functions, because
   these are called from `wifi_settings_init()` (with the correct country code).
 - The call should be after `stdio_init_all()`.
 - If the call returns a non-zero value, an error has occurred. You do not have
   to handle this error; it is still safe to call other `wifi_settings` functions,
   but they will not work and will return error codes where appropriate.

Your application should also call `wifi_settings_connect()` when it wishes to connect
to WiFi. This can be called immediately after `wifi_settings_init()` or at any later
time. `wifi_settings_connect()` does not block, as the connection takes
place in the background.

All other modifications are optional. You can now rebuild your application
and it will include the wifi\_settings\_connect features.

## CMake command line

When running `cmake`, you need to provide the location of the `pico-extras`
repository as well as the `pico-sdk` repository. This is typically done
with `-DPICO_EXTRAS_PATH`, e.g.:
```
    cmake -DPICO_BOARD=pico_w \
        -DPICO_SDK_PATH=/home/user/pico-sdk \
        -DPICO_EXTRAS_PATH=/home/user/pico-extras \
        ..
```

# Optional modifications

Your application can call `wifi_settings_is_connected()` at any time
to determine if the WiFi connection is available or not.

Your application can call various status functions at any time
to get a text report on the connection status. This can be useful for debugging.
Each function should be passed a `char[]` buffer for the output, along with the
size of the buffer.

 - `wifi_settings_get_connect_status_text()` produces a line of
   text showing the connection status, e.g.  `WiFi is connected to ssid1=MyHomeWiFi`.
 - `wifi_settings_get_hw_status_text()` produces a line of
   text describing the status of the `cyw43` hardware driver; this will be empty
   if the hardware is not initialised.
 - `wifi_settings_get_ip_status_text()` produces a line of
   text describing the status of the `lwip` network stack e.g. IP address; this will be empty
   if unconnected.
 - `wifi_settings_get_ip` produces the IP address by itself; this will be empty
   if unconnected.
 - `wifi_settings_get_ssid` produces the current SSID by itself; this will be empty
   if unconnected. If connected using a BSSID, this will be reported as
   a `:`-separated lower-case MAC address, e.g. `01:23:45:67:89:ab`. If the wifi-settings
   file has been updated since the connection was made, then the result may be `?`,
   as the SSID is found by searching the wifi-settings file.

There is also a function to report the current connection state.
`wifi_settings_get_ssid_status()` returns
a pointer to a static string, indicating the status of a connection attempt to
an SSID, e.g. `SUCCESS`, `NOT FOUND`.

Your application can call `wifi_settings_disconnect()` to force disconnect,
or `wifi_settings_deinit()` to deinitialise the driver, but this is never necessary
and these steps can be left out. They exist to allow the application to shut down WiFi,
e.g. to save power, or in order to control the WiFi hardware directly for some other
purpose. For example, the
[setup app](https://github.com/jwhitham/pico-wifi-settings/tree/master/doc/SETUP_APP.md)
uses this feature to perform its own WiFi scan.
