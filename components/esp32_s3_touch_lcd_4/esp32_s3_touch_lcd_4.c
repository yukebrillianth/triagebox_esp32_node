#include <stdio.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_rom_sys.h" /* esp_rom_delay_us() for the I2C bus recovery below */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "esp_lcd_st7701.h"
#include "esp_lcd_touch_gt911.h"

#include "bsp/display.h"
#include "bsp/touch.h"
#include "bsp/esp32_s3_touch_lcd_4.h"
#include "bsp_err_check.h"
#include "bsp/display.h"
#include "bsp_err_check.h"

static const char *TAG = "ESP32-S3-Touch-LCD-4";

static i2c_master_bus_handle_t i2c_handle = NULL;  // I2C Handle
static bool i2c_initialized = false;
static esp_io_expander_handle_t io_expander = NULL; // IO expander tca9554 handle

static lv_display_t *disp;
static lv_indev_t *disp_indev = NULL;
sdmmc_card_t *bsp_sdcard = NULL;    // Global uSD card handler
static esp_lcd_touch_handle_t tp = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;           // LCD panel handle


static const st7701_lcd_init_cmd_t lcd_init_cmds[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x3B, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x21, 0x08}, 2, 0},
    {0xCD, (uint8_t[]){0x08}, 1, 0},
    {0xB0, (uint8_t[]){0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08, 0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18}, 16, 0},
    {0xB1, (uint8_t[]){0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08, 0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x60}, 1, 0},
    {0xB1, (uint8_t[]){0x30}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x49}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 20},
    {0xE0, (uint8_t[]){0x00, 0x1B, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x44, 0x44}, 11, 0},
    {0xE2, (uint8_t[]){0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00, 0xEC, 0xA0, 0x00, 0x00}, 12, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0, 0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0, 0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40}, 7, 0},
    {0xEC, (uint8_t[]){0x3C, 0x00}, 2, 0},
    {0xED, (uint8_t[]){0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x36, (uint8_t[]){0x00}, 1, 0},
    {0x3A, (uint8_t[]){0x66}, 1, 0},
    {0x21, (uint8_t[]){0x00}, 0, 120},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};

/**************************************************************************************************
 *
 * I2C Function
 *
 **************************************************************************************************/
/*
 * Free a slave that is holding SDA low, BEFORE the peripheral claims the pins.
 *
 * Why this has to exist at all: the ESP32 resets far more often than the STM32
 * does -- every flash, every RTS pulse, every panic -- and a soft reset does not
 * power-cycle the STM32 or the GT911. A slave that was mid-byte when we vanished
 * is still mid-byte, still driving SDA low, waiting for clocks that never came.
 * From then on the master cannot even issue a valid START.
 *
 * Why that is fatal rather than an error: esp_lcd_panel_io_i2c passes -1 as the
 * transaction timeout (esp_lcd/i2c/esp_lcd_panel_io_i2c.c:145 in IDF v6.0.2), so
 * the very first GT911 config read blocks FOREVER. No error, no log, no
 * watchdog -- boot simply stops between "I2C address initialization procedure
 * skipped" and "TouchPad_ID:", which is exactly the reported symptom. Nothing
 * downstream can recover from it, so it has to be cleared here.
 *
 * The sequence is the standard one: with SDA released, pulse SCL until the slave
 * finishes the byte it thinks it is transmitting and lets go, then issue a STOP
 * so it returns to idle rather than staying addressed. 9 pulses is one byte plus
 * the ACK, which is the most any slave can still owe us.
 *
 * Open-drain throughout: this bus has other masters' worth of devices on it and
 * a hard-driven high would fight whatever else is on the line.
 */
#define I2C_RECOVER_PULSES 9
#define I2C_RECOVER_HALF_US 5 /* ~100 kHz, the speed every device here agrees on */

static void bsp_i2c_bus_recover(void)
{
    const gpio_config_t od_conf = {
        .pin_bit_mask = BIT64(BSP_I2C_SDA) | BIT64(BSP_I2C_SCL),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    int pulses = 0;

    if (gpio_config(&od_conf) != ESP_OK) {
        return;
    }
    gpio_set_level(BSP_I2C_SDA, 1);
    gpio_set_level(BSP_I2C_SCL, 1);
    esp_rom_delay_us(I2C_RECOVER_HALF_US);

    /*
     * Both lines matter, and they fail differently.
     *
     * SDA low = a slave is mid-byte and can be clocked out of it, which is what
     * the pulses below do.
     *
     * SCL low = a slave is CLOCK STRETCHING and there is nothing a master can do
     * about it: it cannot drive a line another device is holding down. That is
     * the STM32 case -- its I2C slave stretches SCL while its superloop gets
     * round to the transfer, and if we reset in that window it stretches
     * forever, waiting for clocks from a master that no longer exists. Only
     * power-cycling the STM32 (or resetting its I2C peripheral from its own
     * firmware) clears it, so all this can do is name it.
     */
    if (gpio_get_level(BSP_I2C_SCL) == 0) {
        ESP_LOGE(TAG, "I2C SCL held LOW -- a slave is clock-stretching (the "
                      "STM32 at 0x42 does this if the ESP32 reset mid-transfer). "
                      "A master cannot clear this: power-cycle the board. Boot "
                      "would otherwise hang in the GT911 read.");
        return;
    }

    if (gpio_get_level(BSP_I2C_SDA) != 0) {
        return; /* Bus idle. Nothing to report -- this is the normal path. */
    }

    while (pulses < I2C_RECOVER_PULSES && gpio_get_level(BSP_I2C_SDA) == 0) {
        gpio_set_level(BSP_I2C_SCL, 0);
        esp_rom_delay_us(I2C_RECOVER_HALF_US);
        gpio_set_level(BSP_I2C_SCL, 1);
        esp_rom_delay_us(I2C_RECOVER_HALF_US);
        pulses++;
    }

    /* STOP: SDA low while SCL is high, then release SDA. */
    gpio_set_level(BSP_I2C_SDA, 0);
    esp_rom_delay_us(I2C_RECOVER_HALF_US);
    gpio_set_level(BSP_I2C_SCL, 1);
    esp_rom_delay_us(I2C_RECOVER_HALF_US);
    gpio_set_level(BSP_I2C_SDA, 1);
    esp_rom_delay_us(I2C_RECOVER_HALF_US);

    if (gpio_get_level(BSP_I2C_SDA) == 0) {
        /* Still held: not a mid-byte slave, so clocking it will not help. Either
         * something is driving SDA continuously (a second master on the shared
         * header, or a dead device) or the line is shorted. Logged rather than
         * retried, because the boot that follows will hang and this line is the
         * only thing that will say why. */
        ESP_LOGE(TAG, "I2C SDA still low after %d recovery pulses -- boot will "
                      "hang in the GT911 read; power-cycle the board", pulses);
    } else {
        ESP_LOGW(TAG, "I2C bus was wedged (SDA low); freed after %d pulses",
                 pulses);
    }
}

esp_err_t bsp_i2c_init(void)
{
    /* I2C was initialized before */
    if (i2c_initialized) {
        return ESP_OK;
    }

    /* Before i2c_new_master_bus() takes the pins: see bsp_i2c_bus_recover(). */
    bsp_i2c_bus_recover();

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .i2c_port = BSP_I2C_NUM,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_bus_conf, &i2c_handle));

    i2c_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_handle));
    i2c_initialized = false;
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    bsp_i2c_init();
    return i2c_handle;
}

esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,
#ifdef CONFIG_BSP_SPIFFS_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

    BSP_ERROR_CHECK_RETURN_ERR(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret_val;
}

esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}

esp_err_t bsp_sdcard_mount(void)
{
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    const sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    const sdmmc_slot_config_t slot_config = {
        .clk = BSP_SD_CLK,
        .cmd = BSP_SD_CMD,
        .d0 = BSP_SD_D0,
        .d1 = GPIO_NUM_NC,
        .d2 = GPIO_NUM_NC,
        .d3 = GPIO_NUM_NC,
        .d4 = GPIO_NUM_NC,
        .d5 = GPIO_NUM_NC,
        .d6 = GPIO_NUM_NC,
        .d7 = GPIO_NUM_NC,
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 1,
        .flags = 0,
    };

#if !CONFIG_FATFS_LONG_FILENAMES
    ESP_LOGW(TAG, "Warning: Long filenames on SD card are disabled in menuconfig!");
#endif

    return esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, &host, &slot_config, &mount_config, &bsp_sdcard);
}

esp_err_t bsp_sdcard_unmount(void)
{
    return esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, bsp_sdcard);
}

#define LCD_LEDC_CH            CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH

esp_err_t bsp_display_brightness_init(void)
{
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&LCD_backlight_channel));

    return ESP_OK;
}


esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    } else if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    // LEDC resolution set to 10bits, thus: 100% = 1023
    uint32_t duty_cycle = (1023 * brightness_percent) / 100;
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));

    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_lcd_panel_io_handle_t io_handle = NULL;




    ESP_LOGI(TAG, "Install 3-wire SPI panel IO");
    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_GPIO,
        .cs_gpio_num = BSP_LCD_IO_SPI_CS,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = BSP_LCD_IO_SPI_SCL,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = BSP_LCD_IO_SPI_SDA,
        .io_expander = NULL,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

    esp_lcd_rgb_panel_config_t rgb_config = {
           .clk_src = LCD_CLK_SRC_DEFAULT,
           .data_width = BSP_RGB_DATA_WIDTH,
           .de_gpio_num = BSP_LCD_DE,
           .pclk_gpio_num = BSP_LCD_PCLK,
           .vsync_gpio_num = BSP_LCD_VSYNC,
           .hsync_gpio_num = BSP_LCD_HSYNC,
           .disp_gpio_num = BSP_LCD_DISP,
           .data_gpio_nums = {
               BSP_LCD_DATA0,
               BSP_LCD_DATA1,
               BSP_LCD_DATA2,
               BSP_LCD_DATA3,
               BSP_LCD_DATA4,
               BSP_LCD_DATA5,
               BSP_LCD_DATA6,
               BSP_LCD_DATA7,
               BSP_LCD_DATA8,
               BSP_LCD_DATA9,
               BSP_LCD_DATA10,
               BSP_LCD_DATA11,
               BSP_LCD_DATA12,
               BSP_LCD_DATA13,
               BSP_LCD_DATA14,
               BSP_LCD_DATA15,
           },
           .timings = ST7701_480_480_PANEL_60HZ_RGB_TIMING(),
           .flags.fb_in_psram = 1,
           .num_fbs = CONFIG_BSP_LCD_RGB_BUFFER_NUMS,
           /* Keep the bounce buffer: without it the RGB DMA reads the PSRAM
            * framebuffer directly and underruns whenever the CPU repaints,
            * which shifts the image permanently and cumulatively. */
           .bounce_buffer_size_px = BSP_LCD_DRAW_BUFF_SIZE,
    };
    rgb_config.timings.h_res = BSP_LCD_H_RES;
    rgb_config.timings.v_res = BSP_LCD_V_RES;
    st7701_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        .init_cmds = lcd_init_cmds,      // Uncomment these line if use custom initialization commands
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .auto_del_panel_io = 0,
            .mirror_by_cmd = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(io_handle, &panel_config, &panel_handle));
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    if (ret_panel) {
        *ret_panel = panel_handle;
    }
    if (ret_io) {
        *ret_io = io_handle;
    }

    return ESP_OK;
}


esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    /* Initilize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    /* Initialize touch */
    const esp_lcd_touch_config_t tp_cfg = {
        /* esp_lcd_touch mirrors as `x_max - x`, so *_max must be the last valid
         * pixel or a touch at 0 maps to 480 (out of range, LVGL drops it). */
        .x_max = BSP_LCD_H_RES - 1,
        .y_max = BSP_LCD_V_RES - 1,
        .rst_gpio_num = BSP_LCD_TOUCH_RST, // Shared with LCD reset
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            /* Match the 180 panel flip above; esp_lvgl_port does not transform
             * touch coordinates, so the touch driver has to do it. */
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = {
    .dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
    .control_phase_bytes = 1,
    .dc_bit_offset = 0,
    .lcd_cmd_bits = 16,
    .flags = {
        .disable_control_phase = 1,
    }
};
    tp_io_config.scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch);
}


/**************************************************************************************************
 *
 * IO Expander Function
 *
 **************************************************************************************************/
esp_io_expander_handle_t bsp_io_expander_init(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());
    if (!io_expander) {
        BSP_ERROR_CHECK_RETURN_NULL(esp_io_expander_new_i2c_tca9554(i2c_handle, BSP_IO_EXPANDER_I2C_ADDRESS, &io_expander));
    }
    return io_expander;
}


static lv_display_t *bsp_display_lcd_init()
{
    esp_lcd_panel_io_handle_t io_handle = NULL;

    bsp_display_config_t disp_config = { 0 };

    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new(&disp_config, &panel_handle, &io_handle));

    int buffer_size = 0;
#if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
    buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES;
#else
    buffer_size = BSP_LCD_H_RES * LVGL_BUFFER_HEIGHT;
#endif /* CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR */

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = buffer_size,

        .monochrome = false,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif

        /* Board is mounted upside down in the TriageBox enclosure: rotate 180
         * by flipping both axes. Done in the panel (vendor_config.mirror_by_cmd
         * = 1 -> ST7701 SDIR/MADCTL) so it costs no CPU and survives the
         * double-framebuffer flip. sw_rotate must stay false or
         * lvgl_port_disp_rotation_update() skips the panel commands. */
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = true,
        },
        .flags = {
            .sw_rotate = false,
            .buff_dma = false,
#if CONFIG_BSP_DISPLAY_LVGL_PSRAM
            .buff_spiram = false,
#endif
#if CONFIG_BSP_DISPLAY_LVGL_FULL_REFRESH
            .full_refresh = 1,
#elif CONFIG_BSP_DISPLAY_LVGL_DIRECT_MODE
            .direct_mode = 1,
#endif
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = false,
#endif
        }
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
#if CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_MODE
            .bb_mode = 1,
#else
            .bb_mode = 0,
#endif
#if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
            .avoid_tearing = true,
#else
            .avoid_tearing = false,
#endif
        }
    };

#if CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_MODE
    ESP_LOGW(TAG, "CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_MODE");
#endif

    return lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
}


/*
 * Fault-tolerant replacement for esp_lvgl_port's touch read.
 *
 * The port's own callback wraps esp_lcd_touch_read_data() in ESP_ERROR_CHECK
 * (esp_lvgl_port_touch.c:127), so ONE failed GT911 transaction calls abort().
 * That is the confirmed cause of the "screen goes black, backlight stays lit,
 * kinda reset" reports: with PANIC_PRINT_REBOOT at 0 s the SoC restarts
 * instantly while the backlight, driven by the TCA9554, does not reset with it.
 * Captured at ~56 min uptime as
 *   i2c.master: I2C transaction timeout detected
 *   panel_io_i2c_rx_buffer(145) -> GT911 read error -> 0x108 -> abort()
 * on a bus shared by five devices (TCA9554, SW6106, PCF85063A, GT911 and the
 * STM32 polled every 50 ms) with no cross-component lock, so the occasional
 * timeout is expected. Dropping one touch sample is the right trade; rebooting
 * mid-measurement on a triage device is not.
 *
 * The last state is held rather than forced to RELEASED: a synthetic release in
 * the middle of a real press is a phantom click, and on these screens a click
 * starts or aborts a measurement. Errors are logged (WARN survives the default
 * level) so the bus glitch stays visible now that it no longer panics.
 */
static void touch_read_tolerant(lv_indev_t *indev, lv_indev_data_t *data)
{
    static lv_point_t last_point;
    static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
    static uint32_t errors;

    uint8_t cnt = 0;
    esp_lcd_touch_point_data_t pts[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {0};
    esp_err_t err;

    (void)indev;

    err = esp_lcd_touch_read_data(tp);
    if (err == ESP_OK) {
        err = esp_lcd_touch_get_data(tp, pts, &cnt, CONFIG_ESP_LCD_TOUCH_MAX_POINTS);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "touch read failed (%s), holding last state, errors=%u",
                 esp_err_to_name(err), (unsigned)++errors);
        data->point = last_point;
        data->state = last_state;
        return;
    }

    if (cnt > 0) {
        data->point.x = pts[0].x;
        data->point.y = pts[0].y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    last_point = data->point;
    last_state = data->state;
}

static lv_indev_t *bsp_display_indev_init(lv_display_t *disp)
{
    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(NULL, &tp));
    assert(tp);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = tp,
    };

    lv_indev_t *indev = lvgl_port_add_touch(&touch_cfg);
    if (indev) {
        /* Scale stays 1:1 here, so the port's callback adds nothing we need.
         * See touch_read_tolerant() for why it must not stay installed. */
        lv_indev_set_read_cb(indev, touch_read_tolerant);
    }

    return indev;
}


/**********************************************************************************************************
 *
 * Display Function
 *
 **********************************************************************************************************/
lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG()
    };

    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_init(&cfg->lvgl_port_cfg)); /* lvgl task, tick etc*/

    BSP_NULL_CHECK(disp = bsp_display_lcd_init(), NULL);

    BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(disp), NULL);

    return disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

void bsp_display_rotate(lv_display_t *disp, lv_display_rotation_t rotation)
{
    lv_disp_set_rotation(disp, rotation);
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}
