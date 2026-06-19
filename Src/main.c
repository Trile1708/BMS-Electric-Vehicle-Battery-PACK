/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "bq76940.h"
#include "Flash.h"
#include "i2c_lcd.h"
#include "app_bms.h"
#include "stdio.h"
#include "string.h"
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
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

IWDG_HandleTypeDef hiwdg;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
BMS_State_t My_Battery;
extern volatile uint8_t System_Locked;
float Last_Saved_SOC = 100.0f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);	
static void MX_IWDG_Init(void);
/* USER CODE BEGIN PFP */
 uint8_t rx_data_uart[1];
 void BMS_Send_UART_Data(BMS_State_t *bms);
 volatile uint8_t unlock_request = 0;
 volatile uint8_t lcd_next_page_request = 0;
 volatile uint8_t bq_alert_request = 0;
 volatile uint8_t lcd_button_locked = 0;
 //uint32_t last_heartbeat_time = 0;
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_I2C2_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_IWDG_Init();
	HAL_UART_Receive_IT(&huart2, rx_data_uart, 1);
  /* USER CODE BEGIN 2 */
	memset(&My_Battery, 0, sizeof(My_Battery));
  //_Cho_IC_khoi_dong_va_on_dinh_dien_ap
  HAL_Delay(500);
	My_Battery.DesignCapacity_mAh = 30000.0f;
	// Doc % pin cu tu Flash
  Flash_Load_BMS_State(&My_Battery);
	// Cap nhat moc so sanh ngay sau khi doc
  Last_Saved_SOC = My_Battery.SOC;
  My_Battery.Last_Tick_ms = HAL_GetTick();
	My_Battery.SystemState = BMS_STATE_INIT;
	//=====TIMER========
	uint32_t last_bq_time = 0;
  uint32_t last_uart_time = 0;
  uint32_t last_led_time = 0;
  uint32_t last_flash_save_time = 0;
	uint32_t last_lcd_scan_time = 0;
  //=============ZZ
  lcd_init();
  HAL_Delay(50);
  lcd_clear();
	//_2_Khoi_tao_va_xoa_rac_SYS_STAT
  BQ_Init();
  void BQ_Read_Factory_Calibration();
  void LCD_Button_Rearm_Task();
	
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  { 
		/*if (HAL_GetTick() - last_heartbeat_time >= 300)
     {
    last_heartbeat_time = HAL_GetTick();
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
     }*/
		if (bq_alert_request)
     {
       bq_alert_request = 0;

       System_Locked = 1;
       BQ_TurnOff_FETs();
     }
	  LCD_Button_Rearm_Task();
		if (HAL_GetTick() - last_lcd_scan_time >= 200)
       {
        last_lcd_scan_time = HAL_GetTick();
        BMS_Update_LCD(&My_Battery);
       }
		
   if(unlock_request)
    {
    unlock_request = 0;
    BQ_UpdateData(&My_Battery);
	  uint8_t hardware_fault_backup = My_Battery.Hardware_Fault;
	  My_Battery.Hardware_Fault = 0x00;
    if(BMS_Is_System_Safe(&My_Battery))
     {
        BQ_ClearFaults();

        My_Battery.Hardware_Fault = 0x00;
        My_Battery.Software_Fault = SYS_OK;

        System_Locked = 0;
     }
		else 
        {
          My_Battery.Hardware_Fault = hardware_fault_backup;
        }
}
		if (HAL_GetTick() - last_bq_time >= 200)
    {
			last_bq_time = HAL_GetTick();
			
      BQ_UpdateData(&My_Battery);
			
      BMS_Update_State(&My_Battery);
      BMS_Update_LED_Status(&My_Battery);
		  BMS_Auto_Recovery(&My_Battery);
    if (System_Locked == 1 || My_Battery.Hardware_Fault != 0x00 || My_Battery.Software_Fault != SYS_OK) 
      {
         BQ_TurnOff_FETs();
				
        
      } 
			else
				{
      //_Neu_tat_ca_bang_0_he_thong_AN_TOAN
        BQ_TurnOn_FETs();
		// CHI T NH SOC KHI PHAN CUNG B NH THUONG V   OC   NG DU LIEU
        BMS_Calculate_SOC(&My_Battery);
		//  Kiem tra xem co dang cam sac va da day chua de hieu chuan lai 100%
        BMS_Check_Full_Charge(&My_Battery);
		//  Chi luu vao Flash neu do lech >= 1.0%
       if (
            (
             (Last_Saved_SOC - My_Battery.SOC >= 1.0f) ||
             (My_Battery.SOC - Last_Saved_SOC >= 1.0f)
            )
            &&
            (HAL_GetTick() - last_flash_save_time > 30000)
           )
        {
            // Phat hien pin da tut (hoac tang) du 1% -> Thuc hien luu
            Flash_Save_BMS_State(&My_Battery);
            last_flash_save_time = HAL_GetTick();
            // Doi lai cot moc moi
            Last_Saved_SOC = My_Battery.SOC; 
        }
			  } 
				last_bq_time = HAL_GetTick();
    }
		
		  

     if (HAL_GetTick() - last_uart_time >= 1000)
       {
         BMS_Send_UART_Data(&My_Battery);
         last_uart_time = HAL_GetTick();
       }

      if (HAL_GetTick() - last_led_time >= 500) 
         {
          //HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
          last_led_time = HAL_GetTick();
				 }
	       HAL_IWDG_Refresh(&hiwdg); 
  /* USER CODE END 3 */
 }
}
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /*
       STM32G030 chay Vcore scale 1.
    */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    /*
       Dung thach anh ngoai HSE 8MHz.
       Bat LSI de IWDG van dung duoc.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE |
                                       RCC_OSCILLATORTYPE_LSI;

    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;

    /*
       Giu tan so he thong giong HSI noi hien tai:
       HSE = 8 MHz
       SYSCLK = HSE / PLLM * PLLN / PLLR
              = 8 / 1 * 4 / 2
              = 16 MHz

       Nhu vay:
       HCLK  = 16 MHz
       PCLK1 = 16 MHz
       UART/I2C timing it bi anh huong.
    */
    RCC_OscInitStruct.PLL.PLLState  = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM      = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN      = 4;
    RCC_OscInitStruct.PLL.PLLR      = RCC_PLLR_DIV2;

    /*
       KHONG khai bao PLLQ.
       STM32G030 HAL cua ban truoc do bi loi voi PLLQ.
    */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1;

    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    /*
       SYSCLK = 16MHz nen FLASH_LATENCY_0 la du.
    */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}
/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00503D58;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x00503D58;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 1000;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

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
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /* PA0 - nut doi trang LCD */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/* PA1 - input thuong */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PC6 PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
	HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

   HAL_NVIC_SetPriority(EXTI0_1_IRQn, 2, 0);
   HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void LCD_Button_Rearm_Task(void)
  {
    static uint32_t release_tick = 0;

    /*
       Chi xu ly khi nut dang bi lock.
    */
    if (lcd_button_locked)
    {
        /*
           PA0 Pull-up:
           Da nha nut = SET
        */
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
        {
            if (release_tick == 0)
            {
                release_tick = HAL_GetTick();
            }

            /*
               Phai nha on dinh 80 ms moi cho nhan lan tiep theo.
            */
            if ((HAL_GetTick() - release_tick) >= 80)
            {
                lcd_button_locked = 0;
                release_tick = 0;

                /*
                   Xoa co ngat con sot lai neu co.
                */
                __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
            }
        }
        else
        {
            release_tick = 0;
        }
    }
  }
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
 {
    if (GPIO_Pin == GPIO_PIN_8)
     {
        bq_alert_request = 1;
     }
 }
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) 
{
    if (huart->Instance == USART2) 
    {
        if (rx_data_uart[0] == 'U')
          {
           unlock_request = 1;
             }
        HAL_UART_Receive_IT(&huart2, rx_data_uart, 1);
		 }
    }
		// HAM XUAT DU LIEU RA MAY TINH (Monitor)
// =================================================================
#define BMS_CELL_COUNT      15
#define BMS_UART_BUF_SIZE   384

static const char* BMS_State_ToString(BMS_SystemState_t state)
{
    switch (state)
    {
        case BMS_STATE_IDLE:
            return "IDL";

        case BMS_STATE_CHARGING:
            return "CHG";

        case BMS_STATE_DISCHARGING:
            return "DSG";

        case BMS_STATE_FAULT:
            return "FLT";

        case BMS_STATE_SLEEP:
            return "SLP";

        default:
            return "INI";
    }
}

void BMS_Send_UART_Data(BMS_State_t *bms)
{
    char uart_buf[BMS_UART_BUF_SIZE];
    int len = 0;
    int n = 0;

    const char *state_str = BMS_State_ToString(bms->SystemState);

    n = snprintf(uart_buf + len,
                 sizeof(uart_buf) - len,
                 "START,%s,%lu,%ld,%d,%d,%lu,%lu",
                 state_str,
                 (unsigned long)bms->TotalVoltage,
                 (long)bms->PackCurrent,
                 (int)bms->SOC,
                 (int)bms->Temperature[0],
                 (unsigned long)bms->Hardware_Fault,
                 (unsigned long)bms->Software_Fault);

    if (n < 0 || n >= (int)(sizeof(uart_buf) - len))
        return;

    len += n;

    for (int i = 0; i < BMS_CELL_COUNT; i++)
    {
        n = snprintf(uart_buf + len,
                     sizeof(uart_buf) - len,
                     ",%ld",
                     (long)bms->CellVoltage[i]);

        if (n < 0 || n >= (int)(sizeof(uart_buf) - len))
            return;

        len += n;
    }

    n = snprintf(uart_buf + len,
                 sizeof(uart_buf) - len,
                 ",END\r\n");

    if (n < 0 || n >= (int)(sizeof(uart_buf) - len))
        return;

    len += n;

    HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, (uint16_t)len, 100);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    __disable_irq();
}
#ifdef USE_FULL_ASSERT
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
