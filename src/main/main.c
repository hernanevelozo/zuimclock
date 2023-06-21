#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <u8g2.h>
// #include <rom/gpio.h>

#include <driver/ledc.h>
#include <time.h>
#include <sys/time.h>
#include <esp_attr.h>
#include <esp_sleep.h>
#include <esp_sntp.h>

// Wifi
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <lwip/err.h>
#include <lwip/sys.h>

#include "sdkconfig.h"
#include "u8g2_esp32_hal.h"

// Fuction prototypes
void app_main(void);
void task_check_inputs(void *pvParameter);

// IO, flags and tags
#define PUSH_BUTTON_1 GPIO_NUM_19
#define PUSH_BUTTON_2 GPIO_NUM_18
#define PUSH_BUTTON_3 GPIO_NUM_17
#define PUSH_BUTTON_4 GPIO_NUM_16
#define PUSH_BUTTON_MODE GPIO_NUM_5
#define PIN_BUZZER GPIO_NUM_25

#define GPIO_PUSH_BUTTON_1_SEL (1ULL << PUSH_BUTTON_1)
#define GPIO_PUSH_BUTTON_2_SEL (1ULL << PUSH_BUTTON_2)
#define GPIO_PUSH_BUTTON_3_SEL (1ULL << PUSH_BUTTON_3)
#define GPIO_PUSH_BUTTON_4_SEL (1ULL << PUSH_BUTTON_4)
#define GPIO_PUSH_BUTTON_MODE_SEL (1ULL << PUSH_BUTTON_MODE)
#define GPIO_PIN_BUZZER_SEL (1ULL << PIN_BUZZER)
#define GPIO_OUTPUT_SPEED LEDC_HIGH_SPEED_MODE

#define DEBUG true
static const char *TAG = "Main";
uint8_t KILLAPP = 0;
uint8_t KILLWAIT = 0;
uint8_t CURRENT_MODE = 0;
TaskHandle_t xHandle_Main_Clock_Display;
TaskHandle_t xHandle_Chrono_Display;
TaskHandle_t xHandle_Timer_Display;
TaskHandle_t xHandle_Time_Set_Display;
TaskHandle_t xHandle_Chess_Display;
TaskHandle_t xHandle_Chrono_Count;
TaskHandle_t xHandle_Timer_Count;
TaskHandle_t xHandle_Chess_Count;
TaskHandle_t xHandle_Play_Alarm;
SemaphoreHandle_t xSemaphore_Flush_Display;

// Wifi and NTP
#define EXAMPLE_ESP_WIFI_SSID "esp32_network"
#define EXAMPLE_ESP_WIFI_PASS "xctj5056"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_event_group;
static const char *TAG_WIFI = "Wifi";
char Current_Date_Time[100];

// Display
#define PIN_CLK 14	 // CLK - GPIO14
#define PIN_MOSI 13	 // MOSI - GPIO 13
#define PIN_RESET 26 // RESET - GPIO 26
#define PIN_DC 27	 // DC - GPIO 27
#define PIN_CS 15	 // CS - GPIO 15

static char *TAG_DISPLAY = "Display";
u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
u8g2_t u8g2; // a structure which will contain all the data for one display

// Clocks
#define DATE_BUF_SIZE 10

typedef struct str_current_time
{
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	char mday[DATE_BUF_SIZE];
	char mon[DATE_BUF_SIZE];
	char year[DATE_BUF_SIZE];
	char wday[DATE_BUF_SIZE];
} current_time;

typedef struct str_ordinary_time
{
	int8_t hour;
	int8_t min;
	int8_t sec;
} ordinary_time;

typedef struct str_chrono_time
{
	int8_t hour;
	int8_t min;
	int8_t sec;
	int8_t csec;
} chrono_time;

current_time time_clock;
chrono_time time_chrono = {0, 0, 0, 0};
ordinary_time time_timer_config = {0, 0, 10};
ordinary_time time_timer;
ordinary_time chess_time_white_config = {0, 2, 0};
ordinary_time chess_time_black_config = {0, 2, 0};
ordinary_time chess_time_white;
ordinary_time chess_time_black;
ordinary_time chess_increment = {0, 0, 0};
uint8_t CHRONO_STARTED_COUNT = 0;
uint8_t TIMER_STARTED_COUNT = 0;
uint8_t TIME_SET_CURSOR = 0;
uint8_t CHESS_STARTED_COUNT = 0;
uint8_t CHESS_TURN = 0;

// void wait_flush()
// {
// 	xSemaphoreTake(xSemaphore_Flush_Display, portMAX_DELAY);
// }

void flush_display()
{
	xSemaphoreTake(xSemaphore_Flush_Display, portMAX_DELAY);

	u8g2_ClearDisplay(&u8g2);
	// u8g2_ClearBuffer(&u8g2);

	xSemaphoreGive(xSemaphore_Flush_Display);

	ESP_LOGI(TAG_DISPLAY, "Display flushed!");
}

void task_waitscreen(void *pvParameters)
{
	uint8_t changed = 1;
	const TickType_t xDelay = 100 / portTICK_PERIOD_MS;

	while (1)
	{
		if (KILLWAIT == 0 && changed == 1)
		{
			flush_display();

			// wait_flush();
			xSemaphoreTake(xSemaphore_Flush_Display, portMAX_DELAY);
			u8g2_FirstPage(&u8g2);
			do
			{
				u8g2_SetFont(&u8g2, u8g2_font_logisoso22_tf);
				u8g2_DrawStr(&u8g2, 30, 40, "Wait...");
			} while (u8g2_NextPage(&u8g2));
			xSemaphoreGive(xSemaphore_Flush_Display);

			changed = 0;
		}
		else if (KILLWAIT == 1 && changed == 0)
		{
			flush_display();

			// wait_flush();
			xSemaphoreTake(xSemaphore_Flush_Display, portMAX_DELAY);
			u8g2_FirstPage(&u8g2);
			do
			{
				u8g2_SetFont(&u8g2, u8g2_font_logisoso22_tf);
				u8g2_DrawStr(&u8g2, 25, 40, "Ready");
			} while (u8g2_NextPage(&u8g2));
			xSemaphoreGive(xSemaphore_Flush_Display);

			changed = 1;
		}
		else if (KILLWAIT == 2)
		{
			KILLWAIT = 3;
			vTaskDelete(NULL);
		}
		vTaskDelay(xDelay);
	}
}

void task_display_main_clock(void *pvParameters)
{
	ESP_LOGI(TAG_DISPLAY, "Main clock display task is running");

	const TickType_t xDelay = 500 / portTICK_PERIOD_MS;
	char hour_str[3];
	char min_str[3];
	char sec_str[3];
	char mday_str[DATE_BUF_SIZE];
	char mon_str[DATE_BUF_SIZE];
	char year_str[DATE_BUF_SIZE];
	char wday_str[DATE_BUF_SIZE];

	while (true)
	{
		if (KILLAPP == 1)
		{
			KILLAPP = 0;
			vTaskDelete(NULL);
		}

		strcpy(hour_str, u8x8_u8toa(time_clock.hour, 2)); /* convert m to a string with two digits */
		strcpy(min_str, u8x8_u8toa(time_clock.min, 2));	  /* convert m to a string with two digits */
		strcpy(sec_str, u8x8_u8toa(time_clock.sec, 2));	  /* convert m to a string with two digits */
		strcpy(mday_str, time_clock.mday);
		strcpy(mon_str, time_clock.mon);
		strcpy(year_str, time_clock.year);
		strcpy(wday_str, time_clock.wday);

		strcat(mon_str, " ");
		strcat(mon_str, mday_str);

		// wait_flush();
		xSemaphoreTake(xSemaphore_Flush_Display, portMAX_DELAY);
		u8g2_FirstPage(&u8g2);
		do
		{
			u8g2_SetFont(&u8g2, u8g2_font_logisoso22_tf);
			u8g2_DrawStr(&u8g2, 14, 28, hour_str);
			u8g2_DrawStr(&u8g2, 42, 28, ":");
			u8g2_DrawStr(&u8g2, 50, 28, min_str);
			u8g2_DrawStr(&u8g2, 78, 28, ":");
			u8g2_DrawStr(&u8g2, 86, 28, sec_str);

			u8g2_DrawLine(&u8g2, 14, 33, 114, 33);

			u8g2_SetFont(&u8g2, u8g2_font_tenthinnerguys_tf);
			u8g2_DrawStr(&u8g2, 18, 48, mon_str);
			u8g2_DrawStr(&u8g2, 23, 61, year_str);

			u8g2_SetFont(&u8g2, u8g2_font_logisoso22_tf);
			u8g2_DrawLine(&u8g2, 64, 33, 64, 63);
			u8g2_DrawStr(&u8g2, 68, 61, wday_str);
		} while (u8g2_NextPage(&u8g2));
		xSemaphoreGive(xSemaphore_Flush_Display);

		if (KILLAPP == 1)
		{
			KILLAPP = 0;
			vTaskDelete(NULL);
		}

		vTaskDelay(xDelay);
	}
}

void task_display_chrono(void *pvParameters)
{
	ESP_LOGI(TAG_DISPLAY, "Chrono display task is running");

	const TickType_t xDelay = 50 / portTICK_PERIOD_MS;
	char hour_str[3];
	char min_str[3];
	char sec_str[3];
	char csec_str[3];

	while (true)
	{
		if (KILLAPP == 2)
		{
			KILLAPP = 0;
			vTaskDelete(NULL);
		}

		strcpy(hour_str, u8x8_u8toa(time_chrono.hour, 2)); /* convert m to a string with two digits */
		strcpy(min_str, u8x8_u8toa(time_chrono.min, 2));   /* convert m to a string with two digits */
		strcpy(sec_str, u8x8_u8toa(time_chrono.sec, 2));   /* convert m to a string with two digits */
		// strcpy(csec_str, u8x8_u8toa(time_chrono.csec/100, 1)); /* convert m to a string with two digits */
		if (time_chrono.csec < 100)
		{
			csec_str[0] = (time_chrono.csec / 10) + '0';
			csec_str[1] = '\0';
		}
		else
		{
			csec_str[0] = '0';
			csec_str[1] = '\0';
		}

		// wait_flush();
		xSemaphoreTake(xSemaphore_Flush_Display, portMAX_DELAY);
		u8g2_FirstPage(&u8g2);
		do
		{
			u8g2_SetFont(&u8g2, u8g2_font_prospero_nbp_tf);
			u8g2_DrawStr(&u8g2, 48, 10, "CHRONO");
			// u8g2_DrawLine(&u8g2, 64, 15, 64, 15);
			// u8g2_DrawLine(&u8g2, 0, 15, 0, 15);
			// u8g2_DrawLine(&u8g2, 127, 15, 127, 15);

			u8g2_SetFont(&u8g2, u8g2_font_logisoso22_tf);
			u8g2_DrawStr(&u8g2, 2, 45, hour_str);
			u8g2_DrawStr(&u8g2, 30, 45, ":");
			u8g2_DrawStr(&u8g2, 38, 45, min_str);
			u8g2_DrawStr(&u8g2, 66, 45, ":");
			u8g2_DrawStr(&u8g2, 74, 45, sec_str);
			u8g2_DrawStr(&u8g2, 102, 45, ".");
			u8g2_DrawStr(&u8g2, 110, 45, csec_str);
		} while (u8g2_NextPage(&u8g2));
		xSemaphoreGive(xSemaphore_Flush_Display);

		if (KILLAPP == 2)
		{
			KILLAPP = 0;
			vTaskDelete(NULL);
		}

		vTaskDelay(xDelay);
	}
}

void task_display_timer(void *pvParameters)
{

	ESP_LOGI(TAG_DISPLAY, "Timer display task is running");

	const TickType_t xDelay = 500 / portTICK_PERIOD_MS;
	char hour_str[3];
	char min_str[3];
	char sec_str[3];

	while (true)
	{
		if (KILLAPP == 3)
		{
			KILLAPP = 0;
			vTaskDelete(NULL);
		}

		strcpy(hour_str, u8x8_u8toa(time_timer.hour, 2)); /* convert m to a string with two digits */
		strcpy(min_str, u8x8_u8toa(time_timer.min, 2));	  /* convert m to a string with two digits */
		strcpy(sec_str, u8x8_u8toa(time_timer.sec, 2));	  /* convert m to a string with two digits */

		// wait_flush();
		xSemaphoreTake(xSemaphore_Flush_Display, portMAX_DELAY);
		u8g2_FirstPage(&u8g2);
		do
		{
			u8g2_SetFont(&u8g2, u8g2_font_prospero_nbp_tf);
			u8g2_DrawStr(&u8g2, 51, 10, "TIMER");

			u8g2_SetFont(&u8g2, u8g2_font_logisoso22_tf);
			u8g2_DrawStr(&u8g2, 14, 45, hour_str);
			u8g2_DrawStr(&u8g2, 42, 45, ":");
			u8g2_DrawStr(&u8g2, 50, 45, min_str);
			u8g2_DrawStr(&u8g2, 78, 45, ":");
			u8g2_DrawStr(&u8g2, 86, 45, sec_str);
		} while (u8g2_NextPage(&u8g2));
		xSemaphoreGive(xSemaphore_Flush_Display);

		if (KILLAPP == 3)
		{
			KILLAPP = 0;
			vTaskDelete(NULL);
		}

		vTaskDelay(xDelay);
	}
}

void task_time_set_display(void *pvParameters)
{

	ESP_LOGI(TAG_DISPLAY, "Timer display task is running");

	const TickType_t xDelay = 200 / portTICK_PERIOD_MS;
	char hour_str[3];
	char min_str[3];
	char sec_str[3];

	while (true)
	{
		if (KILLAPP == 5)
		{
			KILLAPP = 0;
			vTaskDelete(NULL);
		}

		// if ((int8_t)pvParameters == (int8_t)0)
		if (CURRENT_MODE == 5)
		{
			strcpy(hour_str, u8x8_u8toa(time_timer_config.hour, 2)); /* convert m to a string with two digits */
			strcpy(min_str, u8x8_u8toa(time_timer_config.min, 2));	 /* convert m to a string with two digits */
			strcpy(sec_str, u8x8_u8toa(time_timer_config.sec, 2));	 /* convert m to a string with two digits */
		}
		else if (CURRENT_MODE == 6)
		{
			strcpy(hour_str, u8x8_u8toa(chess_time_white_config.hour, 2)); /* convert m to a string with two digits */
			strcpy(min_str, u8x8_u8toa(chess_time_white_config.min, 2));   /* convert m to a string with two digits */
			strcpy(sec_str, u8x8_u8toa(chess_time_white_config.sec, 2));   /* convert m to a string with two digits */
		}
		else if (CURRENT_MODE == 7)
		{
			strcpy(hour_str, u8x8_u8toa(chess_time_black_config.hour, 2)); /* convert m to a string with two digits */
			strcpy(min_str, u8x8_u8toa(chess_time_black_config.min, 2));   /* convert m to a string with two digits */
			strcpy(sec_str, u8x8_u8toa(chess_time_black_config.sec, 2));   /* convert m to a string with two digits */
		}

		// wait_flush();
		xSemaphoreTake(xSemaphore_Flush_Display, portMAX_DELAY);
		u8g2_FirstPage(&u8g2);
		do
		{
			u8g2_SetFont(&u8g2, u8g2_font_prospero_nbp_tf);

			if (CURRENT_MODE == 5)
			{
				u8g2_DrawStr(&u8g2, 40, 10, "SET TIMER");
			}
			else if (CURRENT_MODE == 6)
			{
				u8g2_DrawStr(&u8g2, 25, 10, "TIME CONTROL W");
			}
			else if (CURRENT_MODE == 7)
			{
				u8g2_DrawStr(&u8g2, 25, 10, "TIME CONTROL B");
			}

			u8g2_SetFont(&u8g2, u8g2_font_logisoso22_tf);
			u8g2_DrawStr(&u8g2, 14, 45, hour_str);
			u8g2_DrawStr(&u8g2, 42, 45, ":");
			u8g2_DrawStr(&u8g2, 50, 45, min_str);
			u8g2_DrawStr(&u8g2, 78, 45, ":");
			u8g2_DrawStr(&u8g2, 86, 45, sec_str);

			if (TIME_SET_CURSOR == 0)
			{
				u8g2_DrawTriangle(&u8g2, 95, 58, 106, 58, 100, 53);
			}
			else if (TIME_SET_CURSOR == 1)
			{
				u8g2_DrawTriangle(&u8g2, 58, 58, 69, 58, 64, 53);
			}
			else
			{
				u8g2_DrawTriangle(&u8g2, 23, 58, 34, 58, 27, 53);
			}

		} while (u8g2_NextPage(&u8g2));
		xSemaphoreGive(xSemaphore_Flush_Display);

		if (KILLAPP == 5)
		{
			KILLAPP = 0;
			vTaskDelete(NULL);
		}

		vTaskDelay(xDelay);
	}
}

void task_display_chess_clock(void *pvParameters)
{
	// 'white-pawn', 20x20px
	static const unsigned char bitmap_white_pawn[] U8X8_PROGMEM = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x80, 0x1f, 0x00, 0xc0, 0x3f, 0x00, 0xc0,
		0x3f, 0x00, 0xc0, 0x3f, 0x00, 0x80, 0x1f, 0x00, 0x00, 0x0f, 0x00, 0xc0, 0x3f, 0x00, 0x80, 0x1f,
		0x00, 0x00, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0x80, 0x1f, 0x00, 0xc0, 0x3f, 0x00, 0xe0, 0x7f, 0x00,
		0xe0, 0x7f, 0x00, 0xf0, 0xff, 0x00, 0xf0, 0xff, 0x00, 0x00, 0x00, 0x00};

	// 'black-pawn', 20x20px
	static const unsigned char bitmap_black_pawn[] U8X8_PROGMEM = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x80, 0x10, 0x00, 0x40, 0x20, 0x00, 0x40,
		0x20, 0x00, 0x40, 0x20, 0x00, 0x80, 0x10, 0x00, 0x00, 0x09, 0x00, 0xc0, 0x39, 0x00, 0x80, 0x10,
		0x00, 0x00, 0x09, 0x00, 0x00, 0x09, 0x00, 0x80, 0x10, 0x00, 0x40, 0x20, 0x00, 0x20, 0x40, 0x00,
		0x20, 0x40, 0x00, 0x10, 0x80, 0x00, 0xf0, 0xff, 0x00, 0x00, 0x00, 0x00};

	const TickType_t xDelay = 200 / portTICK_PERIOD_MS;
	char hour_str_white[3];
	char min_str_white[3];
	char sec_str_white[3];
	char hour_str_black[3];
	char min_str_black[3];
	char sec_str_black[3];

	ESP_LOGI(TAG_DISPLAY, "Chess clock display task is running");

	while (true)
	{
		if (KILLAPP == 4)
		{
			KILLAPP = 0;
			vTaskDelete(NULL);
		}

		strcpy(hour_str_white, u8x8_u8toa(chess_time_white.hour, 2)); /* convert m to a string with two digits */
		strcpy(min_str_white, u8x8_u8toa(chess_time_white.min, 2));	  /* convert m to a string with two digits */
		strcpy(sec_str_white, u8x8_u8toa(chess_time_white.sec, 2));	  /* convert m to a string with two digits */
		strcpy(hour_str_black, u8x8_u8toa(chess_time_black.hour, 2)); /* convert m to a string with two digits */
		strcpy(min_str_black, u8x8_u8toa(chess_time_black.min, 2));	  /* convert m to a string with two digits */
		strcpy(sec_str_black, u8x8_u8toa(chess_time_black.sec, 2));	  /* convert m to a string with two digits */

		// wait_flush();
		xSemaphoreTake(xSemaphore_Flush_Display, portMAX_DELAY);
		u8g2_FirstPage(&u8g2);
		do
		{
			u8g2_SetFont(&u8g2, u8g2_font_logisoso22_tn);

			u8g2_DrawStr(&u8g2, 0, 61, hour_str_black);
			u8g2_DrawStr(&u8g2, 28, 61, ":");
			u8g2_DrawStr(&u8g2, 36, 61, min_str_black);
			u8g2_DrawStr(&u8g2, 64, 61, ":");
			u8g2_DrawStr(&u8g2, 72, 61, sec_str_black);

			u8g2_DrawLine(&u8g2, 0, 33, 128, 33);

			u8g2_DrawStr(&u8g2, 0, 28, hour_str_white);
			u8g2_DrawStr(&u8g2, 28, 28, ":");
			u8g2_DrawStr(&u8g2, 36, 28, min_str_white);
			u8g2_DrawStr(&u8g2, 64, 28, ":");
			u8g2_DrawStr(&u8g2, 72, 28, sec_str_white);

			if (CHESS_TURN == 0)
			{
				u8g2_DrawTriangle(&u8g2, 110, 3, 121, 3, 115, 8);
			}
			else
			{
				u8g2_DrawTriangle(&u8g2, 110, 37, 121, 37, 115, 42);
			}

			u8g2_DrawXBMP(&u8g2, 105, 9, 20, 20, bitmap_white_pawn);
			u8g2_DrawXBMP(&u8g2, 105, 43, 20, 20, bitmap_black_pawn);
		} while (u8g2_NextPage(&u8g2));
		xSemaphoreGive(xSemaphore_Flush_Display);

		if (KILLAPP == 4)
		{
			KILLAPP = 0;
			vTaskDelete(NULL);
		}

		vTaskDelay(xDelay);
	}
}

void reset_chrono_time()
{
	time_chrono.hour = 0;
	time_chrono.min = 0;
	time_chrono.sec = 0;
	time_chrono.csec = 0;
}

void task_chrono_count(void *pvParameters)
{
	const TickType_t xDelay = 10 / portTICK_PERIOD_MS;

	while (1)
	{
		if (time_chrono.csec > 99)
		{
			time_chrono.csec = 0;
			time_chrono.sec++;

			if (time_chrono.sec > 59)
			{
				time_chrono.sec = 0;
				time_chrono.min++;

				if (time_chrono.min > 59)
				{
					time_chrono.min = 0;
					time_chrono.hour++;

					if (time_chrono.hour > 99)
					{
						time_chrono.sec = 59;
						time_chrono.min = 59;
						time_chrono.hour = 99;
						vTaskDelete(NULL);
					}
				}
			}
		}

		time_chrono.csec++;
		vTaskDelay(xDelay);
	}
}

void reset_timer()
{
	time_timer.sec = time_timer_config.sec;
	time_timer.min = time_timer_config.min;
	time_timer.hour = time_timer_config.hour;
}

void increment_time(ordinary_time *time)
{
	if (TIME_SET_CURSOR == 0)
	{
		time->sec++;
		if (time->sec > 59)
		{
			time->sec = 0;
		}
	}
	else if (TIME_SET_CURSOR == 1)
	{
		time->min++;
		if (time->min > 59)
		{
			time->min = 0;
		}
	}
	else
	{
		time->hour++;
		if (time->hour > 99)
		{
			time->hour = 0;
		}
	}
}

void decrement_time(ordinary_time *time)
{
	if (TIME_SET_CURSOR == 0)
	{
		time->sec--;
		if (time->sec < 0)
		{
			time->sec = 59;
		}
	}
	else if (TIME_SET_CURSOR == 1)
	{
		time->min--;
		if (time->min < 0)
		{
			time->min = 59;
		}
	}
	else
	{
		time->hour--;
		if (time->hour < 0)
		{
			time->hour = 99;
		}
	}
}

void play_alarm(void *pvParameters)
{
	int8_t state = 1;
	int8_t count_alarm = 0;

	while(1)
	{
		gpio_set_level(PIN_BUZZER, state);
		state =! state;
		count_alarm++;
		vTaskDelay(500 / portTICK_PERIOD_MS);

		if (DEBUG)
		{
			ESP_LOGI(TAG, "Alarm is runnning");
		}

		if (count_alarm > 11)
		{
			vTaskDelete(NULL);
		}
	}
}

void task_timer_count(void *pvParameters)
{
	const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;

	while (1)
	{
		time_timer.sec--;

		if (time_timer.sec < 0)
		{
			time_timer.sec = 59;
			time_timer.min--;
		}

		if (time_timer.min < 0)
		{
			time_timer.min = 59;
			time_timer.hour--;
		}

		if (time_timer.hour < 0)
		{
			time_timer.sec = 0;
			time_timer.min = 0;
			time_timer.hour = 0;
			TIMER_STARTED_COUNT = 2;

			xTaskCreate(
				play_alarm,
				"PLAYING ALARM",
				2048,
				NULL,
				1,
				// NULL
				&xHandle_Play_Alarm);

			vTaskDelete(NULL);
		}

		vTaskDelay(xDelay);
	}
}

bool count_chess_time(ordinary_time *chess_time)
{
	chess_time->sec -= 1;

	if (chess_time->sec < 0)
	{
		chess_time->sec = 59;
		chess_time->min--;
	}
	if (chess_time->min < 0)
	{
		chess_time->min = 59;
		chess_time->hour--;
	}
	if (chess_time->hour < 0)
	{
		chess_time->sec = 0;
		chess_time->min = 0;
		chess_time->hour = 0;
		return true;
	}

	return false;
}

void reset_chess_time()
{
	chess_time_white.hour = chess_time_white_config.hour;
	chess_time_white.min = chess_time_white_config.min;
	chess_time_white.sec = chess_time_white_config.sec;
	chess_time_black.hour = chess_time_black_config.hour;
	chess_time_black.min = chess_time_black_config.min;
	chess_time_black.sec = chess_time_black_config.sec;
}

void task_chess_clock_count(void *pvParameters)
{
	const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;

	while (1)
	{
		if (CHESS_TURN == 0)
		{
			if (count_chess_time(&chess_time_white))
			{
				CHESS_STARTED_COUNT = 2;
				vTaskDelete(NULL);
			}
		}
		else
		{
			if (count_chess_time(&chess_time_black))
			{
				CHESS_STARTED_COUNT = 2;
				vTaskDelete(NULL);
			}
		}

		vTaskDelay(xDelay);
	}
}

void pb_mode_actions(uint8_t *display_state, uint8_t *button_state, uint8_t pb_level)
{
	if (pb_level == 1 && *button_state == 0)
	{
		if (*display_state == 0)
		{
			// vTaskDelete(xHandle_Chess_Display);
			KILLAPP = 4;
			KILLWAIT = 2;
			CURRENT_MODE = 1;
			flush_display();

			xTaskCreate(
				task_display_main_clock,
				"MAIN CLOCK DISPLAY",
				2048,
				NULL,
				1,
				// NULL
				&xHandle_Main_Clock_Display);
			*display_state = 1;
		}
		*button_state = 1;
	}

	if (pb_level == 1 && *button_state == 2)
	{
		// vTaskDelete(xHandle_Main_Clock_Display);
		KILLAPP = 1;
		CURRENT_MODE = 2;
		flush_display();

		xTaskCreate(
			task_display_chrono,
			"CHRONO DISPLAY",
			2048,
			NULL,
			1,
			// NULL
			&xHandle_Chrono_Display);
		*button_state = 3;
	}

	if (pb_level == 1 && *button_state == 4)
	{
		// vTaskDelete(xHandle_Chrono_Display);
		if (CURRENT_MODE == 2)
		{
			KILLAPP = 2;
		}
		else if (CURRENT_MODE == 5)
		{
			KILLAPP = 5;
		}
		CURRENT_MODE = 3;
		flush_display();
		reset_timer();

		xTaskCreate(
			task_display_timer,
			"TIMER DISPLAY",
			2048,
			NULL,
			1,
			// NULL
			&xHandle_Timer_Display);
		*button_state = 5;
	}

	if (pb_level == 1 && *button_state == 6)
	{
		// vTaskDelete(xHandle_Timer_Display);
		if (CURRENT_MODE == 3)
		{
			KILLAPP = 3;
		}
		else if (CURRENT_MODE == 6 || CURRENT_MODE == 7)
		{
			KILLAPP = 5;
		}
		CURRENT_MODE = 4;
		flush_display();
		reset_chess_time();

		xTaskCreate(
			task_display_chess_clock,
			"CHESS CLOCK DISPLAY",
			4096,
			NULL,
			1,
			// NULL
			&xHandle_Chess_Display);
		*button_state = 7;
	}

	if (CURRENT_MODE == 5 && *button_state == 6)
	{
		*button_state = 4;
	}

	if (CURRENT_MODE == 6 && *button_state == 0)
	{
		*button_state = 6;
	}

	if (pb_level == 0)
	{
		switch (*button_state)
		{
		case 1:
			*button_state = 2;
			*display_state = 0;
			break;
		case 3:
			*button_state = 4;
			*display_state = 0;
			break;
		case 5:
			*button_state = 6;
			*display_state = 0;
			break;
		case 7:
			*button_state = 0;
			*display_state = 0;
			break;
		}
	}
}

void pb_1_actions(uint8_t *button_state, uint8_t pb_level)
{
	if (CURRENT_MODE == 2)
	{
		if (pb_level == 1 && CHRONO_STARTED_COUNT == 0)
		{
			CHRONO_STARTED_COUNT = 1;
			xTaskCreate(
				task_chrono_count,
				"CHRONO COUNT",
				1024,
				NULL,
				1,
				// NULL
				&xHandle_Chrono_Count);
		}
	}

	else if (CURRENT_MODE == 3)
	{
		if (pb_level == 1 && TIMER_STARTED_COUNT == 0)
		{
			TIMER_STARTED_COUNT = 1;
			xTaskCreate(
				task_timer_count,
				"TIMER COUNT",
				1024,
				NULL,
				1,
				// NULL
				&xHandle_Timer_Count);
		}
	}

	else if (CURRENT_MODE == 4)
	{
		if (pb_level == 1 && CHESS_STARTED_COUNT == 0)
		{
			CHESS_STARTED_COUNT = 1;
			CHESS_TURN = 1;
			xTaskCreate(
				task_chess_clock_count,
				"CHESS CLOCK COUNT",
				1024,
				NULL,
				1,
				// NULL
				&xHandle_Chess_Count);
		}
		if (pb_level == 1 && *button_state == 0 && CHESS_STARTED_COUNT == 1 && CHESS_TURN == 0)
		{
			CHESS_TURN = 1;
			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 5)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			increment_time(&time_timer_config);

			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 6)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			increment_time(&chess_time_white_config);

			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 7)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			increment_time(&chess_time_black_config);

			*button_state = 1;
		}
	}

	if (pb_level == 0)
	{
		switch (*button_state)
		{
		case 1:
			*button_state = 0;
			break;
		}
	}
}

void pb_2_actions(uint8_t *button_state, uint8_t pb_level)
{
	if (CURRENT_MODE == 2)
	{
		if (pb_level == 1 && *button_state == 0 && CHRONO_STARTED_COUNT == 1)
		{
			CHRONO_STARTED_COUNT = 0;
			vTaskDelete(xHandle_Chrono_Count);
			reset_chrono_time();

			*button_state = 1;
		}
		if (pb_level == 1 && *button_state == 0 && CHRONO_STARTED_COUNT == 0)
		{
			reset_chrono_time();

			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 3)
	{
		if (pb_level == 1 && *button_state == 0 && TIMER_STARTED_COUNT == 1)
		{
			TIMER_STARTED_COUNT = 0;
			vTaskDelete(xHandle_Timer_Count);
			reset_timer();

			*button_state = 1;
		}
		if (pb_level == 1 && *button_state == 0 && TIMER_STARTED_COUNT == 2)
		{
			TIMER_STARTED_COUNT = 0;
			reset_timer();

			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 4)
	{
		if (pb_level == 1 && *button_state == 0 && CHESS_STARTED_COUNT == 1)
		{
			vTaskDelete(xHandle_Chess_Count);
			CHESS_STARTED_COUNT = 0;
			CHESS_TURN = 0;
			reset_chess_time();

			*button_state = 1;
		}
		if (pb_level == 1 && *button_state == 0 && CHESS_STARTED_COUNT == 2)
		{
			CHESS_STARTED_COUNT = 0;
			CHESS_TURN = 0;
			reset_chess_time();

			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 5)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			TIME_SET_CURSOR++;
			if (TIME_SET_CURSOR > 2)
			{
				TIME_SET_CURSOR = 0;
			}

			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 6)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			TIME_SET_CURSOR++;
			if (TIME_SET_CURSOR > 2)
			{
				TIME_SET_CURSOR = 0;
			}

			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 7)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			TIME_SET_CURSOR++;
			if (TIME_SET_CURSOR > 2)
			{
				TIME_SET_CURSOR = 0;
			}

			*button_state = 1;
		}
	}

	if (pb_level == 0)
	{
		switch (*button_state)
		{
		case 1:
			*button_state = 0;
			break;
		}
	}
}

void pb_3_actions(uint8_t *button_state, uint8_t pb_level)
{
	if (CURRENT_MODE == 3)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			KILLAPP = 3;
			CURRENT_MODE = 5;
			TIME_SET_CURSOR = 0;
			// int8_t arg = 0;

			flush_display();

			xTaskCreate(
				task_time_set_display,
				"TIMER TIME SET DISPLAY",
				2048,
				// (void *) arg,
				NULL,
				1,
				// NULL
				&xHandle_Time_Set_Display);
			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 4)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			KILLAPP = 4;
			CURRENT_MODE = 6;
			TIME_SET_CURSOR = 0;
			// int8_t arg = 1;

			flush_display();

			xTaskCreate(
				task_time_set_display,
				"CHESS TIME SET DISPLAY",
				2048,
				// (void *) arg,
				NULL,
				1,
				// NULL
				&xHandle_Time_Set_Display);
			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 6)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			// KILLAPP = 5;
			CURRENT_MODE = 7;
			TIME_SET_CURSOR = 0;
			// int8_t arg = 2;

			// flush_display();

			// xTaskCreate(
			// 	task_time_set_display,
			// 	"CHESS TIME SET DISPLAY",
			// 	2048,
			// 	(void *) arg,
			// 	1,
			// 	// NULL
			// 	&xHandle_Time_Set_Display);
			*button_state = 1;
		}
	}

	if (pb_level == 0)
	{
		switch (*button_state)
		{
		case 1:
			*button_state = 0;
			break;
		}
	}
}

void pb_4_actions(uint8_t *button_state, uint8_t pb_level)
{
	if (CURRENT_MODE == 2)
	{
		if (pb_level == 1 && *button_state == 0 && CHRONO_STARTED_COUNT == 1)
		{
			CHRONO_STARTED_COUNT = 0;

			vTaskDelete(xHandle_Chrono_Count);
			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 4)
	{
		if (pb_level == 1 && *button_state == 0 && CHESS_STARTED_COUNT == 1 && CHESS_TURN == 1)
		{
			CHESS_TURN = 0;
			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 5)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			decrement_time(&time_timer_config);

			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 6)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			decrement_time(&chess_time_white_config);

			*button_state = 1;
		}
	}

	else if (CURRENT_MODE == 7)
	{
		if (pb_level == 1 && *button_state == 0)
		{
			decrement_time(&chess_time_black_config);

			*button_state = 1;
		}
	}

	if (pb_level == 0)
	{
		switch (*button_state)
		{
		case 1:
			*button_state = 0;
			break;
		}
	}
}

void task_check_inputs(void *arg)
{
	uint8_t display_state = 0;
	uint8_t pb_mode_state = 0;
	uint8_t pb_1_state = 0;
	uint8_t pb_2_state = 0;
	uint8_t pb_3_state = 0;
	uint8_t pb_4_state = 0;

	while (1)
	{
		pb_mode_actions(&display_state, &pb_mode_state, gpio_get_level(PUSH_BUTTON_MODE));
		pb_1_actions(&pb_1_state, gpio_get_level(PUSH_BUTTON_1));
		pb_2_actions(&pb_2_state, gpio_get_level(PUSH_BUTTON_2));
		pb_3_actions(&pb_3_state, gpio_get_level(PUSH_BUTTON_3));
		pb_4_actions(&pb_4_state, gpio_get_level(PUSH_BUTTON_4));

		vTaskDelay(20 / portTICK_PERIOD_MS);
	}
}

void time_sync_notification_cb(struct timeval *tv)
{
	ESP_LOGI(TAG_WIFI, "Notification of a time synchronization event");
}
static void event_handler(void *arg, esp_event_base_t event_base,
						  int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		// if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
		//{
		esp_wifi_connect();
		// s_retry_num++;
		ESP_LOGI(TAG_WIFI, "retry to connect to the AP");
		//} else {
		//    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		ESP_LOGI(TAG_WIFI, "connect to the AP fail");
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(TAG_WIFI, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		// s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void wifi_init_sta(void)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&event_handler,
														NULL,
														&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&event_handler,
														NULL,
														&instance_got_ip));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														WIFI_EVENT_STA_DISCONNECTED,
														&event_handler,
														NULL,
														NULL));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = EXAMPLE_ESP_WIFI_SSID,
			.password = EXAMPLE_ESP_WIFI_PASS,
			/* Setting a password implies station will connect to all security modes including WEP/WPA.
			 * However these modes are deprecated and not advisable to be used. Incase your Access point
			 * doesn't support WPA2, these mode can be enabled by commenting below line */
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,

			.pmf_cfg = {
				.capable = true,
				.required = false},
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG_WIFI, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
										   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
										   pdFALSE,
										   pdFALSE,
										   portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT)
	{
		ESP_LOGI(TAG_WIFI, "connected to ap SSID:%s password:%s",
				 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
	}
	else if (bits & WIFI_FAIL_BIT)
	{
		ESP_LOGI(TAG_WIFI, "Failed to connect to SSID:%s, password:%s",
				 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
	}
	else
	{
		ESP_LOGE(TAG_WIFI, "UNEXPECTED EVENT");
	}

	/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);
}

void Get_current_date_time()
{
	// char strftime_buf[64];
	char strftime_buf[DATE_BUF_SIZE];
	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);

	// Set timezone to Brazil Standard Time
	setenv("TZ", "EST4EDT,M3.2.0/2,M11.1.0", 1);
	tzset();
	localtime_r(&now, &timeinfo);

	time_clock.hour = timeinfo.tm_hour;
	time_clock.min = timeinfo.tm_min;
	time_clock.sec = timeinfo.tm_sec;

	strftime(strftime_buf, DATE_BUF_SIZE, "%d", &timeinfo);
	strncpy(time_clock.mday, strftime_buf, sizeof(time_clock.mday));
	strftime(strftime_buf, DATE_BUF_SIZE, "%b", &timeinfo);
	strncpy(time_clock.mon, strftime_buf, sizeof(time_clock.mon));
	strftime(strftime_buf, DATE_BUF_SIZE, "%Y", &timeinfo);
	strncpy(time_clock.year, strftime_buf, sizeof(time_clock.year));
	strftime(strftime_buf, DATE_BUF_SIZE, "%a", &timeinfo);
	strncpy(time_clock.wday, strftime_buf, sizeof(time_clock.wday));
}

static void initialize_sntp(void)
{
	ESP_LOGI(TAG_WIFI, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");
	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
	sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
	sntp_init();
}

static void obtain_time(void)
{
	initialize_sntp();
	// wait for time to be set
	time_t now = 0;
	struct tm timeinfo = {0};
	int retry = 0;
	const int retry_count = 10;
	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
	{
		ESP_LOGI(TAG_WIFI, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
	time(&now);
	localtime_r(&now, &timeinfo);
}

void Set_SystemTime_SNTP()
{

	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);
	// Is time set? If not, tm_year will be (1970 - 1900).
	if (timeinfo.tm_year < (2016 - 1900))
	{
		ESP_LOGI(TAG_WIFI, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
		obtain_time();
		// update 'now' variable with current time
		time(&now);
	}
}

void io_init()
{
	gpio_config_t pb1_config = {
		.intr_type = GPIO_INTR_POSEDGE,
		.mode = GPIO_MODE_INPUT,
		.pin_bit_mask = GPIO_PUSH_BUTTON_1_SEL,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE,
	};
	gpio_config(&pb1_config);

	gpio_config_t pb2_config = {
		.intr_type = GPIO_INTR_POSEDGE,
		.mode = GPIO_MODE_INPUT,
		.pin_bit_mask = GPIO_PUSH_BUTTON_2_SEL,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE,
	};
	gpio_config(&pb2_config);

	gpio_config_t pb3_config = {
		.intr_type = GPIO_INTR_POSEDGE,
		.mode = GPIO_MODE_INPUT,
		.pin_bit_mask = GPIO_PUSH_BUTTON_3_SEL,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE,
	};
	gpio_config(&pb3_config);

	gpio_config_t pb4_config = {
		.intr_type = GPIO_INTR_POSEDGE,
		.mode = GPIO_MODE_INPUT,
		.pin_bit_mask = GPIO_PUSH_BUTTON_4_SEL,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE,
	};
	gpio_config(&pb4_config);

	gpio_config_t pbmode_config = {
		.intr_type = GPIO_INTR_POSEDGE,
		.mode = GPIO_MODE_INPUT,
		.pin_bit_mask = GPIO_PUSH_BUTTON_MODE_SEL,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE,
	};
	gpio_config(&pbmode_config);

	gpio_config_t buzzer_config = {
	.intr_type = GPIO_INTR_DISABLE,
	.mode = GPIO_MODE_OUTPUT,
	.pin_bit_mask = GPIO_PIN_BUZZER_SEL,
	};
	gpio_config(&buzzer_config);
}

void app_main()
{
	// Initializing procedures
	io_init();
	reset_chess_time();
	reset_timer();
	// xSemaphore_Flush_Display = xSemaphoreCreateBinary();
	xSemaphore_Flush_Display = xSemaphoreCreateMutex();

	// Displays
	u8g2_esp32_hal.clk = PIN_CLK;
	u8g2_esp32_hal.mosi = PIN_MOSI;
	u8g2_esp32_hal.cs = PIN_CS;
	u8g2_esp32_hal.dc = PIN_DC;
	u8g2_esp32_hal.reset = PIN_RESET;
	u8g2_esp32_hal_init(u8g2_esp32_hal);

	u8g2_Setup_ssd1306_128x64_noname_f(
		&u8g2,
		U8G2_R0,
		u8g2_esp32_spi_byte_cb,
		u8g2_esp32_gpio_and_delay_cb); // init u8g2 structure

	u8g2_InitDisplay(&u8g2);	 // send init sequence to the display, display is in sleep mode after this,
	u8g2_SetPowerSave(&u8g2, 0); // wake up display
	flush_display();

	// Waiting screen
	if (xTaskCreate(&task_waitscreen, "WAIT SCREEN", 2048, NULL, 1, NULL))
	{
		if (DEBUG)
		{
			ESP_LOGI(TAG, "Wainting screen was initialized");
		}
	}

	// Wifi and NTP
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG_WIFI, "ESP_WIFI_MODE_STA");
	wifi_init_sta();

	Set_SystemTime_SNTP();

	if (xTaskCreate(&task_check_inputs, "CHECK INPUTS", 2048, NULL, 1, NULL))
	{
		if (DEBUG)
		{
			ESP_LOGI(TAG, "Main task is running");
			KILLWAIT = 1;
		}
	}

	while (1)
	{
		Get_current_date_time();
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}