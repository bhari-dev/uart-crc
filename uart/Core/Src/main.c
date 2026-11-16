/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h> 
#include "stm32h7xx_hal_conf.h"

#include "crc32.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);

/* USER CODE BEGIN PFP */
void PrintTerm(const uint8_t* msg);
void blinkLED(int n);
bool waitForACK();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define MAX_RETRIES 2
#define ACK_TIMEOUT_MS 2000
#define MAX_REPLY_LEN  6
#define RX_FRAME_TIMEOUT  1000

#define BUF_SZ 256
static uint8_t tx_buf[BUF_SZ];   // buffer for PC->Board
static uint8_t rx_buf[BUF_SZ];   // buffer for Board->PC
static uint16_t tx_len = 0;
static uint16_t rx_len = 0;
const uint8_t newline = '\n';
static uint32_t  last_rx_tick = 0;
uint8_t ACK = 0xFF;
uint8_t NACK = 0x00;

uint8_t* NonEmpty = "Msg must contain content\n\r";
static volatile bool waiting_for_ack = false;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  PrintTerm((const uint8_t *)"UART Chat Ready\r\n");
  /* USER CODE END 2 */

  // const uint8_t * msg = "Testing\n";
  // HAL_UART_Transmit(&huart2, (uint8_t*)msg, 9, 100);


  // /* Initialize leds */
  // BSP_LED_Init(LED_GREEN);
  // BSP_LED_Init(LED_BLUE);
  // BSP_LED_Init(LED_RED);

  // /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  // BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  memset(tx_buf, 0, BUF_SZ);
  memset(rx_buf, 0, BUF_SZ);

  while (1)
  {
    uint8_t ch;

    // input form pc and transmitt
    if (HAL_UART_Receive(&huart3, &ch, 1, 100) == HAL_OK)
    {
      // echo
      HAL_UART_Transmit(&huart3, &ch, 1, HAL_MAX_DELAY);

      tx_buf[tx_len++] = ch;

      if (ch == '\r')
      {
        if (tx_len <= 1)
        {
          HAL_UART_Transmit(&huart3, (uint8_t*)NonEmpty, strlen(NonEmpty), 100);
          tx_len = 0;
          continue;
        }
        
        tx_len -= 1;
        CRC32_Append(tx_buf, &tx_len);
        tx_buf[tx_len++] = '\n';
        // show msg to be sent for comparison. 
        HAL_UART_Transmit(&huart3, (uint8_t*)"\r\n", 2, 100);
        HAL_UART_Transmit(&huart3, (uint8_t*)"(1) Sending: ", 13, 100);
        HAL_UART_Transmit(&huart3, tx_buf, tx_len, 100);
        HAL_UART_Transmit(&huart3, (uint8_t*)"\n\r", 2, 100);

        // FlipBit to test if CRC works
        // FlipBit(tx_buf, tx_len, 2, 1);

        
        waiting_for_ack = true;

        HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
        for (uint16_t attempt = 1; attempt <= MAX_RETRIES; attempt++)
        {
          if (waitForACK())
          {
            blinkLED(3);
            break;
          }
          uint16_t i = attempt + 1;
          char sending[13]; 
          sprintf(sending, "Retry (%d)\n\r", i);

          HAL_UART_Transmit(&huart3, (uint8_t*)sending, 11, 100);
          HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100); // send msg again

          if (!waitForACK() && attempt == MAX_RETRIES) {
            PrintTerm((const uint8_t *)"SEND FAILED\r\n");
          }
        }
        
        waiting_for_ack = false;
        tx_len = 0;
        // memset(tx_buf, 0, BUF_SZ);
      }
    }

    uint8_t chrx;
    // recive form board and print 
    if (!waiting_for_ack && HAL_UART_Receive(&huart2, &chrx, 1, 10) == HAL_OK)
    {
      uint32_t now = HAL_GetTick();
      if (rx_len > 0 && (now - last_rx_tick) > RX_FRAME_TIMEOUT) {
          rx_len = 0; // reset buffer if 16B internal FiFo is overfull.
      }
      last_rx_tick = now;
      /* transmit in 16B blocks to allow longer msgs. */
      // retransmit after time out but other board just verry slow. Then other board says ACK but already retransmitt. Maybe just dont retransmit on time out. 
      // Store the new byte
      if (rx_len < BUF_SZ) {
        rx_buf[rx_len++] = chrx;
      }  
      
      
      if (chrx == '\n' || chrx == '\r')
      {
        uint16_t payload_len;

        if (CRC32_ValidateFrame(rx_buf, rx_len, '\n', &payload_len))
        {
            // send only the payload bytes (no CRC, no delim)
            HAL_UART_Transmit(&huart3, (uint8_t*)"No CRC Error\n\r> ", 16, HAL_MAX_DELAY);
            HAL_UART_Transmit(&huart3, rx_buf, payload_len, HAL_MAX_DELAY);
            HAL_UART_Transmit(&huart3, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);

            HAL_UART_Transmit(&huart3, (uint8_t*)"Sending ACK\n\r", 16, 100);
            HAL_UART_Transmit(&huart2, &ACK, 1, HAL_MAX_DELAY);
        }
        else
        {
            // bad packet
            HAL_UART_Transmit(&huart3, (uint8_t*)"CRC ERROR\r\n", 11, HAL_MAX_DELAY);
            HAL_UART_Transmit(&huart3, rx_buf, rx_len, HAL_MAX_DELAY); // rem later just to see errors for now
            
            HAL_UART_Transmit(&huart3, (uint8_t*)"\r\n", 2, 100);
            HAL_UART_Transmit(&huart3, (uint8_t*)"Sending ACK\n\r", 16, 100);
            HAL_UART_Transmit(&huart2, &NACK, 1, HAL_MAX_DELAY); 
        }

        rx_len = 0;
        // memset(rx_buf, 0, BUF_SZ);
      }
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

void blinkLED(int n) {
  for (size_t i = 0; i < n; i++)
  {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_Delay(100);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_Delay(100);
  }  
}

void PrintTerm(const uint8_t *msg)
{
    HAL_UART_Transmit(&huart3, (uint8_t *)msg, strlen((const char *)msg), HAL_MAX_DELAY);
}

bool waitForACK(void)
{
    //uint16_t idx = 0;
    uint32_t start = HAL_GetTick();
    uint8_t ch;

    // Read until newline or timeout
    while ((HAL_GetTick() - start) < ACK_TIMEOUT_MS)
    {
        if (HAL_UART_Receive(&huart2, &ch, 1, 10) == HAL_OK){
        }
    }

    // Compare full string
    if (ch == ACK)
    {
        PrintTerm((const uint8_t *)"ACK received\r\n");
        return true;
    }
    else if (ch == NACK)
    {
        PrintTerm((const uint8_t*)"NACK recived\r\n");
        return false;
    }
    // timeout or garbage → treat as failure
    return false;
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_EnableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
