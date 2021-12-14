#include <stdlib.h>
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
#include <iostream>

// #define TX_TIMER_TICKS_PER_SECOND ((ULONG) 1000)

#define BYTE_POOL_SIZE__ 9120
#define STACK_SIZE__ 1024
#define QUEUE_MSG_SIZE__ TX_2_ULONG
#define QUEUE_MSG_COUNT__ 8
#define QUEUE_SIZE__ (QUEUE_MSG_SIZE__ * QUEUE_MSG_COUNT__)
#define COEFFICIENT 0.15

uint32_t g_ui32SysClock;
TX_BYTE_POOL byte_pool;
UCHAR byte_pool_memory[BYTE_POOL_SIZE__];
TX_THREAD thread_read;
TX_THREAD thread_write;
TX_THREAD thread_control;
TX_QUEUE queue_uart_read;

float rf;
TX_MUTEX mutex_rf;

float turn;
TX_MUTEX mutex_turn;

float stof(const char* s);

void ThreadReadEntry(ULONG);
void ThreadWriteEntry(ULONG);
void ThreadControlEntry(ULONG);
float GetValue(char *sensor, char *buf);

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
    UARTConfigSetExpClk(UART0_BASE, 120000000, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    tx_kernel_enter();

    return 0;
}

void tx_application_define(void *first_unused_memory)
{
    void *alloc_bump_ptr = TX_NULL;

    tx_byte_pool_create(&byte_pool, "Byte Pool", byte_pool_memory, BYTE_POOL_SIZE__);

    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    tx_thread_create(&thread_read, "Thread Read", ThreadReadEntry, 0, alloc_bump_ptr, STACK_SIZE__, 1, 1, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    tx_thread_create(&thread_write, "Thread Write", ThreadWriteEntry, 1, alloc_bump_ptr, STACK_SIZE__, 2, 2, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    tx_thread_create(&thread_control, "Thread Control", ThreadControlEntry, 1, alloc_bump_ptr, STACK_SIZE__, 2, 2, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, QUEUE_SIZE__, TX_NO_WAIT);
    tx_queue_create(&queue_uart_read, "Queue UART Read", QUEUE_MSG_SIZE__, alloc_bump_ptr, QUEUE_SIZE__);

    tx_mutex_create(&mutex_rf, "Mutex RF", TX_NO_INHERIT);

    tx_mutex_create(&mutex_turn, "Mutex Turn", TX_NO_INHERIT);
}

void SendCommand(char *cmd)
{
    for (uint8_t i = 0; i < strlen(cmd); i++)
    {
        UARTCharPut(UART0_BASE, cmd[i]);
    }
}

void ThreadReadEntry(ULONG thread_input)
{
    char buf[16] = {0};

    while (true)
    {
        float value_rf = GetValue("rf", buf);

        if (tx_mutex_get(&mutex_rf, TX_WAIT_FOREVER) != TX_SUCCESS)
            break;

        rf = value_rf;

        if (tx_mutex_put(&mutex_rf) != TX_SUCCESS)
            break;

        tx_thread_sleep(10);
    }
}

void ThreadWriteEntry(ULONG thread_input)
{
    char buf[16] = {0};

    SendCommand("A8;");
    tx_thread_sleep(100);
    SendCommand("A0;");

    while (true)
    {
        if (tx_mutex_get(&mutex_turn, TX_WAIT_FOREVER) != TX_SUCCESS) break;

        float turn_ = turn;

        if (tx_mutex_put(&mutex_turn) != TX_SUCCESS)
            break;

        int count = sprintf(buf, "V%f;", turn_);

        buf[count] = '\0';

        SendCommand(buf);

        tx_thread_sleep(10);
    }
}

void ThreadControlEntry(ULONG thread_input)
{
    while (true)
    {
        if (tx_mutex_get(&mutex_rf, TX_WAIT_FOREVER) != TX_SUCCESS)
            break;

        float turn_ = -COEFFICIENT * rf;

        if (tx_mutex_put(&mutex_rf) != TX_SUCCESS)
            break;

        if (tx_mutex_get(&mutex_turn, TX_WAIT_FOREVER) != TX_SUCCESS)
            break;

        turn = turn_;

        if (tx_mutex_put(&mutex_turn) != TX_SUCCESS)
            break;

        tx_thread_sleep(10);
    }
}

float GetValue(char *sensor, char *buf)
{
    UARTCharPut(UART0_BASE, 'P');
    for (uint8_t i = 0; i < strlen(sensor); i++)
    {
        UARTCharPut(UART0_BASE, sensor[i]);
    }
    UARTCharPut(UART0_BASE, ';');

    // Wait for buffer to fill with response.
    tx_thread_sleep(10);

    uint8_t i = 0;
    while (UARTCharsAvail(UART0_BASE))
    {
        buf[i] = UARTCharGet(UART0_BASE);
        i += 1;
    }

    return atof(buf + 1 + strlen(sensor));
}
