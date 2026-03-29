/*
 * SPDX-License-Identifier: MIT
 *
 * Home screen: date/time labels (upper half) + peripheral battery arc gauges
 * (lower half) + icon buttons (delegated to home_buttons.c).
 */

#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <lvgl.h>
#include <zmk/split/central.h>
#include "page_iface.h"
#include "display_api.h"
#include "endpoint_status.h"
#include "battery_status.h"
#include "home_buttons.h"
#include "prospector_status.h"
#include <stdio.h>
#include <zmk/status_scanner.h>
/* ── RTC device ────────────────────────────────────────────────────────── */

static const struct device *s_rtc = DEVICE_DT_GET(DT_ALIAS(rtc));

/* ── Widget handles ────────────────────────────────────────────────────── */

static lv_obj_t   *s_date_lbl;
static lv_obj_t   *s_time_lbl;
static lv_timer_t *s_timer;
static lv_obj_t   *s_output_lbl;
static lv_obj_t *home_screen;
static lv_obj_t *status_label;
static lv_obj_t *layer_label;
static bool scanner_started = false;
static lv_obj_t *left_battery_label;
static lv_obj_t *right_battery_label;
static lv_obj_t *left_battery_arc;
static lv_obj_t *right_battery_arc;
/* ── Endpoint status callback ──────────────────────────────────────────── */

static void home_endpoint_cb(struct endpoint_state state)
{
	endpoint_status_update_label(s_output_lbl, state);
}

/* ── Month / weekday name tables ───────────────────────────────────────── */

static const char *month_names[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char *day_names[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

/* ── Timer callback ────────────────────────────────────────────────────── */

static void update_datetime(lv_timer_t *t)
{
	ARG_UNUSED(t);

	struct rtc_time time = {};

	if (rtc_get_time(s_rtc, &time) != 0) {
		return;
	}

	/* "Jan 01 Mon" */
	lv_label_set_text_fmt(s_date_lbl, "%s %02d %s",
			      month_names[time.tm_mon],
			      time.tm_mday,
			      day_names[time.tm_wday]);

	/* "23:59" */
	lv_label_set_text_fmt(s_time_lbl, "%02d:%02d",
			      time.tm_hour, time.tm_min);
}
static void update_home_labels(void) {
    static char status_buf[64];
    static char layer_buf[32];
	static char left_battery_buf[16];
	static char right_battery_buf[16];	

    if (prospector_status_has_data()) {
        snprintf(status_buf, sizeof(status_buf), "DEVICE: %s",prospector_status_get_keyboard_name());
        snprintf(layer_buf, sizeof(layer_buf), "LAYER: %s",prospector_status_get_layer_name());
		snprintf(left_battery_buf, sizeof(left_battery_buf), "L: %d%%",prospector_status_get_peripheral_battery(0));
		snprintf(right_battery_buf, sizeof(right_battery_buf), "R: %d%%",prospector_status_get_battery());
    } else {
        snprintf(status_buf, sizeof(status_buf), "WAITING FOR MONA2");
        snprintf(layer_buf, sizeof(layer_buf), "LAYER: ---");
		snprintf(left_battery_buf, sizeof(left_battery_buf), "L: ---");
		snprintf(right_battery_buf, sizeof(right_battery_buf), "R: ---");
    }

    lv_label_set_text(status_label, status_buf);
    lv_label_set_text(layer_label, layer_buf);
	lv_label_set_text(left_battery_label, left_battery_buf);
    lv_label_set_text(right_battery_label, right_battery_buf);
	if (prospector_status_has_data()) {
	    lv_arc_set_value(left_battery_arc, prospector_status_get_peripheral_battery(0));
	    lv_arc_set_value(right_battery_arc, prospector_status_get_battery());
	} else {
	    lv_arc_set_value(left_battery_arc, 0);
	    lv_arc_set_value(right_battery_arc, 0);
	}
}
static void home_timer_cb(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    update_home_labels();
}
/* ── Page create ───────────────────────────────────────────────────────── */
// static int page_home_create(lv_obj_t *tile)
// {
// 	/* ── Date label — upper area ────────────────────────────────────── */
// 	s_date_lbl = lv_label_create(tile);
// 	lv_label_set_text(s_date_lbl, "--- -- ---");
// 	lv_obj_align(s_date_lbl, LV_ALIGN_CENTER, 0, -67);
// 
// 	/* ── Time label ─────────────────────────────────────────────────── */
// 	s_time_lbl = lv_label_create(tile);
// 	lv_label_set_text(s_time_lbl, "--:--");
// 	lv_obj_set_style_text_font(s_time_lbl, &lv_font_montserrat_48, 0);
// 	lv_obj_align(s_time_lbl, LV_ALIGN_CENTER, 0, -27);
// 
// 	/* ── Output status label ────────────────────────────────────────── */
// 	s_output_lbl = create_output_status_label(tile, &lv_font_montserrat_16);
// 	lv_obj_align(s_output_lbl, LV_ALIGN_BOTTOM_MID, 0, -37);
// 
// 	/* ── Peripheral battery arc gauges — lower half ─────────────────── */
// 	lv_obj_t *periph_bat_arcs[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT];
// 	lv_obj_t *periph_bat_lbls[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT];
// 
// 	const int n_periph     = ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT;
// 	const int spacing      = 74;
// 	const int arc_sz       = 48;
// 	const int center_y_off = 34;
// 
// 	for (int i = 0; i < n_periph; i++) {
// 		/* X offset: centres the group symmetrically around x=0 */
// 		int x = (int)((i - (n_periph - 1) / 2.0f) * spacing);
// 
// 		/* Arc widget */
// 		lv_obj_t *arc = lv_arc_create(tile);
// 		lv_obj_set_size(arc, arc_sz, arc_sz);
// 		lv_arc_set_range(arc, 0, 100);
// 		lv_arc_set_value(arc, 0);
// 		lv_arc_set_rotation(arc, 270);    /* start sweep at 12 o'clock */
// 		lv_arc_set_bg_angles(arc, 0, 360); /* full circle background */
// 		lv_arc_set_angles(arc, 0, 0);     /* value arc starts empty */
// 
// 		/* Hide interactive knob */
// 		lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
// 		lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
// 
// 		/* Background arc: dim */
// 		lv_obj_set_style_arc_color(arc, lv_color_hex(0x333333), LV_PART_MAIN);
// 		lv_obj_set_style_arc_width(arc, 4, LV_PART_MAIN);
// 
// 		/* Indicator (value) arc: white */
// 		lv_obj_set_style_arc_color(arc, lv_color_white(), LV_PART_INDICATOR);
// 		lv_obj_set_style_arc_width(arc, 4, LV_PART_INDICATOR);
// 
// 		/* Transparent background */
// 		lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
// 
// 		lv_obj_align(arc, LV_ALIGN_CENTER, x, center_y_off);
// 
// 		/* Percentage label inside the arc */
// 		lv_obj_t *lbl = lv_label_create(tile);
// 		lv_label_set_text(lbl, "--");
// 		lv_obj_align(lbl, LV_ALIGN_CENTER, x, center_y_off);
// 
// 		periph_bat_arcs[i] = arc;
// 		periph_bat_lbls[i] = lbl;
// 	}
// 
// 	endpoint_status_register_cb(home_endpoint_cb);
// 	battery_status_init(periph_bat_arcs, periph_bat_lbls);
// 
// 	/* ── Button ring ─────────────────────────────────────────────────── */
// 	home_buttons_create(tile);
// 
// 	/* 1-second timer, created paused — resumed only while page is active */
// 	s_timer = lv_timer_create(update_datetime, 1000, NULL);
// 	lv_timer_pause(s_timer);
// 
// 	return 0;
// }


void page_home_create(lv_obj_t *tile) {
    home_screen = lv_obj_create(tile);
    lv_obj_set_size(home_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(home_screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(home_screen, LV_OPA_COVER, LV_PART_MAIN);

    status_label = lv_label_create(home_screen);
    lv_label_set_text(status_label, "WAITING FOR MONA2");
    lv_obj_set_style_text_color(status_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, -20);

    layer_label = lv_label_create(home_screen);
    lv_label_set_text(layer_label, "LAYER: ---");
    lv_obj_set_style_text_color(layer_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(layer_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(layer_label, LV_ALIGN_CENTER, 0, 20);

	left_battery_arc = lv_arc_create(home_screen);
	lv_obj_set_size(left_battery_arc, 56, 56);
	lv_arc_set_range(left_battery_arc, 0, 100);
	lv_arc_set_value(left_battery_arc, 0);
	lv_arc_set_rotation(left_battery_arc, 270);
	lv_arc_set_bg_angles(left_battery_arc, 0, 360);
	lv_obj_remove_style(left_battery_arc, NULL, LV_PART_KNOB);
	lv_obj_clear_flag(left_battery_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_color(left_battery_arc, lv_color_hex(0x333333), LV_PART_MAIN);
	lv_obj_set_style_arc_width(left_battery_arc, 4, LV_PART_MAIN);
	lv_obj_set_style_arc_color(left_battery_arc, lv_color_white(), LV_PART_INDICATOR);
	lv_obj_set_style_arc_width(left_battery_arc, 4, LV_PART_INDICATOR);
	lv_obj_set_style_bg_opa(left_battery_arc, LV_OPA_TRANSP, 0);
	lv_obj_align(left_battery_arc, LV_ALIGN_CENTER, -70, 65);
	
	right_battery_arc = lv_arc_create(home_screen);
	lv_obj_set_size(right_battery_arc, 56, 56);
	lv_arc_set_range(right_battery_arc, 0, 100);
	lv_arc_set_value(right_battery_arc, 0);
	lv_arc_set_rotation(right_battery_arc, 270);
	lv_arc_set_bg_angles(right_battery_arc, 0, 360);
	lv_obj_remove_style(right_battery_arc, NULL, LV_PART_KNOB);
	lv_obj_clear_flag(right_battery_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_color(right_battery_arc, lv_color_hex(0x333333), LV_PART_MAIN);
	lv_obj_set_style_arc_width(right_battery_arc, 4, LV_PART_MAIN);
	lv_obj_set_style_arc_color(right_battery_arc, lv_color_white(), LV_PART_INDICATOR);
	lv_obj_set_style_arc_width(right_battery_arc, 4, LV_PART_INDICATOR);
	lv_obj_set_style_bg_opa(right_battery_arc, LV_OPA_TRANSP, 0);
	lv_obj_align(right_battery_arc, LV_ALIGN_CENTER, 70, 65);
	
	left_battery_label = lv_label_create(home_screen);
	lv_label_set_text(left_battery_label, "L: ---");
	lv_obj_set_style_text_color(left_battery_label, lv_color_white(), LV_PART_MAIN);
	lv_obj_align(left_battery_label, LV_ALIGN_CENTER, -40, 65);
	
	right_battery_label = lv_label_create(home_screen);
	lv_label_set_text(right_battery_label, "R: ---");
	lv_obj_set_style_text_color(right_battery_label, lv_color_white(), LV_PART_MAIN);
	lv_obj_align(right_battery_label, LV_ALIGN_CENTER, 40, 65);
	
    update_home_labels();
    lv_timer_create(home_timer_cb, 500, NULL);
    lv_scr_load(home_screen);

    if (!scanner_started) {
        zmk_status_scanner_start();
        scanner_started = true;
    }
}
/* ── Page lifecycle ────────────────────────────────────────────────────── */

static void page_home_enter(void)
{
	update_datetime(NULL); /* show current time immediately on entry */
	lv_timer_resume(s_timer);
	home_buttons_set_visible(false);
}

static void page_home_leave(void)
{
	lv_timer_pause(s_timer);
	home_buttons_pause();
}

/* ── Page ops ──────────────────────────────────────────────────────────── */

const struct page_ops page_home_ops = {
	.name         = "home",
	.create       = page_home_create,
	.on_enter     = page_home_enter,
	.on_leave     = page_home_leave,
};

