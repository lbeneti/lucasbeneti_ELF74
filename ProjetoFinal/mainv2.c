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


uint32_t g_ui32SysClock;
TX_BYTE_POOL byte_pool;
UCHAR byte_pool_memory[BYTE_POOL_SIZE__];
TX_THREAD thread_read;
TX_THREAD thread_write;
TX_THREAD thread_control;
TX_QUEUE queue_uart_read;

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
    UARTConfigSetExpClk(UART0_BASE, g_ui32SysClock, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

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
}

void ThreadReadEntry(ULONG thread_input)
{

    char buf[16] = {0};

    while (true)
    {

        float value_u = GetValue("u", buf);
        float value_rf = GetValue("rf", buf);

        std::cout << "Value: " <<  value_u << std::endl;
        std::cout << "Value: " <<  value_rf << std::endl;

        

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

float stof(const char* s)
{
    float rez = 0, fact = 1;
    if (*s == '-')
    {
        s++;
        fact = -1;
    }

    for (int point_seen = 0; *s; s++)
    {
        if (*s == '.')
        {
            point_seen = 1; 
            continue;
        }

        int d = *s - '0';

        if (d >= 0 && d <= 9)
        {
            if (point_seen) fact /= 10.0f;
            rez = rez * 10.0f + (float)d;
        }
    }

    return rez * fact;
}