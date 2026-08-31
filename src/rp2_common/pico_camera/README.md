# pico_camera

Camera sensor support for RP2040/RP2350 via PIO + DMA, with an API aligned with
[esp32-camera](https://github.com/espressif/esp32-camera) (`esp_` prefix becomes `pico_`).

Supported sensors: OV2640, OV3660, OV7670, OV7725, GC2145, GC0308, GC032A
(auto-detected over SCCB at init). RGB565/YUV422 on all of them, plus JPEG on the
sensors with an on-chip encoder (OV2640/OV3660) and grayscale on GC0308.

All pins (VSYNC/HREF/PCLK/data/SCCB/XCLK) are configured at runtime in
`camera_config_t`; no PIO source editing or pioasm build step is required.
On RP2350 boards with PSRAM, frame buffers can live in PSRAM
(`fb_location = PICO_CAMERA_FB_IN_PSRAM`).

Upstream development happens at [umeiko/PicoCamera](https://github.com/umeiko/PicoCamera)
(also published in the Arduino Library Manager / PlatformIO Registry and bundled
with arduino-pico); this copy is kept for use by pico-playground examples.

```c
#include "pico/stdlib.h"
#include "PicoCamera.h"

camera_config_t config = {
    .pin_pwdn = -1, .pin_reset = 29, .pin_xclk = 5,
    .pin_sccb_sda = 12, .pin_sccb_scl = 13,
    .pin_d0 = 14, .pin_d1 = 15, .pin_d2 = 16, .pin_d3 = 17,
    .pin_d4 = 18, .pin_d5 = 19, .pin_d6 = 20, .pin_d7 = 21,
    .pin_vsync = 2, .pin_href = 3, .pin_pclk = 4,
    .xclk_freq_hz = 10000000,
    .sccb_i2c_port = 0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 10,
    .fb_count = 1,
    .fb_location = PICO_CAMERA_FB_AUTO,
};

pico_camera_init(&config);
camera_fb_t *fb = pico_camera_fb_get();
// ... use fb->buf / fb->len ...
pico_camera_fb_return(fb);
```
