#include <string.h>
#include <esp_log.h>
#include "unit_env_iii.h"
#include "sht3x.h"

#define REPEATABILITY_MODE SHT3X_HIGH

#define QMP6988_SLAVE_ADDRESS_L (0x70)
#define QMP6988_SLAVE_ADDRESS_H (0x56)

#define QMP6988_U16_t unsigned short
#define QMP6988_S16_t short
#define QMP6988_U32_t unsigned int
#define QMP6988_S32_t int
#define QMP6988_U64_t unsigned long long
#define QMP6988_S64_t long long

#define QMP6988_CHIP_ID 0x5C

#define QMP6988_CHIP_ID_REG     0xD1
#define QMP6988_RESET_REG       0xE0 /* Device reset register */
#define QMP6988_DEVICE_STAT_REG 0xF3 /* Device state register */
#define QMP6988_CTRLMEAS_REG    0xF4 /* Measurement Condition Control Register */
/* data */
#define QMP6988_PRESSURE_MSB_REG    0xF7 /* Pressure MSB Register */
#define QMP6988_TEMPERATURE_MSB_REG 0xFA /* Temperature MSB Reg */

/* compensation calculation */
#define QMP6988_CALIBRATION_DATA_START \
    0xA0 /* QMP6988 compensation coefficients */
#define QMP6988_CALIBRATION_DATA_LENGTH 25

#define SHIFT_RIGHT_4_POSITION 4
#define SHIFT_LEFT_2_POSITION  2
#define SHIFT_LEFT_4_POSITION  4
#define SHIFT_LEFT_5_POSITION  5
#define SHIFT_LEFT_8_POSITION  8
#define SHIFT_LEFT_12_POSITION 12
#define SHIFT_LEFT_16_POSITION 16

/* power mode */
#define QMP6988_SLEEP_MODE  0x00
#define QMP6988_FORCED_MODE 0x01
#define QMP6988_NORMAL_MODE 0x03

#define QMP6988_CTRLMEAS_REG_MODE__POS 0
#define QMP6988_CTRLMEAS_REG_MODE__MSK 0x03
#define QMP6988_CTRLMEAS_REG_MODE__LEN 2

/* oversampling */
#define QMP6988_OVERSAMPLING_SKIPPED 0x00
#define QMP6988_OVERSAMPLING_1X      0x01
#define QMP6988_OVERSAMPLING_2X      0x02
#define QMP6988_OVERSAMPLING_4X      0x03
#define QMP6988_OVERSAMPLING_8X      0x04
#define QMP6988_OVERSAMPLING_16X     0x05
#define QMP6988_OVERSAMPLING_32X     0x06
#define QMP6988_OVERSAMPLING_64X     0x07

#define QMP6988_CTRLMEAS_REG_OSRST__POS 5
#define QMP6988_CTRLMEAS_REG_OSRST__MSK 0xE0
#define QMP6988_CTRLMEAS_REG_OSRST__LEN 3

#define QMP6988_CTRLMEAS_REG_OSRSP__POS 2
#define QMP6988_CTRLMEAS_REG_OSRSP__MSK 0x1C
#define QMP6988_CTRLMEAS_REG_OSRSP__LEN 3

/* filter */
#define QMP6988_FILTERCOEFF_OFF 0x00
#define QMP6988_FILTERCOEFF_2   0x01
#define QMP6988_FILTERCOEFF_4   0x02
#define QMP6988_FILTERCOEFF_8   0x03
#define QMP6988_FILTERCOEFF_16  0x04
#define QMP6988_FILTERCOEFF_32  0x05

#define QMP6988_CONFIG_REG             0xF1 /*IIR filter co-efficient setting Register*/
#define QMP6988_CONFIG_REG_FILTER__POS 0
#define QMP6988_CONFIG_REG_FILTER__MSK 0x07
#define QMP6988_CONFIG_REG_FILTER__LEN 3

#define SUBTRACTOR 8388608

typedef struct _qmp6988_cali_data {
    QMP6988_S32_t COE_a0;
    QMP6988_S16_t COE_a1;
    QMP6988_S16_t COE_a2;
    QMP6988_S32_t COE_b00;
    QMP6988_S16_t COE_bt1;
    QMP6988_S16_t COE_bt2;
    QMP6988_S16_t COE_bp1;
    QMP6988_S16_t COE_b11;
    QMP6988_S16_t COE_bp2;
    QMP6988_S16_t COE_b12;
    QMP6988_S16_t COE_b21;
    QMP6988_S16_t COE_bp3;
} qmp6988_cali_data_t;

typedef struct _qmp6988_fk_data {
    float a0, b00;
    float a1, a2, bt1, bt2, bp1, b11, bp2, b12, b21, bp3;
} qmp6988_fk_data_t;

typedef struct _qmp6988_ik_data {
    QMP6988_S32_t a0, b00;
    QMP6988_S32_t a1, a2;
    QMP6988_S64_t bt1, bt2, bp1, b11, bp2, b12, b21, bp3;
} qmp6988_ik_data_t;

typedef struct _qmp6988_data {
    uint8_t slave;
    uint8_t chip_id;
    uint8_t power_mode;
    float temperature;
    float pressure;
    float altitude;
    qmp6988_cali_data_t qmp6988_cali;
    qmp6988_ik_data_t ik;
} qmp6988_data_t;

static esp_err_t _unit_enviii_qmp6988_get( float *pressure, float *temperature );
static sht3x_t _dev;
static const char *_TAG = "UNIT_ENV_III";

esp_err_t unit_enviii_init( uint8_t *duration_to_wait )
{
    esp_err_t err = ESP_OK;

    memset( &_dev, 0, sizeof( sht3x_t ) );

    err |= sht3x_init_desc( &_dev, COMMON_I2C_EXTERNAL, SHT3X_I2C_ADDR_GND, PORT_A_SDA_PIN, PORT_A_SCL_PIN );
    ESP_LOGI( _TAG, "Init_DESC %d", err );
    err |= sht3x_init( &_dev );
    ESP_LOGI( _TAG, "Init %d", err);
    err |= sht3x_start_measurement( &_dev, SHT3X_PERIODIC_1MPS, REPEATABILITY_MODE );
    ESP_LOGI( _TAG, "Start measure %d", err);
    unit_enviii_duration_get( duration_to_wait );

    return err;

}

esp_err_t unit_enviii_duration_get( uint8_t *duration )
{
    esp_err_t ret = ESP_OK;

    *duration = sht3x_get_measurement_duration( REPEATABILITY_MODE );

    return ret;
}

esp_err_t unit_enviii_temp_humidity_get( float *temperature, float *humidity )
{
    return sht3x_get_results( &_dev, temperature, humidity );
}

esp_err_t unit_enviii_pressure_get( float *pressure )
{
    float *temp, zero_temp = 0.00;
    temp = &zero_temp;
    return _unit_enviii_qmp6988_get( pressure, temp );
}

esp_err_t unit_enviii_altitude_get( float *altitude )
{
    return ESP_OK;
}

static esp_err_t unit_enviii_qmp6988_validate()
{
    esp_err_t err = ESP_OK;

    return err;
}

static esp_err_t _unit_enviii_qmp6988_get( float *pressure, float *temperature )
{
    return ESP_OK;
}