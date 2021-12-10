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

#define BYTE_POOL_SIZE__ 9120
#define STACK_SIZE__ 1024
#define QUEUE_SIZE__ 100
#define QUEUE_MSG_SIZE__ TX_4_ULONG

uint32_t g_ui32SysClock;

TX_THREAD thread_uart;
TX_THREAD thread_logic;
TX_QUEUE queue_uart;
TX_BYTE_POOL byte_pool;

UCHAR byte_pool_memory[BYTE_POOL_SIZE__];

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
    CHAR *alloc_bump_ptr = TX_NULL;

    // Create a byte memory pool from which to allocate the thread stacks.
    tx_byte_pool_create(&byte_pool, "byte pool", byte_pool_memory, BYTE_POOL_SIZE__);

    // Allocate the stack for thread UART.
    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    // Create thread UART.
    tx_thread_create(&thread_uart, "thread UART", thread_uart_entry, 0, alloc_bump_ptr, STACK_SIZE__, 1, 1, TX_NO_TIME_SLICE, TX_AUTO_START);

    // Allocate the stack for thread Logic.
    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    // Create thread logic.
    tx_thread_create(&thread_logic, "thread logic", thread_logic_entry, 1, alloc_bump_ptr, STACK_SIZE__, 2, 2, TX_NO_TIME_SLICE, TX_AUTO_START);

    // Allocate the message queue.
    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, QUEUE_SIZE__*sizeof(QUEUE_MSG_SIZE__), TX_NO_WAIT);
    // Create queue.
    tx_queue_create(&queue_uart, "queue UART", QUEUE_MSG_SIZE__, alloc_bump_ptr, QUEUE_SIZE__*sizeof(QUEUE_MSG_SIZE__));
}

void thread_uart_entry(ULONG thread_input)
{
    char buf[16] = {0};

    while (true)
    {
        tx_thread_sleep(100);
        int i = 0;

        while(UARTCharsAvail(UART0_BASE) && i < 15) {
            buf[i] = UARTCharGet(UART0_BASE); // pega o char primeiro da fila
            i += 1;
        }

        buf[i] = '\0';

        if (tx_queue_send(&queue_uart, (void *) &buf, TX_WAIT_FOREVER) != TX_SUCCESS)
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
        {
            // tx_thread_sleep(1);
            // continue;
            break;
        }
        received_messages += 1;
    }
}
