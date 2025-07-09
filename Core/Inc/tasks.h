/*
 * tasks.h
 *
 *  Created on: Jul 1, 2025
 *      Author: hatta
 */

#ifndef INC_TASKS_H_
#define INC_TASKS_H_

#include "dht.h"

#define CLOCK_GPIO_Port GPIOB
#define CLOCK_Pin GPIO_PIN_1

#define DATA_GPIO_Port GPIOB
#define DATA_Pin GPIO_PIN_2

#define humidifier_pin GPIO_PIN_4
#define H_C_Fan_pin GPIO_PIN_0

extern const osThreadAttr_t defaultTask_attributes1;
extern osThreadId_t defaultTaskHandle_humidity;
extern const osThreadAttr_t defaultTask_attributes;
extern osThreadId_t defaultTaskHandle_esp;

void handle_humidity_task(void *argument);
void handle_esp_task(void *argument);
int get_time_date();

#endif /* INC_TASKS_H_ */
