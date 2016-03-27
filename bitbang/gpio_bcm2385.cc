#include <gpio.hh>

#include <bcm2835.h>


static int map_pin(int p)
{
  #define MAP(pi, po) \
    case pi: return po; break;
      switch(p) {
    MAP(2, RPI_BPLUS_GPIO_J8_03)
    MAP(3, RPI_BPLUS_GPIO_J8_05)
    MAP(4, RPI_BPLUS_GPIO_J8_07)
    MAP(14, RPI_BPLUS_GPIO_J8_08)
    MAP(15, RPI_BPLUS_GPIO_J8_10)
    MAP(17, RPI_BPLUS_GPIO_J8_11)
    MAP(18, RPI_BPLUS_GPIO_J8_12)
    MAP(27, RPI_BPLUS_GPIO_J8_13)
    MAP(22, RPI_BPLUS_GPIO_J8_15)
    MAP(23, RPI_BPLUS_GPIO_J8_16)
    MAP(24, RPI_BPLUS_GPIO_J8_18)
    MAP(10, RPI_BPLUS_GPIO_J8_19)
    MAP(9, RPI_BPLUS_GPIO_J8_21)
    MAP(25, RPI_BPLUS_GPIO_J8_22)
    MAP(11, RPI_BPLUS_GPIO_J8_23)
    MAP(8, RPI_BPLUS_GPIO_J8_24)
    MAP(7, RPI_BPLUS_GPIO_J8_26)
    MAP(5, RPI_BPLUS_GPIO_J8_29)
    MAP(6, RPI_BPLUS_GPIO_J8_31)
    MAP(12, RPI_BPLUS_GPIO_J8_32)
    MAP(13, RPI_BPLUS_GPIO_J8_33)
    MAP(19, RPI_BPLUS_GPIO_J8_35)
    MAP(16, RPI_BPLUS_GPIO_J8_36)
    MAP(26, RPI_BPLUS_GPIO_J8_37)
    MAP(20, RPI_BPLUS_GPIO_J8_38)
    MAP(21, RPI_BPLUS_GPIO_J8_40)
  default:
    throw std::runtime_error("No such pin " + std::to_string(p));
    break;
      }
    
}

class BCMGPIO: public GPIO
{
public:
  BCMGPIO()
  {
    if (!bcm2835_init())
      throw std::runtime_error("Failed to initialize libbcm2835");
  }
  ~BCMGPIO()
  {
    bcm2835_close();
  }
  void usleep(int duration) override
  {
    bcm2835_delayMicroseconds(duration);
  }
  void select(int pin, Pin direction) override
  {
     bcm2835_gpio_fsel(map_pin(pin),
     direction == Direction::input ? BCM2835_GPIO_FSEL_INPT : BCM2835_GPIO_FSEL_OUTP);
  }
  void set(int pin, bool high) override
  {
    bcm2835_gpio_write(map_pin(pin), high ? HIGH : LOW);
  }
  int get(int pin) override
  {
    return (bcm2835_gpio_lev(map_pin(pin)) == HIGH) ? 1 : 0;
  }
};

static const int unused = GPIO::reg("bcm2835", [] { return new BCMGPIO();});