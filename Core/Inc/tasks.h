/*
 * tasks.h
 *
 *  Created on: Jul 1, 2025
 *      Author: hatta
 */

#ifndef INC_TASKS_H_
#define INC_TASKS_H_

#include "dht.h"
#include "queue.h"

#define CLOCK_GPIO_Port GPIOB
#define CLOCK_Pin GPIO_PIN_1

#define DATA_GPIO_Port GPIOB
#define DATA_Pin GPIO_PIN_2

#define humidifier_pin GPIO_PIN_4
#define H_C_Fan_pin GPIO_PIN_0

#define MSG_QUEUE_LENGTH 5
#define MSG_MAX_SIZE     100

extern const osThreadAttr_t handle_humidity_task_attributes;
extern osThreadId_t defaultTaskHandle_humidity;
extern const osThreadAttr_t handle_esp_task_attributes;
extern osThreadId_t defaultTaskHandle_esp;
extern const osThreadAttr_t send_data_task_attributes;
extern osThreadId_t TaskHandle_send_data;
extern xQueueHandle msgQueue;

void handle_humidity_task(void *argument);
void send_data_task(void *argument);
void handle_esp_task(void *argument);
void request_time_task(void *argument);
void handle_temp_humidity_send_task(void *argument);
int get_time_date();
void update_stm32_rtc_from_esp8266();
#endif /* INC_TASKS_H_ */
