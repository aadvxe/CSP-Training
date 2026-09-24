/*
 * Copyright (c) 2020-2022 EnduroSat AD. All rights reserved.
 *
 * Contents and presentations are protected world-wide.
 * Any kind of using, copying etc. is prohibited without prior permission.
 */
/**
 * @addtogroup drv_csp_can
 * @{
 *
 * @file     drv_csp_can.c
 * @brief    CAN driver for the CSP library.
 *
 * @}
 */

#include "drv_csp_can.h"
#include "stm32l4xx_hal_can.h"
#include "csp_queue.h"
#include "csp_if_can.h"
#include "cmsis_os.h"
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "csp_if_can.h"

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

#define FS_FDCAN_SZ_2_DATA_SZ(sz) ((sz) >> 16)
#define FS_DATA_SZ_2_FDCAN_SZ(sz) ((sz) << 16)

static csp_queue_handle_t can_rx_queue;

static CAN_HandleTypeDef fs_hcan;
static drv_csp_can_rx_user_clbk_t fs_rx_user_callback;
static uint32_t TxMailbox;

static void fs_can_msp_init_callback(CAN_HandleTypeDef *hcan);
static void fs_can_msp_deinit_callback(CAN_HandleTypeDef *hcan);
static void fs_can_error_callback(CAN_HandleTypeDef *hcan);
static void fs_can_rxfifo0_callback(CAN_HandleTypeDef *hcan, uint32_t rx_fifo0_its);
static void fs_can_enable(bool enable);

osThreadId_t CanRxTaskHandle;
const osThreadAttr_t CanRxTask_attributes = { .name = "canrx", .stack_size = 128
		* 70, .priority = (osPriority_t) osPriorityHigh, };

void csp_can_rx_task(void *argument);

int drv_csp_can_reg_rx_clbk(drv_csp_can_rx_user_clbk_t callback) {
	if (NULL == callback) {
		return CSP_ERR_INVAL;
	}

	fs_rx_user_callback = callback;

	return CSP_ERR_NONE;
}

int drv_csp_can_init(void) {

	fs_hcan.Instance = CAN1;
	fs_hcan.Init.Prescaler = 20;
	fs_hcan.Init.Mode = CAN_MODE_NORMAL;
	fs_hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
	fs_hcan.Init.TimeSeg1 = CAN_BS1_2TQ;
	fs_hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
	fs_hcan.Init.TimeTriggeredMode = DISABLE;
	fs_hcan.Init.AutoBusOff = DISABLE;
	fs_hcan.Init.AutoWakeUp = DISABLE;
	fs_hcan.Init.AutoRetransmission = ENABLE;
	fs_hcan.Init.ReceiveFifoLocked = DISABLE;
	fs_hcan.Init.TransmitFifoPriority = DISABLE;

	if (HAL_OK
			!= HAL_CAN_RegisterCallback(&fs_hcan, HAL_CAN_MSPINIT_CB_ID,
					HAL_CAN_MspInit)) {
		return CSP_ERR_DRIVER;
	}

	if (HAL_OK != HAL_CAN_Init(&fs_hcan)) {
		return CSP_ERR_DRIVER;
	}

	if (HAL_OK
			!= HAL_CAN_RegisterCallback(&fs_hcan, HAL_CAN_ERROR_CB_ID, HAL_CAN_MspInit)) {
		return CSP_ERR_DRIVER;
	}

	if (HAL_CAN_RegisterCallback(&fs_hcan, HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID, fs_can_rxfifo0_callback)
			!= HAL_OK) {
		return CSP_ERR_DRIVER;
	}
	/*
	 if (HAL_OK != HAL_CAN_RegisterCallback(&fs_hcan, fs_can_rxfifo0_callback))
	 {
	 return false;
	 }
	 */

	/*
	 if (HAL_OK != HAL_FDCAN_ConfigInterruptLines(&fs_hcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, FDCAN_INTERRUPT_LINE0))
	 {
	 return false;
	 }
	 */



	if (HAL_CAN_ActivateNotification(&fs_hcan, CAN_IT_RX_FIFO0_MSG_PENDING)
			!= HAL_OK) {
		/* Notification Error */
		Error_Handler();
	}

	if (HAL_OK != HAL_CAN_Start(&fs_hcan)) {
		return CSP_ERR_DRIVER;
	}

	if (HAL_OK != drv_csp_can_reg_rx_clbk(drv_csp_can_receive)) {
		return CSP_ERR_DRIVER;
	}

	/* Configure extended ID reception filter to Rx FIFO 0 */
	CAN_FilterTypeDef sFilterConfig;
	sFilterConfig.FilterBank = 0;
	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	sFilterConfig.FilterIdHigh = 0x00;
	sFilterConfig.FilterIdLow = 0x0000;
	sFilterConfig.FilterMaskIdHigh = 0x00;
	sFilterConfig.FilterMaskIdLow = 0x0000;
	sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
	sFilterConfig.FilterActivation = ENABLE;
	sFilterConfig.SlaveStartFilterBank = 14;

	if (HAL_CAN_ConfigFilter(&fs_hcan, &sFilterConfig) != HAL_OK) {
		return CSP_ERR_DRIVER;
	}

	can_rx_queue = csp_queue_create(100, sizeof(can_frame_t));
	if (can_rx_queue == NULL) {
		csp_log_error("Failed to create CAN RX queue\r\n");
		return CSP_ERR_NOMEM;
	}

	CanRxTaskHandle = osThreadNew(csp_can_rx_task, NULL, &CanRxTask_attributes);

	HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);

	return CSP_ERR_NONE;
}

int drv_csp_can_deinit(void) {
	fs_can_enable(false);

	if (HAL_OK
			!= HAL_CAN_RegisterCallback(&fs_hcan, HAL_CAN_MSPDEINIT_CB_ID,
					fs_can_msp_deinit_callback)) {
		return CSP_ERR_DRIVER;
	}

	if (HAL_OK != HAL_CAN_DeInit(&fs_hcan)) {
		return CSP_ERR_DRIVER;
	}

	fs_rx_fifo.full = false;
	fs_rx_fifo.head = 0;
	fs_rx_fifo.tail = 0;
	fs_rx_fifo.size = 0;
	fs_rx_fifo.msgs = NULL;

	return CSP_ERR_NONE;
}

static uint8_t TxDataW[8] = { 0 };
int drv_csp_can_transmit(void *driver_data, uint32_t id, const uint8_t *data,
		uint8_t data_sz) {
	if (NULL == data || DRV_CSP_CAN_BUFFER_SZ < data_sz) {
		return CSP_ERR_INVAL;
	}
	//uint8_t  TxDataW[8]={0};
	int i;
	for (i = 0; i < data_sz; i++) {
		TxDataW[i] = data[i];
	}

	CAN_TxHeaderTypeDef header = { .StdId = 0x103, .ExtId = 0, .RTR =
			CAN_RTR_DATA, .IDE = CAN_ID_STD, .DLC = data_sz, .TransmitGlobalTime =
			DISABLE, };

	if (HAL_OK
			!= HAL_CAN_AddTxMessage(&fs_hcan, &header, TxDataW,
					(uint32_t*) 0)) {
		return CSP_ERR_TX;
	}

	return CSP_ERR_NONE;
}

int drv_csp_can_receive(drv_csp_can_msg_t *msg) {

	if (NULL == msg) {
		return CSP_ERR_INVAL;
	}

}

void drv_csp_can_it0_irq_handler(void) {
	HAL_CAN_IRQHandler(&fs_hcan);
}

static void fs_can_msp_deinit_callback(CAN_HandleTypeDef *hcan) {
	if (CAN1 == hcan->Instance) {
		__HAL_RCC_FDCAN_CLK_DISABLE();

		HAL_GPIO_DeInit(GPIOH, GPIO_PIN_14);
		HAL_GPIO_DeInit(GPIOD, GPIO_PIN_1);

		HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
	}
}

static void fs_can_enable(bool enable) {
	GPIO_InitTypeDef gpio_init_struct = { 0 };

	__HAL_RCC_GPIOA_CLK_ENABLE();

	if (enable) {
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
	} else {
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
	}

	gpio_init_struct.Pin = GPIO_PIN_8;
	gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
	gpio_init_struct.Pull = GPIO_NOPULL;
	gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &gpio_init_struct);
}

// check csp_if_can.c for more info. if we don't use this we don't know when
// a given message has been fully transmitted. This leads to too many interrupts.
#define FS_IS_LAST_MSG(id) (0 == CFP_REMAIN(id))

static int cek = 1, cek2 = 1;
static void fs_can_rxfifo0_callback(CAN_HandleTypeDef *hcan,
		uint32_t rx_fifo0_its) {   //drv_csp_can_msg_t* msg_head;
//packet = csp_buffer_get(100);
	if (CAN1 == hcan->Instance) {
/*		if (__HAL_CAN_GET_FLAG(&fs_hcan, CAN_FLAG_FOV0) == RESET) {
			return;
		}*/
		//csp_packet_t packet;
		can_frame_t frame;
		CAN_RxHeaderTypeDef header;
		uint8_t data[DRV_CSP_CAN_BUFFER_SZ] = { 0 };



		if (HAL_CAN_GetRxMessage(&fs_hcan, CAN_RX_FIFO0, &header, data)	!= HAL_OK) {
			return;
		}

        if (!IS_CAN_DLC(header.DLC))
        {
            return;
        }


		frame.dlc = header.DLC;
		frame.id = header.StdId;
		memcpy(frame.data, data, DRV_CSP_CAN_BUFFER_SZ);

		// Enqueue the received frame into RX queue
		if (cek2 == 34) {
			if (frame.data[7] == 0x3d)
				cek2 = 1;
		}
		cek2++;
		int ret = csp_queue_enqueue_isr(can_rx_queue, &frame, 0);
		if (ret == CSP_QUEUE_OK) {
			int b = 0;
		} else {
			int b = 0;
		}

	}
}

void csp_can_rx_task(void *argument) {

	int ret;
	can_frame_t frame;
	//csp_packet_t packet;

	while (1) {

		ret = csp_queue_dequeue(can_rx_queue, &frame, 1000);

		if (ret == CSP_QUEUE_OK) {
			//csp_can_process_frame(&frame);
			/*
			 if(cek==33) {
			 int a =0;
			 }
			 if(cek==34) {
			 cek=1;
			 }
			 cek++;
			 */
			csp_can_rx(&csp_if_can, frame.id, &frame.data, frame.dlc, 0);
		} else {
			//pbuf_cleanup();
			continue;
		}
		vTaskDelay(10);
	}

	//vTaskDelete(NULL);

}

