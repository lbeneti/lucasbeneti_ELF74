#include <stdlib.h>
#include <stdio.h>
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

typedef struct Write_t {
    float acceleration;
    float turn;
} Write;

typedef struct Read_t {
    bool ready;
    float laser;
    float ultrasound;
    float rf;
    float cameras;
} Read;

uint32_t g_ui32SysClock;

TX_MUTEX write_mutex;
Write write;

TX_MUTEX read_mutex;
Read read;

TX_THREAD thread_read;
TX_THREAD thread_write;
TX_BYTE_POOL byte_pool;

UCHAR byte_pool_memory[BYTE_POOL_SIZE__];

void read_to_buf(char *cmd, int cmd_size, char *expected_prefix, int prefix_size, char *buf, int buf_size);

void thread_read_entry(ULONG thread_input);
void thread_write_entry(ULONG thread_input);

int main()
{
    write.acceleration = 0.0;
    write.turn = 0.0;
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

    // Allocate the stack for thread Read.
    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    // Create thread UART.
    tx_thread_create(&thread_read, "thread UART", thread_read_entry, 0, alloc_bump_ptr, STACK_SIZE__, 1, 1, TX_NO_TIME_SLICE, TX_AUTO_START);

    // Allocate the stack for thread Write.
    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    // Create thread logic.
    tx_thread_create(&thread_write, "thread logic", thread_write_entry, 1, alloc_bump_ptr, STACK_SIZE__, 2, 2, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_mutex_create(&write_mutex, "next_cmd_mutex", TX_NO_INHERIT);
    tx_mutex_create(&read_mutex, "read_mutex", TX_NO_INHERIT);
}

void thread_read_entry(ULONG thread_input)
{
    char buf[32] = {0};
    while (true)
    {
        read_to_buf("Prf;", 4, "Lrf", 3, buf, 32);
        read_to_buf("Pl;", 3, "Ll", 2, buf, 32);
        read_to_buf("Pu;", 3, "Lu", 2, buf, 32);
        //read_to_buf("Pbcam;", 6, "Lbcam", 5, buf, 32);

        tx_thread_sleep(50);
    }
}

void thread_write_entry(ULONG thread_input)
{
    char write_str[32];
    while(true)
    {
        tx_thread_sleep(100);
        // if(tx_mutex_get(&write_mutex, TX_WAIT_FOREVER) != TX_SUCCESS)
        //     break;

        // sprintf(
        //     write_str,
        //     "A%.2f;V%.2f;Pl;Pu;Prf;Pbcam;",
        //     write.acceleration,
        //     write.turn);

        // for (int i = 0; i < 32; i++)
        // {
        //     UARTCharPut(UART0_BASE, write_str[i]);
        // }

        // UARTCharPut(UART0_BASE, '\0');

        // if(tx_mutex_put(&write_mutex) != TX_SUCCESS)
        //     break;
    }
}

void read_to_buf(
    char *cmd,
    int cmd_size,
    char *expected_prefix,
    int prefix_size,
    char *buf,
    int buf_size)
{
    // BEWARE
    // CURSED CODE

    // Send command through UART.
    for (int i = 0; i < cmd_size; i++)
    {
        UARTCharPut(UART0_BASE, cmd[i]);
    }

    // Read until expected command is obtained.
    int received = 0;
    while (received < prefix_size)
    {
        char w = UARTCharGet(UART0_BASE);
        if (w == expected_prefix[received])
        {
            buf[received] = w;
            received += 1;
        }
        else
        {
            received = 0;
        }
    }

    // Read until a full numerical value is received.
    while (UARTCharsAvail(UART0_BASE) && received < buf_size)
    {
        char w = UARTCharGet(UART0_BASE);
        // If we receive a letter (example: L) we reached another command;
        if (w == '.' || w == '+' || w == '-' || (w >= 48 && w <= 57))
        {
            buf[received] = w;
            received += 1;
        }
        // If we reached another command we're done reading.
        else
        {
            break;
        }
    }
}
