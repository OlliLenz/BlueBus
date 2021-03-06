/*
 * File: main.c
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     The main code for our PIC24FJ bootloader
 */
#include <xc.h>
#include "sysconfig.h"
#include "mappings.h"
#include "lib/eeprom.h"
#include "lib/protocol.h"
#include "lib/timer.h"
#include "lib/uart.h"

int main(void)
{
    // Set the IVT mode to regular
    IVT_MODE = IVT_MODE_BOOT;

    // Set all ports to digital mode
    ANSB = 0;
    ANSC = 0;
    ANSD = 0;
    ANSE = 0;
    ANSF = 0;
    ANSG = 0;

    // Set all ports as outputs
    TRISB = 0;
    TRISC = 0;
    TRISD = 0;
    TRISE = 0;
    TRISF = 0;
    TRISG = 0;

    // Drive all ports to logic low
    LATB = 0;
    LATC = 0;
    LATD = 0;
    LATE = 0;
    LATF = 0;
    LATG = 0;

    // Set the I/O ports to outputs
    ON_LED_MODE = 0;
    BT_DATA_SEL_MODE = 0;
    UART_SEL_MODE = 0;
    IBUS_EN_MODE = 0;
    PAM_SHDN_MODE = 0;
    TEL_ON_MODE = 0;
    TEL_MUTE_MODE = 0;

    // Set the RX / input pins to inputs
    IBUS_UART_RX_PIN_MODE = 1;
    IBUS_UART_STATUS_MODE = 1;
    BC127_UART_RX_PIN_MODE = 1;
    SYSTEM_UART_RX_PIN_MODE = 1;
    EEPROM_SDI_PIN_MODE = 1;
    RECOVERY_MODE = 1;
    SYS_DTR_MODE = 1;
    
    // Set the UART mode to MCU for the remainder of this application code
    UART_SEL = UART_SEL_MCU;
    // Enable the IBus Regulator
    IBUS_EN = 1;
    // Turn the PAM8406 off until it's required
    PAM_SHDN = 0;
    // Keep the vehicle unmuted and telephone on disengaged
    TEL_MUTE = 0;
    TEL_ON = 0;
    // Keep the BC127 out of data mode
    BT_DATA_SEL = 0;

    // Ensure the Power LED is off
    ON_LED = 0;

    // Init the timer interrupt and the SPI module for the EEPROM
    TimerInit();
    EEPROMInit();

    UART_t systemUart;
    uint8_t BOOT_MODE = BOOT_MODE_APPLICATION;
    unsigned char configuredBootmode = EEPROMReadByte(CONFIG_BOOTLOADER_MODE);
    // If the bootloader flag is set in the EEPROM or the recovery pin
    // is set, then lock into bootloader mode
    if (configuredBootmode != 0x00 || RECOVERY_STATUS == 0) {
        BOOT_MODE = BOOT_MODE_BOOTLOADER;
        EEPROMWriteByte(CONFIG_BOOTLOADER_MODE, 0x00);
        TimerEnableLED();
        // We listen for firmware from a single UART based on the value stored
        // in the EEPROM. This makes recovery from the BT module impossible
        // but that is okay. If we listen on both UARTs, things get messy.
        if (configuredBootmode == BOOT_SOURCE_BC127) {
            BT_DATA_SEL = 1;
            systemUart = UARTInit(
                SYSTEM_UART_MODULE,
                BC127_UART_RX_RPIN,
                BC127_UART_TX_RPIN,
                UART_BAUD_115200,
                UART_PARITY_NONE
            );
        } else {
            systemUart = UARTInit(
                SYSTEM_UART_MODULE,
                SYSTEM_UART_RX_RPIN,
                SYSTEM_UART_TX_RPIN,
                UART_BAUD_115200,
                UART_PARITY_ODD
            );
        }
        // Register the module handlers at a global scope
        UARTAddModuleHandler(&systemUart);
    }

    while (BOOT_MODE == BOOT_MODE_BOOTLOADER && BOOT_MODE != BOOT_MODE_NOW) {
        UARTReadData(&systemUart);
        if (systemUart.rxQueueSize > 0) {
            ProtocolProcessMessage(&systemUart, &BOOT_MODE);
        }
    }
    
    // Close the UART module so the application can utilize it
    if (BOOT_MODE == BOOT_MODE_NOW) {
        UARTDestroy(SYSTEM_UART_MODULE);
    }

    // Close the EEPROM (SPI module) so that the application can utilize it
    EEPROMDestroy();

    // Turn off the LED & Set the BC127 back to normal mode
    ON_LED = 0;
    BT_DATA_SEL = 0;

    // Wait until the specified timeout so that the rest of the board
    // has a chance to power up before we start configuring it in the application
    while (TimerGetMillis() <= BOOTLOADER_TIMEOUT);

    // Disable the timers we set up
    TimerDestroy();

    // Set the IVT mode to regular
    IVT_MODE = IVT_MODE_APP;

    // Call the application code
    void (*appptr)(void);
    appptr = (void (*)(void))BOOTLOADER_APPLICATION_VECTOR;
    appptr();

    return 0;
}

// Trap Catches
void __attribute__ ((__interrupt__, auto_psv)) _OscillatorFail(void)
{
    // Clear the trap flag
    INTCON1bits.OSCFAIL = 0;
    ON_LED = 0;
    while (1);
}

void __attribute__ ((__interrupt__, auto_psv)) _AddressError(void)
{
    // Clear the trap flag
    INTCON1bits.ADDRERR = 0;
    ON_LED = 0;
    while (1);
}


void __attribute__ ((__interrupt__, auto_psv)) _StackError(void)
{
    // Clear the trap flag
    INTCON1bits.STKERR = 0;
    ON_LED = 0;
    while (1);
}

void __attribute__ ((__interrupt__, auto_psv)) _MathError(void)
{
    // Clear the trap flag
    INTCON1bits.MATHERR = 0;
    ON_LED = 0;
    while (1);
}

void __attribute__ ((__interrupt__, auto_psv)) _NVMError(void)
{
    ON_LED = 0;
    while (1);
}

void __attribute__ ((__interrupt__, auto_psv)) _GeneralError(void)
{
    ON_LED = 0;
    while (1);
}
