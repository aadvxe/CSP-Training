/*
 * Copyright (c) 2020-2022 EnduroSat AD. All rights reserved.
 *
 * Contents and presentations are protected world-wide.
 * Any kind of using, copying etc. is prohibited without prior permission.
 */
#ifndef DRV_CSP_CAN_H_
#define DRV_CSP_CAN_H_

/**
 * @addtogroup Drivers
 * @{
 *
 * @defgroup drv_csp_can CSP CAN Driver
 * @{
 * Provide an interface to the CSP library for the CAN communication
 *
 * ### Usage
 *
 * Examples can be found in the official [CSP repository](https://github.com/libcsp/libcsp)
 *
 * @file drv_csp_can.h
 *
 * @brief CAN driver for the CSP library
 *
 * @}
 * @}
 *
*/

#include <stdint.h>
#include <stddef.h>
#include "stm32h7xx_hal.h"
#include <stdbool.h>
#include "csp.h"

/** Size of the CAN data buffer */
#define DRV_CSP_CAN_BUFFER_SZ (8)

/** CAN message type as stored in the fifo queue */
typedef struct
{
    uint32_t id;            /**< CAN ID of the message */
    uint8_t  data_sz;       /**< size of the current message */
    uint8_t  data[DRV_CSP_CAN_BUFFER_SZ]; /**< pointer to the data */
} drv_csp_can_msg_t;

typedef struct
{
    bool     full;
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint32_t last_frame;
    drv_csp_can_msg_t* msgs;
} fs_can_fifo_t;

extern fs_can_fifo_t fs_rx_fifo;

/** CAN Identifier */
typedef uint32_t can_id_t;

/** CAN Frame */
typedef struct {
	/** 32 bit CAN identifier */
	can_id_t id;
	/** Data Length Code */
	uint8_t dlc;
	/**< Frame Data - 0 to 8 bytes */
	union __attribute__((aligned(8))) {
		uint8_t data[8];
		uint16_t data16[4];
		uint32_t data32[2];
	};
} can_frame_t;

/** Type for the user defined RX function */
typedef void(*drv_csp_can_rx_user_clbk_t)(void);

/**
 * Register the function as the RX callback
 * @param callback: callback function to be registered
 *
 * @retval CSP_ERR_INVAL: the function provided is NULL
 * @retval CSP_ERR_NONE: the function is registered as callback
 */
int  drv_csp_can_reg_rx_clbk    (drv_csp_can_rx_user_clbk_t callback);

/**
 * Initialization of the HW peripherals for CAN communication and the interrupt callback
 * @param fifo: queue where to store the messages
 * @param fifo_sz: max size of the queue
 *
 * @retval CSP_ERR_DRIVER: any error while initializing the peripherals
 * @retval CSP_ERR_NONE: everything is initialized correctly
 */
int  drv_csp_can_init  (void);

/**
 * Deinitialization of the HW peripherals for CAN communication
 *
 * @retval CSP_ERR_DRIVER: any error while stopping the peripherals
 * @retval CSP_ERR_NONE: peripherals are disabled
 */
int  drv_csp_can_deinit         (void);

/**
 * Add a CAN message to the queue to be sent
 * @param driver_data: unused
 * @param id: CAN message ID
 * @param data: pointer to the data to be sent
 * @param data_sz: size of the data
 *
 * @retval CSP_ERR_TX: error adding the message to the FIFO queue
 * @retval CSP_ERR_NONE: message successfully added to the FIFO queue
 */
int  drv_csp_can_transmit       (void* driver_data, uint32_t id, const uint8_t* data, uint8_t data_sz);

/**
 * Attempt to receive a message from the receive queue
 * @param msg: pointer where to copy the received message
 *
 * @retval CSP_ERR_INVAL: the pointer to the message is invalid
 * @retval CSP_ERR_TIMEDOUT: there are no messages in the fifo RX queue
 * @retval CSP_ERR_NONE: the message is read from the queue into the pointer
 */
int  drv_csp_can_receive        (drv_csp_can_msg_t* msg);

/**
 * Process an interrupt in the CAN peripheral
 */
void drv_csp_can_it0_irq_handler(void);

void kosonginFIFO (int sz);

#endif /* DRV_CSP_CAN_H_ */
