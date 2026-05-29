# MQTT Topics and Payloads

> **🧪 BETA branch (`feature/multi-device-mppt`, firmware v2.0.0).** Largely
> untested, functionality not guaranteed — read the README before relying on it.

## Topic Structure

All topics are published under a configurable base topic (default: `victron`).
Each device publishes to a topic derived from its **name** (set in the web
portal):

```
{base}/{name}/state            → one device (schema depends on its type)
{base}/gateway/status          → Gateway health/status
```

The device `name` is the topic segment. Names are constrained to lowercase
`[a-z0-9_]` (sanitized in the portal and validated by the form) so they are
always valid MQTT topic segments.

### Back-compat defaults

To keep existing Home Assistant / Node-RED mappings working unchanged, the
default names reproduce the legacy fixed topics exactly:

| Device | Default name | Topic |
|---|---|---|
| First SmartSolar MPPT | `mppt` | `{base}/mppt/state` |
| SmartShunt | `smartshunt` | `{base}/smartshunt/state` |
| SmartBatterySense | `battery_sense` | `{base}/battery_sense/state` |

Additional MPPTs get incrementing names automatically when left blank:

```
{base}/mppt2/state             → 2nd SmartSolar MPPT
{base}/mppt3/state             → 3rd SmartSolar MPPT
```

You can override any name in the portal; the payload schema is still chosen by
the device **type**, not its name.

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
| `device_state` | enum | Charge state: 0=off, 1=low_power, 2=fault, 3=bulk, 4=absorption, 5=float, 6=storage, 7=equalize (values >7 emit `device_state_text:"unknown"`) |
| `device_state_text` | string | Human-readable state name derived from `device_state` |
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

    # Second MPPT — identical schema, different topic (name "mppt2")
    - name: "MPPT2 PV Power"
      state_topic: "victron/mppt2/state"
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
