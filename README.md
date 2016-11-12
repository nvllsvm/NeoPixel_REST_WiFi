# NeoPixel REST WiFi
REST API for controlling NeoPixel strips on WiFi enabled, Arudino-compatible devices.

## Configuration

A few constants must be modified before installing on a device.

### `neopixel_config.h`
Name | Description
--- | ---
PIN | Digital PIN which controls the NeoPixels
NUMPIXELS | Number of connected NeoPixels
STRIP_TYPE | Organization of the NeoPixel subpixels

### `wifi_config.h`
Name | Description
--- | ---
SSID | WiFi network name
PASSWORD | Password

## API

The API will send and receive a JSON-encoded body.

### `/`
The current state of operation.

Make a `PUT` request to modify the state.

The only key present in each request is listed below. Other keys are dependent on which mode is set.

Key |  Type | Description
--- | --- | ---
mode | string | The active mode

##### Mode: `off`
The NeoPixels are off.

##### Mode: `color`
All NeoPixels have the same state. When setting, at least one subpixel must be greater than 0.

Key |  Type | Valid Values | Default | Description
--- | --- | --- | --- | ---
red | integer |0 through 255 | 0 | Red subpixel
blue | integer | 0 through 255 | 0 | Blue subpixel
green | integer | 0 through 255 | 0 | Green subpixel
white | integer | 0 through 255 | 0 | White subpixel (only displayed/accepted for configurations which have it)
brightness | integer | 0 through 255 | 255 | Brightness of all NeoPixels

##### Mode: `rainbow`
A fading rainbow pattern.

Key |  Type | Valid Values | Default | Description
--- | --- | --- | --- | ---
interval | integer | 0 through 255 | 50 | Speed the rainbow changes at
brightness | integer | 0 through 255 | 255 | Brightness of all NeoPixels


### `/info`
Basic info of the device.

Key |  Type | Description
--- | --- | ---
has_white | boolean | Indicates if the device is connected to NeoPixels with a white subpixel
size | integer | Number of NeoPixels
version | string | Version of NeoPixel REST WiFi
