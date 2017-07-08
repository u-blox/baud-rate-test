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

// The accepted percentage difference between
// the baud rate and the achieved throughput
#ifndef MBED_CONF_APP_TEST_SPEED_TOLERANCE
# define MBED_CONF_APP_TEST_SPEED_TOLERANCE  20
#endif

// The maximum baud rate for which the above
// tolerance should apply
#ifndef MBED_CONF_APP_TEST_MAX_BAUD_RATE
# define MBED_CONF_APP_TEST_MAX_BAUD_RATE    460800
#endif

// Serial port RX pin
#ifndef MBED_CONF_APP_RX_PIN
# define MBED_CONF_APP_RX_PIN                D0
#endif

// Serial port TX pin
#ifndef MBED_CONF_APP_TX_PIN
# define MBED_CONF_APP_TX_PIN                D1
#endif

// Serial port RTS pin
#ifndef MBED_CONF_APP_RTS_PIN
# define MBED_CONF_APP_RTS_PIN               D3
#endif

// Serial port CTS pin
#ifndef MBED_CONF_APP_CTS_PIN
# define MBED_CONF_APP_CTS_PIN               D2
#endif

// Whether to use flow control or not
#ifndef MBED_CONF_APP_USE_FLOW_CONTROL
# define MBED_CONF_APP_USE_FLOW_CONTROL      true
#endif

// How long to run each speed test for
#ifndef MBED_CONF_APP_TEST_DURATION_SECONDS
# define MBED_CONF_APP_TEST_DURATION_SECONDS 10
#endif

// Poll return value that means "resource not available, try again"
#define EAGAIN 11

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// The possible baud rates to test at
static const int baudRates[] = {9600, 57600, 115200, 230400, 460800, 921600, 1843200, 3686400, 7372800};

// The serial port to test on
static FileHandle * serial = new UARTSerial(MBED_CONF_APP_TX_PIN, MBED_CONF_APP_RX_PIN);

// A thread for receiving from UARTSeral
static Thread receiveTask;

// The total number of characters transmitted and received
static unsigned int txCount = 0;
static unsigned int rxCount = 0;

// A pointer to the next character expected to be received
static const char * nextRxChar = NULL;

// An array of human readable values to send/receive
static const char data[] =  "_____0000:0123456789012345678901234567890123456789"
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

// Receive data function, receiving data on fh.
// This function should be run inside receiveTask.  It
// flushes fh and then checks for repeated patterns of
// data, asserting if the received data ever doesn't
// match the expected pattern.
static void receiveData(FileHandle *fh)
{
    pollfh fhs;
    char buffer[16];
    int ret;
    char lastChars[16] = {' '};
    unsigned int y = 0;

    fhs.fh = fh;
    fhs.events = POLLIN;

    // Flush the receiver
    while (fh->readable()) {
        fh->read(buffer, 1);
    }

    nextRxChar = data;

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
                ret = fh->read(buffer, sizeof buffer);
                if (ret == -EAGAIN) {
                    // Nothing left, will wait for poll again
                } else if (ret <= 0) {
                    // Some form of error has occurred
                    TEST_ASSERT(false);
                } else {
                    // A character has been received, check that it is as expected
                    for (int x = 0; x < ret; x++) {
                        if (buffer[x] == *nextRxChar) {
                            nextRxChar++;
                            if (nextRxChar >= data + sizeof (data) - 1) { // -1 for terminator
                                nextRxChar = data;
                            }
                            rxCount++;
                        } else {
                            // Not as expected, print out some context info
                            printf ("\n!!! Received %d character(s) (transmitted %d character(s)), received '%c', expected '%c', last %d character(s) received were !!!\n",
                                    rxCount + 1, txCount + 1, buffer[x], *nextRxChar, sizeof (lastChars) + 1);
                            for (unsigned int i = 0; i < sizeof (lastChars); i++) {
                                y++;
                                if (y > sizeof (lastChars)) {
                                    y = 0;
                                }
                                printf("%c", lastChars[y]);
                            }
                            printf("%c", buffer[x]);
                            printf("\n\n");
                            TEST_ASSERT(false);
                        }
                        // Store the last n characters so that we can print them out for info
                        // if an error occurs
                        lastChars[y] = buffer[x];
                        y++;
                        if (y > sizeof (lastChars)) {
                            y = 0;
                        }
                    }
                }
            }
        }
    }
}

// Send repeated patterns of data to fh at the given speed
// for the given duration
static void sendData(FileHandle *fh, int speed, int durationMilliseconds)
{
    pollfh fhs;
    Timer timer;
    bool stopNow = false;
    const char * buffer;
    int len;
    int ret;

    ((UARTSerial *) fh)->set_baud(speed);

    fhs.fh = fh;
    fhs.events = POLLOUT;

    // Send data for durationMilliseconds or until we've sent 0xFFFFFFFF bytes
    timer.start();
    while (!stopNow) {
        buffer = data;
        len = sizeof(data) - 1;  // -1 for terminator
        while (!stopNow && (len > 0)) {
            if ((timer.read_ms() > durationMilliseconds) || (txCount == 0xFFFFFFFF)) {
                stopNow = true;
            } else {
                // This is a blocking poll for room in the buffer to transmit into
                poll(&fhs, 1, -1);
                ret = fh->write(buffer, len);
                if (ret == -EAGAIN) {
                    // No room, wait to try again
                } else if (ret < 0) {
                    // Some form of error has occurred
                    TEST_ASSERT(false);
                } else {
                    // Successful transmission of [some of] the buffer
                    buffer += ret;
                    len -= ret;
                    txCount += ret;
                }
            }
        }
    }
    timer.stop();
}

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test serial echo
void test_serial_speed()
{
    Timer timer;
    int throughput;
    int limit;

    // Enable flow control
#if MBED_CONF_APP_USE_FLOW_CONTROL
# if DEVICE_SERIAL_FC
    ((UARTSerial *) serial)->set_flow_control(SerialBase::RTSCTS, MBED_CONF_APP_RTS_PIN, MBED_CONF_APP_CTS_PIN);
# endif
#endif

    // Start the receiver task
    TEST_ASSERT(receiveTask.start(callback(receiveData, serial)) == osOK);

    // Run through all the baud rates up to the maximum rate at which
    // the given test tolerance should apply
    for (int x = 0; (x < sizeof (baudRates) / sizeof (baudRates[0])) &&
                    baudRates[x] <= MBED_CONF_APP_TEST_MAX_BAUD_RATE; x++) {

        txCount = 0;
        rxCount = 0;
        nextRxChar = data;

        // Send the data
        timer.reset();
        timer.start();
        sendData(serial, baudRates[x], MBED_CONF_APP_TEST_DURATION_SECONDS * 1000);
        timer.stop();

        // Wait for the last few bytes to be received in the receiving task
        wait_ms(1000);

        // Calculate the throughput assuming 10 bits per byte (i.e. including start and stop bits)
        throughput = rxCount / (timer.read_ms() / 10000);

        // Calculate the minimum expected throughput
        limit = baudRates[x] * (100 - MBED_CONF_APP_TEST_SPEED_TOLERANCE) / 100;

        printf ("\n=== Test run %d, at %d bits/s completed after %.3f seconds, sent %d byte(s), received %d byte(s) (throughput %d bits/s with a threshold of %d bits/s) ===\n\n",
                x + 1, baudRates[x], (float) timer.read_ms() / 1000, txCount, rxCount, throughput, limit);

        // Check the limits
        TEST_ASSERT(throughput >= limit);
        TEST_ASSERT(txCount > 0);
        TEST_ASSERT(rxCount == txCount);
    }
}

// ----------------------------------------------------------------
// TEST ENVIRONMENT
// ----------------------------------------------------------------

// Setup the test environment
utest::v1::status_t test_setup(const size_t number_of_cases) {
    // Setup Greentea with a timeout, which is set for maximum the number of iterations plus a margin
    GREENTEA_SETUP(MBED_CONF_APP_TEST_DURATION_SECONDS * (sizeof (baudRates) / sizeof (baudRates[0])) + 5, "default_auto");
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

