/*
 * general.h
 *
 *  Created on: Jul 1, 2025
 *      Author: hatta
 */

#ifndef INC_GENERAL_H_
#define INC_GENERAL_H_

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

void DWT_Delay_us(volatile uint32_t microseconds);
void DWT_Init(void);
void delay(uint16_t time);

extern UART_HandleTypeDef huart2;

#endif /* INC_GENERAL_H_ */
