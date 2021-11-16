#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"

#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

uint32_t g_ui32SysClock;

unsigned char convertUpperCharacter(unsigned char character) {
    // se ao fizermos cast pra int e o valor do character for
    // maior que 90 ou menor que 65, singnifica que o character
    // não está entre as letras maiúsculas e portanto não deverá
    // ser convertida
    if((int) character < 65 || (int) character > 90) {
        return character;
    }
    return (unsigned char) ((int) character + 32);
}

void CustomUARTIntHandler(void){
  uint32_t ui32Status;
  ui32Status = UARTIntStatus(UART0_BASE, true); // pega status da interrupcao
  UARTIntClear(UART0_BASE, ui32Status); // limpa a interrupcao encontrada
  
    // enquanto tiver algum character na FIFO da porta, o loop continua
  while(UARTCharsAvail(UART0_BASE)){
    unsigned char letter = UARTCharGet(UART0_BASE); // pega o char primeiro da fila
    UARTCharPut(UART0_BASE, letter); // manda pela UART esse mesmo character
   
    unsigned char converted = convertUpperCharacter(letter); // caso precise, o character eh convertido
    UARTCharPut(UART0_BASE, converted); // o char eh mandado pela UART para o receptor
    UARTCharPut(UART0_BASE, '\n'); // adicionando uma quebra de linha para saber quando a msg acaba    
    }  
  }

int main() {
  // configura o clock para 120MHz
  g_ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_240), 120000000);
  
  // ****** configuracoes da porta UART que utilizaremos ******   
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
  
  //Habilita interrup��o
  IntMasterEnable();
  
  // Configura a porta para receber e transmitir dados
  GPIOPinConfigure(GPIO_PA0_U0RX);
  GPIOPinConfigure(GPIO_PA1_U0TX);
  GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
  
  // seta a porta UART que usaremos pra baud rate de 115200Hz 
  UARTConfigSetExpClk(UART0_BASE, g_ui32SysClock, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    
  // Permite interrupcoes para a porta UART0 usada
  IntEnable(INT_UART0);
  UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
  while(1) {}
}