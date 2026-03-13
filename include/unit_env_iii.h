#ifndef UNIT_ENV_III_H
#define UNIT_ENV_III_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// I2C addresses
#define UNIT_ENV_III_SHT30_ADDR   0x44
#define UNIT_ENV_III_QMP6988_ADDR 0x70

  // SHT30 measurement repeatability
  typedef enum
  {
    UNIT_ENV_III_SHT30_REPEATABILITY_HIGH = 0,
    UNIT_ENV_III_SHT30_REPEATABILITY_MEDIUM,
    UNIT_ENV_III_SHT30_REPEATABILITY_LOW
  } unit_env_iii_sht30_repeatability_t;

  // QMP6988 oversampling settings (register values per datasheet)
  // Value 0 = skip measurement, so valid oversampling starts at 1
  typedef enum
  {
    UNIT_ENV_III_QMP6988_OSR_1 = 1,
    UNIT_ENV_III_QMP6988_OSR_2 = 2,
    UNIT_ENV_III_QMP6988_OSR_4 = 3,
    UNIT_ENV_III_QMP6988_OSR_8 = 4,
    UNIT_ENV_III_QMP6988_OSR_16 = 5,
    UNIT_ENV_III_QMP6988_OSR_32 = 6,
    UNIT_ENV_III_QMP6988_OSR_64 = 7
  } unit_env_iii_qmp6988_osr_t;

  // QMP6988 IIR filter settings
  typedef enum
  {
    UNIT_ENV_III_QMP6988_FILTER_OFF = 0,
    UNIT_ENV_III_QMP6988_FILTER_2,
    UNIT_ENV_III_QMP6988_FILTER_4,
    UNIT_ENV_III_QMP6988_FILTER_8,
    UNIT_ENV_III_QMP6988_FILTER_16,
    UNIT_ENV_III_QMP6988_FILTER_32
  } unit_env_iii_qmp6988_filter_t;

  // Configuration structure
  typedef struct
  {
    // SHT30 configuration
    unit_env_iii_sht30_repeatability_t sht30_repeatability;
    bool sht30_heater_enable;

    // QMP6988 configuration
    unit_env_iii_qmp6988_osr_t qmp6988_temp_osr;
    unit_env_iii_qmp6988_osr_t qmp6988_press_osr;
    unit_env_iii_qmp6988_filter_t qmp6988_filter;
  } unit_env_iii_config_t;

  // Measurement data structure
  typedef struct
  {
    float temperature_c; // Temperature in Celsius
    float humidity_rh;   // Relative humidity in %
    float pressure_pa;   // Pressure in Pascal
    bool temp_hum_valid; // SHT30 data validity
    bool pressure_valid; // QMP6988 data validity
  } unit_env_iii_data_t;

  /**
   * @brief Initialize Unit ENV-III with default configuration
   *
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t unit_env_iii_init( void );

  /**
   * @brief Initialize Unit ENV-III with custom configuration
   *
   * @param config Configuration structure
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t
  unit_env_iii_init_with_config( const unit_env_iii_config_t *config );

  /**
   * @brief Deinitialize Unit ENV-III
   *
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t unit_env_iii_deinit( void );

  /**
   * @brief Read all sensor data
   *
   * @param data Pointer to data structure to fill
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t unit_env_iii_read_data( unit_env_iii_data_t *data );

  /**
   * @brief Read only temperature and humidity from SHT30
   *
   * @param temperature_c Pointer to temperature variable (can be NULL)
   * @param humidity_rh Pointer to humidity variable (can be NULL)
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t unit_env_iii_read_temp_humidity( float *temperature_c,
                                             float *humidity_rh );

  /**
   * @brief Read only pressure from QMP6988
   *
   * @param pressure_pa Pointer to pressure variable
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t unit_env_iii_read_pressure( float *pressure_pa );

  /**
   * @brief Get default configuration
   *
   * @param config Pointer to configuration structure to fill
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t unit_env_iii_get_default_config( unit_env_iii_config_t *config );

  /**
   * @brief Set SHT30 heater state
   *
   * @param enable true to enable heater, false to disable
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t unit_env_iii_set_sht30_heater( bool enable );

  /**
   * @brief Reset SHT30 sensor
   *
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t unit_env_iii_reset_sht30( void );

  /**
   * @brief Reset QMP6988 sensor
   *
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t unit_env_iii_reset_qmp6988( void );

  /**
   * @brief Check if Unit ENV-III is properly connected
   *
   * @return esp_err_t ESP_OK if both sensors respond
   */
  esp_err_t unit_env_iii_check_connection( void );

#ifdef __cplusplus
}
#endif

#endif