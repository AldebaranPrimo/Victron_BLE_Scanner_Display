# MQTT Topics and Payloads

## Topic Structure

All topics are published under a configurable base topic (default: `victron`).

```
{base}/mppt/state              → SmartSolar MPPT data
{base}/smartshunt/state        → SmartShunt data
{base}/battery_sense/state     → SmartBatterySense data
{base}/gateway/status          → Gateway health/status
```

All messages are published as **retained** so that new subscribers immediately get the latest values.

## Payloads

### SmartSolar MPPT

```json
{
  "device_state": 3,
  "device_state_text": "bulk",
  "charger_error": 0,
  "battery_voltage": 13.45,
  "battery_current": 2.1,
  "yield_today": 1230,
  "pv_power": 284,
  "load_current": 0.5,
  "rssi": -67
}
```

| Field | Unit | Description |
|---|---|---|
| `device_state` | enum | 0=off, 3=bulk, 4=absorption, 5=float, 7=equalize |
| `device_state_text` | string | Human-readable state name |
| `charger_error` | enum | 0=no error |
| `battery_voltage` | V | Battery voltage (0.01V resolution) |
| `battery_current` | A | Charge current (0.1A resolution) |
| `yield_today` | Wh | Energy harvested today |
| `pv_power` | W | Solar panel input power |
| `load_current` | A | Load output current |
| `rssi` | dBm | BLE signal strength |

### SmartShunt

```json
{
  "soc": 87.0,
  "battery_voltage": 13.45,
  "battery_current": 2.5,
  "time_to_go_min": 765,
  "consumed_ah": 5.4,
  "alarm_reason": 0,
  "rssi": -65
}
```

| Field | Unit | Description |
|---|---|---|
| `soc` | % | State of charge (0-100) |
| `battery_voltage` | V | Battery voltage |
| `battery_current` | A | Positive=charging, negative=discharging |
| `time_to_go_min` | min | Estimated time remaining (65535=N/A) |
| `consumed_ah` | Ah | Consumed amp-hours |
| `alarm_reason` | bitfield | Alarm flags |
| `rssi` | dBm | BLE signal strength |

### SmartBatterySense

```json
{
  "battery_voltage": 13.44,
  "temperature": 22.5,
  "rssi": -72
}
```

### Gateway Status

```json
{
  "wifi_rssi": -45,
  "uptime_sec": 3600,
  "free_heap": 140000,
  "min_free_heap": 125000
}
```

## Home Assistant Integration

Example `configuration.yaml` entries:

```yaml
mqtt:
  sensor:
    - name: "MPPT PV Power"
      state_topic: "victron/mppt/state"
      value_template: "{{ value_json.pv_power }}"
      unit_of_measurement: "W"
      device_class: power

    - name: "SmartShunt SOC"
      state_topic: "victron/smartshunt/state"
      value_template: "{{ value_json.soc }}"
      unit_of_measurement: "%"
      device_class: battery

    - name: "Battery Temperature"
      state_topic: "victron/battery_sense/state"
      value_template: "{{ value_json.temperature }}"
      unit_of_measurement: "°C"
      device_class: temperature

    - name: "Battery Voltage"
      state_topic: "victron/smartshunt/state"
      value_template: "{{ value_json.battery_voltage }}"
      unit_of_measurement: "V"
      device_class: voltage

    - name: "Battery Current"
      state_topic: "victron/smartshunt/state"
      value_template: "{{ value_json.battery_current }}"
      unit_of_measurement: "A"
      device_class: current
```
