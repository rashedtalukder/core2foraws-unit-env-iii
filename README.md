# M5Stack Unit ENV-III ESP-IDF Component

Driver for the [M5Stack Unit ENV-III](https://docs.m5stack.com/en/unit/envIII) (U001-C):

| Sensor | Measurement | Address |
| --- | --- | ---: |
| Sensirion SHT30 | temperature and relative humidity | `0x44` |
| QST QMP6988 | barometric pressure | `0x70` |

## Usage

```c
#include "core2foraws.h"
#include "unit_env_iii.h"

core2foraws_init();
ESP_ERROR_CHECK( core2foraws_expports_i2c_begin() );
ESP_ERROR_CHECK( unit_env_iii_init() );

unit_env_iii_data_t data = { 0 };
if( unit_env_iii_read_data( &data ) == ESP_OK )
{
  if( data.temp_hum_valid ) { /* use temperature_c and humidity_rh */ }
  if( data.pressure_valid ) { /* use pressure_pa */ }
}
```

`unit_env_iii_read_data()` succeeds when either sensor succeeds; always inspect both validity flags. Custom oversampling, filtering, repeatability, and heater state are set with `unit_env_iii_init_with_config()`.

Reset APIs restore the active driver configuration. `unit_env_iii_deinit()` releases both managed I2C device handles. Enable `CONFIG_UNIT_ENV_III_USE_PAHUB` to use a PaHub channel.

See [datasheets/unit-env-iii.md](datasheets/unit-env-iii.md), [datasheets/SHT30.md](datasheets/SHT30.md), and [datasheets/QMP6988.md](datasheets/QMP6988.md).
