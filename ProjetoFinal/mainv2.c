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
#define QUEUE_MSG_SIZE__ TX_4_ULONG
#define QUEUE_MSG_COUNT__ 4
#define QUEUE_SIZE__ (QUEUE_MSG_SIZE__ * QUEUE_MSG_COUNT__)

/*
1 word = 4 bytes
16 bytes por mensagem = 4 words por mensagem
4 mensagens de capacidade na queue * 16 bytes por mensagem = 64 bytes de tamanho da queue
*/

uint32_t g_ui32SysClock;
TX_THREAD thread_read;
TX_THREAD thread_write;
TX_THREAD thread_control;
TX_QUEUE queue_uart;
TX_BYTE_POOL byte_pool;
UCHAR byte_pool_memory[BYTE_POOL_SIZE__];

void ThreadReadEntry(ULONG);
void ThreadWriteEntry(ULONG);
void ThreadControlEntry(ULONG);
void CustomUARTIntHandler(void);

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
        
    // Permite interrupcoes para a porta UART0 usada

    tx_kernel_enter();

    return 0;
}

void tx_application_define(void *first_unused_memory)
{
    CHAR *alloc_bump_ptr = TX_NULL;

    tx_byte_pool_create(&byte_pool, "Byte Pool", byte_pool_memory, BYTE_POOL_SIZE__);

    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    tx_thread_create(&thread_read, "Thread Read", ThreadReadEntry, 0, alloc_bump_ptr, STACK_SIZE__, 1, 1, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    tx_thread_create(&thread_write, "Thread Write", ThreadWriteEntry, 1, alloc_bump_ptr, STACK_SIZE__, 2, 2, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    tx_thread_create(&thread_control, "Thread Control", ThreadControlEntry, 1, alloc_bump_ptr, STACK_SIZE__, 2, 2, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, QUEUE_SIZE__, TX_NO_WAIT);
    tx_queue_create(&queue_uart, "Queue UART", QUEUE_MSG_SIZE__, alloc_bump_ptr, QUEUE_SIZE__);

    IntEnable(INT_UART0);
    UARTIntRegister(UART0_BASE, CustomUARTIntHandler);
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
}

void ThreadReadEntry(ULONG thread_input)
{
    char buf[128] = {0};
    char msg[16];
    int received = 0;
    while (true)
    {
        if (tx_queue_receive(&queue_uart, msg, TX_WAIT_FOREVER) != TX_SUCCESS)
        {
            break;
        }
        received += 1;
        tx_thread_sleep(100);
    }
}

void ThreadWriteEntry(ULONG thread_input)
{
    while (true)
    {
        tx_thread_sleep(1000);
    }
}

void ThreadControlEntry(ULONG thread_input)
{
    while (true)
    {
        tx_thread_sleep(1000);
    }
}

void CustomUARTIntHandler(void)
{
    uint32_t ui32Status;
    ui32Status = UARTIntStatus(UART0_BASE, true);
    UARTIntClear(UART0_BASE, ui32Status);
 
    char buf[16] = {0};
    int i = 0;

    while(UARTCharsAvail(UART0_BASE) && i < 16)
    {
        char letter = UARTCharGet(UART0_BASE);
        buf[i] = letter;
        i += 1;
    }

    if (i <= 15)
    {
        buf[i] = '\0';
    }

    tx_queue_send(&queue_uart, buf, TX_NO_WAIT);
}

#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif
