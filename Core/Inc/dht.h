#ifndef __DHT_H
#define __DHT_H

#include "stm32f4xx_hal.h"
#include "general.h"

// Define your two sensor pins here or elsewhere in your project
#define DHT11_1_PORT GPIOA
#define DHT11_1_PIN  GPIO_PIN_1

#define DHT11_2_PORT GPIOB
#define DHT11_2_PIN  GPIO_PIN_1

void Set_Pin_Output(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void Set_Pin_Input(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

uint8_t DHT11_Check_Response(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
uint8_t DHT11_Read(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
uint8_t DHT11_Read_Data(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, int16_t *temperature, int16_t *humidity);

#endif /* __DHT_H */
