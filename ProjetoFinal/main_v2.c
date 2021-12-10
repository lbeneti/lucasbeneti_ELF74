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
#include "tx_api.h"

#define STACK_SIZE__ 1024
#define QUEUE_SIZE__ 100

TX_THREAD thread_uart;
TX_THREAD thread_logic;
TX_QUEUE queue_uart;

void thread_uart_entry(ULONG thread_input);
void thread_logic_entry(ULONG thread_input);

int main()
{
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

    tx_kernel_enter();
}

void tx_application_define(void *first_unused_memory)
{
    // TODO: allocate memory blocks for threads and queue...

    tx_queue_create(
        &queue_uart,
        "queue UART",
        TX_16_ULONG,
        TX_NULL,
        QUEUE_SIZE__*sizeof(ULONG));

    tx_thread_create(
        &thread_uart,
        "thread UART",
        thread_uart_entry,
        0,
        TX_NULL,
        STACK_SIZE__,
        1,
        1,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);

    tx_thread_create(
        &thread_logic,
        "thread logic",
        thread_logic_entry,
        0,
        TX_NULL,
        STACK_SIZE__,
        2,
        1,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);
}

void thread_uart_entry(ULONG thread_input)
{
    while (true)
    {
        int i = 0;

        char buf[16];

        while(UARTCharsAvail(UART0_BASE) && i < 15) {
            buf[i] = UARTCharGet(UART0_BASE); // pega o char primeiro da fila
            i += 1;
        }

        buf[i] = '\0';

        if (tx_queue_send(&queue_uart, &buf, TX_WAIT_FOREVER) != TX_SUCCESS)
            break;
    }
}

void thread_logic_entry(ULONG thread_input)
{
    int received_messages = 0;
    char buf[16];

    while(true)
    {
        if (tx_queue_receive(&queue_uart, &buf, TX_WAIT_FOREVER) != TX_SUCCESS)
            break;
        received_messages += 1;
    }
}