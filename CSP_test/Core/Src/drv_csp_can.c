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
#include "stm32h7xx_hal_fdcan.h"
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

static FDCAN_HandleTypeDef        fs_hfdcan;
static drv_csp_can_rx_user_clbk_t fs_rx_user_callback;

static void fs_can_msp_init_callback  (FDCAN_HandleTypeDef* hfdcan);
static void fs_can_msp_deinit_callback(FDCAN_HandleTypeDef* hfdcan);
static void fs_can_error_callback     (FDCAN_HandleTypeDef* hfdcan);
static void fs_can_rxfifo0_callback   (FDCAN_HandleTypeDef* hfdcan, uint32_t rx_fifo0_its);
static void fs_can_enable             (bool enable);

osThreadId_t CanRxTaskHandle;
const osThreadAttr_t CanRxTask_attributes = {
  .name = "canrx",
  .stack_size = 128 * 70,
  .priority = (osPriority_t) osPriorityHigh,
};

void csp_can_rx_task(void *argument);

int drv_csp_can_reg_rx_clbk(drv_csp_can_rx_user_clbk_t callback)
{
    if (NULL == callback)
    {
        return CSP_ERR_INVAL;
    }

    fs_rx_user_callback = callback;

    return CSP_ERR_NONE;
}


int drv_csp_can_init(void)
{

	  fs_hfdcan.Instance 					= FDCAN1;
	  fs_hfdcan.Init.FrameFormat 			= FDCAN_FRAME_CLASSIC;
	  fs_hfdcan.Init.Mode					= FDCAN_MODE_NORMAL;
	  fs_hfdcan.Init.AutoRetransmission 	= ENABLE;
	  fs_hfdcan.Init.TransmitPause			= DISABLE;
	  fs_hfdcan.Init.ProtocolException 		= ENABLE;
	  fs_hfdcan.Init.NominalPrescaler 		= 10;
	  fs_hfdcan.Init.NominalSyncJumpWidth 	= 1;
	  fs_hfdcan.Init.NominalTimeSeg1 		= 2;
	  fs_hfdcan.Init.NominalTimeSeg2 		= 2;
	  fs_hfdcan.Init.DataPrescaler 			= 1;
	  fs_hfdcan.Init.DataSyncJumpWidth 		= 1;
	  fs_hfdcan.Init.DataTimeSeg1 			= 1;
	  fs_hfdcan.Init.DataTimeSeg2 			= 1;
	  fs_hfdcan.Init.MessageRAMOffset 		= 0;
	  fs_hfdcan.Init.StdFiltersNbr 			= 0;
	  fs_hfdcan.Init.ExtFiltersNbr 			= 0;
	  fs_hfdcan.Init.RxFifo0ElmtsNbr 		= 0;
	  fs_hfdcan.Init.RxFifo0ElmtSize 		= FDCAN_DATA_BYTES_8;
	  fs_hfdcan.Init.RxFifo1ElmtsNbr 		= 64;
	  fs_hfdcan.Init.RxFifo1ElmtSize 		= FDCAN_DATA_BYTES_8;
	  fs_hfdcan.Init.RxBuffersNbr 			= 0;
	  fs_hfdcan.Init.RxBufferSize 			= FDCAN_DATA_BYTES_8;
	  fs_hfdcan.Init.TxEventsNbr 			= 0;
	  fs_hfdcan.Init.TxBuffersNbr 			= 0;
	  fs_hfdcan.Init.TxFifoQueueElmtsNbr  	= 32;
	  fs_hfdcan.Init.TxFifoQueueMode 		= FDCAN_TX_FIFO_OPERATION;
	  fs_hfdcan.Init.TxElmtSize 			= FDCAN_DATA_BYTES_8;
	  if (HAL_OK != HAL_FDCAN_RegisterCallback(&fs_hfdcan, HAL_FDCAN_MSPINIT_CB_ID, HAL_FDCAN_MspInit))
	     {
	           return CSP_ERR_DRIVER;
	     }

	     if (HAL_OK != HAL_FDCAN_Init(&fs_hfdcan))
	     {
	         return CSP_ERR_DRIVER;
	     }

	     if (HAL_OK != HAL_FDCAN_RegisterCallback(&fs_hfdcan, HAL_FDCAN_ERROR_CALLBACK_CB_ID, HAL_FDCAN_MspInit))
	     {
	         return CSP_ERR_DRIVER;
	     }

	     if (HAL_OK != HAL_FDCAN_RegisterRxFifo0Callback(&fs_hfdcan, fs_can_rxfifo0_callback))
	     {
	         return false;
	     }

	     if (HAL_OK != HAL_FDCAN_ConfigInterruptLines(&fs_hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, FDCAN_INTERRUPT_LINE0))
	     {
	         return false;
	     }

	     if (HAL_FDCAN_ActivateNotification(&fs_hfdcan, FDCAN_IT_TX_COMPLETE | FDCAN_IT_TX_FIFO_EMPTY, 0xFFFFFFFF) != HAL_OK)
	       {

	     	return false;

	       }

	     if (HAL_OK != HAL_FDCAN_ActivateNotification(&fs_hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0))
	     {
	         return false;
	     }

	     if (HAL_OK != HAL_FDCAN_Start(&fs_hfdcan))
	     {
	         return CSP_ERR_DRIVER;
	     }

	     if (HAL_OK != drv_csp_can_reg_rx_clbk(drv_csp_can_receive))
	     {
	        return CSP_ERR_DRIVER;
	     }


	     FDCAN_FilterTypeDef sFilterConfig;
	      /* Configure extended ID reception filter to Rx FIFO 0 */
	     sFilterConfig.IdType		 = FDCAN_STANDARD_ID;
	     sFilterConfig.FilterIndex    = 0;
	     sFilterConfig.FilterType 	 = FDCAN_FILTER_MASK;
	     sFilterConfig.FilterConfig   = FDCAN_FILTER_TO_RXFIFO0;
	     sFilterConfig.FilterID1 	 = OBC_ADDRESS;
	     sFilterConfig.FilterID2 	 = OBC_ADDRESS;
	     sFilterConfig.RxBufferIndex = 0;
	     if (HAL_FDCAN_ConfigFilter(&fs_hfdcan, &sFilterConfig) != HAL_OK)
	     {
	    	  return CSP_ERR_DRIVER;
	     }

	     can_rx_queue = csp_queue_create(900, sizeof(can_frame_t));
	     	if (can_rx_queue == NULL) {
	     		csp_log_error("Failed to create CAN RX queue\r\n");
	     		return CSP_ERR_NOMEM;
	    	}

	     CanRxTaskHandle = osThreadNew(csp_can_rx_task, NULL, &CanRxTask_attributes);

	     HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 5, 0);
	     HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

    return CSP_ERR_NONE;
}


int drv_csp_can_deinit(void)
{
    fs_can_enable(false);

    if (HAL_OK != HAL_FDCAN_RegisterCallback(&fs_hfdcan, HAL_FDCAN_MSPDEINIT_CB_ID, fs_can_msp_deinit_callback))
    {
        return CSP_ERR_DRIVER;
    }

    if (HAL_OK != HAL_FDCAN_DeInit(&fs_hfdcan))
    {
        return CSP_ERR_DRIVER;
    }

    fs_rx_fifo.full = false;
    fs_rx_fifo.head = 0;
    fs_rx_fifo.tail = 0;
	fs_rx_fifo.size = 0;
    fs_rx_fifo.msgs = NULL;

    return CSP_ERR_NONE;
}

static uint8_t  TxDataW[8]={0};
int drv_csp_can_transmit(void* driver_data, uint32_t id, const uint8_t* data, uint8_t data_sz)
{
    if (NULL == data || DRV_CSP_CAN_BUFFER_SZ < data_sz)
    {
        return CSP_ERR_INVAL;
    }
    //uint8_t  TxDataW[8]={0};
    int i;
    for (i=0;i<data_sz; i++) {
    	TxDataW[i] = data [i];
    }

    FDCAN_TxHeaderTypeDef header =
    {
        .Identifier          = id,
        .IdType              = FDCAN_STANDARD_ID,
        .TxFrameType         = FDCAN_DATA_FRAME,
        .DataLength          = data_sz,
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .BitRateSwitch       = FDCAN_BRS_OFF,
        .FDFormat            = FDCAN_FD_CAN,
        .TxEventFifoControl  = FDCAN_NO_TX_EVENTS,
        .MessageMarker       = 0,
    };

    if (HAL_OK != HAL_FDCAN_AddMessageToTxFifoQ(&fs_hfdcan, &header, &TxDataW))
    {
        return CSP_ERR_TX;
    }

    return CSP_ERR_NONE;
}


int drv_csp_can_receive(drv_csp_can_msg_t* msg)
{

    if (NULL == msg)
    {
        return CSP_ERR_INVAL;
    }

}


void drv_csp_can_it0_irq_handler(void)
{
    HAL_FDCAN_IRQHandler(&fs_hfdcan);
}



static void fs_can_msp_deinit_callback(FDCAN_HandleTypeDef* hfdcan)
{
    if (FDCAN1 == hfdcan->Instance)
    {
        __HAL_RCC_FDCAN_CLK_DISABLE();

        HAL_GPIO_DeInit(GPIOH, GPIO_PIN_14);
        HAL_GPIO_DeInit(GPIOD, GPIO_PIN_1);

        HAL_NVIC_DisableIRQ(FDCAN1_IT0_IRQn);
    }
}


static void fs_can_enable(bool enable)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    if (enable)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    }

    gpio_init_struct.Pin   = GPIO_PIN_8;
    gpio_init_struct.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull  = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);
}


// check csp_if_can.c for more info. if we don't use this we don't know when
// a given message has been fully transmitted. This leads to too many interrupts.
#define FS_IS_LAST_MSG(id) (0 == CFP_REMAIN(id))

static int cek=1, cek2=1;
static void fs_can_rxfifo0_callback(FDCAN_HandleTypeDef* hfdcan, uint32_t rx_fifo0_its)
{   //drv_csp_can_msg_t* msg_head;
	//packet = csp_buffer_get(100);
    if (FDCAN1 == hfdcan->Instance)
    {
        if (RESET == (rx_fifo0_its & FDCAN_IT_RX_FIFO0_NEW_MESSAGE))
        {
            return;
        }
        //csp_packet_t packet;
        can_frame_t frame;
        FDCAN_RxHeaderTypeDef header;
        uint8_t data[DRV_CSP_CAN_BUFFER_SZ]={0};

        if (HAL_OK != HAL_FDCAN_GetRxMessage(&fs_hfdcan, FDCAN_RX_FIFO0, &header, data))
        {
            return;
        }

        if (HAL_FDCAN_ActivateNotification(&fs_hfdcan, FDCAN_IT_TX_COMPLETE | FDCAN_IT_TX_FIFO_EMPTY, 0xFFFFFFFF) != HAL_OK)
          {

        	return;

          }

        if (HAL_OK != HAL_FDCAN_ActivateNotification(&fs_hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0))
        {
            return;
        }

        if (!IS_FDCAN_DLC(header.DataLength))
        {
            return;
        }

        frame.dlc = FS_FDCAN_SZ_2_DATA_SZ(header.DataLength);
        frame.id  = header.Identifier;
        memcpy(frame.data, data, DRV_CSP_CAN_BUFFER_SZ);

	   if(cek2==34) {
			if (frame.data[7]==0x3d)cek2=1;
		}
        			cek2++;
        int ret = csp_queue_enqueue_isr(can_rx_queue, &frame, 0);
        if(ret==CSP_QUEUE_OK){
			int b =0;
        }else {
        	int b =0;
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



