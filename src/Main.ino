#include <LilyGoWatch.h>

// * Global defs
typedef struct
{
  lv_obj_t *hour;
  lv_obj_t *minute;
  lv_obj_t *second;
} str_datetime_t;

static str_datetime_t g_data;
TTGOClass *watch = nullptr;
PCF8563_Class *rtc;

// * Macro calls
LV_IMG_DECLARE(black_bg);
LV_FONT_DECLARE(liquidCrystal_nor_64);

void setup()
{
  Serial.begin(115200);
  watch = TTGOClass::getWatch();
  watch->begin();

  // Turn on the IRQ used
  watch->power->adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1, AXP202_ON);
  watch->power->enableIRQ(AXP202_VBUS_REMOVED_IRQ | AXP202_VBUS_CONNECT_IRQ | AXP202_CHARGING_FINISHED_IRQ, AXP202_ON);
  watch->power->clearIRQ();

  // Turn off unused power
  watch->power->setPowerOutPut(AXP202_EXTEN, AXP202_OFF);
  watch->power->setPowerOutPut(AXP202_DCDC2, AXP202_OFF);
  watch->power->setPowerOutPut(AXP202_LDO3, AXP202_OFF);
  watch->power->setPowerOutPut(AXP202_LDO4, AXP202_OFF);

  watch->lvgl_begin();
  rtc = watch->rtc;

  // Use compile time
  rtc->check();

  watch->openBL();

  //Lower the brightness
  watch->bl->adjust(30);

  lv_obj_t *face = lv_img_create(lv_scr_act(), NULL);
  lv_img_set_src(face, &black_bg);
  lv_obj_align(face, NULL, LV_ALIGN_CENTER, 0, 0);

  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_text_color(&style, LV_STATE_DEFAULT, LV_COLOR_RED);
  lv_style_set_text_font(&style, LV_STATE_DEFAULT, &liquidCrystal_nor_64);

  g_data.hour = lv_label_create(face, nullptr);
  lv_obj_add_style(g_data.hour, LV_OBJ_PART_MAIN, &style);

  lv_label_set_text(g_data.hour, "00");
  lv_obj_align(g_data.hour, face, LV_ALIGN_IN_TOP_LEFT, 10, 85);

  g_data.minute = lv_label_create(face, nullptr);
  lv_obj_add_style(g_data.minute, LV_OBJ_PART_MAIN, &style);
  lv_label_set_text(g_data.minute, ":00");
  lv_obj_align(g_data.minute, g_data.hour, LV_ALIGN_IN_TOP_LEFT, 75, 0);

  g_data.second = lv_label_create(face, nullptr);
  lv_obj_add_style(g_data.second, LV_OBJ_PART_MAIN, &style);
  lv_label_set_text(g_data.second, ":00");
  lv_obj_align(g_data.second, g_data.minute, LV_ALIGN_IN_TOP_LEFT, 75, 0);

  lv_task_create([](lv_task_t *t) {
    RTC_Date curr_datetime = rtc->getDateTime();
    lv_label_set_text_fmt(g_data.second, ":%02u", curr_datetime.second);
    lv_label_set_text_fmt(g_data.minute, ":%02u", curr_datetime.minute);
    lv_label_set_text_fmt(g_data.hour, "%02u", curr_datetime.hour);
  },
                 1000, LV_TASK_PRIO_MID, nullptr);

  // Lower operating speed in mhz to reduce power consumption
  setCpuFrequencyMhz(2);
}

void loop()
{
  lv_task_handler();
}