#include "mbed.h"
#include "Callback.h"
#include "FileHandle.h"
#include "UARTSerial.h"
#include "mbed_poll.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"

using namespace utest::v1;

// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

// The baud rate to test at
#ifndef MBED_CONF_APP_TEST_BAUD_RATE
# define MBED_CONF_APP_TEST_BAUD_RATE        (115200 * 8)
#endif

// How long to run the test for
#ifndef MBED_CONF_APP_TEST_DURATION_SECONDS
# define MBED_CONF_APP_TEST_DURATION_SECONDS 10
#endif

// Resource not available, try again
#define EAGAIN 11

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// The serial port to test on
FileHandle * serial = new UARTSerial(D1, D0, MBED_CONF_APP_TEST_BAUD_RATE);

// A thread for receiving from UARTSeral
static Thread receiveTask;

// The total number of characters transmitted and received
unsigned int txCount = 0;
unsigned int rxCount = 0;

// An array of human readable data to send
static const char sendData[] =  "_____0000:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____0100:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____0200:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____0300:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____0400:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____0500:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____0600:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____0700:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____0800:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____0900:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____1000:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____1100:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____1200:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____1300:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____1400:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____1500:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____1600:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____1700:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____1800:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____1900:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789"
                                "_____2000:0123456789012345678901234567890123456789"
                                "01234567890123456789012345678901234567890123456789";

// ----------------------------------------------------------------
// PRIVATE FUNCTIONS
// ----------------------------------------------------------------

// Receive data function, to run inside receiveTask
void receiveData()
{
    pollfh fhs;
    fhs.fh = serial;
    fhs.events = POLLIN;
    char buffer[16];
    int ret;
    unsigned int y = 0;
    char lastChars[16] = {' '};
    unsigned int z = 0;

    // Flush the receiver
    while (serial->readable()) {
        serial->read(buffer, 1);
    }

    while (1) {
        // Block waiting for a character to arrive
        poll(&fhs, 1, -1);
        // Check if one of the other output events that can
        // always occur has occurred
        if (fhs.revents & (POLLHUP | POLLERR | POLLNVAL)) {
            TEST_ASSERT(false);
        } else {
            // Read until there's nothing left to read
            ret = 0;
            while (ret != -EAGAIN) {
                ret = serial->read(buffer, sizeof buffer);
                if (ret == -EAGAIN) {
                    // Nothing left, will wait for poll again
                } else if (ret <= 0) {
                    // Some form of error has occurred
                    TEST_ASSERT(false);
                } else {
                    // A character has been received, check that it is as expected
                    for (int x = 0; x < ret; x++) {
                        if (buffer[x] == sendData[y]) {
                            y++;
                            if (y >= sizeof (sendData) - 1) { // -1 for terminator
                                y = 0;
                            }
                            rxCount++;
                        } else {
                            // Not as expected, print out some context info
                            printf ("\n!!! Received %d character(s) (transmitted %d character(s)), received '%c', expected '%c', last %d character(s) received were !!!\n",
                                    rxCount + 1, txCount + 1, buffer[x], sendData[y], sizeof (lastChars));
                            for (unsigned int i = 0; i < sizeof (lastChars); i++) {
                                z++;
                                if (z > sizeof (lastChars)) {
                                    z = 0;
                                }
                                printf("%c", lastChars[z]);
                            }
                            printf("\n\n");
                            TEST_ASSERT(false);
                        }
                        // Store the last n characters so that we can print them out for info
                        // if an error occurs
                        lastChars[z] = buffer[x];
                        z++;
                        if (z > sizeof (lastChars)) {
                            z = 0;
                        }
                    }
                }
            }
        }
    }
}

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test serial echo
void test_serial_speed() {
    pollfh fhs;
    Timer timer;
    bool stopNow = false;
    const char * data;
    int len;
    int ret;

    fhs.fh = serial;
    fhs.events = POLLOUT;

// Enable flow control (you can enable this if you have Kevin's fork of mbed-os)
#if 1
# if DEVICE_SERIAL_FC
    ((UARTSerial *) serial)->set_flow_control(SerialBase::RTSCTS, D3, D2);
# endif
#endif

    // Start the receiver task
    TEST_ASSERT(receiveTask.start(callback(receiveData)) == osOK);
    
    // Send data for MBED_CONF_APP_TEST_DURATION_SECONDS or until we've sent 0xFFFFFFFF bytes
    timer.start();
    while (!stopNow) {
        data = sendData;
        len = sizeof(sendData) - 1;  // -1 for terminator
        while (!stopNow && (len > 0)) {
            if ((timer.read_ms() > MBED_CONF_APP_TEST_DURATION_SECONDS * 1000) || (txCount == 0xFFFFFFFF)) {
                stopNow = true;
            } else {
                // This is a blocking poll for room in the buffer to transmit into
                poll(&fhs, 1, -1);
                ret = serial->write(data, len);
                if (ret == -EAGAIN) {
                    // No room, wait to try again
                } else if (ret < 0) {
                    // Some form of error has occurred
                    TEST_ASSERT(false);
                } else {
                    // Successful transmission of [some of] the buffer
                    data += ret;
                    len -= ret;
                    txCount += ret;
                }
            }
        }
    }
    timer.stop();
    
    // Wait for the last few bytes to be received in the receiving task
    wait_ms(100);
    
    printf ("\n=== Test at %d bits/s completed after %.3f seconds, sent %d byte(s), received %d byte(s) (%d bits/s) ===\n\n",
            MBED_CONF_APP_TEST_BAUD_RATE, (float) timer.read_ms() / 1000, txCount, rxCount,
            (rxCount * 10) * 1000 / timer.read_ms()); // * 10 rather than * 8 to allow for start and stop bits

    TEST_ASSERT(txCount > 0);
    TEST_ASSERT(rxCount > 0);
}

// ----------------------------------------------------------------
// TEST ENVIRONMENT
// ----------------------------------------------------------------

// Setup the test environment
utest::v1::status_t test_setup(const size_t number_of_cases) {
    // Setup Greentea with a timeout
    GREENTEA_SETUP(MBED_CONF_APP_TEST_DURATION_SECONDS + 10, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
    Case("Serial speed test", test_serial_speed)
};

Specification specification(test_setup, cases);

// ----------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------

int main() {
    // Run tests
    return !Harness::run(specification);
}

// End Of File

