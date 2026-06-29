# mqttradar — ESP32-C3 presence sensor

An ESP32-C3 (Seeed XIAO) presence sensor built around an **HLK-LD2450** mmWave
multi-target tracking radar. Detected targets are published over MQTT; the
device is configured over MQTT and HTTP, and supports OTA updates with rollback.

Built on **ESP-IDF v6.0.1**. Shared infrastructure comes from the `mianos/mianesp`
components (`wifimanager`, `webserver`, `jsonwrapper`, `nvsstoragemanager`) plus
local `mqttwrapper` and `settings` components; the radar base classes come from
the `mianos/ldradar` component.

## Build

Components are pulled by the IDF component manager from `main/idf_component.yml`
(fetching `git@github.com:mianos/mianesp.git` over SSH and the Espressif
registry), so no manual menuconfig steps are required — `sdkconfig.defaults`
selects the esp32c3 target, the custom dual-OTA partition table and OTA
rollback.

```sh
./build.sh            # wraps idf.py build against ESP-IDF v6.0.1
# or
idf.py build flash monitor
```

CI builds the firmware for esp32c3 under `espressif/idf:release-v6.0`
(`.gitlab-ci.yml`).

## Hardware / wiring

| Signal        | XIAO pin | GPIO   |
|---------------|----------|--------|
| LD2450 TX → ESP RX | D7  | GPIO20 |
| ESP TX → LD2450 RX | D6  | GPIO21 |

UART1 at **256000 baud**. The pins are constants in [main/main.cpp](main/main.cpp)
(`kUartTx` / `kUartRx`); swap them if your TX/RX are reversed.

## Wi-Fi setup

On first boot the device is unprovisioned. Use **ESP-Touch V2** to provision
Wi-Fi: https://www.espressif.com/en/technology/esp-touch (leave AES / additional
settings blank).

There is **no reset button**. To re-provision (clear saved credentials and
reboot into ESP-Touch), either:

- publish to `cmnd/<sensorName>/reprovision`, or
- `POST /config/reset` with body `{"wifi": true}`.

## Settings

Defaults live in the `settings` component
([components/settings/include/Settings.h](components/settings/include/Settings.h)):

```cpp
std::string mqttBrokerUri    = "mqtt://mqtt2.mianos.com";
std::string mqttUserName     = "";
std::string mqttUserPassword = "";   // JSON key: "mqttPassword"
std::string sensorName       = "radar3";
std::string tz               = "AEST-10AEDT,M10.1.0,M4.1.0/3";
std::string ntpServer        = "time.google.com";
int         presencePeriodSec = 1;   // min seconds between presence publishes; <=0 disables
```

All settings are persisted as a single JSON blob in NVS under the key `config`.
(Upgrading from the pre-v6 firmware, which used per-field NVS keys, resets
settings to these defaults on first boot; Wi-Fi credentials are unaffected.)

### Read / change settings over HTTP

```sh
curl http://<IP_ADDRESS>/config | jq
curl -X POST http://<IP_ADDRESS>/config \
     -H "Content-Type: application/json" \
     -d '{"presencePeriodSec": 5}'
```

### Change settings over MQTT

```sh
mosquitto_pub -t 'cmnd/radar3/settings' -m '{"presencePeriodSec": 5}'
```

The device replies on `tele/<sensorName>/settingsack` with the keys that
changed. Other commands: `cmnd/<sensorName>/restart`,
`cmnd/<sensorName>/reprovision`.

## HTTP endpoints

| Method | Path            | Purpose                                            |
|--------|-----------------|----------------------------------------------------|
| GET    | `/healthz`      | version, partition, free heap, sensor name         |
| GET    | `/config`       | current settings as JSON                           |
| POST   | `/config`       | apply + persist a subset of settings               |
| POST   | `/config/reset` | restore defaults; `{"wifi":true}` also clears Wi-Fi|
| GET    | `/firmware`     | running image version / partition                  |
| POST   | `/firmware`     | raw `.bin` body → inactive OTA slot → reboot        |

OTA deploy:

```sh
curl --data-binary @build/mqttradar.bin http://<IP_ADDRESS>/firmware
```

A freshly-OTA'd image boots pending-verify and is only marked valid once the
device is back on the network, so an image that can't reach Wi-Fi rolls back to
the previous slot.

## Data published

At startup (`tele/<sensorName>/init`):

```
tele/radar3/init {"version":5,"time":"...","gmt":"...","hostname":"radar3","ip":"<IP>","settings":"cmnd/radar3/settings"}
```

Radar events (`tele/<sensorName>/radar`) — coordinates in metres, speed in m/s:

```
tele/radar3/radar {"event":"detected","type":"rng","x":-0.08,"y":0.31,"speed":0,"reference":0}
tele/radar3/radar {"event":"presence","type":"rng","x":-0.17,"y":0.65,"speed":0,"reference":0}
tele/radar3/radar {"event":"cleared"}
```

Periodic status every 60s (`tele/<sensorName>/status`): uptime and heap. A
Last-Will of `{"status":"offline"}` is published on the same topic on
disconnect.
