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
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "stdbool.h"
#include "SEGGER_SYSVIEW.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CONTROLLER_ADDR 			0xAF
#define MY_ADDR 					0x01

#define LATCH_PIN 					GPIO_PIN_7
#define CLOCK_PIN 					GPIO_PIN_8
#define DATA_PIN 					GPIO_PIN_6
#define LED7_PORT					GPIOB

#define GREEN_LED_PIN 				GPIO_PIN_2
#define YELLOW_LED_PIN 				GPIO_PIN_1
#define RED_LED_PIN 				GPIO_PIN_3
#define LED_PORT 					GPIOA

//BOARD 4
//#define GREEN_LED_PIN 				GPIO_PIN_3
//#define YELLOW_LED_PIN 				GPIO_PIN_1
//#define RED_LED_PIN 				GPIO_PIN_2
//#define LED_PORT 					GPIOA

#define TIME_LED_GREEN_DEFAULT		15
#define TIME_LED_YELLOW_DEFAULT		5
#define TIME_LED_RED_DEFAULT		20
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
const uint8_t Seg[10] =
		{ 0b00010010, 0b10110111, 0b00011001, 0b10010001, 0b10110100,
				0b11010000, 0b01010000, 0b10010111, 0b00010000, 0b10010000, };

typedef enum {
	LEDColor_UNKNOWN = 0x0000,
	LEDColor_RED = 0x0001,
	LEDColor_YELLOW = 0x0002,
	LEDColor_GREEN = 0x0003
} LED_COLOR;

typedef enum {
	CMD_CYCLE = 0x0000,
	CMD_REQUEST = 0x0001,
	CMD_ACK = 0x0002,
	CMD_RESPONSE = 0x0100
} CMD_CODE;

typedef struct {
	LED_COLOR currentColor;
	int32_t remainSeconds;
	int32_t initSeconds[4];
	bool isResp;
} TRAFFIC_LIGHT_STATION;

TRAFFIC_LIGHT_STATION station = { .currentColor = LEDColor_YELLOW,
		.remainSeconds = 0, .initSeconds = { [LEDColor_UNKNOWN] = 0,
				[LEDColor_RED ] = TIME_LED_RED_DEFAULT, [LEDColor_YELLOW
						] = TIME_LED_YELLOW_DEFAULT, [LEDColor_GREEN
						] = TIME_LED_GREEN_DEFAULT }, .isResp = false };

uint8_t canTxData[8];
CAN_RxHeaderTypeDef rxHeader;
CAN_TxHeaderTypeDef txHeader;
uint8_t canRX[8] = { 0 };
CAN_FilterTypeDef canfil;
uint32_t canMailbox;

QueueHandle_t xQueue_LEDColor;
QueueHandle_t xQueue_LED7Seg;

SemaphoreHandle_t xChangeState;
SemaphoreHandle_t xGetReq;

TimerHandle_t xCANCycle;
TimerHandle_t xCANRequest;

//SemaphoreHandle_t xCANMutex;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
static void MX_USART1_UART_Init(void);
void StartDefaultTask(void const *argument);

/* USER CODE BEGIN PFP */
void LED7_Display(int32_t value);
void LED_SetColor(LED_COLOR color);
void CAN_SendMessage(CMD_CODE cmd, LED_COLOR led_color, uint32_t time_counter);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void TASK_LED7_DisplayValue(void *pvParams);
void TASK_LEDColor_Control(void *pvParams);
void TASK_ManagerPoint(void *pvParams);

void CAN_CycleCallback(TimerHandle_t xTimer);
void CAN_RequestCallback(TimerHandle_t xTimer);
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();
	HAL_Delay(360);

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_CAN_Init();
	MX_USART1_UART_Init();
	/* USER CODE BEGIN 2 */
	txHeader.DLC = 8;
	txHeader.IDE = CAN_ID_EXT;
	txHeader.RTR = CAN_RTR_DATA;
	txHeader.StdId = 0x01;
	txHeader.ExtId = (CONTROLLER_ADDR << 8) | MY_ADDR;
	txHeader.TransmitGlobalTime = DISABLE;

	canfil.FilterBank = 0;
	canfil.FilterMode = CAN_FILTERMODE_IDMASK;
	canfil.FilterFIFOAssignment = CAN_RX_FIFO0;
	canfil.FilterIdHigh = 0;
	canfil.FilterIdLow = 0;
	canfil.FilterMaskIdHigh = 0;
	canfil.FilterMaskIdLow = 0;
	canfil.FilterScale = CAN_FILTERSCALE_32BIT;
	canfil.FilterActivation = ENABLE;
	canfil.SlaveStartFilterBank = 14;

	HAL_CAN_ConfigFilter(&hcan, &canfil); //Initialize CAN Filter
	HAL_CAN_Start(&hcan); //Initialize CAN Bus
	HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING); // Initialize CAN Bus Rx Interrupt
	/* USER CODE END 2 */

	/* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
//	xCANMutex = xSemaphoreCreateMutex();
	/* USER CODE END RTOS_MUTEX */

	/* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	SEGGER_UART_init(500000);
	SEGGER_SYSVIEW_Conf();
	vSetVarulMaxPRIGROUPValue();
	//	SEGGER_SYSVIEW_Start();

	xChangeState = xSemaphoreCreateBinary();
	xGetReq = xSemaphoreCreateBinary();
	/* USER CODE END RTOS_SEMAPHORES */

	/* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
	xCANCycle = xTimerCreate("CANCycle", pdMS_TO_TICKS(100), pdTRUE, (void*) 0,
			CAN_CycleCallback);

	if (xCANCycle != NULL) {
		xTimerStart(xCANCycle, 0);
	}

	xCANRequest = xTimerCreate("CANRequest", pdMS_TO_TICKS(100), pdTRUE,
			(void*) 0, CAN_RequestCallback);
	/* USER CODE END RTOS_TIMERS */

	/* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
	xQueue_LEDColor = xQueueCreate(1, sizeof(LED_COLOR));
	xQueue_LED7Seg = xQueueCreate(1, sizeof(int32_t));
	/* USER CODE END RTOS_QUEUES */

	/* Create the thread(s) */
	/* definition and creation of defaultTask */

	/* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */

	xTaskCreate(TASK_LED7_DisplayValue, "LED7", 128, NULL, 1, NULL);
	xTaskCreate(TASK_LEDColor_Control, "LEDColor", 128, NULL, 2, NULL);
	xTaskCreate(TASK_ManagerPoint, "Manager", 128, NULL, 3, NULL);

	vTaskStartScheduler();
	/* USER CODE END RTOS_THREADS */

	/* Start scheduler */

	/* We should never get here as control is now taken by the scheduler */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief CAN Initialization Function
 * @param None
 * @retval None
 */
static void MX_CAN_Init(void) {

	/* USER CODE BEGIN CAN_Init 0 */

	/* USER CODE END CAN_Init 0 */

	/* USER CODE BEGIN CAN_Init 1 */

	/* USER CODE END CAN_Init 1 */
	hcan.Instance = CAN1;
	hcan.Init.Prescaler = 9;
	hcan.Init.Mode = CAN_MODE_NORMAL;
	hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
	hcan.Init.TimeSeg1 = CAN_BS1_3TQ;
	hcan.Init.TimeSeg2 = CAN_BS2_4TQ;
	hcan.Init.TimeTriggeredMode = DISABLE;
	hcan.Init.AutoBusOff = DISABLE;
	hcan.Init.AutoWakeUp = DISABLE;
	hcan.Init.AutoRetransmission = DISABLE;
	hcan.Init.ReceiveFifoLocked = DISABLE;
	hcan.Init.TransmitFifoPriority = DISABLE;
	if (HAL_CAN_Init(&hcan) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN CAN_Init 2 */

	/* USER CODE END CAN_Init 2 */

}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8,
			GPIO_PIN_RESET);

	/*Configure GPIO pins : PA1 PA2 PA3 */
	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PB6 PB7 PB8 */
	GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void LED7_Display(int32_t value) {
	uint8_t tens = value / 10;
	uint8_t units = value % 10;

	HAL_GPIO_WritePin(LED7_PORT, LATCH_PIN, GPIO_PIN_RESET);

	for (int i = 7; i >= 0; i--) {
		HAL_GPIO_WritePin(LED7_PORT, DATA_PIN, (Seg[units] >> i) & 0x01);
		HAL_GPIO_WritePin(LED7_PORT, CLOCK_PIN, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED7_PORT, CLOCK_PIN, GPIO_PIN_RESET);
	}

	for (int i = 7; i >= 0; i--) {
		HAL_GPIO_WritePin(LED7_PORT, DATA_PIN, (Seg[tens] >> i) & 0x01);
		HAL_GPIO_WritePin(LED7_PORT, CLOCK_PIN, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED7_PORT, CLOCK_PIN, GPIO_PIN_RESET);
	}

	HAL_GPIO_WritePin(LED7_PORT, LATCH_PIN, GPIO_PIN_SET);
}

void LED_SetColor(LED_COLOR color) {
	HAL_GPIO_WritePin(LED_PORT, RED_LED_PIN,
			(color == LEDColor_RED) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED_PORT, YELLOW_LED_PIN,
			(color == LEDColor_YELLOW) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED_PORT, GREEN_LED_PIN,
			(color == LEDColor_GREEN) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void CAN_SendMessage(CMD_CODE cmd, LED_COLOR led_color, uint32_t time_counter) {
//	if (xSemaphoreTake(xCANMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
	canTxData[0] = (cmd >> 8) & 0xFF;
	canTxData[1] = cmd & 0xFF;
	canTxData[2] = (led_color >> 8) & 0xFF;
	canTxData[3] = led_color & 0xFF;
	canTxData[4] = (time_counter >> 24) & 0xFF;
	canTxData[5] = (time_counter >> 16) & 0xFF;
	canTxData[6] = (time_counter >> 8) & 0xFF;
	canTxData[7] = time_counter & 0xFF;

	HAL_CAN_AddTxMessage(&hcan, &txHeader, canTxData, &canMailbox);
//		xSemaphoreGive(xCANMutex);
//	}
}

void TASK_LED7_DisplayValue(void *pvParams) {
	int32_t LED7_Val = -1;
	while (1) {
		if (LED7_Val < 0) {
			xSemaphoreGive(xChangeState);
			xQueueReceive(xQueue_LED7Seg, &LED7_Val, portMAX_DELAY);
		}

		if (LED7_Val == 5)
			xSemaphoreGive(xGetReq);

		if (LED7_Val >= 0) {
			SEGGER_SYSVIEW_PrintfTarget("abc");
			LED7_Display(LED7_Val--);
		}

		station.remainSeconds = LED7_Val;

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void TASK_LEDColor_Control(void *pvParams) {
	LED_COLOR color;
	while (1) {
		if (xQueueReceive(xQueue_LEDColor, &color, portMAX_DELAY) == pdPASS) {
			LED_SetColor(color);
			SEGGER_SYSVIEW_PrintfTarget("hehehe");
			station.currentColor = color;
		}
	}
}

void TASK_ManagerPoint(void *pvParams) {

	int32_t time_counter = 0;
	LED_COLOR color = LEDColor_YELLOW;

	while (1) {
		if (station.remainSeconds <= 2)
			xTimerStop(xCANRequest, 0);

		if (xSemaphoreTake(xGetReq, 0) == pdTRUE
				&& (station.currentColor == LEDColor_YELLOW
						|| station.currentColor == LEDColor_RED)) {
			xTimerStart(xCANRequest, 0);
			station.isResp = false;
		}

		if (xSemaphoreTake(xChangeState, 0) == pdTRUE) {
			if (!station.isResp) {
				station.initSeconds[LEDColor_RED] = TIME_LED_RED_DEFAULT;
				station.initSeconds[LEDColor_YELLOW] = TIME_LED_YELLOW_DEFAULT;
				station.initSeconds[LEDColor_GREEN] = TIME_LED_GREEN_DEFAULT;
			}

			switch (station.currentColor) {
			case LEDColor_YELLOW:
				color = LEDColor_RED;
				time_counter = station.initSeconds[LEDColor_RED] + 1;
				xQueueSend(xQueue_LED7Seg, &time_counter, 0);
				break;
			case LEDColor_RED:
				color = LEDColor_GREEN;
				time_counter = station.initSeconds[LEDColor_GREEN];
				xQueueSend(xQueue_LED7Seg, &time_counter, 0);
				break;
			case LEDColor_GREEN:
			default:
				color = LEDColor_YELLOW;
				time_counter = station.initSeconds[LEDColor_YELLOW];
				xQueueSend(xQueue_LED7Seg, &time_counter, 0);
				break;
			}
			xQueueSend(xQueue_LEDColor, &color, 0);
		}
//		SEGGER_SYSVIEW_PrintfTarget("okoko");
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}
void CAN_CycleCallback(TimerHandle_t xTimer) {
	if (station.remainSeconds != 0xFFFFFFFF)
		CAN_SendMessage(CMD_CYCLE, station.currentColor,
				(uint32_t) station.remainSeconds);
}

void CAN_RequestCallback(TimerHandle_t xTimer) {
	CAN_SendMessage(CMD_REQUEST, station.currentColor,
			(uint32_t) station.remainSeconds);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan1) {
	HAL_CAN_GetRxMessage(hcan1, CAN_RX_FIFO0, &rxHeader, canRX);

	uint8_t destAddr = (rxHeader.StdId >> 8) & 0xFF;
	uint8_t srcAddr = rxHeader.StdId & 0xFF;

	if (destAddr != MY_ADDR || srcAddr != CONTROLLER_ADDR)
		return;

	uint16_t cmd = (canRX[0] << 8) | canRX[1];
	uint16_t ledColor = (canRX[2] << 8) | canRX[3];
	uint32_t timeCounter = (canRX[4] << 24) | (canRX[5] << 16) | (canRX[6] << 8)
			| canRX[7];

	if (cmd != CMD_RESPONSE)
		return;

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTimerStopFromISR(xCANRequest, &xHigherPriorityTaskWoken);

	if (ledColor < LEDColor_RED || ledColor > LEDColor_GREEN)
		return;

	station.isResp = true;
	station.initSeconds[ledColor] = timeCounter;

	CAN_SendMessage(CMD_ACK, station.currentColor,
			(uint32_t) station.remainSeconds);

	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const *argument) {
	/* USER CODE BEGIN 5 */
	/* Infinite loop */
	for (;;) {

	}
	/* USER CODE END 5 */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM1 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	/* USER CODE BEGIN Callback 0 */

	/* USER CODE END Callback 0 */
	if (htim->Instance == TIM1) {
		HAL_IncTick();
	}
	/* USER CODE BEGIN Callback 1 */

	/* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
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
