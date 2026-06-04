/*!
 * @brief Library for the ENV III (SHT30 + QMP6988) Unit by M5Stack on the
 * Core2 for AWS IoT Kit.
 *
 * The ENV III Unit combines a Sensirion SHT30 temperature/humidity sensor
 * (I2C 0x44) and a Bosch QMP6988 barometric pressure sensor (I2C 0x70). It
 * connects to Port A of the Core2 for AWS IoT Kit, or to a channel of the
 * PA Hub (PCA9548A) I2C multiplexer when CONFIG_UNIT_ENV_III_USE_PAHUB is
 * enabled.
 *
 * @copyright Copyright (c) 2025 by Rashed Talukder[https://rashedtalukder.com]
 *
 * @license SPDX-License-Identifier: Apache 2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Typical usage:
 * @code{c}
 *   #include "unit_env_iii.h"
 *
 *   void app_main( void )
 *   {
 *       core2foraws_init();
 *       unit_env_iii_init();
 *
 *       unit_env_iii_data_t env = { 0 };
 *       if ( unit_env_iii_read_data( &env ) == ESP_OK && env.temp_hum_valid )
 *       {
 *           ESP_LOGI( "ENV", "%.2f C, %.1f %%RH",
 *                     env.temperature_c, env.humidity_rh );
 *       }
 *   }
 * @endcode
 *
 * @links [ENV III Unit](https://docs.m5stack.com/en/unit/envIII)
 * @version  V1.1.0
 * @date  2026-06-04
 */

#pragma once

#ifndef UNIT_ENV_III_H
#define UNIT_ENV_III_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 7-bit I2C address of the SHT30 temperature/humidity sensor.
 */
#define UNIT_ENV_III_SHT30_ADDR   0x44

/**
 * @brief 7-bit I2C address of the QMP6988 barometric pressure sensor.
 */
#define UNIT_ENV_III_QMP6988_ADDR 0x70

  /**
   * @brief SHT30 measurement repeatability (accuracy vs. measurement time).
   *
   * Higher repeatability yields lower noise at the cost of a longer
   * measurement duration and slightly higher power draw.
   */
  typedef enum
  {
    UNIT_ENV_III_SHT30_REPEATABILITY_HIGH = 0, /*!< Lowest noise, slowest */
    UNIT_ENV_III_SHT30_REPEATABILITY_MEDIUM,   /*!< Balanced noise/speed */
    UNIT_ENV_III_SHT30_REPEATABILITY_LOW       /*!< Highest noise, fastest */
  } unit_env_iii_sht30_repeatability_t;

  /**
   * @brief QMP6988 oversampling rate (OSR) for temperature or pressure.
   *
   * These are the raw register values from the QMP6988 datasheet. A value of
   * 0 means "skip measurement", so valid oversampling starts at 1. Higher
   * oversampling reduces noise at the cost of conversion time.
   */
  typedef enum
  {
    UNIT_ENV_III_QMP6988_OSR_1 = 1,  /*!< 1x oversampling */
    UNIT_ENV_III_QMP6988_OSR_2 = 2,  /*!< 2x oversampling */
    UNIT_ENV_III_QMP6988_OSR_4 = 3,  /*!< 4x oversampling */
    UNIT_ENV_III_QMP6988_OSR_8 = 4,  /*!< 8x oversampling */
    UNIT_ENV_III_QMP6988_OSR_16 = 5, /*!< 16x oversampling */
    UNIT_ENV_III_QMP6988_OSR_32 = 6, /*!< 32x oversampling */
    UNIT_ENV_III_QMP6988_OSR_64 = 7  /*!< 64x oversampling */
  } unit_env_iii_qmp6988_osr_t;

  /**
   * @brief QMP6988 IIR (infinite impulse response) filter coefficient.
   *
   * A stronger filter smooths short-term pressure fluctuations at the cost
   * of slower response to genuine changes.
   */
  typedef enum
  {
    UNIT_ENV_III_QMP6988_FILTER_OFF = 0, /*!< Filter disabled */
    UNIT_ENV_III_QMP6988_FILTER_2,       /*!< Coefficient 2 */
    UNIT_ENV_III_QMP6988_FILTER_4,       /*!< Coefficient 4 */
    UNIT_ENV_III_QMP6988_FILTER_8,       /*!< Coefficient 8 */
    UNIT_ENV_III_QMP6988_FILTER_16,      /*!< Coefficient 16 */
    UNIT_ENV_III_QMP6988_FILTER_32       /*!< Coefficient 32 */
  } unit_env_iii_qmp6988_filter_t;

  /**
   * @brief ENV III configuration.
   *
   * Populate with unit_env_iii_get_default_config() then override individual
   * fields before passing to unit_env_iii_init_with_config().
   */
  typedef struct
  {
    unit_env_iii_sht30_repeatability_t
        sht30_repeatability;     /*!< SHT30 measurement repeatability */
    bool sht30_heater_enable;    /*!< Enable SHT30 on-chip heater at init */
    unit_env_iii_qmp6988_osr_t
        qmp6988_temp_osr;        /*!< QMP6988 temperature oversampling */
    unit_env_iii_qmp6988_osr_t
        qmp6988_press_osr;       /*!< QMP6988 pressure oversampling */
    unit_env_iii_qmp6988_filter_t
        qmp6988_filter;          /*!< QMP6988 IIR filter coefficient */
  } unit_env_iii_config_t;

  /**
   * @brief A single ENV III measurement set.
   *
   * The two *_valid flags indicate which sensors produced fresh data on the
   * most recent read; always check them before using a field.
   */
  typedef struct
  {
    float temperature_c; /*!< Temperature in degrees Celsius (SHT30) */
    float humidity_rh;   /*!< Relative humidity in %% (SHT30) */
    float pressure_pa;   /*!< Barometric pressure in Pascal (QMP6988) */
    bool temp_hum_valid; /*!< true if temperature_c/humidity_rh are valid */
    bool pressure_valid; /*!< true if pressure_pa is valid */
  } unit_env_iii_data_t;

  /**
   * @brief Initialize the ENV III Unit with the default configuration.
   *
   * Equivalent to filling a config via unit_env_iii_get_default_config() and
   * passing it to unit_env_iii_init_with_config(). Registers both I2C
   * devices, resets and configures the SHT30, and reads the QMP6988
   * calibration coefficients. Calling again while already initialized
   * returns ESP_OK without re-initializing.
   *
   * @return
   *  - ESP_OK    : Success (or already initialized)
   *  - Other     : Error propagated from PA Hub / I2C device add / sensor
   *                init (e.g. ESP_ERR_NOT_FOUND, ESP_ERR_INVALID_RESPONSE)
   */
  esp_err_t unit_env_iii_init( void );

  /**
   * @brief Initialize the ENV III Unit with a custom configuration.
   *
   * @param[in] config Configuration to apply. Must not be NULL.
   * @return
   *  - ESP_OK              : Success (or already initialized)
   *  - ESP_ERR_INVALID_ARG : config is NULL
   *  - Other               : Error propagated from PA Hub / I2C device add /
   *                          sensor init
   */
  esp_err_t
  unit_env_iii_init_with_config( const unit_env_iii_config_t *config );

  /**
   * @brief Deinitialize the ENV III Unit.
   *
   * Marks the driver uninitialized so subsequent reads are rejected.
   *
   * @return
   *  - ESP_OK                : Success
   *  - ESP_ERR_INVALID_STATE : Not initialized
   */
  esp_err_t unit_env_iii_deinit( void );

  /**
   * @brief Read all sensors (temperature, humidity and pressure) at once.
   *
   * Each sub-sensor is read independently; inspect data->temp_hum_valid and
   * data->pressure_valid to know which fields are populated. Succeeds when
   * at least one sensor responds.
   *
   * @param[out] data Destination for the measurement set. Must not be NULL.
   * @return
   *  - ESP_OK                : At least one sensor produced valid data
   *  - ESP_ERR_INVALID_ARG   : data is NULL
   *  - ESP_ERR_INVALID_STATE : Not initialized
   *  - ESP_FAIL              : Both sensors failed to read
   */
  esp_err_t unit_env_iii_read_data( unit_env_iii_data_t *data );

  /**
   * @brief Read temperature and/or humidity from the SHT30.
   *
   * Either output pointer may be NULL to skip storing that value; the
   * measurement is still performed. The on-wire CRC is validated.
   *
   * @param[out] temperature_c Temperature in degrees Celsius, or NULL.
   * @param[out] humidity_rh   Relative humidity in %%, or NULL.
   * @return
   *  - ESP_OK                : Success
   *  - ESP_ERR_INVALID_STATE : Not initialized
   *  - ESP_ERR_INVALID_ARG   : Invalid configured repeatability
   *  - ESP_ERR_INVALID_CRC   : Measurement failed CRC validation
   *  - Other                 : I2C read/write error
   */
  esp_err_t unit_env_iii_read_temp_humidity( float *temperature_c,
                                             float *humidity_rh );

  /**
   * @brief Read barometric pressure from the QMP6988.
   *
   * Performs temperature/pressure compensation using the calibration
   * coefficients loaded at init.
   *
   * @param[out] pressure_pa Pressure in Pascal. Must not be NULL.
   * @return
   *  - ESP_OK                : Success
   *  - ESP_ERR_INVALID_ARG   : pressure_pa is NULL
   *  - ESP_ERR_INVALID_STATE : Not initialized
   *  - Other                 : I2C read/write error
   */
  esp_err_t unit_env_iii_read_pressure( float *pressure_pa );

  /**
   * @brief Populate a configuration struct with the library defaults.
   *
   * Defaults: SHT30 high repeatability, heater off; QMP6988 16x temperature
   * and pressure oversampling with a filter coefficient of 16.
   *
   * @param[out] config Destination configuration. Must not be NULL.
   * @return
   *  - ESP_OK              : Success
   *  - ESP_ERR_INVALID_ARG : config is NULL
   */
  esp_err_t unit_env_iii_get_default_config( unit_env_iii_config_t *config );

  /**
   * @brief Enable or disable the SHT30 on-chip heater.
   *
   * The heater is used to evaporate condensation off the sensor; it is not
   * for measurement and raises the reported temperature while active.
   *
   * @param[in] enable true to enable the heater, false to disable.
   * @return
   *  - ESP_OK                : Success
   *  - ESP_ERR_INVALID_STATE : Not initialized
   *  - Other                 : I2C write error
   */
  esp_err_t unit_env_iii_set_sht30_heater( bool enable );

  /**
   * @brief Issue a soft reset to the SHT30.
   *
   * @return
   *  - ESP_OK                : Success
   *  - ESP_ERR_INVALID_STATE : Not initialized
   *  - Other                 : I2C write error
   */
  esp_err_t unit_env_iii_reset_sht30( void );

  /**
   * @brief Issue a soft reset to the QMP6988.
   *
   * @return
   *  - ESP_OK                : Success
   *  - ESP_ERR_INVALID_STATE : Not initialized
   *  - Other                 : I2C write error
   */
  esp_err_t unit_env_iii_reset_qmp6988( void );

  /**
   * @brief Verify both ENV III sensors are present and responding.
   *
   * Reads the SHT30 status register and validates the QMP6988 chip ID.
   *
   * @return
   *  - ESP_OK                : Both sensors responded correctly
   *  - ESP_ERR_INVALID_STATE : Not initialized
   *  - ESP_ERR_NOT_FOUND     : QMP6988 chip ID mismatch
   *  - Other                 : I2C read/write error
   */
  esp_err_t unit_env_iii_check_connection( void );

#ifdef __cplusplus
}
#endif

#endif