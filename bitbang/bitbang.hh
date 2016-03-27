#ifndef BITBANG_HH
#define BITBANG_HH
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <gpio.hh>

struct BitBangConfig
{
  int clock_pins[2];
  std::vector<int> data_pins;
  int speed; // target speed in microseconds per clock swap
};

class BitBang
{
public:
  BitBang(std::string const& gpioDriver,
          bool writer, BitBangConfig const& config);
  ~BitBang();
  void write(const unsigned char* data, int size, bool wait);
  int read(unsigned char* data, int size, bool wait);
private:
  BitBangConfig config;
  GPIO* gpio;
  volatile int exit;
  void realtime();
  void loop_reader();
  void loop_writer();
  std::vector<unsigned char> buffer;
  std::mutex mutex;
  std::condition_variable cond_nonempty;
  std::condition_variable cond_empty;
};

#endif