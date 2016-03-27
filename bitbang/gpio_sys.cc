#include <gpio.hh>
#include <fstream>
#include <set>
#include <unistd.h>


class GPIOSys: public GPIO
{
public:
  ~GPIOSys() {};
  template<typename T>
  void write(std::string const& where, T what)
  {
    std::ofstream ofs("/sys/class/gpio/" + where);
    ofs << what;
  }
  void usleep(int duration) override
  {
    ::usleep(duration);
  }
  void select(int pin, Direction direction) override
  {
    if (exported.find(pin) == exported.end())
    {
      write("export", pin);
      exported.insert(pin);
    }
    write("gpio" + std::to_string(pin) + "/direction",
      direction == Direction::input? "in" : "out");
  }
  void set(int pin, bool high) override
  {
    write("gpio" + std::to_string(pin) + "/value", high? 1:0);
  }
  int get(int pin)
  {
    std::ifstream ifs("/sys/class/gpio/gpio" + std::to_string(pin) + "/value");
    int res;
    ifs >> res;
    return res;
  }
  std::set<int> exported;
};

static const int inu = GPIO::reg("sys", [] { return new GPIOSys();});