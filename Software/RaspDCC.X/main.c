 /*********************************************************
 *   Welcome to RaspDCC                                   *
 *   Description: software for the PIC12F1840 micro       *
 *   used in the expansion shield for the Raspberry Pi    *
 *   It converts info received from the Raspberry Pi      *
 *   over the I2C channel into a PWM control signal for   *
 *   the L6205 MOSFET bridge.                             *
 *                                                        *
 *   Version: 0.2.1                                         *
 *   Licence: GNU GPL v3                                  *
 *   Website: www.nicolinux.eu/raspnerry-dcc              *
 *                                                        *
 *********************************************************/


#include <xc.h>
#include <stdint.h>
#include "PICconfig.h"
#include "PICinit.h"

// rename CCP1 output as DCC_TRACK_COMMAND
#define DCC_TRACK_COMMAND_WRITE LATAbits.LATA5
#define DCC_TRACK_COMMAND_READ PORTAbits.RA5

// RA3 is connected to the emergency pushbutton
#define PUSH_BUTTON_EMERGENCY PORTAbits.RA3

// configure the I2C address for comunication
#define I2C_slave_address 0x30

// definition of DCC semiwave HI and LOW duration (us)
#define DCC_RUN_LSB_LO  58
#define DCC_RUN_LSB_HI  100
#define DCC_RUN_MSB 0

// semi wave of 0.5ms (500us = 0h01f4)
// if timer 1 tick = 1us
#define DCC_PAUSE_MSB 0x01
#define DCC_PAUSE_LSB 0xf4

// set max number of buffers for I2C (max 42)
#define I2C_index_array_max 42
// DCC standard needs at most 4 byte per package
#define DCC_index_array_max 4 

// set the array for 8 different buffers
// (4 bytes long) to be used with I2C
volatile uint8_t I2C_RX_array[I2C_index_array_max][DCC_index_array_max] = 0;
// marker to save how many bytes have been send by the master
volatile uint8_t I2C_RX_array_usage[I2C_index_array_max] = 0;

// prepare variable used to work through the received and transmitted data
volatile uint8_t I2C_index_array = 0;
volatile uint8_t I2C_index_pkg_byte = 0;
uint8_t DCC_index_read = 0;

// function declarations
void DCC_send_bit(uint8_t, uint8_t);

/******* MAIN ROUTINE *******/
int main(void) {

    // create some control variables
    uint8_t index = 0;
    uint8_t index2 = 0;
    uint8_t xorbuffer = 0;

    // call function for registers set up
    initialize(I2C_slave_address);

    //reset Timer1 - NOW COUNTING STARTS
    TMR1H = 0;
    TMR1L = 0;

    // TODO force an inversion of DCC_TRACK_COMMAND_WRITE

    //clear CCP1 interrupt flag
    PIR1bits.CCP1IF = 0;

    // start with a pause wave
    DCC_send_bit(DCC_PAUSE_MSB, DCC_PAUSE_LSB);

    // here starts the infinite loop
    while (1) {

        // ENABLE control. if NOT ENABLE skip DCC transmission.
        if (PUSH_BUTTON_EMERGENCY) {

            // DCC transmit is started only if the I2C is ready to write the next array.
            // This if statement checks if I2C_index_array is higher than DCC_index_read
            // including some additional check to fit the return to the array start
            if ((DCC_index_read < I2C_index_array) || ((DCC_index_read == I2C_index_array_max - 1) && (I2C_index_array == 0))) {

                // before transmission start (second semiperiod of pause wave - 500us)
                // calculare XOR byte in buffer variable
                // enough time is assured to be available
                xorbuffer = I2C_RX_array[DCC_index_read][0];
                for (index = 1; index <= I2C_RX_array_usage[DCC_index_read]; index++) {
                    xorbuffer = xorbuffer ^ I2C_RX_array[DCC_index_read][index];
                }

                // send preambel (>11bit HI)
                for (index = 0; index < 13; index++) {
                    DCC_send_bit(DCC_RUN_MSB, DCC_RUN_LSB_HI);
                }

                // send start bit (1 bit LO)
                DCC_send_bit(DCC_RUN_MSB, DCC_RUN_LSB_LO);

                // for cycle to send all received bytes
                // separated by 1 bit LO
                for (index = 0; index <= I2C_RX_array_usage[DCC_index_read]; index++) {

                    // for cycle for each bit in byte
                    // transmit from MSB (bit 7) to LSB (bit 0)
                    for (index2 = 0; index2 < 8; index2++) {
                        if (I2C_RX_array[DCC_index_read][I2C_index_pkg_byte] & (0b10000000 >> index2)) {
                            DCC_send_bit(DCC_RUN_MSB, DCC_RUN_LSB_HI);
                        } else {
                            DCC_send_bit(DCC_RUN_MSB, DCC_RUN_LSB_LO);
                        }
                    }

                    // byte transmission completed
                    // send bit 0 (LO) at the end of each byte
                    DCC_send_bit(DCC_RUN_MSB, DCC_RUN_LSB_LO);

                } // end of for cycle for sending all received bytes

                // send error byte (XOR of all bytes): already calculated in xorbuffer
                // transmit XOR byte from MSB (bit 7) to LSB (bit 0)
                for (index2 = 0; index2 < 8; index2++) {
                    //if (I2C_RX_array[DCC_index_read][0] & (0b10000000 >> index2)) {
                    if (xorbuffer & (0b10000000 >> index2)) {
                        DCC_send_bit(DCC_RUN_MSB, DCC_RUN_LSB_HI);
                    } else {
                        DCC_send_bit(DCC_RUN_MSB, DCC_RUN_LSB_LO);
                    }
                }

                // send stop bit (1 bit HI)
                DCC_send_bit(DCC_RUN_MSB, DCC_RUN_LSB_HI);

                // clear buffers
                for (index = 0; index <= I2C_RX_array_usage[DCC_index_read]; index++) {
                    I2C_RX_array[DCC_index_read][index] = 0;
                }


            }// end of if( (DCC_index_read < I2C_index_array)...

            // after a package is sent send a pause wave, so the PIC
            // can calculate the next xorbuffer if needed
            // I do not trust the assembly code generated by the XC8 free
            // to be fast enought to generate xor within the 58us of a HI wave
            DCC_send_bit(DCC_PAUSE_MSB, DCC_PAUSE_LSB);


        } // enf of if (DCC_COMMAND_ENABLE)
        else {
            // if the enable command was pressed, just wair for a while
            _delay(255);
        } // enf of if (DCC_COMMAND_ENABLE)

        // clear watch dog counter
        CLRWDT();

    } // end of infinite loop

    return 0;
}

/******* DCC_send_bit ROUTINE *******/
void DCC_send_bit(uint8_t msb, uint8_t lsb) {
    // wait for interrupt flag bit of CCP1 to be set
    // wait for the previous wave transmission to be completed
    while (!PIR1bits.CCP1IF) {
        _delay(5);
    }

    // update the CCPR1L and CCPR1H
    // add the value of semi period to the registers. in this way I do not
    // change the timer count and there fore I do not mess up the calculation
    // (prescaler erased - 2 cycles skipped etc.)

    //set compare value for CCP1 low byte
    CCPR1L += lsb;

    // check if activity overflowed (status - carry bit)
    if(STATUSbits.CARRY) {
        CCPR1H++;
    }

    // update high byte of CCP1
    // works always if not in pause wave msb = 0
    CCPR1H += msb;
    // in assembly the last 2 instruction can be done in assembly
    // only with 1 instruction ADDWFC

    // clear CCP1IF flag
    PIR1bits.CCP1IF = 0;

    // wait for the first semiwave to be transmitted
    while (!PIR1bits.CCP1IF) {
        _delay(5);
    }

    // clear CCP1IF flag
    PIR1bits.CCP1IF = 0;
}

/******* INTERRUPT ROUTINE *******/
void interrupt ISR(void) {

    // check if I2C interrupt
    if (PIR1bits.SSP1IF) {
        if (!SSPSTATbits.R_nW) { // Raspberry is sending infos
            if (SSPSTATbits.P) {
                // save array length for transmission to track
                I2C_RX_array_usage[I2C_index_array] = I2C_index_pkg_byte - 1;

                // increase index of I2C_index_array
                I2C_index_array++;
                if (I2C_index_array == I2C_index_array_max) {
                    I2C_index_array = 0;
                }

                // reset I2C_index_pkg_byte for the mext transmission
                I2C_index_pkg_byte = 0;

            } else {
                if (!SSPSTATbits.D_nA) { // address byte received
                    // read SSPBUT to clear SSPSTAT.BF bit will be overwritten
                    // when data byte arrives
                    I2C_RX_array[I2C_index_array][I2C_index_pkg_byte] = SSPBUF;

                    SSP1CON1bits.CKP = 1; // release slock stretch
                }
                else { // data byte received
                    // save the received data in the right place :)
                    I2C_RX_array[I2C_index_array][I2C_index_pkg_byte] = SSPBUF;
                    // increase the index for byte
                    I2C_index_pkg_byte++;
                    SSP1CON1bits.CKP = 1; // release slock stretch
                }
            }
        } // end of raspberry sending info if (!SSPSTATbits.R_nW)

        //TODO: add case raspberry requests infos
        // the reply should contain
        // length of buffer: I2C_index_array_max
        // I2C_index_array
        // DCC_index_read

        PIR1bits.SSP1IF = 0;
    } // end of I2C interrupt

}

