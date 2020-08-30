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
QueueHandle_t g_event_queue_handle = NULL;
EventGroupHandle_t g_event_group = NULL;
EventGroupHandle_t isr_group = NULL;
bool lenergy = false;

#define G_EVENT_VBUS_PLUGIN _BV(0)
#define G_EVENT_VBUS_REMOVE _BV(1)
#define G_EVENT_CHARGE_DONE _BV(2)

#define G_EVENT_WIFI_SCAN_START _BV(3)
#define G_EVENT_WIFI_SCAN_DONE _BV(4)
#define G_EVENT_WIFI_CONNECTED _BV(5)
#define G_EVENT_WIFI_BEGIN _BV(6)
#define G_EVENT_WIFI_OFF _BV(7)

enum
{
  Q_EVENT_WIFI_SCAN_DONE,
  Q_EVENT_WIFI_CONNECT,
  Q_EVENT_BMA_INT,
  Q_EVENT_AXP_INT,
};

#define WATCH_FLAG_SLEEP_MODE _BV(1)
#define WATCH_FLAG_SLEEP_EXIT _BV(2)
#define WATCH_FLAG_BMA_IRQ _BV(3)
#define WATCH_FLAG_AXP_IRQ _BV(4)

// * Macro calls
LV_IMG_DECLARE(black_bg);
LV_FONT_DECLARE(liquidCrystal_nor_64);

void init_power(TTGOClass *watch)
{
  // Turn on the IRQ used for power bus
  watch->power->adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1, AXP202_ON);
  watch->power->enableIRQ(AXP202_VBUS_REMOVED_IRQ | AXP202_VBUS_CONNECT_IRQ | AXP202_CHARGING_FINISHED_IRQ, AXP202_ON);
  watch->power->clearIRQ();

  // Turn off unused power bus flags
  watch->power->setPowerOutPut(AXP202_EXTEN, AXP202_OFF);
  watch->power->setPowerOutPut(AXP202_DCDC2, AXP202_OFF);
  watch->power->setPowerOutPut(AXP202_LDO3, AXP202_OFF);
  watch->power->setPowerOutPut(AXP202_LDO4, AXP202_OFF);

  // Connection interrupted to the specified pin
  pinMode(AXP202_INT, INPUT);
  attachInterrupt(
      AXP202_INT, [] {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        EventBits_t bits = xEventGroupGetBitsFromISR(isr_group);
        if (bits & WATCH_FLAG_SLEEP_MODE)
        {
          // For quick wake up, use the group flag
          xEventGroupSetBitsFromISR(isr_group, WATCH_FLAG_SLEEP_EXIT | WATCH_FLAG_AXP_IRQ, &xHigherPriorityTaskWoken);
        }
        else
        {
          uint8_t data = Q_EVENT_AXP_INT;
          xQueueSendFromISR(g_event_queue_handle, &data, &xHigherPriorityTaskWoken);
        }
        if (xHigherPriorityTaskWoken)
        {
          portYIELD_FROM_ISR();
        }
      },
      FALLING);
}

void setup()
{
  Serial.begin(115200);

  //Create a program that allows the required message objects and group flags
  g_event_queue_handle = xQueueCreate(20, sizeof(uint8_t));
  g_event_group = xEventGroupCreate();
  isr_group = xEventGroupCreate();

  // Instantiate and initialize watch
  watch = TTGOClass::getWatch();
  watch->begin();

  // Initialize and start lvgl - graphics lib
  watch->lvgl_begin();

  // Init watch rtc
  rtc = watch->rtc;
  rtc->check();

  // Turn on backlight and set brightness
  watch->openBL();
  watch->bl->adjust(20);

  // create and init face img obj
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