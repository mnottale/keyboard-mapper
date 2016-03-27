#ifndef GPIO_HH
# define GPIO_HH
#include <string>
#include <functional>

enum class Direction
{
  input,
  output
};
class GPIO
{
public:
  virtual ~GPIO() {};
  virtual void usleep(int duration) = 0;
  virtual void select(int pin, Direction direction) = 0;
  virtual void set(int pin, bool high) = 0;
  virtual int get(int pin) = 0;

  static int reg(std::string const& name, std::function<GPIO*()> factory);
  static GPIO* create(std::string const& name);
};

#endif