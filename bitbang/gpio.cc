#include <unordered_map>
#include <functional>
#include <gpio.hh>

std::unordered_map<std::string, std::function<GPIO*()>> map;

int GPIO::reg(std::string const& name, std::function<GPIO*()> f)
{
  map[name] = f;
}

GPIO* GPIO::create(std::string const& name)
{
  return map.at(name)();
}