BitBang: serial communication over GPIO.
========================================

1. Principle
------------

This C++ software implements a unidirectional serial link between two computors
using 10 GPIO pins.

8 pins are used for data, and two for signaling: the sender notifies of data
readiness by swapping the first signal pin, and the recipient notifies data
acquisition by swapping the second signal pin. This techniques means the
serial link does not run at a fixed clock speed, but as fast as both peers
can push/pull data.

2. API
------

### Initialization


    struct BitBangConfig
    {
      int clock_pins[2]; // pin numbers of the two clock pins
      std::vector<int> data_pins; // pin numbers of the 8 data pins
      int speed; // target link speed in microseconds per byte
    };
    BitBang::BitBang(std::string const& gpioDriver, // driver to use, see below
                     bool writer, // true for writer side, false for reader side
                     BitBangConfig const& config); // pin configuration


### Reading and writing


    /// Write 'size' bytes from 'data'. If wait is true, block until data
    /// has been fully transmited
    void BitBang::write(const unsigned char* data, int size, bool wait);
    /// Read at most 'size' bytes into 'data'. If wait is false, copy what is
    /// available in buffer, else wait to transfer exactly 'size' bytes.
    /// Return number of bytes transfered
    int BitBang::read(unsigned char* data, int size, bool wait);


3. GPIO backends
----------------

The following GPIO backends are available:

- sys: Use /sys/class/gpio interface
- bcm2835: Use the bcm2835 library
- dummy: Dummy implementation using files into /tmp
- dummyshm: Dummy implementation using files in /dev/shm

4. Compiling
------------

Should be as simple as:

    g++ -std=c++11 -pthread bitbang.cc gpio.cc BACKENDS


5. Testing
----------

A sample test program is provided in ```bbcat.cc```.
Running in two terminals:

    bbcat w dummy 1000 1 2 3 4 5 6 7 8 9 10
    bbcat r dummy 1000 1 2 3 4 5 6 7 8 9 10

should give you a working one-way communication channel.
