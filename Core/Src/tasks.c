/*
 * tasks.c
 *
 *  Created on: Jul 1, 2025
 *      Author: hatta
 */

#include "tasks.h"

#define RX_BUFFER_SIZE 128
char rx_buffer[RX_BUFFER_SIZE];
int rx_index = 0;

osThreadId_t defaultTaskHandle_esp;
const osThreadAttr_t defaultTask_attributes = { .name = "defaultTask",
		.stack_size = 128 * 8, .priority = (osPriority_t) osPriorityRealtime7, };

osThreadId_t defaultTaskHandle_humidity;
const osThreadAttr_t defaultTask_attributes1 = { .name = "defaultTask1",
		.stack_size = 128 * 8, .priority = (osPriority_t) osPriorityHigh, };

char read_bit(void) {
	char bit;

	HAL_GPIO_WritePin(CLOCK_GPIO_Port, CLOCK_Pin, GPIO_PIN_RESET);
	HAL_Delay(1);

	HAL_GPIO_WritePin(CLOCK_GPIO_Port, CLOCK_Pin, GPIO_PIN_SET);
	HAL_Delay(1);

	bit = (HAL_GPIO_ReadPin(DATA_GPIO_Port, DATA_Pin) == GPIO_PIN_SET) ? 1 : 0;

	HAL_GPIO_WritePin(CLOCK_GPIO_Port, CLOCK_Pin, GPIO_PIN_RESET);
	HAL_Delay(1);

	return bit;
}

void request_time_from_esp8266(void) {
	rx_index = 0;
	memset(rx_buffer, 0, sizeof(rx_buffer));

	while (rx_index < sizeof(rx_buffer) - 1) {
		char byte = 0;
		for (int i = 0; i < 8; i++) {
			byte |= (read_bit() << i);
		}
		// Ignore non-printable chars except newline
		if ((byte >= 32 && byte <= 126)) {
			rx_buffer[rx_index++] = byte;
		}
		if (byte == '\n')
			break;
	}
	rx_buffer[rx_index] = '\0'; // Correct termination
}

void update_stm32_rtc_from_esp8266() {

	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	osDelay(20);
	request_time_from_esp8266();

	int day, month, year, hour, minute, second;
	printf("Raw received string: '%s'\r\n", rx_buffer);
	int parsed = sscanf(rx_buffer, "%d/%d/%d %d:%d:%d", &day, &month, &year,
			&hour, &minute, &second);

	if (parsed == 6) {
		RTC_TimeTypeDef sTime = { 0 };
		RTC_DateTypeDef sDate = { 0 };

		// Fill time struct
		sTime.Hours = hour;
		sTime.Minutes = minute;
		sTime.Seconds = second;
		sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
		sTime.StoreOperation = RTC_STOREOPERATION_RESET;

		// Fill date struct
		sDate.Date = day;
		sDate.Month = month;
		sDate.Year = year - 2000; // HAL expects year offset from 2000
		sDate.WeekDay = RTC_WEEKDAY_MONDAY; // You can calculate correct weekday if needed

		if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
			printf("Failed to set RTC time\r\n");
		}

		if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
			printf("Failed to set RTC date\r\n");
		}

		printf("RTC updated from ESP8266 time: %02d/%02d/%d %02d:%02d:%02d\r\n",
				day, month, year, hour, minute, second);

	} else {
		printf("Failed to parse time string!\r\n");
	}
}
void handle_humidity_task(void *argument) {
	int16_t temperature_cooler = 0, humidity_cooler = 0;
	int16_t temperature_humidifier = 0, humidity_humidifier = 0;
	uint8_t dht_status_cooler, dht_status_humidifier;
	// 🔥 Update RTC once before loop
	//update_stm32_rtc_from_esp8266();

	for (;;) {
		// Get RTC time
		RTC_TimeTypeDef sTime;
		RTC_DateTypeDef sDate;
		HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

		printf("STM32 RTC time: %02d:%02d:%02d\r\n", sTime.Hours, sTime.Minutes,
				sTime.Seconds);

		// --- Read DHT_Cooler ---
		dht_status_cooler = DHT11_Read_Data(GPIOC, GPIO_PIN_1,
				&temperature_cooler, &humidity_cooler);

		if (dht_status_cooler == 1) {
			printf("DHT_Cooler -> Humidity: %d%%, Temperature: %d°C\r\n",
					humidity_cooler, temperature_cooler);

		} else if (dht_status_cooler == 2) {
			printf("DHT_Cooler -> Checksum Error\r\n");
		} else {
			printf("DHT_Cooler -> No Response\r\n");
		}

		// --- Read DHT_Humidifier ---
		dht_status_humidifier = DHT11_Read_Data(GPIOC, GPIO_PIN_0,
				&temperature_humidifier, &humidity_humidifier);

		if (dht_status_humidifier == 1) {
			printf("DHT_Humidifier -> Humidity: %d%%, Temperature: %d°C\r\n",
					humidity_humidifier, temperature_humidifier);
		} else if (dht_status_humidifier == 2) {
			printf("DHT_Humidifier -> Checksum Error\r\n");
		} else {
			printf("DHT_Humidifier -> No Response\r\n");
		}

		// ---------- Control logic ----------
		int hour = sTime.Hours;

		// Check time: Day if between 6 and 20
		if (hour >= 6 && hour < 20) {
			if (humidity_cooler < 50) {
				HAL_GPIO_WritePin(GPIOA, humidifier_pin, GPIO_PIN_SET);
			} else {
				HAL_GPIO_WritePin(GPIOA, humidifier_pin, GPIO_PIN_RESET);
			}
		}
		// Night: between 20 and next day 6
		else {
			if (humidity_cooler < 70) {
				HAL_GPIO_WritePin(GPIOA, humidifier_pin, GPIO_PIN_SET);
			} else {
				HAL_GPIO_WritePin(GPIOA, humidifier_pin, GPIO_PIN_RESET);
			}
		}

		// Extra condition: high humidity > 85%
		if (humidity_cooler > 85) {
			HAL_GPIO_WritePin(GPIOA, H_C_Fan_pin, GPIO_PIN_SET);
		} else {
			HAL_GPIO_WritePin(GPIOA, H_C_Fan_pin, GPIO_PIN_RESET);
		}

		if (humidity_humidifier >= 90) {
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
		} else {
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
		}

		osDelay(1000);
	}

}

void handle_esp_task(void *argument) {

	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	for (;;) {
		// Wait for start condition: data line HIGH for 1 sec
		uint32_t high_start_time = 0;

		while (1) {
			osDelay(1); // Avoid busy-waiting, adjust if needed

			// Check data line state
			uint8_t bit =
					(HAL_GPIO_ReadPin(DATA_GPIO_Port, DATA_Pin) == GPIO_PIN_SET) ?
							1 : 0;

			if (bit) {
				if (high_start_time == 0) {
					high_start_time = HAL_GetTick(); // Start measuring
				} else if ((HAL_GetTick() - high_start_time) >= 1000) {
					// High for >= 1 sec -> start condition met
					printf("Start condition detected!\r\n");

					break;
				}
			} else {
				// Reset timer if line goes low
				high_start_time = 0;
			}

		}

		printf("now receiving command!\r\n");

		osDelay(1000);
		// Reset buffer index at start of each command reception
		rx_index = 0;
		memset(rx_buffer, 0, sizeof(rx_buffer));
		while (1) {
			char byte = 0;

			// Read 8 bits, LSB first
			for (int i = 0; i < 8; i++) {
				byte |= (read_bit() << i);
			}

			// Only keep printable chars or newline
			if ((byte >= 32 && byte <= 126) || byte == '\n') {
				rx_buffer[rx_index++] = byte;

			}
			if (byte == '\n') {
				break; // End of command string
			}
		}

		rx_buffer[rx_index] = '\0'; // Null terminate

		printf("Received command: '%s'\r\n", rx_buffer);

		if (strncmp(rx_buffer, "Humidfier ON", 12) == 0) {
			printf("Humidifier ON command received!\r\n");
			HAL_GPIO_WritePin(GPIOA, humidifier_pin, GPIO_PIN_SET); // Turn humidifier ON

		} else if (strncmp(rx_buffer, "Humidfier OFF", 13) == 0) {
			printf("Humidifier OFF command received!\r\n");
			HAL_GPIO_WritePin(GPIOA, humidifier_pin, GPIO_PIN_RESET); // Turn humidifier OFF

		} else if (strncmp(rx_buffer, "Humidfier Fans ON", 17) == 0) {
			printf("Humidifier Fans ON command received!\r\n");
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);  // Turn fans ON

		} else if (strncmp(rx_buffer, "Humidfier Fans OFF", 18) == 0) {
			printf("Humidifier Fans OFF command received!\r\n");
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET); // Turn fans OFF

		} else if (strncmp(rx_buffer, "End esp Task", 12) == 0) {
			printf("End esp Task command received!\r\n");
			defaultTaskHandle_humidity = osThreadNew(handle_humidity_task, NULL,
					&defaultTask_attributes1);

			osThreadExit();

		}else if (strncmp(rx_buffer, "Download Firmware", 12) == 0) {
			printf("Download Firmware command received!\r\n");
		    HAL_NVIC_SystemReset();
		} else if (strlen(rx_buffer) > 0) {
			printf("Unknown command 	: %s\r\n", rx_buffer);
		}
		osDelay(100);
	}
}

