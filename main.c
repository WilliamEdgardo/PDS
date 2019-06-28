/*********************************************************************
* Oscilador, PDS
* Universidad Tecnológica de la Mixteca
* Hernández Guzmán William Edgardo && Peralta Sánchez Fernando
* <hdezgwilliam@gmail.com>  <fer_peralta10@hotmail.com>
*********************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "inc/tm4c123gh6pm.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/adc.h"
#include "inc/hw_ssi.h"
#include "driverlib/ssi.h"
#include "driverlib/pin_map.h"
#include"inc/hw_gpio.h" //Hab. PF0

#define FS 1500 // Número de muestras por segundo

uint32_t muestra, B=0; // Variable para leer el ADC
uint32_t ui32Period; // Periodo para escribir en el temporizador
volatile uint32_t select = 1; //Para selección de la frecuencia

/*********************************************************************
* Valores iniciales de la entrada y la salida, así como sus retardos
*********************************************************************/

double x = 0.0;

double y = 0.0;
double yn1 = 0.0;
double yn2 = 0.0;

/***********************************************************************************
* Funcion que inicia la comunicación entre la TM4C123G y el DAC MCP4922 (Microchip)
* mediante SSI (SPI). Se transfieren 16 bits, 12 de datos y 4 de configuración. La
* velocidad del reloj es de 20MHz
***********************************************************************************/

void init_SSI0()
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	GPIOPinConfigure(GPIO_PA2_SSI0CLK); // Habilita el pin CLK
	GPIOPinConfigure(GPIO_PA5_SSI0TX); // Habilita el pin TX
	GPIOPinConfigure(GPIO_PA3_SSI0FSS); // Habilita el pin SS
	// Habilita los pines para el módulo SSI
	GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_5 | GPIO_PIN_3 | GPIO_PIN_2);
	// Habilita el módulo SSI0
	SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);
	// Configuración de SSI0
	SSIConfigSetExpClk(SSI0_BASE, SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 20000000, 16);
	SSIEnable(SSI0_BASE); // Habilita SSI0
}

void Interrupt(){
  uint32_t status=0;

  status = TimerIntStatus(TIMER5_BASE,true);
  TimerIntClear(TIMER5_BASE,status);

  /*
  if ( B==0 ){
	  GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_3, 0x08);  // Se inicia PE.3 en Bajo
	  B=1;
  } else {
	  GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_3, 0x00);  // Se inicia PE.3 en Bajo
	  B=0;
  } */

  x=0;
  if( B==0 ){
	  x = 1000.0;
	  B=1;
  }

  if ( select == 1 ){
	  y = x + (1.3382*yn1) - yn2; // 400
  } else if( select == 2 ){
	  y = x + yn1 - yn2; //  500
  } else if( select == 3 ){
	  y = x + (0.618*yn1)  - yn2; //   600
  }

  yn2 = yn1; yn1 = y;

  muestra = (uint32_t)( y  + 2048.0);
  SSIDataPut(SSI0_BASE, (0x3000 | muestra) & 0x3FFF);
  while(SSIBusy(SSI0_BASE))
  {
  }
}

void TimerBegin(){

  uint32_t Period;
  Period = 26666;

  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER5);
  SysCtlDelay(3);
  TimerConfigure(TIMER5_BASE, TIMER_CFG_PERIODIC);
  TimerLoadSet(TIMER5_BASE, TIMER_A, Period -1);

  TimerIntRegister(TIMER5_BASE, TIMER_A, Interrupt);
  TimerIntEnable(TIMER5_BASE, TIMER_TIMA_TIMEOUT);

  TimerEnable(TIMER5_BASE, TIMER_A);
}

/*******************************************
* Programa principal
*******************************************/

int main(void) {

	/********************************
	* Reloj del sistema
	*********************************/
	// Establecimiento de la velocidad del reloj del sistema
	SysCtlClockSet(SYSCTL_SYSDIV_2_5|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|SYSCTL_OSC_MAIN);

	// Se usa el pin PB6, PF0 y PF4 para la selección de frecuencias
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	GPIOPinTypeGPIOInput(GPIO_PORTB_BASE, GPIO_PIN_6 );
	GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_6, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

	//Cod. agregado para liberar PF0
	HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
	HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;
	HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0;

	GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_4); //config. entrada SW1, 0
	GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_0 |GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

	// Para medir el tiempo del Timer se usa el pin 3 del Puerto E
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_3 | GPIO_PIN_4);
	GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_3 | GPIO_PIN_4, 0x00);  // Se inicia PE3 y PE4 en Bajo

	/********************************
	* Rutinas de interrupción del TimerA
	*********************************/
	TimerBegin();

	/********************************
	* Rutinas de inicio de periféricos
	*********************************/

	init_SSI0();

	/************************************
	* Habilitación global de interrupciones
	*************************************/
	IntMasterEnable();

	/*****************************
	* Ciclo ocioso infinito
	*****************************/

	while(1){
		if( !GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0) ){
			select = 1;
			B=0; yn1=0; yn2=0; y=0;
		}
		if( !GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) ){
			select = 2;
			B=0; yn1=0; yn2=0; y=0;
		}
		if( !GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_6) ){
			select = 3;
			B=0; yn1=0; yn2=0; y=0;
		}
	}
	return 0;
}

