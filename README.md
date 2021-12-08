# ESP Air Sensor

Get air quality & CO2 data from SM300D2 & Senseair S8 with ESP32, and export
as OpenMetrics (Prometheus exporter) via WiFi.

I used to have a Raspberry Pi Zero reading the SM300D2 (other's work) and an
Arduino reading the Senseair S8 (my own project). Recently I got a batch of
ESP32 chips, and looking for something to do with it. So I decide to combine
the two using a ESP32 module, free the Raspberry Pi, and hoping all the bugs
in old projects would go away.

## Sensors

SM300D2:
A cheap mulitsensor module, see the good summary at
[alemela/SM300D2-air-quality](https://github.com/alemela/SM300D2-air-quality/).

Senseair S8:
CO2 sensor that seems more reliable,
[their product page](https://senseair.com/products/size-counts/s8-residential/).

I use SM300D2 & Senseair S8 because I already have them on hand. But I
wouldn't recommand SM300D2 due to its unreliable readings of CO2, CH2O, and
TVOC. Even the temperature reading is often higher than it should be, affected
by itself heat I guess. In fact, I only take the PM2.5 readings from the module
alhought all data are read in this project.

## Software

[PlatformIO](https://platformio.org/) (the IDE) and
[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/),
(the offical framework of ESP32) were used.

### Sturcture of source code

* components/metrics/
  Initlize Wi-Fi and HTTP server, serving OpenMetrics page.
* components/sm300d2/
  Read SM300D2 data from UART serial, put them on a queue.
* components/sense_air_s8/
  Provide functions to to read Senseair S8 data from UART serial.
* src/main.c
  Keep reading data from modules and publish them as metrics.
 
### Configure

Use `pio run -t menuconfig`

* Component config -> OpenMetrics Exporter
  Hostname, Wi-Fi SSID & PSK, HTTP endpoints
* Component config -> Senseair S8 CO2 Sensor
  UART port number, RX/TX pins
* Component config -> SM300D2 Air Quality Sensor
  UART port number, RX pin, aggregation period

### Build

Install PlatformIO and execute `pio run` on terminal, or click "Build" on
Visual Code with PlatformIO plugin.
