# M5Stack Unit ENV-III Driver for Core2 for AWS IoT Kit

ESP-IDF component driver for the [M5Stack Unit ENV-III](https://docs.m5stack.com/en/unit/envIII) (SKU: U001-C) environmental sensor, designed for the [Core2 for AWS IoT Kit](https://github.com/m5stack/Core2-for-AWS-IoT-Kit).

## Overview

The Unit ENV-III is a composite I2C environmental sensing module containing two sensors on a shared bus:

| Sensor | Function | I2C Address |
|---|---|---|
| **SHT30** (Sensirion) | Temperature and relative humidity | `0x44` |
| **QMP6988** (QST) | Barometric pressure | `0x70` |

The module includes an HT7533 LDO regulating the input 5V down to 3.3V for the sensors, and 4.7kΩ pull-ups on SDA/SCL.

## Specifications

### SHT30 — Temperature & Humidity

| Parameter | Value |
|---|---|
| Temperature range | -40°C to +125°C |
| Temperature accuracy (0–65°C) | ±0.2°C typical |
| Humidity range | 0–100% RH |
| Humidity accuracy | ±2% RH typical |
| Resolution | 0.01°C / 0.01% RH |

### QMP6988 — Barometric Pressure

| Parameter | Value |
|---|---|
| Pressure range | 30–110 kPa (300–1100 hPa) |
| Pressure resolution | 0.06 Pa |
| RMS noise (ultra high accuracy) | 1.3 Pa |
| Temperature resolution | 0.0002°C |

## Hardware Connection

Connect the Unit ENV-III to the Core2 for AWS IoT Kit **Port A** (external I2C, GPIO32/GPIO33) using the HY2.0-4P (Grove-compatible) cable:

| Pin | Wire Color | Signal |
|---|---|---|
| 1 | Black | GND |
| 2 | Red | 5V |
| 3 | Yellow | SDA (GPIO 32) |
| 4 | White | SCL (GPIO 33) |

## Installation

Add the component to your ESP-IDF project's `components` directory and include the header:

```c
#include "unit_env_iii.h"
```

## Basic Usage

```c
#include "core2foraws.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unit_env_iii.h"

static const char *TAG = "env_example";

void app_main( void )
{
  core2foraws_init();

  esp_err_t ret = unit_env_iii_init();
  if( ret != ESP_OK )
  {
    ESP_LOGE( TAG, "ENV-III init failed: %s", esp_err_to_name( ret ) );
    return;
  }

  while( 1 )
  {
    unit_env_iii_data_t data;

    ret = unit_env_iii_read_data( &data );
    if( ret == ESP_OK )
    {
      if( data.temp_hum_valid )
      {
        ESP_LOGI( TAG, "Temperature: %.2f°C, Humidity: %.2f%%",
                  data.temperature_c, data.humidity_rh );
      }

      if( data.pressure_valid )
      {
        ESP_LOGI( TAG, "Pressure: %.2f hPa", data.pressure_pa / 100.0f );
      }
    }

    vTaskDelay( pdMS_TO_TICKS( 5000 ) );
  }
}
```

## Custom Configuration

```c
unit_env_iii_config_t config;
unit_env_iii_get_default_config( &config );

// SHT30 repeatability: HIGH (most accurate), MEDIUM, or LOW (fastest)
config.sht30_repeatability = UNIT_ENV_III_SHT30_REPEATABILITY_HIGH;

// SHT30 heater: use briefly to clear condensation from the humidity sensor
config.sht30_heater_enable = false;

// QMP6988 oversampling: OSR_1 (fastest) through OSR_64 (most accurate)
config.qmp6988_temp_osr = UNIT_ENV_III_QMP6988_OSR_16;
config.qmp6988_press_osr = UNIT_ENV_III_QMP6988_OSR_16;

// QMP6988 IIR filter: FILTER_OFF, FILTER_2, FILTER_4, FILTER_8, FILTER_16, FILTER_32
config.qmp6988_filter = UNIT_ENV_III_QMP6988_FILTER_16;

esp_err_t ret = unit_env_iii_init_with_config( &config );
```

## API Reference

### Initialization

| Function | Description |
|---|---|
| `unit_env_iii_init()` | Initialize with default configuration |
| `unit_env_iii_init_with_config(config)` | Initialize with custom configuration |
| `unit_env_iii_deinit()` | Deinitialize the driver |
| `unit_env_iii_get_default_config(config)` | Populate a config struct with defaults |

### Reading Sensor Data

| Function | Description |
|---|---|
| `unit_env_iii_read_data(data)` | Read temperature, humidity, and pressure. Returns `ESP_OK` if at least one sensor succeeded. Check `data->temp_hum_valid` and `data->pressure_valid` flags. |
| `unit_env_iii_read_temp_humidity(temp, hum)` | Read SHT30 only. Either pointer may be `NULL` to skip that value. |
| `unit_env_iii_read_pressure(pressure)` | Read QMP6988 pressure in Pascals. Internally reads QMP6988 temperature for compensation. |

### Control

| Function | Description |
|---|---|
| `unit_env_iii_set_sht30_heater(enable)` | Enable/disable SHT30 on-chip heater |
| `unit_env_iii_reset_sht30()` | Soft-reset the SHT30 |
| `unit_env_iii_reset_qmp6988()` | Soft-reset the QMP6988 |
| `unit_env_iii_check_connection()` | Verify both sensors respond on the bus |

## PA Hub Support

To route I2C through a PA Hub (for connecting multiple I2C units), enable in `menuconfig`:

```
Component config → Unit ENV-III Configuration → Use PA Hub for I2C communication
```

Or set in `sdkconfig.defaults`:

```
CONFIG_UNIT_ENV_III_USE_PAHUB=y
CONFIG_UNIT_ENV_III_PAHUB_CHANNEL=0
```

## Error Handling

All functions return `esp_err_t`. Common return values:

| Error | Meaning |
|---|---|
| `ESP_OK` | Success |
| `ESP_ERR_INVALID_STATE` | Driver not initialized (call `unit_env_iii_init()` first) |
| `ESP_ERR_INVALID_ARG` | NULL pointer or invalid parameter |
| `ESP_ERR_INVALID_CRC` | SHT30 CRC check failed (I2C noise or wiring issue) |
| `ESP_ERR_NOT_FOUND` | QMP6988 chip ID mismatch (wrong device or address) |

## Troubleshooting

- **Init fails**: Verify the cable is seated in **Port A** and that `core2foraws_expports_i2c_begin()` was called before initializing this driver.
- **CRC errors**: Check cable connections, reduce I2C clock speed, or shorten cable length.
- **Pressure seems wrong**: `pressure_pa` is in Pascals. Divide by 100 for hPa (hectopascals/millibars).
- **Debug logging**: `esp_log_level_set( "unit_env_iii", ESP_LOG_DEBUG );`

## Dependencies

- ESP-IDF v4.4+
- [Core2 for AWS IoT Kit BSP](https://github.com/m5stack/Core2-for-AWS-IoT-Kit)

## License

Apache 2.0 — see [LICENSE](LICENSE).
