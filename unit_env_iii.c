#include "unit_env_iii.h"
#include "core2foraws_expports.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#ifdef CONFIG_UNIT_ENV_III_USE_PAHUB
#include "unit_pahub.h"
#endif

static const char *_TAG = "unit_env_iii";

// SHT30 Commands
#define _SHT30_CMD_MEASURE_HIGH_REP 0x2400
#define _SHT30_CMD_MEASURE_MED_REP  0x240B
#define _SHT30_CMD_MEASURE_LOW_REP  0x2416
#define _SHT30_CMD_HEATER_ENABLE    0x306D
#define _SHT30_CMD_HEATER_DISABLE   0x3066
#define _SHT30_CMD_SOFT_RESET       0x30A2
#define _SHT30_CMD_READ_STATUS      0xF32D

// QMP6988 Registers
#define _QMP6988_CHIP_ID_REG     0xD1
#define _QMP6988_RESET_REG       0xE0
#define _QMP6988_IIR_REG         0xF1
#define _QMP6988_DEVICE_STAT_REG 0xF3
#define _QMP6988_CTRL_MEAS_REG   0xF4
#define _QMP6988_IO_SETUP_REG    0xF5
#define _QMP6988_PRESS_MSB_REG   0xF7
#define _QMP6988_TEMP_MSB_REG    0xFA

// QMP6988 Constants
#define _QMP6988_CHIP_ID   0x5C
#define _QMP6988_RESET_CMD 0xE6

// Calibration registers
#define _QMP6988_CALIB_DATA_START 0xA0
#define _QMP6988_CALIB_DATA_LEN   25

// Static variables
static bool _initialized = false;
static unit_env_iii_config_t _config;
static i2c_master_dev_handle_t _sht30_dev = NULL;
static i2c_master_dev_handle_t _qmp6988_dev = NULL;

// QMP6988 calibration coefficients
static struct
{
  float COE_a0, COE_a1, COE_a2;
  float COE_b00, COE_bt1, COE_bt2, COE_bp1, COE_b11, COE_bp2, COE_b12, COE_b21,
      COE_bp3;
} _qmp6988_calib;

// Static function declarations
static esp_err_t _write_i2c( i2c_master_dev_handle_t dev, const uint8_t *data, size_t len );
static esp_err_t _read_i2c( i2c_master_dev_handle_t dev, uint8_t *data, size_t len );
static esp_err_t _write_read_i2c( i2c_master_dev_handle_t dev, const uint8_t *write_data,
                                  size_t write_len, uint8_t *read_data,
                                  size_t read_len );
static uint8_t _crc8( const uint8_t *data, int len );
static esp_err_t _sht30_init( void );
static esp_err_t _qmp6988_init( void );
static esp_err_t _qmp6988_read_calibration( void );
static float _qmp6988_compensate_temperature( int32_t raw_temp );
static float _qmp6988_compensate_pressure( int32_t raw_press, float comp_temp );

// I2C communication functions
static esp_err_t _write_i2c( i2c_master_dev_handle_t dev, const uint8_t *data, size_t len )
{
#ifdef CONFIG_UNIT_ENV_III_USE_PAHUB
  if( len > 1 )
  {
    return unit_pahub_i2c_write( CONFIG_UNIT_ENV_III_PAHUB_CHANNEL, dev,
                                 data[ 0 ], &data[ 1 ], len - 1 );
  }
  else if( len == 1 )
  {
    // For single byte writes (like writing just a register address)
    return unit_pahub_i2c_write( CONFIG_UNIT_ENV_III_PAHUB_CHANNEL, dev,
                                 CORE2FORAWS_I2C_NO_REG, data, 1 );
  }
  return ESP_ERR_INVALID_ARG;
#else
  if( len > 1 )
  {
    return core2foraws_expports_i2c_write( dev, data[ 0 ], &data[ 1 ],
                                           len - 1 );
  }
  else if( len == 1 )
  {
    // For single byte writes, use CORE2FORAWS_I2C_NO_REG
    return core2foraws_expports_i2c_write( dev, CORE2FORAWS_I2C_NO_REG, data, 1 );
  }
  return ESP_ERR_INVALID_ARG;
#endif
}

static esp_err_t _read_i2c( i2c_master_dev_handle_t dev, uint8_t *data, size_t len )
{
#ifdef CONFIG_UNIT_ENV_III_USE_PAHUB
  // For raw reads without register address, use CORE2FORAWS_I2C_NO_REG
  return unit_pahub_i2c_read( CONFIG_UNIT_ENV_III_PAHUB_CHANNEL, dev,
                              CORE2FORAWS_I2C_NO_REG, data, len );
#else
  return core2foraws_expports_i2c_read( dev, CORE2FORAWS_I2C_NO_REG, data, len );
#endif
  return ESP_FAIL;  /* unreachable — silences -Wreturn-type */
}

static esp_err_t _write_read_i2c( i2c_master_dev_handle_t dev, const uint8_t *write_data,
                                  size_t write_len, uint8_t *read_data,
                                  size_t read_len )
{
#ifdef CONFIG_UNIT_ENV_III_USE_PAHUB
  // For register reads, use the unit_pahub_i2c_read with the register address
  if( write_len == 1 )
  {
    // Simple register read
    return unit_pahub_i2c_read( CONFIG_UNIT_ENV_III_PAHUB_CHANNEL, dev,
                                write_data[ 0 ], read_data, read_len );
  }
  else if( write_len > 1 )
  {
    // Write command/data first
    esp_err_t ret = unit_pahub_i2c_write( CONFIG_UNIT_ENV_III_PAHUB_CHANNEL,
                                          dev, write_data[ 0 ],
                                          &write_data[ 1 ], write_len - 1 );
    if( ret != ESP_OK )
      return ret;

    vTaskDelay( pdMS_TO_TICKS( 10 ) );

    // Then read the response
    return unit_pahub_i2c_read( CONFIG_UNIT_ENV_III_PAHUB_CHANNEL, dev,
                                CORE2FORAWS_I2C_NO_REG, read_data, read_len );
  }
  return ESP_ERR_INVALID_ARG;
#else
  if( write_len == 1 )
  {
    // Simple register read
    return core2foraws_expports_i2c_read( dev, write_data[ 0 ], read_data,
                                          read_len );
  }
  else if( write_len > 1 )
  {
    // Write additional data first
    esp_err_t ret = core2foraws_expports_i2c_write(
        dev, write_data[ 0 ], &write_data[ 1 ], write_len - 1 );
    if( ret != ESP_OK )
      return ret;

    vTaskDelay( pdMS_TO_TICKS( 10 ) );

    // Then read
    return core2foraws_expports_i2c_read( dev, CORE2FORAWS_I2C_NO_REG, read_data,
                                          read_len );
  }
  return ESP_ERR_INVALID_ARG;
#endif
}

// CRC8 calculation for SHT30
static uint8_t _crc8( const uint8_t *data, int len )
{
  const uint8_t polynomial = 0x31;
  uint8_t crc = 0xFF;

  for( int i = 0; i < len; i++ )
  {
    crc ^= data[ i ];
    for( int j = 0; j < 8; j++ )
    {
      if( crc & 0x80 )
      {
        crc = ( crc << 1 ) ^ polynomial;
      }
      else
      {
        crc <<= 1;
      }
    }
  }
  return crc;
}

// SHT30 initialization
static esp_err_t _sht30_init( void )
{
  // Send soft reset
  uint8_t reset_cmd[ 2 ] = { ( _SHT30_CMD_SOFT_RESET >> 8 ) & 0xFF,
                             _SHT30_CMD_SOFT_RESET & 0xFF };
  esp_err_t ret = _write_i2c( _sht30_dev, reset_cmd, 2 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "SHT30 reset failed: %s", esp_err_to_name( ret ) );
    return ret;
  }

  vTaskDelay( pdMS_TO_TICKS( 10 ) ); // Wait for reset

  // Set heater state
  uint16_t heater_cmd = _config.sht30_heater_enable ? _SHT30_CMD_HEATER_ENABLE
                                                    : _SHT30_CMD_HEATER_DISABLE;
  uint8_t cmd[ 2 ] = { ( heater_cmd >> 8 ) & 0xFF, heater_cmd & 0xFF };
  ret = _write_i2c( _sht30_dev, cmd, 2 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "SHT30 heater config failed: %s", esp_err_to_name( ret ) );
    return ret;
  }

  ESP_LOGI( _TAG, "SHT30 initialized successfully" );
  return ESP_OK;
}

// QMP6988 initialization
static esp_err_t _qmp6988_init( void )
{
  // Check chip ID
  uint8_t chip_id_reg = _QMP6988_CHIP_ID_REG;
  uint8_t chip_id = 0;
  esp_err_t ret = _write_read_i2c( _qmp6988_dev, &chip_id_reg, 1,
                                   &chip_id, 1 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "QMP6988 chip ID read failed: %s", esp_err_to_name( ret ) );
    return ret;
  }

  ESP_LOGI( _TAG, "QMP6988 chip ID: 0x%02X (expected: 0x%02X)", chip_id,
            _QMP6988_CHIP_ID );

  if( chip_id != _QMP6988_CHIP_ID )
  {
    ESP_LOGE( _TAG, "QMP6988 chip ID mismatch: expected 0x%02X, got 0x%02X",
              _QMP6988_CHIP_ID, chip_id );
    return ESP_ERR_NOT_FOUND;
  }

  // Soft reset
  uint8_t reset_data[ 2 ] = { _QMP6988_RESET_REG, _QMP6988_RESET_CMD };
  ret = _write_i2c( _qmp6988_dev, reset_data, 2 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "QMP6988 reset failed: %s", esp_err_to_name( ret ) );
    return ret;
  }

  vTaskDelay( pdMS_TO_TICKS( 10 ) ); // Wait for reset

  // Wait for OTP data to be ready (datasheet: otp_update must be 0)
  uint8_t stat_reg = _QMP6988_DEVICE_STAT_REG;
  uint8_t stat_val = 0;
  for( int i = 0; i < 10; i++ )
  {
    ret = _write_read_i2c( _qmp6988_dev, &stat_reg, 1, &stat_val,
                           1 );
    if( ret != ESP_OK )
    {
      ESP_LOGE( _TAG, "QMP6988 status read failed: %s",
                esp_err_to_name( ret ) );
      return ret;
    }
    if( !( stat_val & 0x01 ) )
    {
      break; // OTP update complete
    }
    vTaskDelay( pdMS_TO_TICKS( 5 ) );
  }

  // Read calibration data
  ret = _qmp6988_read_calibration();
  if( ret != ESP_OK )
  {
    return ret;
  }

  // Configure IIR filter (register 0xF1, bits [2:0])
  uint8_t iir_data[ 2 ] = { _QMP6988_IIR_REG, _config.qmp6988_filter };
  ret = _write_i2c( _qmp6988_dev, iir_data, 2 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "QMP6988 IIR filter config failed: %s",
              esp_err_to_name( ret ) );
    return ret;
  }

  // Configure oversampling and enter normal mode (continuous measurements)
  uint8_t ctrl_meas =
      ( ( _config.qmp6988_temp_osr << 5 ) | ( _config.qmp6988_press_osr << 2 ) |
        0x03 ); // Normal mode
  uint8_t ctrl_data[ 2 ] = { _QMP6988_CTRL_MEAS_REG, ctrl_meas };
  ret = _write_i2c( _qmp6988_dev, ctrl_data, 2 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "QMP6988 ctrl_meas config failed: %s",
              esp_err_to_name( ret ) );
    return ret;
  }

  ESP_LOGI( _TAG, "QMP6988 initialized successfully" );
  return ESP_OK;
}

// QMP6988 calibration data reading
static esp_err_t _qmp6988_read_calibration( void )
{
  uint8_t calib_reg = _QMP6988_CALIB_DATA_START;
  uint8_t calib_data[ _QMP6988_CALIB_DATA_LEN ];

  esp_err_t ret = _write_read_i2c( _qmp6988_dev, &calib_reg, 1,
                                   calib_data, _QMP6988_CALIB_DATA_LEN );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "QMP6988 calibration read failed: %s",
              esp_err_to_name( ret ) );
    return ret;
  }

  // Parse calibration coefficients per QMP6988 datasheet register map.
  // Reading starts at 0xA0 and auto-increments through 0xB8 (25 bytes).
  //
  // Register-to-array mapping:
  //   [0:1]   = 0xA0:0xA1 = b00 (upper 16 of 20 bits)
  //   [2:3]   = 0xA2:0xA3 = bt1
  //   [4:5]   = 0xA4:0xA5 = bt2
  //   [6:7]   = 0xA6:0xA7 = bp1
  //   [8:9]   = 0xA8:0xA9 = b11
  //   [10:11] = 0xAA:0xAB = bp2
  //   [12:13] = 0xAC:0xAD = b12
  //   [14:15] = 0xAE:0xAF = b21
  //   [16:17] = 0xB0:0xB1 = bp3
  //   [18:19] = 0xB2:0xB3 = a0 (upper 16 of 20 bits)
  //   [20:21] = 0xB4:0xB5 = a1
  //   [22:23] = 0xB6:0xB7 = a2
  //   [24]    = 0xB8       = b00[3:0] (upper nibble) | a0[3:0] (lower nibble)
  //
  // 16-bit coefficients use: K = A + S * OTP / 32767.0
  // 20-bit coefficients (a0, b00) use: K = OTP / 16.0

  // b00 (20-bit: data[0:1] + upper nibble of data[24])
  uint32_t COE_b00_otp = ( (uint32_t)calib_data[ 0 ] << 12 ) |
                         ( (uint32_t)calib_data[ 1 ] << 4 ) |
                         ( ( calib_data[ 24 ] & 0xF0 ) >> 4 );
  if( COE_b00_otp & 0x80000 )
    COE_b00_otp -= 0x100000; // Sign extend 20-bit
  _qmp6988_calib.COE_b00 = (float)( (int32_t)COE_b00_otp ) / 16.0f;

  // bt1 (data[2:3])
  int16_t COE_bt1_otp = (int16_t)( ( calib_data[ 2 ] << 8 ) | calib_data[ 3 ] );
  _qmp6988_calib.COE_bt1 = 1.00e-01f + ( 9.10e-02f * COE_bt1_otp ) / 32767.0f;

  // bt2 (data[4:5])
  int16_t COE_bt2_otp = (int16_t)( ( calib_data[ 4 ] << 8 ) | calib_data[ 5 ] );
  _qmp6988_calib.COE_bt2 = 1.20e-08f + ( 1.20e-06f * COE_bt2_otp ) / 32767.0f;

  // bp1 (data[6:7])
  int16_t COE_bp1_otp = (int16_t)( ( calib_data[ 6 ] << 8 ) | calib_data[ 7 ] );
  _qmp6988_calib.COE_bp1 = 3.30e-02f + ( 1.90e-02f * COE_bp1_otp ) / 32767.0f;

  // b11 (data[8:9])
  int16_t COE_b11_otp = (int16_t)( ( calib_data[ 8 ] << 8 ) | calib_data[ 9 ] );
  _qmp6988_calib.COE_b11 = 2.10e-07f + ( 1.40e-07f * COE_b11_otp ) / 32767.0f;

  // bp2 (data[10:11])
  int16_t COE_bp2_otp =
      (int16_t)( ( calib_data[ 10 ] << 8 ) | calib_data[ 11 ] );
  _qmp6988_calib.COE_bp2 = -6.30e-10f + ( 3.50e-10f * COE_bp2_otp ) / 32767.0f;

  // b12 (data[12:13])
  int16_t COE_b12_otp =
      (int16_t)( ( calib_data[ 12 ] << 8 ) | calib_data[ 13 ] );
  _qmp6988_calib.COE_b12 = 2.90e-13f + ( 7.60e-13f * COE_b12_otp ) / 32767.0f;

  // b21 (data[14:15])
  int16_t COE_b21_otp =
      (int16_t)( ( calib_data[ 14 ] << 8 ) | calib_data[ 15 ] );
  _qmp6988_calib.COE_b21 = 2.10e-15f + ( 1.20e-14f * COE_b21_otp ) / 32767.0f;

  // bp3 (data[16:17])
  int16_t COE_bp3_otp =
      (int16_t)( ( calib_data[ 16 ] << 8 ) | calib_data[ 17 ] );
  _qmp6988_calib.COE_bp3 = 1.30e-16f + ( 7.90e-17f * COE_bp3_otp ) / 32767.0f;

  // a0 (20-bit: data[18:19] + lower nibble of data[24])
  uint32_t COE_a0_otp = ( (uint32_t)calib_data[ 18 ] << 12 ) |
                        ( (uint32_t)calib_data[ 19 ] << 4 ) |
                        ( calib_data[ 24 ] & 0x0F );
  if( COE_a0_otp & 0x80000 )
    COE_a0_otp -= 0x100000; // Sign extend 20-bit
  _qmp6988_calib.COE_a0 = (float)( (int32_t)COE_a0_otp ) / 16.0f;

  // a1 (data[20:21])
  int16_t COE_a1_otp =
      (int16_t)( ( calib_data[ 20 ] << 8 ) | calib_data[ 21 ] );
  _qmp6988_calib.COE_a1 = -6.30e-03f + ( 4.30e-04f * COE_a1_otp ) / 32767.0f;

  // a2 (data[22:23])
  int16_t COE_a2_otp =
      (int16_t)( ( calib_data[ 22 ] << 8 ) | calib_data[ 23 ] );
  _qmp6988_calib.COE_a2 = -1.90e-11f + ( 1.20e-10f * COE_a2_otp ) / 32767.0f;

  ESP_LOGI( _TAG, "QMP6988 calibration coefficients:" );
  ESP_LOGI( _TAG, "a0=%.2f, a1=%.6f, a2=%.2e", _qmp6988_calib.COE_a0,
            _qmp6988_calib.COE_a1, _qmp6988_calib.COE_a2 );
  ESP_LOGI( _TAG, "b00=%.2f, bt1=%.6f, bt2=%.2e", _qmp6988_calib.COE_b00,
            _qmp6988_calib.COE_bt1, _qmp6988_calib.COE_bt2 );
  ESP_LOGI( _TAG, "bp1=%.6f, b11=%.2e, bp2=%.2e", _qmp6988_calib.COE_bp1,
            _qmp6988_calib.COE_b11, _qmp6988_calib.COE_bp2 );
  ESP_LOGI( _TAG, "b12=%.2e, b21=%.2e, bp3=%.2e", _qmp6988_calib.COE_b12,
            _qmp6988_calib.COE_b21, _qmp6988_calib.COE_bp3 );

  return ESP_OK;
}

// Temperature compensation for QMP6988
static float _qmp6988_compensate_temperature( int32_t raw_temp )
{
  // Apply datasheet formula: Dt = raw_temp - 2^23 for signed conversion
  float Dt = (float)( raw_temp - ( 1 << 23 ) );

  // Apply temperature compensation formula: Tr = a0 + a1*Dt + a2*Dt^2
  float Tr = _qmp6988_calib.COE_a0 + _qmp6988_calib.COE_a1 * Dt +
             _qmp6988_calib.COE_a2 * Dt * Dt;

  // Tr is already in the correct format (256*degC), return as is for pressure
  // calc
  return Tr;
}

// Pressure compensation for QMP6988
static float _qmp6988_compensate_pressure( int32_t raw_press, float Tr )
{
  // Apply datasheet formula: Dp = raw_press - 2^23 for signed conversion
  float Dp = (float)( raw_press - ( 1 << 23 ) );

  // Apply pressure compensation formula exactly as specified in datasheet
  float Pr = _qmp6988_calib.COE_b00 + _qmp6988_calib.COE_bt1 * Tr +
             _qmp6988_calib.COE_bp1 * Dp + _qmp6988_calib.COE_b11 * Tr * Dp +
             _qmp6988_calib.COE_bt2 * Tr * Tr +
             _qmp6988_calib.COE_bp2 * Dp * Dp +
             _qmp6988_calib.COE_b12 * Dp * Tr * Tr +
             _qmp6988_calib.COE_b21 * Dp * Dp * Tr +
             _qmp6988_calib.COE_bp3 * Dp * Dp * Dp;

  return Pr; // Result is in Pascal
}

// Public API implementation
esp_err_t unit_env_iii_get_default_config( unit_env_iii_config_t *config )
{
  if( config == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }

  config->sht30_repeatability = UNIT_ENV_III_SHT30_REPEATABILITY_HIGH;
  config->sht30_heater_enable = false;
  config->qmp6988_temp_osr = UNIT_ENV_III_QMP6988_OSR_16;
  config->qmp6988_press_osr = UNIT_ENV_III_QMP6988_OSR_16;
  config->qmp6988_filter = UNIT_ENV_III_QMP6988_FILTER_16;

  return ESP_OK;
}

esp_err_t unit_env_iii_init( void )
{
  unit_env_iii_config_t default_config;
  unit_env_iii_get_default_config( &default_config );
  return unit_env_iii_init_with_config( &default_config );
}

esp_err_t unit_env_iii_init_with_config( const unit_env_iii_config_t *config )
{
  if( config == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }

  if( _initialized )
  {
    ESP_LOGW( _TAG, "Already initialized" );
    return ESP_OK;
  }

  _config = *config;
  esp_err_t ret = ESP_FAIL;

#ifdef CONFIG_UNIT_ENV_III_USE_PAHUB
  ret = unit_pahub_init();
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "PA Hub initialization failed: %s",
              esp_err_to_name( ret ) );
    return ret;
  }

  ret = unit_pahub_channel_set( CONFIG_UNIT_ENV_III_PAHUB_CHANNEL );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "PA Hub channel set failed: %s", esp_err_to_name( ret ) );
    return ret;
  }
#endif

  ret = core2foraws_expports_i2c_device_add( UNIT_ENV_III_SHT30_ADDR, 100000, &_sht30_dev );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "Failed to add SHT30 I2C device: %s", esp_err_to_name( ret ) );
    return ret;
  }

  ret = core2foraws_expports_i2c_device_add( UNIT_ENV_III_QMP6988_ADDR, 100000, &_qmp6988_dev );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "Failed to add QMP6988 I2C device: %s", esp_err_to_name( ret ) );
    return ret;
  }

  // Initialize SHT30
  ret = _sht30_init();
  if( ret != ESP_OK )
  {
    return ret;
  }

  // Initialize QMP6988
  ret = _qmp6988_init();
  if( ret != ESP_OK )
  {
    return ret;
  }

  _initialized = true;
  ESP_LOGI( _TAG, "Unit ENV-III initialized successfully" );
  return ESP_OK;
}

esp_err_t unit_env_iii_deinit( void )
{
  if( !_initialized )
  {
    return ESP_ERR_INVALID_STATE;
  }

  _initialized = false;
  ESP_LOGI( _TAG, "Unit ENV-III deinitialized" );
  return ESP_OK;
}

esp_err_t unit_env_iii_read_temp_humidity( float *temperature_c,
                                           float *humidity_rh )
{
  if( !_initialized )
  {
    return ESP_ERR_INVALID_STATE;
  }

  // Select measurement command based on repeatability
  uint16_t measure_cmd;
  switch( _config.sht30_repeatability )
  {
  case UNIT_ENV_III_SHT30_REPEATABILITY_HIGH:
    measure_cmd = _SHT30_CMD_MEASURE_HIGH_REP;
    break;
  case UNIT_ENV_III_SHT30_REPEATABILITY_MEDIUM:
    measure_cmd = _SHT30_CMD_MEASURE_MED_REP;
    break;
  case UNIT_ENV_III_SHT30_REPEATABILITY_LOW:
    measure_cmd = _SHT30_CMD_MEASURE_LOW_REP;
    break;
  default:
    return ESP_ERR_INVALID_ARG;
  }

  // Send measurement command
  uint8_t cmd[ 2 ] = { ( measure_cmd >> 8 ) & 0xFF, measure_cmd & 0xFF };
  esp_err_t ret = _write_i2c( _sht30_dev, cmd, 2 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "SHT30 measurement command failed: %s",
              esp_err_to_name( ret ) );
    return ret;
  }

  // Wait for measurement
  vTaskDelay( pdMS_TO_TICKS( 20 ) );

  // Read measurement data
  uint8_t data[ 6 ];
  ret = _read_i2c( _sht30_dev, data, 6 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "SHT30 data read failed: %s", esp_err_to_name( ret ) );
    return ret;
  }

  // Verify CRC
  if( _crc8( &data[ 0 ], 2 ) != data[ 2 ] ||
      _crc8( &data[ 3 ], 2 ) != data[ 5 ] )
  {
    ESP_LOGE( _TAG, "SHT30 CRC check failed" );
    return ESP_ERR_INVALID_CRC;
  }

  // Convert raw data
  uint16_t raw_temp = ( data[ 0 ] << 8 ) | data[ 1 ];
  uint16_t raw_hum = ( data[ 3 ] << 8 ) | data[ 4 ];

  if( temperature_c != NULL )
  {
    *temperature_c = -45.0f + 175.0f * ( (float)raw_temp / 65535.0f );
  }

  if( humidity_rh != NULL )
  {
    *humidity_rh = 100.0f * ( (float)raw_hum / 65535.0f );
  }

  return ESP_OK;
}

esp_err_t unit_env_iii_read_pressure( float *pressure_pa )
{
  if( !_initialized )
  {
    return ESP_ERR_INVALID_STATE;
  }

  if( pressure_pa == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }

  // Read pressure and temperature data (6 bytes total)
  uint8_t press_reg = _QMP6988_PRESS_MSB_REG;
  uint8_t press_data[ 6 ]; // 3 bytes pressure + 3 bytes temperature

  esp_err_t ret = _write_read_i2c( _qmp6988_dev, &press_reg, 1,
                                   press_data, 6 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "QMP6988 pressure read failed: %s",
              esp_err_to_name( ret ) );
    return ret;
  }

  // Parse raw data (24-bit values) as per datasheet
  int32_t raw_press = ( (int32_t)press_data[ 0 ] << 16 ) |
                      ( (int32_t)press_data[ 1 ] << 8 ) | press_data[ 2 ];

  int32_t raw_temp = ( (int32_t)press_data[ 3 ] << 16 ) |
                     ( (int32_t)press_data[ 4 ] << 8 ) | press_data[ 5 ];

  ESP_LOGD( _TAG, "Raw pressure: %ld, Raw temperature: %ld", (long)raw_press,
            (long)raw_temp );

  // Compensate temperature first (returns Tr in 256*degC format)
  float Tr = _qmp6988_compensate_temperature( raw_temp );

  // Then compensate pressure using Tr
  *pressure_pa = _qmp6988_compensate_pressure( raw_press, Tr );

  // Convert Tr to actual temperature for logging
  float actual_temp = Tr / 256.0f;
  ESP_LOGD( _TAG, "Compensated temp: %.2f°C, pressure: %.2f Pa", actual_temp,
            *pressure_pa );

  return ESP_OK;
}

esp_err_t unit_env_iii_read_data( unit_env_iii_data_t *data )
{
  if( !_initialized )
  {
    return ESP_ERR_INVALID_STATE;
  }

  if( data == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }

  // Initialize validity flags
  data->temp_hum_valid = false;
  data->pressure_valid = false;

  // Read temperature and humidity
  esp_err_t ret = unit_env_iii_read_temp_humidity( &data->temperature_c,
                                                   &data->humidity_rh );
  if( ret == ESP_OK )
  {
    data->temp_hum_valid = true;
  }
  else
  {
    ESP_LOGW( _TAG, "Temperature/humidity read failed: %s",
              esp_err_to_name( ret ) );
  }

  // Read pressure
  ret = unit_env_iii_read_pressure( &data->pressure_pa );
  if( ret == ESP_OK )
  {
    data->pressure_valid = true;
  }
  else
  {
    ESP_LOGW( _TAG, "Pressure read failed: %s", esp_err_to_name( ret ) );
  }

  // Return success if at least one sensor worked
  if( data->temp_hum_valid || data->pressure_valid )
  {
    return ESP_OK;
  }

  return ESP_FAIL;
}

esp_err_t unit_env_iii_set_sht30_heater( bool enable )
{
  if( !_initialized )
  {
    return ESP_ERR_INVALID_STATE;
  }

  uint16_t heater_cmd =
      enable ? _SHT30_CMD_HEATER_ENABLE : _SHT30_CMD_HEATER_DISABLE;
  uint8_t cmd[ 2 ] = { ( heater_cmd >> 8 ) & 0xFF, heater_cmd & 0xFF };

  esp_err_t ret = _write_i2c( _sht30_dev, cmd, 2 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "SHT30 heater control failed: %s", esp_err_to_name( ret ) );
    return ret;
  }

  _config.sht30_heater_enable = enable;
  return ESP_OK;
}

esp_err_t unit_env_iii_reset_sht30( void )
{
  if( !_initialized )
  {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t reset_cmd[ 2 ] = { ( _SHT30_CMD_SOFT_RESET >> 8 ) & 0xFF,
                             _SHT30_CMD_SOFT_RESET & 0xFF };
  esp_err_t ret = _write_i2c( _sht30_dev, reset_cmd, 2 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "SHT30 reset failed: %s", esp_err_to_name( ret ) );
    return ret;
  }

  vTaskDelay( pdMS_TO_TICKS( 10 ) );
  return ESP_OK;
}

esp_err_t unit_env_iii_reset_qmp6988( void )
{
  if( !_initialized )
  {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t reset_data[ 2 ] = { _QMP6988_RESET_REG, _QMP6988_RESET_CMD };
  esp_err_t ret = _write_i2c( _qmp6988_dev, reset_data, 2 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "QMP6988 reset failed: %s", esp_err_to_name( ret ) );
    return ret;
  }

  vTaskDelay( pdMS_TO_TICKS( 10 ) );
  return ESP_OK;
}

esp_err_t unit_env_iii_check_connection( void )
{
  if( !_initialized )
  {
    return ESP_ERR_INVALID_STATE;
  }

  // Test SHT30 by reading status
  uint8_t sht30_cmd[ 2 ] = { ( _SHT30_CMD_READ_STATUS >> 8 ) & 0xFF,
                             _SHT30_CMD_READ_STATUS & 0xFF };
  uint8_t sht30_status[ 3 ];
  esp_err_t ret =
      _write_read_i2c( _sht30_dev, sht30_cmd, 2, sht30_status, 3 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "SHT30 connection check failed: %s",
              esp_err_to_name( ret ) );
    return ret;
  }

  // Test QMP6988 by reading chip ID
  uint8_t qmp6988_reg = _QMP6988_CHIP_ID_REG;
  uint8_t chip_id;
  ret = _write_read_i2c( _qmp6988_dev, &qmp6988_reg, 1, &chip_id,
                         1 );
  if( ret != ESP_OK )
  {
    ESP_LOGE( _TAG, "QMP6988 connection check failed: %s",
              esp_err_to_name( ret ) );
    return ret;
  }

  if( chip_id != _QMP6988_CHIP_ID )
  {
    ESP_LOGE( _TAG, "QMP6988 chip ID mismatch during connection check" );
    return ESP_ERR_NOT_FOUND;
  }

  return ESP_OK;
}
