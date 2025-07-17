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
const osThreadAttr_t handle_esp_task_attributes = { .name = "handle_esp_task",
		.stack_size = 128 * 8, .priority = (osPriority_t) osPriorityHigh6, };

osThreadId_t defaultTaskHandle_humidity;
const osThreadAttr_t handle_humidity_task_attributes = { .name =
		"handle_humidity_task", .stack_size = 128 * 8, .priority =
		(osPriority_t) osPriorityHigh7, };

osThreadId_t TaskHandle_send_data;

const osThreadAttr_t send_data_task_attributes = { .name = "send_data_task",
		.priority = (osPriority_t) osPriorityRealtime, .stack_size = 512 * 4 };

xQueueHandle msgQueue;

void MX_GPIO_Init_USART1_PB6_PB7(void) {
	__HAL_RCC_GPIOB_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	// PB6 - USART1_TX
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	// PB7 - USART1_RX
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void MX_USART1_UART_Init(void) {
	__HAL_RCC_USART1_CLK_ENABLE();

	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;

	if (HAL_UART_Init(&huart1) != HAL_OK) {
		printf("USART1 init failed!\n");
		while (1)
			;
	}
}
void request_time_from_esp8266(void) {
    rx_index = 0;
    memset(rx_buffer, 0, sizeof(rx_buffer));

    const uint32_t totalTimeoutMs = 3000;
    uint32_t startTick = HAL_GetTick();

    while ((HAL_GetTick() - startTick < totalTimeoutMs) && (rx_index < sizeof(rx_buffer) - 1)) {
        uint8_t byte;
        HAL_StatusTypeDef ret = HAL_UART_Receive(&huart1, &byte, 1, 100);

        if (ret == HAL_OK) {
            rx_buffer[rx_index++] = byte;
            if (byte == '\n') break;
        } else if (ret == HAL_TIMEOUT) {
            continue;
        } else {
            printf("❌ UART receive error (ret = %d), resetting UART\r\n", ret);
            goto reset_uart;
        }
    }

    rx_buffer[rx_index] = '\0';

    if (rx_index == 0) {
        printf("⚠️ No data received from ESP8266 after %lu ms\r\n", HAL_GetTick() - startTick);
        goto reset_uart;
    } else {
        printf("📥 Received: '%s'\r\n", rx_buffer);
        return;
    }

reset_uart:
    // Safely reset UART hardware
    HAL_UART_Abort(&huart1);
    HAL_UART_DeInit(&huart1);
    HAL_Delay(100); // Give hardware time to settle
    MX_USART1_UART_Init(); // Reinitialize UART (this must exist in your project)
    printf("🔄 UART reinitialized after timeout/error\r\n");
}

void update_stm32_rtc_from_esp8266() {
    int day, month, year, hour, minute, second;
    int attempts = 0;
    int parsed = 0;

    while (attempts < 10) {
        printf("🔄 Attempt %d to fetch time from ESP8266...\r\n", attempts + 1);

        // Notify ESP8266 to send time
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
        HAL_Delay(50);
        request_time_from_esp8266();
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);

        printf("Raw received string: '%s'\r\n", rx_buffer);
        parsed = sscanf(rx_buffer, "%d/%d/%d %d:%d:%d", &day, &month, &year,
                        &hour, &minute, &second);

        if (parsed == 6) {
            RTC_TimeTypeDef sTime = { 0 };
            RTC_DateTypeDef sDate = { 0 };

            sTime.Hours = hour;
            sTime.Minutes = minute;
            sTime.Seconds = second;
            sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
            sTime.StoreOperation = RTC_STOREOPERATION_RESET;

            sDate.Date = day;
            sDate.Month = month;
            sDate.Year = year - 2000;
            sDate.WeekDay = RTC_WEEKDAY_MONDAY;

            if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
                printf("❌ Failed to set RTC date\r\n");

            if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
                printf("❌ Failed to set RTC time\r\n");

            printf("✅ RTC updated from ESP8266 time: %02d/%02d/%d %02d:%02d:%02d\r\n",
                   day, month, year, hour, minute, second);

            // Send ACK back to ESP
            const char *ackMsg = "ok\n";
            if (HAL_UART_Transmit(&huart1, (uint8_t *)ackMsg, strlen(ackMsg), 1000) != HAL_OK)
                printf("❌ Failed to send ACK to ESP8266\r\n");
            else
                printf("📤 Sent ACK: '%s'\r\n", ackMsg);

            return;
        } else {
            printf("❌ Failed to parse time string on attempt %d\r\n", attempts + 1);
            attempts++;
            HAL_Delay(300);  // Shorter delay between retries
        }
    }

    printf("⛔ Failed to get time after 10 attempts. Rebooting...\r\n");
    HAL_Delay(1000);
    NVIC_SystemReset();
}

void setup_bitbang_pins(void) {
	// Enable GPIO clock
	__HAL_RCC_GPIOA_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	// Configure DATA pin as output push-pull
	GPIO_InitStruct.Pin = DATA_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(DATA_GPIO_Port, &GPIO_InitStruct);

	// Configure CLOCK pin as output push-pull
	GPIO_InitStruct.Pin = CLOCK_Pin;
	HAL_GPIO_Init(CLOCK_GPIO_Port, &GPIO_InitStruct);

	// Initialize both pins low
	HAL_GPIO_WritePin(DATA_GPIO_Port, DATA_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(CLOCK_GPIO_Port, CLOCK_Pin, GPIO_PIN_RESET);
}

void handle_humidity_task(void *argument) {
	int16_t temperature_cooler = 0, humidity_cooler = 0;
	int16_t temperature_humidifier = 0, humidity_humidifier = 0;
	uint8_t dht_status_cooler, dht_status_humidifier;

	// Enable GPIO clock
	__HAL_RCC_GPIOA_CLK_ENABLE();

	// Configure GPIO pins for fans, etc.
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// Init USART1 pins and peripheral
	MX_GPIO_Init_USART1_PB6_PB7();
	MX_USART1_UART_Init();

	// Receive buffer for optional incoming commands
	char rx_buffer[RX_BUFFER_SIZE];
	uint8_t rx_index = 0;
	uint8_t rx_byte;

	for (;;) {
		// Get RTC time
		RTC_TimeTypeDef sTime;
		RTC_DateTypeDef sDate;
		HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

		printf("STM32 RTC time: %02d:%02d:%02d\r\n", sTime.Hours, sTime.Minutes,
				sTime.Seconds);

		// Read DHT_Cooler
		dht_status_cooler = DHT11_Read_Data(GPIOC, GPIO_PIN_1,
				&temperature_cooler, &humidity_cooler);
		if (dht_status_cooler == 1) {
			printf("DHT_Cooler -> Humidity: %d%%, Temperature: %d°C\r\n",
					humidity_cooler, temperature_cooler);
		} else {
			printf("DHT_Cooler -> Error\r\n");
		}

		// Read DHT_Humidifier
		dht_status_humidifier = DHT11_Read_Data(GPIOC, GPIO_PIN_0,
				&temperature_humidifier, &humidity_humidifier);
		if (dht_status_humidifier == 1) {
			printf("DHT_Humidifier -> Humidity: %d%%, Temperature: %d°C\r\n",
					humidity_humidifier, temperature_humidifier);
		} else {
			printf("DHT_Humidifier -> Error\r\n");
		}

		// Format message exactly as requested
		char msg[64];
		snprintf(msg, sizeof(msg), "T1:%dC H1:%d%%; T2:%dC H2:%d%%\n",
				temperature_cooler, humidity_cooler, temperature_humidifier,
				humidity_humidifier);


		// Send over USART1
		HAL_StatusTypeDef ret = HAL_UART_Transmit(&huart1, (uint8_t*) msg,
				strlen(msg), 1000);
		if (ret == HAL_OK) {
			printf("Sent via UART: %s", msg);
		} else {
			printf("UART transmit error: %d\n\r", ret);
		}


		// Receive any possible incoming bytes (optional)
		while (HAL_UART_Receive(&huart1, &rx_byte, 1, 10) == HAL_OK) {
			if (rx_index < RX_BUFFER_SIZE - 1) {
				rx_buffer[rx_index++] = (char) rx_byte;
			}

			// If newline received, print message
			if (rx_byte == '\n') {
				rx_buffer[rx_index] = '\0'; // null terminate
				printf("Received: %s", rx_buffer);
				rx_index = 0; // reset
			}
		}

		// Control logic (same as before)
		int hour = sTime.Hours;
		if (hour >= 6 && hour < 20) {
			HAL_GPIO_WritePin(GPIOA, humidifier_pin,
					humidity_cooler < 50 ? GPIO_PIN_SET : GPIO_PIN_RESET);
		} else {
			HAL_GPIO_WritePin(GPIOA, humidifier_pin,
					humidity_cooler < 70 ? GPIO_PIN_SET : GPIO_PIN_RESET);
		}

		HAL_GPIO_WritePin(GPIOA, H_C_Fan_pin,
				humidity_cooler > 85 ? GPIO_PIN_SET : GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1,
				humidity_humidifier >= 90 ? GPIO_PIN_SET : GPIO_PIN_RESET);

		osDelay(1000);
	}
}

void handle_temp_humidity_send_task(void *argument) {
	int16_t temperature_cooler = 0, humidity_cooler = 0;
	int16_t temperature_humidifier = 0, humidity_humidifier = 0;
	uint8_t dht_status_cooler, dht_status_humidifier;

	// Init USART1 pins and peripheral (if not already)
	MX_GPIO_Init_USART1_PB6_PB7();
	MX_USART1_UART_Init();

	for (;;) {
		// Get RTC time
		RTC_TimeTypeDef sTime;
		RTC_DateTypeDef sDate;
		HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

		printf("STM32 RTC time: %02d:%02d:%02d\r\n", sTime.Hours, sTime.Minutes,
				sTime.Seconds);

		// Read DHT_Cooler
		dht_status_cooler = DHT11_Read_Data(GPIOC, GPIO_PIN_1,
				&temperature_cooler, &humidity_cooler);
		if (dht_status_cooler == 1) {
			printf("DHT_Cooler -> Humidity: %d%%, Temperature: %d°C\r\n",
					humidity_cooler, temperature_cooler);
		} else {
			printf("DHT_Cooler -> Error\r\n");
		}

		// Read DHT_Humidifier
		dht_status_humidifier = DHT11_Read_Data(GPIOC, GPIO_PIN_0,
				&temperature_humidifier, &humidity_humidifier);
		if (dht_status_humidifier == 1) {
			printf("DHT_Humidifier -> Humidity: %d%%, Temperature: %d°C\r\n",
					humidity_humidifier, temperature_humidifier);
		} else {
			printf("DHT_Humidifier -> Error\r\n");
		}

		// Format message
		char msg[64];
		snprintf(msg, sizeof(msg), "T1:%dC H1:%d%%; T2:%dC H2:%d%%\n",
				temperature_cooler, humidity_cooler, temperature_humidifier,
				humidity_humidifier);


		// Send over USART1
		HAL_StatusTypeDef ret = HAL_UART_Transmit(&huart1, (uint8_t*) msg,
				strlen(msg), 1000);
		if (ret == HAL_OK) {
			printf("Sent via UART: %s", msg);
		} else {
			printf("UART transmit error: %d\r\n", ret);
		}


		osDelay(500); // Delay 1 sec (adjust if needed)
	}
}

I2C_HandleTypeDef hi2c1;

#define I2C_SLAVE_ADDRESS (0x42 << 1)  // Adjust slave address here

void MX_GPIO_Init_I2C(void) {
	__HAL_RCC_GPIOB_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9; // PB8=SCL, PB9=SDA
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

void MX_I2C1_Init(void) {
	__HAL_RCC_I2C1_CLK_ENABLE();

	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		printf("I2C1 init failed!\n");
		// handle error or loop here
		for (;;)
			;
	}
}

void handle_esp_task(void *argument) {
	char rx_buffer[RX_BUFFER_SIZE];
	uint8_t rx_index = 0;
	uint8_t rx_byte;

	// Initialize UART (assume huart1 already configured)
	MX_GPIO_Init_USART1_PB6_PB7();
	MX_USART1_UART_Init();

	const char ack_msg[] = "ACK\n";

	for (;;) {
		printf("Waiting for ESP command...\r\n");

		rx_index = 0;
		memset(rx_buffer, 0, sizeof(rx_buffer));

		while (1) {
			if (HAL_UART_Receive(&huart1, &rx_byte, 1, HAL_MAX_DELAY)
					== HAL_OK) {
				if (rx_index < RX_BUFFER_SIZE - 1) {
					rx_buffer[rx_index++] = (char) rx_byte;
				}

				if (rx_byte == '\n') {
					rx_buffer[rx_index] = '\0';
					break;
				}
			}

		}

		printf("Received command: '%s'\r\n", rx_buffer);

		// Process received command
		if (strncmp(rx_buffer, "Humidfier ON", 12) == 0) {
			printf("Humidifier ON command received!\r\n");
			HAL_GPIO_WritePin(GPIOA, humidifier_pin, GPIO_PIN_SET);
			HAL_UART_Transmit(&huart1, (uint8_t*) ack_msg, strlen(ack_msg),
					100);

		} else if (strncmp(rx_buffer, "Humidfier OFF", 13) == 0) {
			printf("Humidifier OFF command received!\r\n");
			HAL_GPIO_WritePin(GPIOA, humidifier_pin, GPIO_PIN_RESET);
			HAL_UART_Transmit(&huart1, (uint8_t*) ack_msg, strlen(ack_msg),
					100);

		} else if (strncmp(rx_buffer, "Humidfier Fans ON", 17) == 0) {
			printf("Humidifier Fans ON command received!\r\n");
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
			HAL_UART_Transmit(&huart1, (uint8_t*) ack_msg, strlen(ack_msg),
					100);

		} else if (strncmp(rx_buffer, "Humidfier Fans OFF", 18) == 0) {
			printf("Humidifier Fans OFF command received!\r\n");
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
			HAL_UART_Transmit(&huart1, (uint8_t*) ack_msg, strlen(ack_msg),
					100);

		} else if (strncmp(rx_buffer, "End esp Task", 12) == 0) {
			printf("End esp Task command received!\r\n");
			HAL_UART_Transmit(&huart1, (uint8_t*) ack_msg, strlen(ack_msg),
					100);
			defaultTaskHandle_humidity = osThreadNew(handle_humidity_task, NULL,
					&handle_humidity_task_attributes);
			osThreadExit();

		} else if (strncmp(rx_buffer, "Download Firmware", 17) == 0) {
			printf("Download Firmware command received!\r\n");
			HAL_UART_Transmit(&huart1, (uint8_t*) ack_msg, strlen(ack_msg),
					100);
			HAL_NVIC_SystemReset();

		} else if (strlen(rx_buffer) > 0) {
			printf("Unknown command: %s\r\n", rx_buffer);
			// Do not send ACK for unknown
		}

		osDelay(1);
	}
}
