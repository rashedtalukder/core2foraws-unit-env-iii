/*!
 * @brief Library for the 4 relay unit by M5Stack
 * @copyright Copyright (c) 2022 by Rashed Talukder[https://rashedtalukder.com]
 *
 * @Links [4-Relay](https://docs.m5stack.com/en/unit/4relay)
 * @version  V0.0.1
 * @date  2022-11-03
 */
#ifndef _UNIT_ENV_III_H_
#define _UNIT_ENV_III_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include "core2foraws.h"

#define UNIT_ENV_III_ADDR      0X44

/** 
 * @brief Initialize the temperature/humidity and pressure sensors.
 * @param duration_to_wait The time to wait before taking the first reading and subsequent readings.
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
 * @brief Get temp/humidity measurement from the SHT3x sensor.
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
