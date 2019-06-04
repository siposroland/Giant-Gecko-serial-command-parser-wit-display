#include "em_device.h"
#include "em_chip.h"
#include "em_usart.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_timer.h"
#include "segmentlcd.h"
#include "segmentlcd_spec.h"
#include "segmentlcdconfig.h"
#include "InitDevice.h"
#include "print.h"
#include "command.h"
#include "caplesense.h"


#define DEBUG 1
#define Mode_BUTTON 0
#define Mode_SLIDER 1

/* !!! VALTOZOK DEKLARALASA !!! */

volatile uint8_t  ch;				// soros porton erkezo karaktert tarolja
volatile bool new_char = 0;			// uj karakter kezelesere
volatile bool timerIT = 0;			// 1sec usztatas engedelyezo
volatile bool getMode = 0;			// Timer 1 utemzo Slider/Button kozti valasztasahoz
volatile uint8_t charBuffer[100];	// kimeneti szoveg tarolo buffer
volatile uint8_t inBuffer[100];		// bemeneti szoveg tarolo buffer

volatile bool wait4Text2Write = 0;	// szoveg kiiratas engedelyezo
volatile bool cycleStatus = 1;		// szoveg usztatas engedelyezo
volatile uint8_t cyclePos = 0;		// szoveg usztatas pozicioja
volatile uint8_t text2Cycle[104];	// szoveg usztatasahoz buffer
volatile uint8_t buttonValue;		// gombok aktualis ertekenek tarolasa

/* !!! INTERRUPTOK KEZELESE !!! */

/* Soros port uj karakter beolvasas */
void UART0_RX_IRQHandler(void) {
	intUartValue();
}

/* Timer 0 1sec-es usztatashoz */
void TIMER0_IRQHandler(void) {
	intTimer1Sec();
}

/* Btn1 lekezelese */
void GPIO_EVEN_IRQHandler(void)
{
  intButtonPro(10, 2);
}

/* Btn0 lekezelese */
void GPIO_ODD_IRQHandler(void)
{
	intButtonPro(9, 100);
}

/* Timer 1 interrupt button es slider utemzesere */
void TIMER1_IRQHandler(void) {
	intSliderOrButtonTimer();
}

/* !!! MAIN FUGGVENY !!! */

int main(void){
  /* Chip errata */
  CHIP_Init();

  // Init via Configurator
  enter_DefaultMode_from_RESET();

  // minden egyeb eszkoz inicializalasa, engedelyzese
  initOthers();

  /* !!! KEZDETI BEALLITASOK !!! */

  // soros port kezdeti szoveg
  printToSerial("Az elerheto parancsokhoz adja ki a 'Get Help' utasitast!");

  // LCD number reszenek kinullazasa
  SegmentLCD_Number(0);
  wait4Command();

#if DEBUG
  // LCD kijelzo usztatott soraban kedzeti szoveg
  strcpy(charBuffer, "DEBUGGING IS LOVE");
  printToLcd(charBuffer);
#endif

  /* !!! VEGTELEN CIKLUS !!! */
  while (1) {

	  // sleep tight
	  EMU_EnterEM1();

	  // interrupt felebreszti es lefut a tobbi kod
	  if (new_char) {
		  newChar(inBuffer, ch);

		  // lekezeltuk az uj karaktert
		  new_char = 0;
	  }
	  // lcd-n az usztatast vegzi
	  if (timerIT) {

		  // 1s szoveg usztatas
		  timerIT = 0;
		  if (cycleStatus) {
			  cycleText();
		  }
	  }
  }
}
