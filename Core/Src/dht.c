#include "dht.h"

void Set_Pin_Output(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void Set_Pin_Input(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

uint8_t DHT11_Check_Response(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    uint8_t Response = 0;
    DWT_Delay_us(40);
    if (!(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin)))
    {
        DWT_Delay_us(80);
        if ((HAL_GPIO_ReadPin(GPIOx, GPIO_Pin))) Response = 1;
        DWT_Delay_us(80);
    }
    return Response;
}

uint8_t DHT11_Read(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    uint8_t i = 0, j;
    for (j = 0; j < 8; j++)
    {
        while (!(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin))); // wait for high
        DWT_Delay_us(40);
        if (!(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin)))
        {
            i &= ~(1 << (7 - j)); // write 0
        }
        else
        {
            i |= (1 << (7 - j)); // write 1
        }
        while (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin)); // wait for low
    }
    return i;
}

uint8_t DHT11_Read_Data(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, int16_t *temperature, int16_t *humidity)
{
    uint8_t Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2, SUM;

    // Start signal
    Set_Pin_Output(GPIOx, GPIO_Pin);
    HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
    HAL_Delay(18);
    HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET);
    DWT_Delay_us(20);
    Set_Pin_Input(GPIOx, GPIO_Pin);

    if (DHT11_Check_Response(GPIOx, GPIO_Pin))
    {
        Rh_byte1 = DHT11_Read(GPIOx, GPIO_Pin);
        Rh_byte2 = DHT11_Read(GPIOx, GPIO_Pin);
        Temp_byte1 = DHT11_Read(GPIOx, GPIO_Pin);
        Temp_byte2 = DHT11_Read(GPIOx, GPIO_Pin);
        SUM = DHT11_Read(GPIOx, GPIO_Pin);

        if (SUM == (Rh_byte1 + Rh_byte2 + Temp_byte1 + Temp_byte2))
        {
            *humidity = Rh_byte1;   // DHT11 only uses integer part
            *temperature = Temp_byte1;
            return 1;  // Success
        }
        else
        {
            return 2;  // Checksum error
        }
    }
    else
    {
        return 0;  // No response
    }
}
