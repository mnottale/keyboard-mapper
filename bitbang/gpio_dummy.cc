#include <gpio.hh>
#include <fstream>
#include <unistd.h>


class DummyGPIO: public GPIO
{
public:
  void usleep(int duration) override
  {
    ::usleep(duration);
  }
  void select(int pin, Direction direction) override
  {
  }
  void set(int pin, bool high) override
  {
    std::ofstream ofs ("/tmp/gpio" + std::to_string(pin));
    ofs << (high ? 1 : 0);
  }
  int get(int pin)
  {
    std::ifstream ifs ("/tmp/gpio" + std::to_string(pin));
    int res;
    ifs >> res;
    return res;
  }
};


static const int inu = GPIO::reg("dummy", [] { return new DummyGPIO();});