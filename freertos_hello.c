/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"

/* Freescale includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"

#include "pin_mux.h"
#include <stdbool.h>
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_ftm.h"
#include "fsl_tsi_v4.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define DEBUG 0

/* Task priorities. */
#define hello_task_PRIORITY (configMAX_PRIORITIES - 1)
#define TOUCH_SENSOR_THRESHOLD  (1000)

#define NOTE_ON     (0x90)
#define NOTE_OFF    (0x80)
#define SPEED       100

#if DEBUG
#define SEND_MIDI_NOTE_ON(note)     PRINTF("NOTE_ON\r\n");\
                                    PRINTF("%i\r\n",note);\
                                    PRINTF("SPEED\r\n");

#define SEND_MIDI_NOTE_OFF(note)    PRINTF("NOTE_OFF\r\n");\
                                    PRINTF("%i\r\n",note);\
                                    PRINTF("SPEED\r\n");
#else
#define SEND_MIDI_NOTE_ON(note)     PUTCHAR(NOTE_ON);\
                                    PUTCHAR(note);\
                                    PUTCHAR(SPEED); //PRINTF("NOTE ON:%i\r\n",note)

#define SEND_MIDI_NOTE_OFF(note)    vTaskDelay(50);\
                                    PUTCHAR(NOTE_OFF);\
                                    PUTCHAR(note);\
                                    PUTCHAR(SPEED); //PRINTF("NOTE OFF:%i\r\n",note)
#endif

#define CHANGE_MIDI_NOTE(last_note, note)       SEND_MIDI_NOTE_OFF(last_note);\
                                                SEND_MIDI_NOTE_ON(note);\
                                                last_note = note;
typedef enum
{
    IDLE,
    TOUCH_PRESSED,
    TOUCH_RELEASED,
    SEND_MIDI_ON,
    SEND_MIDI_OFF
}states_t;

typedef enum
{
    PRESSED,
    NOT_PRESSED
}touch_state_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void theremin_task(void *pvParameters);

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief Application entry point.
 */

/*******************************************************************************
 * Global variables
 *******************************************************************************/
static uint32_t us_distance_pulse_width = 0;
tsi_calibration_data_t tsi_calibration_buffer;
static states_t state_machine = IDLE;
static touch_state_t last_touch_state = NOT_PRESSED;

SemaphoreHandle_t xSemaphore = NULL;
static BaseType_t xHigherPriorityTaskWoken;


/*******************************************************************************
 * Function declarations
 *******************************************************************************/

void FTM1_IRQHandler(void)
{
    static uint32_t pulse_init = 0;
    static uint32_t ov = 0;

    static uint8_t first_edge = 0;

    if( kFTM_TimeOverflowFlag == (FTM_GetStatusFlags(FTM1) & kFTM_TimeOverflowFlag) )
    {
        FTM_ClearStatusFlags(FTM1, kFTM_TimeOverflowFlag);
        ov++;
    }

    if ((FTM_GetStatusFlags(FTM1) & kFTM_Chnl0Flag))
    {
        FTM_ClearStatusFlags(FTM1, kFTM_Chnl0Flag);

        // First edge.
        if(last_touch_state == PRESSED)
        {
            if ( 0 == first_edge )
            {
                pulse_init = FTM1->CNT;
                ov = 0;
                first_edge = 1;
            }
            else
            {
                // Falling edge.
                // Store result
//                xSemaphoreTakeFromISR(xSemaphore,portMAX_DELAY);
                us_distance_pulse_width = (FTM1->CNT + ov * 0xFFFF) - pulse_init;
                FTM1->CNT = 0;
                first_edge = 0;
                pulse_init = 0;
//                xSemaphoreGiveFromISR(xSemaphore,&xHigherPriorityTaskWoken);
//                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//                PRINTF("pulse: %i\r\n",us_distance_pulse_width);
            }
        }
        else
        {
            // reset flags
            us_distance_pulse_width = 0;
            FTM1->CNT = 0;
            first_edge = 0;
            pulse_init = 0;
        }
    }
}

void custom_delay(uint32_t interval)
{
    volatile uint32_t   i = 0;
    while(i < interval )
    {
        i++;
    }
    i = 0;
}

/*!
 * @brief Task responsible for printing of "Hello world." message.
 */
uint8_t read_touch_sensor()
{
    uint8_t ret = false;

    // Read touch sensor status
    TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
    TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_1);
    TSI_StartSoftwareTrigger(TSI0);
    while (!(TSI_GetStatusFlags(TSI0) & kTSI_EndOfScanFlag))
    {
    }

    if(TSI_GetCounter(TSI0) > (tsi_calibration_buffer.calibratedData[BOARD_TSI_ELECTRODE_1] + TOUCH_SENSOR_THRESHOLD))
    {
        ret = true;
    }

    return ret;
}
#define MIN_OFFSET (30)
#define MAX_OFFSET (765)
uint32_t get_note()
{
    int32_t adjust;

//    xSemaphoreTake(xSemaphore, portMAX_DELAY);
    adjust = us_distance_pulse_width - MIN_OFFSET;
//    xSemaphoreGive(xSemaphore);

    if((adjust < 0) || (adjust > MAX_OFFSET))
    {
        return 0;
    }
    else
    {
        adjust = adjust / 3;
    }


    return adjust;
}

uint8_t get_filtered_note()
{
    uint8_t ret = us_distance_pulse_width;
    if(ret > 255)
    {
        ret = 255;
    }
    return ret;
}

static void theremin_task(void *pvParameters)
{
    static uint8_t last_note = 0;
    for (;;)
    {
        switch(state_machine)
        {
        case IDLE:

            if(true == read_touch_sensor())
            {
                state_machine = TOUCH_PRESSED;
            }
            else
            {
                state_machine = TOUCH_RELEASED;
            }

            break;
        case TOUCH_PRESSED:
            if(last_touch_state == NOT_PRESSED)
            {
                last_touch_state = PRESSED;
                last_note = get_note();
                SEND_MIDI_NOTE_ON(last_note);
            }
            else
            {
                // Hold note
                CHANGE_MIDI_NOTE(last_note, get_note());
            }
            state_machine = IDLE;
            break;
        case TOUCH_RELEASED:
            if(last_touch_state == PRESSED)
            {
                last_touch_state = NOT_PRESSED;
                SEND_MIDI_NOTE_OFF(last_note);
            }
            else
            {
                // Hold note
            }
            state_machine = IDLE;
            break;
        default:
            break;
        }


        //Update trigger
        GPIO_WritePinOutput(GPIOC, 0, 1);
        custom_delay(200);
        GPIO_WritePinOutput(GPIOC, 0, 0);

        //Uodate LED
        if(last_touch_state == PRESSED)
        {
            GPIO_WritePinOutput(GPIOC, 8, 0);
        }
        else
        {
            GPIO_WritePinOutput(GPIOC, 8, 1);
        }

        vTaskDelay(50);

    }
}

void configure_peripherals(void)
{

    gpio_pin_config_t out_config =
    {
            kGPIO_DigitalOutput,
            0,
    };

    gpio_pin_config_t LED_config =
    {
            kGPIO_DigitalOutput,
            1,
    };

    /* Sets the configuration for GPIOs */
    // Init HC-SR04 trigger
    GPIO_PinInit(GPIOC,0,&out_config);
    // Init LED
    GPIO_PinInit(GPIOC,8,&LED_config);


    /* Set the configuration for FTM (echo)*/
    ftm_config_t ftmInfo;
    FTM_GetDefaultConfig(&ftmInfo);
    ftmInfo.prescale = kFTM_Prescale_Divide_128;
    /* Initializes FTM.*/
    FTM_Init(FTM1, &ftmInfo);

    FTM_SetupInputCapture(FTM1,kFTM_Chnl_0,kFTM_RiseAndFallEdge,0);
    FTM1->MOD = 0xffff;
    FTM1->CNTIN = 0;
    FTM_EnableInterrupts(FTM1,kFTM_Chnl0InterruptEnable);
    EnableIRQ(FTM1_IRQn);
    FTM_StartTimer(FTM1, kFTM_SystemClock);

    /* Set configuration for touch sensor */
    tsi_config_t tsiConfig_normal = {0};

    TSI_GetNormalModeDefaultConfig(&tsiConfig_normal);
    TSI_Init(TSI0, &tsiConfig_normal);
    EnableIRQ(TSI0_IRQn);
    TSI_EnableModule(TSI0, true);



    /*********  TEST CALIBRATION PROCESS ************/
    memset((void *)&tsi_calibration_buffer, 0, sizeof(tsi_calibration_buffer));
    TSI_Calibrate(TSI0, &tsi_calibration_buffer);
//    /* Print calibrated counter values */
//    for (int i = 0U; i < FSL_FEATURE_TSI_CHANNEL_COUNT; i++)
//    {
//        PRINTF("Calibrated counters for channel %d is: %d \r\n", i, tsi_calibration_buffer.calibratedData[i]);
//    }
//
//    TSI_EnableHardwareTriggerScan(TSI0, false); /* Enable software trigger scan */
//    TSI_DisableInterrupts(TSI0, kTSI_EndOfScanInterruptEnable);
//    TSI_ClearStatusFlags(TSI0, kTSI_EndOfScanFlag);
//    TSI_SetMeasuredChannelNumber(TSI0, BOARD_TSI_ELECTRODE_1);
}
int main(void)
{
    /* Init board hardware. */
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    configure_peripherals();

    xSemaphore = xSemaphoreCreateBinary();

    xTaskCreate(theremin_task, "Theremin_task", configMINIMAL_STACK_SIZE, NULL, hello_task_PRIORITY, NULL);
    vTaskStartScheduler();
    for (;;)
        ;
}

