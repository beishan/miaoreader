#include "epd.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "epd";

#define EPD_SPI_HOST SPI2_HOST
#define PIN_SPI_CS   GPIO_NUM_5
#define PIN_SPI_CLK  GPIO_NUM_6
#define PIN_SPI_MOSI GPIO_NUM_7
#define PIN_SPI_MISO GPIO_NUM_2
#define PIN_DC       GPIO_NUM_3
#define PIN_RST      GPIO_NUM_4
#define PIN_BUSY     GPIO_NUM_1

static uint8_t *framebuffer_bw = NULL;
static uint8_t *framebuffer_red = NULL;
static uint8_t current_rotation = 0;

static void epd_send_cmd(uint8_t cmd);
static void epd_send_data(uint8_t data);
static void epd_send_data_bulk(const uint8_t *data, int len);
static void epd_wait_busy(void);
static void epd_reset(void);

esp_err_t epd_init(void)
{
    ESP_LOGI(TAG, "初始化 E-Ink 驱动");
    
    spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EPD_BUF_SIZE,
    };
    
    esp_err_t err = spi_bus_initialize(EPD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI 总线初始化失败");
        return err;
    }
    
    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_SPI_CS,
        .queue_size = 7,
    };
    
    spi_device_handle_t spi;
    err = spi_bus_add_device(EPD_SPI_HOST, &dev_config, &spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI 设备添加失败");
        return err;
    }
    
    gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BUSY, GPIO_MODE_INPUT);
    
    framebuffer_bw = heap_caps_malloc(EPD_BUF_SIZE, MALLOC_CAP_DMA);
    framebuffer_red = heap_caps_malloc(EPD_BUF_SIZE, MALLOC_CAP_DMA);
    
    if (!framebuffer_bw || !framebuffer_red) {
        ESP_LOGE(TAG, "帧缓冲区分配失败");
        return ESP_ERR_NO_MEM;
    }
    
    memset(framebuffer_bw, 0xFF, EPD_BUF_SIZE);
    memset(framebuffer_red, 0x00, EPD_BUF_SIZE);
    
    epd_reset();
    
    epd_send_cmd(0x01);
    epd_send_data(0x07);
    epd_send_data(0x00);
    epd_send_data(0x0F);
    epd_send_data(0x00);
    
    epd_send_cmd(0x06);
    epd_send_data(0x07);
    epd_send_data(0x07);
    epd_send_data(0x07);
    
    epd_send_cmd(0x04);
    epd_wait_busy();
    
    epd_send_cmd(0x00);
    epd_send_data(0x0F);
    
    epd_send_cmd(0x50);
    epd_send_data(0x37);
    
    epd_send_cmd(0x61);
    epd_send_data(0x01);
    epd_send_data(0x90);
    epd_send_data(0x01);
    epd_send_data(0x2C);
    
    ESP_LOGI(TAG, "E-Ink 驱动初始化完成");
    return ESP_OK;
}

void epd_clear(EpdColor color)
{
    uint8_t bw_val = (color == EPD_COLOR_WHITE) ? 0xFF : 0x00;
    uint8_t red_val = (color == EPD_COLOR_RED) ? 0xFF : 0x00;
    
    memset(framebuffer_bw, bw_val, EPD_BUF_SIZE);
    memset(framebuffer_red, red_val, EPD_BUF_SIZE);
}

void epd_set_pixel(int x, int y, EpdColor color)
{
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) return;
    
    int rx = x, ry = y;
    switch (current_rotation) {
        case 1: rx = EPD_HEIGHT - 1 - y; ry = x; break;
        case 2: rx = EPD_WIDTH - 1 - x; ry = EPD_HEIGHT - 1 - y; break;
        case 3: rx = y; ry = EPD_WIDTH - 1 - x; break;
    }
    
    int byte_idx = (ry * EPD_WIDTH + rx) / 8;
    int bit_idx = 7 - (rx % 8);
    
    if (color == EPD_COLOR_RED) {
        framebuffer_red[byte_idx] |= (1 << bit_idx);
        framebuffer_bw[byte_idx] |= (1 << bit_idx);
    } else if (color == EPD_COLOR_BLACK) {
        framebuffer_red[byte_idx] &= ~(1 << bit_idx);
        framebuffer_bw[byte_idx] &= ~(1 << bit_idx);
    } else {
        framebuffer_red[byte_idx] &= ~(1 << bit_idx);
        framebuffer_bw[byte_idx] |= (1 << bit_idx);
    }
}

void epd_draw_rect(int x, int y, int w, int h, EpdColor color)
{
    for (int i = x; i < x + w; i++) {
        epd_set_pixel(i, y, color);
        epd_set_pixel(i, y + h - 1, color);
    }
    for (int j = y; j < y + h; j++) {
        epd_set_pixel(x, j, color);
        epd_set_pixel(x + w - 1, j, color);
    }
}

void epd_fill_rect(int x, int y, int w, int h, EpdColor color)
{
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            epd_set_pixel(i, j, color);
        }
    }
}

void epd_display_full(void)
{
    ESP_LOGI(TAG, "三色全刷 (~15s)");
    
    epd_send_cmd(0x10);
    epd_send_data_bulk(framebuffer_bw, EPD_BUF_SIZE);
    
    epd_send_cmd(0x13);
    epd_send_data_bulk(framebuffer_red, EPD_BUF_SIZE);
    
    epd_send_cmd(0x12);
    epd_wait_busy();
}

void epd_display_partial(void)
{
    ESP_LOGI(TAG, "黑白局部刷新 (~250ms)");
    
    epd_send_cmd(0x10);
    epd_send_data_bulk(framebuffer_bw, EPD_BUF_SIZE);
    
    epd_send_cmd(0x12);
    epd_wait_busy();
}

void epd_sleep(void)
{
    epd_send_cmd(0x02);
    epd_wait_busy();
    epd_send_cmd(0x07);
    epd_send_data(0xA5);
}

void epd_set_rotation(uint8_t rot)
{
    current_rotation = rot % 4;
}

static void epd_send_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_DC, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_transmit(NULL, &t);
}

static void epd_send_data(uint8_t data)
{
    gpio_set_level(PIN_DC, 1);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
    };
    spi_device_transmit(NULL, &t);
}

static void epd_send_data_bulk(const uint8_t *data, int len)
{
    gpio_set_level(PIN_DC, 1);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_transmit(NULL, &t);
}

static void epd_wait_busy(void)
{
    while (gpio_get_level(PIN_BUSY) == 1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void epd_reset(void)
{
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}
