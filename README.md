# ESP Air Sensor

Get air quality & CO2 data from SM300D2 & Senseair S8 with ESP32, and export
as OpenMetrics (Prometheus exporter) via WiFi.

I used to have a Raspberry Pi Zero reading the SM300D2 (other's work) and an
Arduino reading the Senseair S8 (my own project). Recently I got a batch of
ESP32 chips, and looking for something to do with them. So I decide to combine
the two using one ESP32 module, free the Raspberry Pi, and hoping all the bugs
in old projects would go away.

Also, I'm migrating from Graphite to Prometheus for metrics collection.

At the time, I'm new to embedded development and C programming in general.
This should be treated as a toy side project, not a serious one :)

<img alt="Top view of assembled case" src="/case/pictures/case_photo_top.webp" width="320" /><img alt="Inside of the semi-assembled case" src="/case/pictures/case_photo_inside.webp" width="320" />

## Sensors

SM300D2:
A cheap mulitsensor module, see the good summary at
[alemela/SM300D2-air-quality](https://github.com/alemela/SM300D2-air-quality/).

Senseair S8:
CO2 sensor that seems more reliable.
[Their product page](https://senseair.com/products/size-counts/s8-residential/).

I use SM300D2 & Senseair S8 because I already have them on hands. But I
wouldn't recommand SM300D2 due to its unreliable readings of CO2, CH2O, and
TVOC. Even its temperature reading is often higher than it should be, affected
by itself heat I guess. In fact, I only take the PM2.5 readings from the module
alhought all data are read in this project.

## Software

[PlatformIO](https://platformio.org/) (the IDE) and
[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
(the offical framework of ESP32) were used.

I was trying to make it modular and extensible, to facilitate adding/removing
particular sensor devices.

### Sturcture of source code

* [components/metrics/](/components/metrics/)\
  Initlize Wi-Fi and HTTP server, serving OpenMetrics page.
* [components/sm300d2/](/components/sm300d2/)\
  Read SM300D2 data from UART serial, put them on a queue.
* [components/sense_air_s8/](/components/sense_air_s8/)\
  Provide functions to to read Senseair S8 data from UART serial.
* [src/main.c](/src/main.c)\
  Keep reading data from modules and publish them as metrics.
 
### Configure

Use `pio run -t menuconfig`

* Component config -> OpenMetrics Exporter\
  Hostname, Wi-Fi SSID & PSK, HTTP endpoints
* Component config -> Senseair S8 CO2 Sensor\
  UART port number, RX/TX pins
* Component config -> SM300D2 Air Quality Sensor\
  UART port number, RX pin, aggregation period

### Build

Install PlatformIO and execute `pio run` on terminal, or click "Build" on
Visual Code with PlatformIO plugin.

### Metrics

The exporter is on `/metrics` by default.

```
$ curl http://<espair-hostname>/metrics
> # UNIT espair_senseairs8_co2_ppm ppm
> # TYPE espair_senseairs8_co2_ppm gauge
> espair_senseairs8_co2_ppm{host="espair-01",mac="807d3a***"} 551
> (omitted...)
> # UNIT espair_sm300d2_humi_precent precent
> # TYPE espair_sm300d2_humi_precent gauge
> espair_sm300d2_humi_precent{host="espair-01",mac="807d3a***""} 37.00
> # EOF
```

# Case

<img alt="Rendered case" src="/case/pictures/case_rendered.webp" height="400" />

Files:

* [case/espair_case.FCStd](/case/espair_case.FCStd)\
  FreeCAD source of case body & ESP32 tray
* [case/espair_case-Body.stl](/case/espair_case-Body.stl)\
  STL of case body
* [case/espair_case-ESP.stl](/case/espair_case-ESP.stl)\
  STL of ESP32 tray
* [case/espair_cap.stl](/case/espair_cap.stl)\
  STL of rare case cover (Draw by Swin)

Printing:

* Printer: Ender-3 V2
* Filament: PLA, 1.75mm
* Resolution: 0.20mm
* Supports: yes
* Infill: no effect

The particulate matter sensor was placed outside the case, fasten along with
the case body and ESP32 tray using two M2 screws & nuts.
