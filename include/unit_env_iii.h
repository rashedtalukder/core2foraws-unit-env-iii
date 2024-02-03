/*!
 * @brief Library for the ENV III (SHT30+QMP6988) unit by M5Stack used on the
 * Core2 for AWS IoT Kit
 * @copyright Copyright (c) 2023 by Rashed Talukder[https://rashedtalukder.com]
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
 * @todo Add support for QMP6988 pressure sensor
 * 
 * @Links [ENV III](https://docs.m5stack.com/en/unit/envIII)
 * @version  V0.0.1
 * @date  2023-04-03
 */

#ifndef _UNIT_ENV_III_H_
#define _UNIT_ENV_III_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include "core2foraws.h"

/** 
 * @brief Initialize the temperature/humidity and pressure sensors.
 * @param duration_to_wait The ticks to wait before taking the first reading and subsequent readings.
 * @return [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
 *  - ESP_OK                : Success
 *  - ESP_ERR_INVALID_ARG	: Driver parameter error
 */
esp_err_t unit_enviii_init( uint8_t *duration_to_wait );

/** 
 * @brief Duration to wait between sensor readings.
 * @param duration The time in RTOS ticks between sensor readings.
 * @return [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
 *  - ESP_OK                : Success
 */
esp_err_t unit_enviii_duration_get( uint8_t *duration );

/** 
 * @brief Duration to wait between sensor readings.
 * @param duration The time in RTOS ticks between sensor readings.
 * @return [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
 *  - ESP_OK                : Success
 */
esp_err_t unit_enviii_duration_get( uint8_t *duration );

/** 
 * @brief Take a single measurement of the temperature and humidity using the SHT330 sensor. 
 * Must wait at least for duration ticks before retrieving the data.
 * @return [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
 *  - ESP_OK                : Success
 */
esp_err_t unit_enviii_temp_humidity_measure( void );

/**
 * @brief Get the stored temp/humidity measurement from the SHT3x sensor.
 *
 * @param temperature Temperature in degree Celsius
 * @param humidity    Humidity in percent
 * @return            `ESP_OK` on success
 */
esp_err_t unit_enviii_temp_humidity_get( float *temperature, float *humidity );

/**
 * @brief Get the pressure measurement from the QMP6988 sensor.
 *
 * @param pressure Pressure in bar
 * @return            `ESP_OK` on success
 */
esp_err_t unit_enviii_pressure_get( float *pressure );

/**
 * @brief Get the calculated altitude by reading the pressure and temperature.
 *
 * @param altitude The calculated altitude.
 * @return            `ESP_OK` on success
 */
esp_err_t unit_enviii_altitude_get( float *altitude );

#ifdef __cplusplus
}
#endif
#endif
