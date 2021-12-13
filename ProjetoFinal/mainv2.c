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

typedef enum DoorState_t {
    OPEN,
    CLOSED,
    MOVING,
} DoorState;

typedef struct Elevator_t {
    DoorState door_state;
    uint8_t current_floor;
    uint16_t internal_buttons; // 0b0000_0000_0000_1000
    uint16_t external_buttons_up;
    uint16_t external_buttons_down;
    bool is_moving;
    uint8_t target;
} Elevator;

bool is_button_pressed(uint16_t buttons, int number)
{
    return (buttons & number) == number;
}

uint32_t g_ui32SysClock;
TX_THREAD thread_read;
TX_THREAD thread_write;
TX_THREAD thread_control;
TX_QUEUE queue_uart_read;
TX_MUTEX elevator_e_mutex;
Elevator elevator_e;
TX_MUTEX elevator_c_mutex;
Elevator elevator_c;
TX_MUTEX elevator_d_mutex;
Elevator elevator_d;

TX_BYTE_POOL byte_pool;
UCHAR byte_pool_memory[BYTE_POOL_SIZE__];


void ThreadReadEntry(ULONG);
void ThreadWriteEntry(ULONG);
void ThreadControlEntry(ULONG);
void CustomUARTIntHandler(void);

int main()
{
    elevator_e.door_state = CLOSED;
    elevator_e.current_floor = 0;
    elevator_e.internal_buttons = 0;
    elevator_e.external_buttons_up = 0;
    elevator_e.external_buttons_down = 0;

    elevator_c.door_state = CLOSED;
    elevator_c.current_floor = 0;
    elevator_c.internal_buttons = 0;
    elevator_c.external_buttons_up = 0;
    elevator_c.external_buttons_down = 0;

    elevator_d.door_state = CLOSED;
    elevator_d.current_floor = 0;
    elevator_d.internal_buttons = 0;
    elevator_d.external_buttons_up = 0;
    elevator_d.external_buttons_down = 0;

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

    tx_mutex_create(&elevator_e_mutex, "Elevator E Mutex", TX_NO_INHERIT);
    tx_mutex_create(&elevator_c_mutex, "Elevator C Mutex", TX_NO_INHERIT);
    tx_mutex_create(&elevator_d_mutex, "Elevator D Mutex", TX_NO_INHERIT);

    IntEnable(INT_UART0);
    UARTIntRegister(UART0_BASE, CustomUARTIntHandler);
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
}

void ThreadReadEntry(ULONG thread_input)
{
    char buf[32];
    for (uint8_t i = 0; i < 32; i++)
        buf[i] = 0;

    char msg[16] = {0};
    for (uint8_t i = 0; i < 16; i++)
        msg[i] = 0;

    while (true)
    {
        if (tx_queue_receive(&queue_uart_read, msg, TX_WAIT_FOREVER) != TX_SUCCESS)
        {
            break;
        }

        // fill buf with msg
        int buffer_cursor = 0;
        bool hasDelimiter = false;
        for(uint8_t i = 0; i < 16; i++)
        {
            if (msg[i] == '\0')
            {
                break;
            }

            buf[buffer_cursor] = msg[i];
            buffer_cursor += 1;
            if (msg[i] == 0xD)
            {
                hasDelimiter = true;
            }
        }

        if (!hasDelimiter)
        {
            continue;
        }

        char identifier = buf[1];

        Elevator *elevator;
        TX_MUTEX *mutex;

        if (buf[0] == 'e') 
        {
            elevator = &elevator_e;
            mutex = &elevator_e_mutex;
        }
        else if (buf[0] == 'c')
        {
            elevator = &elevator_c;
            mutex = &elevator_c_mutex;
        }
        else if (buf[0] == 'd')
        {
            elevator = &elevator_d;
            mutex = &elevator_d_mutex;
        }

        if(tx_mutex_get(mutex, TX_WAIT_FOREVER) != TX_SUCCESS)
            break;

        if (identifier == 'A') // porta abriu completamente
        {
            elevator->door_state = OPEN;
        }
        else if (identifier == 'F') // porta fechou completamente
        {
            elevator->door_state = CLOSED;
        }
        else if (identifier == 'I') // botão interno pressionado
        {
            uint8_t floor = buf[2] - 97; // andar vem no formato "a..p"
            elevator->internal_buttons |= (1 << floor);
        }
        else if (identifier == 'E') // botão externo pressionado
        {
            int dicker = buf[2] == '0' ? 0 : 10;
            uint8_t floor = dicker + (buf[3] - '0');
            if (buf[4] == 's')
            {
                elevator->external_buttons_up |= (1 << floor);
            }
            else
            {
                elevator->external_buttons_down |= (1 << floor);
            }
        }
        else if (identifier >= '0' && identifier <= '9') // elevador está em determinado andar
        {
            if (identifier == '1') // pode haver segundo dígito
            {
                if (buf[2] == 0xD)
                {
                    elevator->current_floor = 1; // não há segundo digito, valor é 1
                }
                else
                {
                    elevator->current_floor = 10 + (buf[2] - '0'); // há segundo dígito
                }
            }
        }

        if(tx_mutex_put(mutex) != TX_SUCCESS)
            break;
          
        // clear buffer for parsed command
        uint8_t next = 0;
        while (buf[next] != 0xD)
        {
            next += 1;
        }
        next += 2;
        for (uint8_t i = next; i < (32 - next); i++)
        {
            buf[i - next] = buf[i];
        }
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

void UpdateElevatorAndSendCommands(Elevator *elevator)
{
    if (elevator->external_buttons_down != 0)
    {
    }
}

void CustomUARTIntHandler(void)
{
    uint32_t ui32Status = UARTIntStatus(UART0_BASE, true);
    UARTIntClear(UART0_BASE, ui32Status);
 
    char buf[16] = {0};
    for (uint8_t i = 0; i < 16; i++)
        buf[i] = 0;

    int i = 0;

    while(UARTCharsAvail(UART0_BASE) && i < 16)
    {
        char letter = UARTCharGet(UART0_BASE);
        buf[i] = letter;
        i += 1;
    }

    tx_queue_send(&queue_uart_read, buf, TX_NO_WAIT);
}
