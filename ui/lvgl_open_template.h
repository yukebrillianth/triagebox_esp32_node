/* TEMPORARY stub — Editor normally generates this on export.
 * Provides multi-target check used by every *_gen.c. */
#ifndef LVGL_OPEN_TEMPLATE_H
#define LVGL_OPEN_TEMPLATE_H
#define LVGL_OPEN_TEMPLATE_TARGET_ALL 0
#define LVGL_OPEN_TEMPLATE_TARGET_TARGET1 1
#define LVGL_OPEN_TEMPLATE_CHECK_COMPILE_TARGET(target) 0
static inline int lvgl_open_template_check_target(int target)
{
    (void)target;
    return 1; /* always match; single 480x480 target */
}
#endif
