/* --COPYRIGHT--,BSD
 * Copyright (c) 2017, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
/******************************************************************************
 * MSP432 Empty Project
 *
 * Description: An empty project that uses DriverLib
 *
 *                MSP432P401
 *             ------------------
 *         /|\|                  |
 *          | |                  |
 *          --|RST               |
 *            |                  |
 *            |                  |
 *            |                  |
 *            |                  |
 *            |                  |
 * Author: Bunmi Oluwatudimu
*******************************************************************************/
/* DriverLib Includes */
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

/* Standard Includes */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define PWM_PERIOD 32000 // Timer period
#define DUTY_CYCLE_LENGTH 0.2 * PWM_PERIOD // Initial duty cycle length

/* Configuration parameters for Timer_A module in PWM configuration */
Timer_A_PWMConfig pwmConfig =
{
 TIMER_A_CLOCKSOURCE_ACLK,                  // Clock source
 TIMER_A_CLOCKSOURCE_DIVIDER_1,             // Divider ratio, 1:1
 PWM_PERIOD,                                // Timer period which will stored in CCR0
 TIMER_A_CAPTURECOMPARE_REGISTER_1,         // Compare register to use
 TIMER_A_OUTPUTMODE_RESET_SET,              // Output mode for PWM signal
 DUTY_CYCLE_LENGTH                          // Duty cycle length which will be stored in CCR1
};

volatile uint16_t adcResult;

int main(void)
{
    /* Halting WDT */
    WDT_A_holdTimer();
    Interrupt_enableSleepOnIsrExit();

    //![Simple ADC14 Configure]
    /* Initializing ADC (MCLK/1/1) */
    ADC14_enableModule();
    ADC14_initModule(ADC_CLOCKSOURCE_MCLK, ADC_PREDIVIDER_1, ADC_DIVIDER_1,
            0);

    /* Configuring ADC Memory (ADC_MEM0 A0/A1) in repeat mode
     * with use of external references */
    ADC14_configureSingleSampleMode(ADC_MEM0, true);
    ADC14_configureConversionMemory(ADC_MEM0, ADC_VREFPOS_EXTPOS_VREFNEG_EXTNEG,
            ADC_INPUT_A0, false);

    /* Setting up GPIO pins as analog inputs (and references) */
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P5,GPIO_PIN7 | GPIO_PIN6 | GPIO_PIN5 | GPIO_PIN4, GPIO_TERTIARY_MODULE_FUNCTION);

    /* Enabling sample timer in auto iteration mode and interrupts*/
    ADC14_enableSampleTimer(ADC_AUTOMATIC_ITERATION);
    ADC14_enableInterrupt(ADC_INT0);

    /* Enabling Interrupts */
    Interrupt_enableInterrupt(INT_ADC14);
    Interrupt_enableMaster();

    /* Triggering the start of the sample */
    ADC14_enableConversion();
    ADC14_toggleConversionTrigger();
    //![Simple ADC14 Configure]

    /* Initialize the high-speed clock system. */
    CS_setDCOCenteredFrequency (CS_DCO_FREQUENCY_12); // Set DCO to 12 MHz
    CS_initClockSignal (CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);
    CS_initClockSignal (CS_HSMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);
    CS_initClockSignal (CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);

    /* Initialize the low-speed auxiliary clock system. */
    CS_setReferenceOscillatorFrequency (CS_REFO_32KHZ); // Reference oscillator is set to 32KHz
    CS_initClockSignal (CS_ACLK, CS_REFOCLK_SELECT, CS_CLOCK_DIVIDER_1); // Auxiliary clock derives from reference

    /* Initialize the UART module. */
    initUART ();
    writeString ("Established communication with the board");

    /* Configure output LED. */
    GPIO_setAsOutputPin (GPIO_PORT_P1, GPIO_PIN0);
    GPIO_setOutputLowOnPin (GPIO_PORT_P1, GPIO_PIN0);

    /* Configure switch S1. */
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN1);

    /* Configure P2.4 as the pin that outputs the PWM signal. */
    GPIO_setAsPeripheralModuleFunctionOutputPin (GPIO_PORT_P2, GPIO_PIN4, GPIO_PRIMARY_MODULE_FUNCTION);

    /* Configure PWM. */
    Timer_A_generatePWM (TIMER_A0_BASE, &pwmConfig);

    /* Configure and enable interrupt system. */
    GPIO_clearInterruptFlag (GPIO_PORT_P1, GPIO_PIN1);
    GPIO_enableInterrupt (GPIO_PORT_P1, GPIO_PIN1);
    Interrupt_enableInterrupt (INT_PORT1); // ISR for when button S1 is pressed

    Timer_A_enableCaptureCompareInterrupt(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
    Timer_A_enableCaptureCompareInterrupt(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_1);
    Interrupt_enableInterrupt (INT_TA0_0);
    Interrupt_enableInterrupt (INT_TA0_N);
    Interrupt_enableMaster ();


    /* Going to sleep */
    while (1)
    {
        PCM_gotoLPM0();
    }
}

/* This interrupt happens whenever a conversion has been completed and placed
 * into ADC_MEM0. */
void ADC14_IRQHandler(void)
{
    uint64_t status;

    status = ADC14_getEnabledInterruptStatus();
    ADC14_clearInterruptFlag(status);

    if(status & ADC_INT0)
    {
        adcResult = ADC14_getResult(ADC_MEM0);

    }

}

/* ISR to handle switch S1. */
void
PORT1_IRQHandler (void)
{
    uint32_t status;
    static float duty_cycle = 0.1;

    status = GPIO_getEnabledInterruptStatus (GPIO_PORT_P1);
    GPIO_clearInterruptFlag (GPIO_PORT_P1, status);

    /* Isolate P1.1 */
    if (status & GPIO_PIN1) {
        if (duty_cycle >= 1.0)
            duty_cycle = 0.1;
        else
            duty_cycle += 0.1;

        /* Update the configuration with the length of the new duty cycle. */
        int duty_cycle_length = (int) PWM_PERIOD * duty_cycle;
        pwmConfig.dutyCycle = duty_cycle_length;
        Timer_A_generatePWM (TIMER_A0_BASE, &pwmConfig);
    }

    return;
}

/* ISR to handle end of timer period. */
void
TA0_0_IRQHandler (void)
{
    Timer_A_clearCaptureCompareInterrupt (TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
    writeString ("\n\rEnd of timer period");
    /* User-defined code. */

    return;
}

/* ISR to handle end of PWM pulse. */
void
TA0_N_IRQHandler (void)
{
    Timer_A_clearCaptureCompareInterrupt (TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_1);
    writeString ("\n\rEnd of PWM pulse");
    /* User-defined code. */

    return;
}
