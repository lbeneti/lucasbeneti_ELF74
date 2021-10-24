#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
    while(1);
}
#endif

//*****************************************************************************
//
// Press response game
//
//*****************************************************************************

void SwitchInt(void);

volatile uint32_t millis = 0; // volatile because can change values between readings
static uint32_t response_cycles = 0; // variable used for the number of cycles the response took

void SysTickInt() {
  millis++;
}

void SysTickBegin() {
  SysTickIntRegister(SysTickInt); // registers the SysTickInt interuption for the Systick
  SysTickIntEnable();
  SysTickEnable();  
}

void SwitchInt() {
  uint32_t status = GPIOIntStatus(GPIO_PORTJ_BASE, false);
  if(status & GPIO_PIN_0) {
    SysTickIntDisable();
    SysTickDisable();
    GPIOIntDisable(GPIO_PORTJ_BASE, GPIO_PIN_0);
    GPIOIntClear(GPIO_PORTJ_BASE, GPIO_PIN_0);
    response_cycles = millis - 1000;
  }
}

int main(void) {
    SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_240, 120000000);

    // systick setup
    SysTickDisable();
    SysTickIntDisable();
    SysTickPeriodSet(120000);

    // peripherals
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ);
    SysCtlDelay(3); // its good practices wait for 3 cycles after anableing preripherals

    // setup leds
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_0 | GPIO_PIN_1); // setting pin 0 as output
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0x0);
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0x0);

    // setup button
    GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0); // setting PIN_0 as input
    GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU); // button config
    GPIOIntDisable(GPIO_PORTJ_BASE, GPIO_PIN_0); // disable interupts, for safety
    GPIOIntClear(GPIO_PORTJ_BASE, GPIO_PIN_0); // clears any pending interrupt
    GPIOIntRegister(GPIO_PORTJ_BASE, SwitchInt); // registers the interupt for the buttons
    GPIOIntEnable(GPIO_PORTJ_BASE, GPIO_PIN_0); // enable the interupt by the PIN_0
    SysTickBegin();
    //
    // Check if the peripheral access is enabled.
    //
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPION))
    {
    }

    bool isLedOn = false;
    while(1) {
        
        if (millis == 1000) { // passed 1 second, turn the LED 1 on
          GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0x1);
          isLedOn = true;
        }
        if(isLedOn && response_cycles > 0) {
          printf("Button pressing took %d cycles\n", response_cycles*1000);
          break;
        }
        if(millis >= 4000) {
          printf("Jogador perdeu, demorou mais de 3 segundos!");
        }
    }
    while(1) {} // just a nop to act after the game ends
    
}
