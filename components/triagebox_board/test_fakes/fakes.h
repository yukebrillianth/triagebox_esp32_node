/*
 * Host stand-ins for the ESP-IDF headers ui_board.c includes, so
 * ui_board_power_selftest.c can compile the real file instead of a copy of it.
 * Only what ui_board.c actually uses. Test-only -- never in a firmware build.
 */
#ifndef TB_TEST_FAKES_H
#define TB_TEST_FAKES_H

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

typedef int esp_err_t;
#define ESP_OK 0

/* --- esp_log.h --- */
#define ESP_LOGI(t, ...) do { (void)(t); printf("I: " __VA_ARGS__); printf("\n"); } while (0)
#define ESP_LOGW(t, ...) do { (void)(t); printf("W: " __VA_ARGS__); printf("\n"); } while (0)
#define ESP_LOGE(t, ...) do { (void)(t); printf("E: " __VA_ARGS__); printf("\n"); } while (0)
const char *esp_err_to_name(esp_err_t err);

/* --- freertos --- */
#define pdMS_TO_TICKS(ms) (ms)
void vTaskDelay(int ticks);

/* Single-threaded on the host, so the critical sections that protect the RX-task
 * copies in tb_ui_source.c are nothing to enter. Kept as no-ops rather than
 * #ifdef'd out of that file: the point is to test the shipped code. */
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))

/* --- driver/i2c_master.h --- */
typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
typedef enum { I2C_ADDR_BIT_LEN_7 = 0 } i2c_addr_bit_len_t;
typedef struct {
    i2c_addr_bit_len_t dev_addr_length;
    uint16_t device_address;
    uint32_t scl_speed_hz;
} i2c_device_config_t;
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,
                                   const i2c_device_config_t *cfg,
                                   i2c_master_dev_handle_t *out);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev, const uint8_t *w,
                              size_t wlen, int timeout_ms);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev, const uint8_t *w,
                                      size_t wlen, uint8_t *r, size_t rlen,
                                      int timeout_ms);

/* --- esp_io_expander.h --- */
typedef void *esp_io_expander_handle_t;
#define IO_EXPANDER_PIN_NUM_1 (1U << 1)
#define IO_EXPANDER_PIN_NUM_5 (1U << 5)
#define IO_EXPANDER_OUTPUT 0
esp_err_t esp_io_expander_set_dir(esp_io_expander_handle_t h, uint32_t mask, int dir);
esp_err_t esp_io_expander_set_level(esp_io_expander_handle_t h, uint32_t mask, uint8_t lvl);

/* --- bsp --- */
esp_io_expander_handle_t bsp_io_expander_init(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);
#define ESP_ERROR_CHECK(x) ((void)(x))

#endif /* TB_TEST_FAKES_H */
