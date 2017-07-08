# Introduction
This repo is intended to help resolve the issue raised here: https://github.com/ARMmbed/mbed-os/issues/4722.  It is written as an mbed Greentea test.  The test uses a serial port wired in loop-back at increasing baud rates and checks that there is no character loss and that the expected throughput is achieved.

As of 7th July 2017, this repo should be used with the following fork of mbed-os:

https://github.com/kjbracey-arm/mbed-os/commits/uartserial_flow

...rather than `mbed-os` master, since flow control (which is really needed for high data rate throughput tests) is only exposed through the `UARTSerial()` class in this fork.

Note: you may experience a compilation error at line 194 of `UARTSerial.cpp`, if so just edit `rx_buf` to be `_rxbuf`.

# Hardware Setup
This test requires an mbed board with a spare serial port on which the `Tx` output is looped back to the `Rx` input and the `RTS` output is looped back to the `CTS` input.  The pins used are assumed to be these pins on the board's Arduino header:

* `Rx`:  `D0`
* `Tx`:  `D1`
* `CTS`: `D2`
* `RTS`: `D3`

If other pins are to be used, the pin allocation may be modified in `mbed_app.json`; please see the `mbed_app.json` in this repo for further information.

# Configuration
In addition to chosing different serial port pins, there are four other things that can be configured in `mbed_app.json`:

* `TEST_SPEED_TOLERANCE`: the permissible percentage difference between the baud rate and the achieved throughput, default 20%.
* `TEST_MAX_BAUD_RATE`: the maximum baud rate for which the above tolerance should apply, default 460800.
* `TEST_USE_FLOW_CONTROL`: `true` if flow control is to be used, defaults to `true` (HIGHLY recommended if the tests are to have any chance of success at high baud rates).
* `TEST_DURATION_SECONDS`: duration of each iteration of the test, default 10 seconds.

# Running The Test
* Clone this repo, fetch your chosen version of `mbed-os` into it and set your chosen mbed target/toolchain as defaults.
* Perform the hardware setup steps above and consider any items you may wish to change in `mbed_app.json`.
* Connect your target hardware and then compile/run the test with:

`mbed test -v -n tests-unit_tests-default`

* When the test is running, you will see diagnostic prints of the following form:

```
    [1499504813.43][CONN][RXD] >>> Running case #1: 'Serial speed test'...
    [1499504813.43][CONN][INF] found KV pair in stream: {{__testcase_start;Serial speed test}}, queued...
    [1499504824.43][CONN][RXD]
    [1499504824.45][CONN][RXD] === Test run 1, at 9600 bits/s completed after 10.001 seconds, sent 8984 byte(s), received 8984 byte(s) (throughput 8984 bits/s with a threshold of 7680 bits/s) ===
    [1499504824.45][CONN][RXD]
    [1499504835.45][CONN][RXD]
    [1499504835.47][CONN][RXD] === Test run 2, at 57600 bits/s completed after 10.001 seconds, sent 48269 byte(s), received 48269 byte(s) (throughput 48269 bits/s with a threshold of 46080 bits/s) ===
    ...
```

* If there is character loss, or the expected throughput is not achieved, you will see something like:

    `:301::FAIL: Expression Evaluated To FALSE`

    ...and, usually, the reason for failure will be obvious from the diagnostic print that precedes that line.  If not, please check the line number in the file `TESTS\unit_tests\default\main.cpp` to determine the error cause.

# Running The Test Under A Debugger
If you wish to run the test under a debugger, then first do a clean compilation as follows to get debug information into your `.elf` file.

`mbed test -n tests-unit_tests-default --profile mbed-os/tools/profiles/debug.json --compile -c`

Drag and drop the built binary (`BUILD\tests\<target name>\<toolchain>\TESTS\unit_tests\default\default.bin`) onto your target.

Run `mbedls` to determine the COM port that your mbed board is connected to. Supposing it is `COM1`, you would then start the target board under your debugger and, on the PC side, enter the following to begin the test:

`mbedhtrun --skip-flashing --skip-reset -p COM1:115200`
