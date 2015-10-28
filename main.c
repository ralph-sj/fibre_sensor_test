#include <msp430.h> 

// define times (based on TimerB,                 // LFXT1 = ACLK = VLO, Divider = 8;
#define WakeupPeriod      15000             // ~10 sec (=1500/(12000/8))
#define a_d_wakeup_time   4500              // ~3 sec
#define TXPeriod          7500              // ~5 sec  (=7500/(12000*5/8))
#define delay_time        500               // led delay time
#define debounce_time     750               // key debounce


//Timer count for time between transmit
#define sec1              1500              // ~1 sec
#define sec2              3000				// ~ 2 sec
#define sec5              7500              // ~5 sec  (=7500/(12000/8))
#define sec10             15000             // ~10 sec
#define sec20             30000             // ~20 swec
#define sec30             35000             // ~30 swec (experimentally validated)
#define sec40             60000             // ~40 sec
#define sec30_2           43000             // ~30sec 2 min?
#define sec30_4           50434             // ~30sec 4 min?
#define one_hour          5400000

#define port_delay        10                // 6ms -

void Set_TimerA (void);
void Set_TimerB(void);
void delay(unsigned int BlinkCount);
void delay_minutes(unsigned int minutes);

// Data
/*
 * OA_Func_Set.h
 *
 *  Created on: 16 Nov 2014
 *      Author: Ralph S-J
 */
unsigned int ReadFibreSensor(void);
unsigned int V_fibre;

void OA1_Config(void);	// configure OA1
void Start_OA1_Slow(void);	// start OA1 (in slow slew rate)
void Start_OA1_Fast(void);	// start OA1 (in fast slew rate)
void Stop_OA1(void);		// stop OA1 from any slew rate

void USCI_A_Init(void);		// initialise USCI for tx'ing data to COM port
void TXString( char* string, int length );
void TXData(unsigned int data);




/*
 * main.c
 */
void main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

    if( CALBC1_1MHZ == 0xFF && CALDCO_1MHZ == 0xFF )// Do not run if cal values
     {
       while(1)
       {
         __delay_cycles(65000);
       }
     }

    BCSCTL1 = CALBC1_1MHZ;                    // Set DCO = 1MHz
    DCOCTL = CALDCO_1MHZ;


    Set_TimerB();
    USCI_A_Init();

    P1OUT = 0x00;
    P1DIR = 0xFF;

    while(1)
    {

    	V_fibre = ReadFibreSensor();
        TXString("\r\nFibre=",8);
       TXData(V_fibre);

        P1OUT ^= 0x01;
        delay(sec1);
    }
}

// Timer
void Set_TimerB(void)
{
	BCSCTL3 |= LFXT1S_2;                      // LFXT1 = ACLK = VLO
	TBCCTL0 = 0;						// TBCCR0 interrupt disabled
	TBCTL = TBSSEL_1 + MC_1 + ID_3;           // ACLK, upmode, Divider = 8
}
/*******************************************************************************
* BEGHDR
* Function:    void delay(unsigned int BlinkCount)
* DESCRIPTION: Creates a low-power delay by entering LPM3 using Timer B.
*              Timer B frequency = VLO/8 = 1500 Hz.
* INPUTS:      BlinkCount
* PROCESSING:  Delay length of time of BlinkCount
* OUTPUTS:     VOID
********************************************************************************/
void delay(unsigned int BlinkCount)
{
  int TimerTemp;
  TimerTemp = TBCCR0;                       // Save current content of TBCCR0
  TBCCR0 = BlinkCount;                      // Set new TBCCR0 delay
  TBCTL |= TBCLR;                           // Clear TBR counter
  TBCCTL0 &= ~CCIFG;                        // Clear CCIFG Flag
  TBCTL |= MC_1;                            // Start Timer B
  TBCCTL0 = CCIE;							// enable timer b interrupt
  __bis_SR_register(LPM3_bits + GIE);       // Enter LPM3
  TBCTL &= ~(MC_1);                         // Stop Timer B
  TBCCR0 = TimerTemp;						// Reset content of TBCCR0;
}
/*******************************************************************************
* BEGHDR
* NAME:        __interrupt void Timer_B (void)
* DESCRIPTION: Timer B0 interrupt service routine
* INPUTS:      Void
* PROCESSING:  Exit from LPM after interrupt
* OUTPUTS:     Void
*******************************************************************************/
#pragma vector=TIMERB0_VECTOR
__interrupt void TimerB_ISR (void)
{
  __bic_SR_register_on_exit(LPM3_bits);     // Clear LPM3 bit from 0(SR)
}
// DATA
/*******************************************************************************
* BEGHDR
* Function:    unsigned int get_voltage(void)
* DESCRIPTION: Get battery voltage with A/D
* INPUTS:      void
* PROCESSING:  Read battery voltage from ADC10 and returns the value
* OUTPUTS:     Battery voltage from A/D
*******************************************************************************/
unsigned int get_voltage(void)
{
  unsigned int rt_volts;

  ADC10CTL1 = INCH_11;                    // AVcc/2
  ADC10CTL0 = SREF_1 + ADC10SHT_2 + REFON + ADC10ON + ADC10IE + REF2_5V;
  __delay_cycles(250);                    // delay to allow reference to settle
  ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start
  __bis_SR_register(LPM0_bits + GIE);     // LPM0 with interrupts enabled
  rt_volts = ADC10MEM;
  ADC10CTL0 &= ~ENC;
  ADC10CTL0 &= ~(REFON + ADC10ON);        // turn off A/D to save power
  rt_volts = (rt_volts*63)/13;				// return reading in V*1000 (~ 2500 * 2/1024)
  return (rt_volts);
}


unsigned int ReadADC_2_5(unsigned int INCH)	//Vref = 2.5V read ADC from INCH channel
{
	unsigned int ADC_Read;
    // Measure PD Voltage
    ADC10CTL1 = INCH;                    // read input channel
    ADC10CTL0 = SREF_1 + ADC10SHT_3 + REFON + ADC10ON + ADC10IE + REF2_5V;
    __delay_cycles(350);                    // delay to allow reference to settle
    ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start
    __bis_SR_register(LPM0_bits + GIE);     // LPM0 with interrupts enabled
    ADC_Read = ADC10MEM;
    ADC10CTL0 &= ~ENC;
    ADC10CTL0 &= ~(REFON + ADC10ON);        // turn off A/D to save power

	return ADC_Read;
}

unsigned int ReadADC_1_5(unsigned int INCH)	//Vref = 2.5V read ADC from INCH channel
{
	unsigned int ADC_Read;
    // Measure PD Voltage
    ADC10CTL1 = INCH;                    // read input channel
    ADC10CTL0 = SREF_1 + ADC10SHT_3 + REFON + ADC10ON + ADC10IE + REF2_5V;
    __delay_cycles(350);                    // delay to allow reference to settle
    ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start
    __bis_SR_register(LPM0_bits + GIE);     // LPM0 with interrupts enabled
    ADC_Read = ADC10MEM;
    ADC10CTL0 &= ~ENC;
    ADC10CTL0 &= ~(REFON + ADC10ON);        // turn off A/D to save power

	return ADC_Read;
}

/***********************************************************************
*BEGHDR
*NAME:        __interrupt void ADC10_ISR(void)
*DESCRIPTION: ADC10 interrupt service routine
*INPUTS:      void
*PROCESSING:  Exit from LPM after interrupt
*OUTPUTS:     void
***********************************************************************/
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void)
{
  __bic_SR_register_on_exit(LPM0_bits);     // Clear CPUOFF bit from 0(SR)
}

unsigned int ReadFibreSensor(void)	// pulse LED and read photodiode current
{
	unsigned int voltage;
	unsigned long voltage_long;
	unsigned int ADC;

	OA1_Config();	// configure OA1 and start op-amp
	Start_OA1_Slow();	// start op amp (in slow slew rate)
	delay(150);
	P4DIR |= BIT5;	// instead of Vcc
	P4OUT |= BIT5;
	P4DIR |= BIT3;		// IO OUT
	P4OUT &= ~BIT3;		// IO LO LED ON
	delay(150);
	ADC = ReadADC_1_5(INCH_13);	//Vref = 1.5V read ADC from INCH channel A13
	P4OUT |= BIT3;		// IO HI LED OFF
	P4OUT &= ~BIT5;	// instead of Vcc
	Stop_OA1();

	voltage_long = (unsigned long)ADC;
	voltage_long = voltage_long*1500;
	voltage = voltage_long>>10;

	return voltage;
}

/*
 * OA_Func_Set.c
 *
 *  Created on: 16 Nov 2014
 *      Author: Ralph S-J
 * Functions for reading op-amps
 */

void OA1_Config(void)	// configure OA0 and start op-amp
{
	OA1CTL0 = OAN_1 | OAP_0 | OAADC0;		// select input/output channels I-:OA1I1, I+:OA1I0, O:A12,13,14
    ADC10AE1 = BIT5;		// enable AO1 output to A13 (p4.4)
    ADC10AE0 =  BIT3 + BIT4;	// enable OA1I1, enable OA1I0
}

void Start_OA1_Slow(void)	// start op amp (in slow slew rate)
{
	OA1CTL0 |= OAPM_1;		// start op-amp in slow slew rate mode (lowest power consumption
}
void Start_OA1_Fast(void)	// start op amp (in slow slew rate)
{
	OA1CTL0 |= OAPM_3;		// start op-amp in fast slew rate mode (highest power consumption
}
void Stop_OA1(void)
{
	OA1CTL0 &= ~OAPM_3;		// stops op-amp by clearing bits form slew rate selector (will turn op-amp off form fast. medium or slow mode)
}

void USCI_A_Init(void)		// initialise USCI for tx'ing data to COM port
{
	  BCSCTL1 = CALBC1_1MHZ;                    // Set DCO
	  DCOCTL = CALDCO_1MHZ;

	  P3SEL |= 0x30;                            // P3.4,5 = USCI_A0 TXD/RXD
	  UCA0CTL1 = UCSSEL_2;                      // SMCLK
	  UCA0BR0 = 0x68;                           // 9600 from 1Mhz
	  UCA0BR1 = 0x0;
	  UCA0MCTL = UCBRS_2;
	  UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
	  IE2 |= UCA0RXIE;                          // Enable USCI_A0 RX interrupt
}

void TXString( char* string, int length )
{
  int pointer;
  for( pointer = 0; pointer < length; pointer++)
  {
    //volatile int i;
    UCA0TXBUF = string[pointer];
    while (!(IFG2&UCA0TXIFG));              // USCI_A0 TX buffer ready?
  }
}

void TXData(unsigned int data)
{
	char DataString[] = {"XXXX"};	// to hold integer from 0-9999

	DataString[0] = '0' + (data/1000)%10;
	DataString[1] = '0' + (data/100)%10;
	DataString[2] = '0' + (data/10)%10;
	DataString[3] = '0' + data%10;

	TXString(DataString,sizeof(DataString));
}
