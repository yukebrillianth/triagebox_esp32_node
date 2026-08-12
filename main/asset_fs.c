#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include "asset_fs.h"

/* SPIFFS per-seek latency x 50 tiny_ttf fonts x hundreds of seeks each during
 * stbtt_InitFont starves the watchdog. Slurp each file into PSRAM once and serve
 * all reads/seeks from RAM. */

#define ASSET_FS_LETTER   'A'
#define ASSET_FS_MAX_BLOBS 12

typedef struct {
    char path[64];
    uint8_t *data;
    size_t size;
} asset_blob_t;

typedef struct {
    const asset_blob_t *blob;
    size_t pos;
} asset_handle_t;

static const char *TAG = "asset_fs";
static asset_blob_t s_blobs[ASSET_FS_MAX_BLOBS];
static uint8_t s_blob_count;
static lv_fs_drv_t s_drv;

static const asset_blob_t *blob_get(const char *path)
{
    FILE *f;
    long size;
    asset_blob_t *slot;

    for (uint8_t i = 0; i < s_blob_count; i++) {
        if (strcmp(s_blobs[i].path, path) == 0) {
            return &s_blobs[i];
        }
    }
    if (s_blob_count >= ASSET_FS_MAX_BLOBS) {
        ESP_LOGE(TAG, "blob table full, cannot cache %s", path);
        return NULL;
    }

    f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "open failed: %s", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) <= 0) {
        fclose(f);
        ESP_LOGE(TAG, "size probe failed: %s", path);
        return NULL;
    }
    rewind(f);

    slot = &s_blobs[s_blob_count];
    slot->data = heap_caps_malloc((size_t)size, MALLOC_CAP_SPIRAM);
    if (slot->data == NULL) {
        fclose(f);
        ESP_LOGE(TAG, "PSRAM alloc failed (%ld bytes) for %s", size, path);
        return NULL;
    }
    if (fread(slot->data, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        heap_caps_free(slot->data);
        slot->data = NULL;
        ESP_LOGE(TAG, "read failed: %s", path);
        return NULL;
    }
    fclose(f);

    strncpy(slot->path, path, sizeof(slot->path) - 1);
    slot->path[sizeof(slot->path) - 1] = '\0';
    slot->size = (size_t)size;
    s_blob_count++;

    ESP_LOGI(TAG, "cached %s (%u bytes) in PSRAM", path, (unsigned)slot->size);
    return slot;
}

static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    const asset_blob_t *blob;
    asset_handle_t *h;

    LV_UNUSED(drv);
    if (mode != LV_FS_MODE_RD) {
        return NULL;
    }
    blob = blob_get(path);
    if (blob == NULL) {
        return NULL;
    }
    h = lv_malloc(sizeof(asset_handle_t));
    if (h == NULL) {
        return NULL;
    }
    h->blob = blob;
    h->pos = 0;
    return h;
}

static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    LV_UNUSED(drv);
    lv_free(file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    asset_handle_t *h = file_p;
    size_t left;

    LV_UNUSED(drv);
    left = h->blob->size - h->pos;
    if (btr > left) {
        btr = (uint32_t)left;
    }
    memcpy(buf, h->blob->data + h->pos, btr);
    h->pos += btr;
    *br = btr;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    asset_handle_t *h = file_p;
    size_t base;

    LV_UNUSED(drv);
    switch (whence) {
    case LV_FS_SEEK_SET: base = 0; break;
    case LV_FS_SEEK_CUR: base = h->pos; break;
    case LV_FS_SEEK_END: base = h->blob->size; break;
    default: return LV_FS_RES_INV_PARAM;
    }
    if (base + pos > h->blob->size) {
        return LV_FS_RES_INV_PARAM;
    }
    h->pos = base + pos;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    asset_handle_t *h = file_p;

    LV_UNUSED(drv);
    *pos_p = (uint32_t)h->pos;
    return LV_FS_RES_OK;
}

void asset_fs_init(void)
{
    lv_fs_drv_init(&s_drv);
    s_drv.letter = ASSET_FS_LETTER;
    s_drv.cache_size = 0;
    s_drv.open_cb = fs_open;
    s_drv.close_cb = fs_close;
    s_drv.read_cb = fs_read;
    s_drv.seek_cb = fs_seek;
    s_drv.tell_cb = fs_tell;
    lv_fs_drv_register(&s_drv);
}
