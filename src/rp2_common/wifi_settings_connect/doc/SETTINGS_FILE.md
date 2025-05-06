# Creating and updating a WiFi settings file

wifi\_settings\_connect stores WiFi hotspot names and passwords
in a Flash sector that isn't normally used by programs. This
is called the "WiFi settings file". It is similar to a file on a disk,
except that it is always at the same location, and the size is
limited to 4096 bytes.

The file can be updated over USB by using [picotool](https://github.com/raspberrypi/picotool).
It is a text file which can be edited with any text editor.
Here is an example of typical contents:
```
    ssid1=MyHomeWiFi
    pass1=mypassword1234
    ssid2=MyPhoneHotspot
    pass2=secretpassword
    country=GB
```
wifi\_settings\_connect will automatically scan for hotspots and connect to
hotspots matching the SSID names and passwords in the file.

- On the [pico-wifi-settings home page](https://github.com/jwhitham/pico-wifi-settings)
  you can also find a setup app which runs on your Pico and automates much of the
  setup process. The pico-wifi-settings library is a superset of
  wifi\_settings\_connect. It includes a remote update feature that allows
  a new wifi-settings file to be installed via WiFi.

# Creating the file on a computer

Use any text editor to create a text file similar to the example above.

Each line in the file should contain a key and a value, separated by `=`,
with no spaces around `=`, or at the beginning or end of each line.

The file must have at least `ssid1` or `bssid1`, otherwise there will
be no connection attempts, and wifi\_settings\_connect will stay in the
STORAGE\_EMPTY\_ERROR state.

You can also use the following:

 - `ssid<N>` - SSID name for hotspot N (a number from 1 to 100)
 - `pass<N>` - Password for hotspot N
 - `country` - Your two-letter country code from [ISO-3166-1](https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes)
 - `bssid<N>` - The BSSID ID for hotspot N
 - `name` - The hostname of the Pico (sent to DHCP servers)

# Copying the WiFi settings file by USB

You can use [picotool](https://github.com/raspberrypi/picotool) to copy the
file from your PC to your Pico via USB.

picotool is part of the Pico SDK. 
You need to build picotool with USB support for your OS (or download a pre-built copy).

To use picotool,
boot the Pico in bootloader mode by holding down the BOOTSEL button while plugging it
into USB. In bootloader mode, you can upload files with picotool.
The default address is 16kb before the final address in Flash:

 - On Pico W, use `0x101fc000` as the address.
 - On Pico 2 W, use `0x103fc000` as the address.

You must also rename your WiFi settings file so that it ends with `.bin` as 
picotool is not able to upload files unless they are `.bin`, `.elf` or `.uf2`.

Here is a sample upload command for Pico W (RP2040):
```
    picotool load -o 0x101fc000 mywifisettings.bin
```
and the equivalent for Pico 2 W (RP2350):
```
    picotool load -o 0x103fc000 mywifisettings.bin
```

## Location of the wifi-settings file

The default location of the file (16kb before the final address in Flash)
has been chosen because the final three 4kb Flash sectors are already assigned
a function by the Pico SDK. The Bluetooth library uses two 4kb sectors for storage of
devices that have been paired by Bluetooth. The final 4kb sector is used for a workaround
for the RP2350-E10 bug - this sector may be erased when copying a UF2 file to a Pico 2
via drag-and-drop. Therefore, these three sectors are avoided.

If you wish to store the wifi-settings file at a specific address you can
do so by setting `-DWIFI_SETTINGS_FILE_ADDRESS=0x....` when running `cmake`.
The value `0x...` should be an address relative to the start of Flash, so Flash address
`0x1fc000` corresponds to absolute address `0x101fc000`.

## Backing up a WiFi settings file

picotool can be used to download WiFi settings files from a Pico W:
```
    picotool save -r 0x101fc000 0x101fd000 backup.bin
```
and Pico 2 W (RP2350):
```
    picotool save -r 0x103fc000 0x103fd000 backup.bin
```
Characters after the end of the file will be copied (usually either 0x00 or 0xff).
These can be safely deleted using your text editor. The backup is restored by
using `picotool load` as described in "Copying the WiFi settings file by USB".

These examples use the default location for the wifi-settings file. If you
are using a custom location, e.g. building with
`-DWIFI_SETTINGS_FILE_ADDRESS=0x...`, then
you would need to substitute the actual address.

# File format details

The file format is very simple so that it can be read by a simple algorithm
that doesn't require much code space. The parser ignores any line that it
doesn't understand, and skips any keys that are not known. Here are the rules:

 - The key and the value should be separated only by an `=` character, e.g. `ssid1=HomeWiFi`.
 - Lines that don't match the form `key=value` are completely ignored;
   you can add text, comments etc. in order to help you manage your configuration.
 - On a line that does match `key=value`, whitespace is NOT ignored.
   Be careful to avoid adding extra spaces around `=`.
   A space before `=` will be part of the key, and a space after `=` will be part of the value.
 - Unix and Windows line endings are supported.
 - The maximum size of the file is 4096 bytes.
 - Values can contain any printable UTF-8 character.
 - Keys can also contain any printable UTF-8 character except for '='.
 - There is no maximum size for a key or a value (except for the file size).
 - Values can be zero length.
 - Keys must be at least 1 byte.
 - If a key appears more than once in the file, the first value is used.
 - The end of the file is the first byte with value 0x00, 0xff or 0x1a, or the 4097th byte,
   whichever comes first.

# WiFi settings

 - `ssid<N+1>` is only checked if `ssid<N>` is present.
 - The number reflects the priority. Lower numbers take priority over higher
   numbers when more than one SSID is found.
 - If `pass<N>` is not specified then wifi\_settings\_connect will assume
   an open WiFi hotspot.
 - If both `bssid<N>` and `ssid<N>` are specified, then the BSSID is used
   and the SSID is ignored.
 - If you don't specify a country, the default worldwide settings are used, which might work
   slightly less well (e.g. fewer WiFi channels are supported).
 - `bssid<N>` should be specified as
   a `:`-separated lower-case MAC address, e.g. `01:23:45:67:89:ab`. BSSIDs are
   not normally required and should only be used if you have a special requirement
   e.g. a "hidden" hotspot without an SSID name.

# Custom keys and values

The WiFi settings file can have keys which are not used by the wifi\_settings\_connect library.
Your application can obtain their values using the `wifi_settings_get_value_for_key()` function.
This can be a useful way to store additional configuration data for your Pico application.
For example you might use it to store encryption keys, server addresses, user names
or any other setting that you may wish to update without rebuilding your application.

`wifi_settings_get_value_for_key` uses a linear search, starting at the beginning
of the file. This search does not backtrack and is fast because of the simplistic
nature of the file format. However, in algorithmic terms, this is not the best way
to implement or search a key/value store, and if you need frequent access to keys/values,
you may wish to implement something better (e.g. use a hash table to implement a dictionary)
or just load the values when your application starts up and then store them elsewhere.
