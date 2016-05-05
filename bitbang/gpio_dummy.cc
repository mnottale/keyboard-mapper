#include <gpio.hh>
#include <fstream>
#include <unistd.h>


class DummyGPIO: public GPIO
{
public:
  DummyGPIO(bool shm)
  : shm(shm)
  {}
  
  void usleep(int duration) override
  {
    ::usleep(duration);
  }
  void select(int pin, Direction direction) override
  {
  }
  void set(int pin, bool high) override
  {
    std::ofstream ofs ((shm ? "/dev/shm/gpio" : "/tmp/gpio") + std::to_string(pin));
    ofs << (high ? 1 : 0);
  }
  int get(int pin)
  {
    std::ifstream ifs ((shm ? "/dev/shm/gpio" : "/tmp/gpio") + std::to_string(pin));
    int res;
    ifs >> res;
    return res;
  }
  bool shm;
};


static const int inu = GPIO::reg("dummy", [] { return new DummyGPIO(false);});
static const int inu2 = GPIO::reg("dummyshm", [] { return new DummyGPIO(true);});