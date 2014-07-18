/* 
 * File:   PICinit.h
 * Author: ultimodruido
 *
 * Created on 16 maggio 2014, 19.11
 */

#ifndef PICINIT_H
#define	PICINIT_H

#ifdef	__cplusplus
extern "C" {
#endif




#ifdef	__cplusplus
}
#endif


#include <stdint.h>

/******* INIT ROUTINE *******/
void initialize(uint8_t addr) {

    // uC set up
    OSCCON = 0b01111000; // Configure oscillator
             //1-------     use PLL to get 32Mhz (system clock)
             //-1110---     8 MHz internal oscillator (insctruction clock)
             //------00     Oscillator selected with FOSC
             //             in Configwords

    // configure watchdog
    WDTCONbits.WDTPS = 0b01100; //reset will occur every 4s
    
    // define GPIO configuration
    PORTA = 0b000000; // clear PORTA
    LATA  = 0b000000; // clear LATA
    TRISA = 0b001110; // set INPUT I2C pins and RA3

    // I2C comunication slave setup
    SSPSTAT = 0b10000000; // SSP status register
              //1-------    standard speed mode
              //-0------    non SMBus

    SSP1CON1 = 0b00110110; // SSP1 control register
               //--1-----   connect to pins RA1 e RA2
               //---1----   enable clock
               //----0110   I2C slave mode 7-bit address

    SSP1CON2bits.SEN = 1; //clock stretching is enables
    SSP1CON3bits.BOEN = 1; //????
    SSP1CON3bits.SDAHT = 1; //300ns holding time
    SSP1CON3bits.SBCDE = 1; //enable bus collision detect interrupt
    SSP1CON3bits.PCIE = 1; //enable interrupt on STOP bit

    SSPADD = addr;

    //setup Timer1 and Capture and Compare
    APFCONbits.CCP1SEL = 1; //set CCP1 on RA5 (RA2 is used for I2C)

    T1CON = 0b00110000;
            //00------   FOSC/4 as counting source
            //--11----   prescaler 1:8 (counting every us)
    T1GCONbits.TMR1GE = 0; // timer is not controlled by gate.

    TMR1H = 0; // reset timer1 High
    TMR1L = 0; // and Low bytes - prescaler automatic reset
    
    CCP1CON = 0b00001011; // set up capture and compare
              //----1011  Special trigger event. Set CCP1IF and reset Timer1

    // set ccp1 register to the highest value to avoid useless interrupt
    CCPR1H = 0xFF;
    CCPR1L = 0xFF;

    // setup interrupt events
    //clear all relevant interrupt flags
    PIR1bits.SSP1IF = 0;
    PIR1bits.TMR1IF = 0;
    PIR1bits.CCP1IF = 0;
    //activate interrupt bits
    PIE1bits.CCP1IE = 1;
    PIE1bits.SSP1IE = 1;
    INTCONbits.PEIE = 0;
    INTCONbits.GIE = 0;

}

#endif	/* PICINIT_H */

