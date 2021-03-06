#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
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


uint32_t g_ui32SysClock;
TX_BYTE_POOL byte_pool;
UCHAR byte_pool_memory[BYTE_POOL_SIZE__];
TX_THREAD thread_read;
TX_THREAD thread_write;
TX_THREAD thread_control;
TX_THREAD thread_start_button;

typedef struct {
    float pRf;
    float pUltrasound;
    float pLaser;
    float pBcam;
} SensorData;

SensorData sensorData;
TX_MUTEX sensorData_mutex;

typedef struct {
    float acceleration;
    float turnAngle;
} Vehicle;

Vehicle vehicle;
TX_MUTEX vehicle_mutex;

bool vehicle_started = false;

void ReadingThreadEntry(ULONG);
void WriteToUARTEntry(ULONG);
void ControlThreadEntry(ULONG);
void SendVehicleCommand(char *command);
void StartButtonThreadEntry(ULONG);
float GetSensorReading(char *sensor);

float HandleCollision(SensorData);

int main() {
    // configura o clock para 120MHz
    g_ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_240), 120000000);

    // Habilitando perifericos que serao utilizados
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ);
    SysCtlDelay(3);

    IntMasterEnable();

    // Configura a porta para receber e transmitir dados
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);


    GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0); // setting PIN_0 as input
    GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU); // button config
    // GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_1); // setting PIN_0 as input
    // GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU); // button config

    // seta a porta UART que usaremos pra baud rate de 115200Hz 
    UARTConfigSetExpClk(UART0_BASE, g_ui32SysClock, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    tx_kernel_enter();

    return 0;
}

void tx_application_define(void *first_unused_memory)
{
    void *alloc_bump_ptr = TX_NULL;

    tx_byte_pool_create(&byte_pool, (char *) "Byte Pool", byte_pool_memory, BYTE_POOL_SIZE__);

    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    tx_thread_create(&thread_read, (char *) "Reading Thread", ReadingThreadEntry, 0, alloc_bump_ptr, STACK_SIZE__, 1, 1, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    tx_thread_create(&thread_write, (char *) "Write Thread", WriteToUARTEntry, 1, alloc_bump_ptr, STACK_SIZE__, 2, 2, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    tx_thread_create(&thread_control, (char *) "Control Thread", ControlThreadEntry, 1, alloc_bump_ptr, STACK_SIZE__, 2, 2, TX_NO_TIME_SLICE, TX_AUTO_START);
    
    tx_byte_allocate(&byte_pool, (VOID **) &alloc_bump_ptr, STACK_SIZE__, TX_NO_WAIT);
    tx_thread_create(&thread_start_button, (char *) "Start Button Thread", StartButtonThreadEntry, 1, alloc_bump_ptr, STACK_SIZE__, 3, 3, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_mutex_create(&sensorData_mutex, (char *) "Sensor Data Mutex", TX_NO_INHERIT);
    tx_mutex_create(&vehicle_mutex, (char *) "Vehicle Mutex", TX_NO_INHERIT);
}

void SendVehicleCommand(char *command) {
    for (uint8_t i = 0; i < strlen(command); i++) {
        UARTCharPut(UART0_BASE, command[i]);
    }
}

float GetSensorReading(char *sensor) {
    char buf[16];
    for (uint8_t i = 0; i < strlen(sensor); i++) {
        UARTCharPut(UART0_BASE, sensor[i]);
    }
    UARTCharPut(UART0_BASE, ';');
    tx_thread_sleep(5);

    uint8_t i = 0;
    while (UARTCharsAvail(UART0_BASE)) {
        buf[i] = UARTCharGet(UART0_BASE);
        i += 1;
    }

    return atof(buf + strlen(sensor));
}

void StartButtonThreadEntry(ULONG thread_input) {
    while(true) {  
        if(!GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0)) {
            if(!vehicle_started) {
                SendVehicleCommand("A5;");
                tx_thread_sleep(100);
                SendVehicleCommand("A0;");
                vehicle_started = true;
            } else if(vehicle_started) {
                SendVehicleCommand("S;");
                vehicle_started = false;       
            }
        }        
        tx_thread_sleep(10);
    }
}

void ReadingThreadEntry(ULONG thread_input) {
    while (true) {
        float rfReading = GetSensorReading("Prf"); // faz a leitura do sensor de RF
        if (tx_mutex_get(&sensorData_mutex, TX_WAIT_FOREVER) != TX_SUCCESS) {
            break;
        }

        sensorData.pRf = rfReading;

        if (tx_mutex_put(&sensorData_mutex) != TX_SUCCESS) {
            break;
        }
        tx_thread_sleep(5);
    }
}

void WriteToUARTEntry(ULONG thread_input) {
    char commandBuf[16] = {0};

    while (true) {
        if (tx_mutex_get(&vehicle_mutex, TX_WAIT_FOREVER) != TX_SUCCESS){
            break;
        }

        float turnAngle = vehicle.turnAngle;

        if (tx_mutex_put(&vehicle_mutex) != TX_SUCCESS) {
            break;
        }

        int count = sprintf(commandBuf, "V%f;", turnAngle);

        commandBuf[count] = '\0';
        
        SendVehicleCommand(commandBuf);
        
        tx_thread_sleep(5);
    }
}

void ControlThreadEntry(ULONG thread_input) {
    while (true) {
        if (tx_mutex_get(&sensorData_mutex, TX_WAIT_FOREVER) != TX_SUCCESS) {
            break;
        }
        float turnAngle = 0.0;
        float multiplier = 0.2;
        if (vehicle_started) {
            float modulo_rf = sensorData.pRf > 0.0 ? sensorData.pRf : (-1*sensorData.pRf) ;
            if(modulo_rf >= 0.0 && modulo_rf < 1.5) {
                multiplier = 0.0;
            }
            turnAngle = -sensorData.pRf * multiplier;        
        }        

        if (tx_mutex_put(&sensorData_mutex) != TX_SUCCESS) {
            break;
        }

        if (tx_mutex_get(&vehicle_mutex, TX_WAIT_FOREVER) != TX_SUCCESS) {
            break;
        }

        vehicle.turnAngle = turnAngle;

        if (tx_mutex_put(&vehicle_mutex) != TX_SUCCESS) {
            break;
        }
        tx_thread_sleep(5);
    }
}
