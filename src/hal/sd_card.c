#include "sd_card.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"

static const char *TAG = "sd_card";

#define MOUNT_POINT "/sdcard"
#define SPI_MOSI_IO   GPIO_NUM_11
#define SPI_MISO_IO   GPIO_NUM_13
#define SPI_SCLK_IO   GPIO_NUM_12
#define SPI_CS_IO     GPIO_NUM_14

static sdmmc_card_t *card = NULL;
static bool mounted = false;

esp_err_t sd_card_init(void)
{
    ESP_LOGI(TAG, "初始化 SD 卡");
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };
    
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SPI_MOSI_IO,
        .miso_io_num = SPI_MISO_IO,
        .sclk_io_num = SPI_SCLK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    esp_err_t err = spi_bus_initialize(SPI3_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI 总线初始化失败");
        return err;
    }
    
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;
    
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SPI_CS_IO;
    slot_config.host_id = SPI3_HOST;
    
    err = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD 卡挂载失败");
        spi_bus_free(SPI3_HOST);
        return err;
    }
    
    mounted = true;
    
    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "SD 卡初始化完成");
    
    return ESP_OK;
}

bool sd_card_is_mounted(void)
{
    return mounted;
}

const char* sd_card_get_mount_point(void)
{
    return MOUNT_POINT;
}

void sd_card_deinit(void)
{
    if (mounted) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        spi_bus_free(SPI3_HOST);
        mounted = false;
        card = NULL;
        ESP_LOGI(TAG, "SD 卡已卸载");
    }
}
