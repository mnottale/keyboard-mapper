#include <bitbang.hh>
#include <cstring>
#include <iostream>
#include <sys/mman.h>




BitBang::BitBang(std::string const& gpioDriver,
          bool writer, BitBangConfig const& config)
  : config(config)
  , gpio(GPIO::create(gpioDriver))
  , exit(0)
{
  if (config.data_pins.size() != 8)
    throw std::runtime_error("Only 8 bit bus is supported");
  std::thread t([this, writer] { if (writer) loop_writer(); else loop_reader();});
  t.detach();
}

BitBang::~BitBang()
{
  exit = 1;
  while (exit != 2)
    gpio->usleep(50000);
}

void BitBang::realtime()
{
  struct sched_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
  sched_setscheduler(0, SCHED_FIFO, &sp);
  mlockall(MCL_CURRENT | MCL_FUTURE);
}

void BitBang::loop_writer()
{
  realtime();
  gpio->select(config.clock_pins[0], Direction::output);
  gpio->select(config.clock_pins[1], Direction::input);
  for (int i: config.data_pins)
    gpio->select(i, Direction::output);
  gpio->set(config.clock_pins[0], 0);
  int ctrlout = config.clock_pins[0];
  int ctrlin = config.clock_pins[1];
  while (!exit)
  {
    std::vector<unsigned char> b;
    {
      std::unique_lock<std::mutex> lock(mutex);
      cond_nonempty.wait(lock, [&] { return !buffer.empty();});
      std::swap(b, buffer);
    }
    for (auto c: b)
    {
      // wait for reader
      //std::cerr << "wait for reader" << std::endl;
      int expect = 1 - gpio->get(ctrlout);
      while (gpio->get(ctrlin) != expect)
      {
        gpio->usleep(config.speed);
        if (exit)
        {
          exit = 2;
          return;
        }
      }
      //std::cerr << "transmiting" << std::endl;
      for (int i=0; i < 8; ++i)
        gpio->set(config.data_pins[i], (c & (1 << i)) ? true : false);
      gpio->set(ctrlout, expect);
    }
    {
      std::unique_lock<std::mutex> lock(mutex);
      if (buffer.empty())
        cond_empty.notify_all();
    }
  }
  exit = 2;
}

void BitBang::loop_reader()
{
  realtime();
  gpio->select(config.clock_pins[0], Direction::input);
  gpio->select(config.clock_pins[1], Direction::output);
  for (int i: config.data_pins)
    gpio->select(i, Direction::input);
  gpio->set(config.clock_pins[1], 0);
  int ctrlout = config.clock_pins[1];
  int ctrlin = config.clock_pins[0];
  while (!exit)
  {
    //std::cerr << "wait for writer" << std::endl;
    int expect = 1 - gpio->get(ctrlout);
    gpio->set(ctrlout, expect);
    while (gpio->get(ctrlin) != expect)
    {
      gpio->usleep(config.speed);
      if (exit)
      {
        exit = 2;
        return;
      }
    }
    //std::cerr << "reading" << std::endl;
    unsigned char res = 0;
    for (int i=0; i < 8; ++i)
      res |= (gpio->get(config.data_pins[i]) ? 1 : 0) << i;
    {
      std::unique_lock<std::mutex> lock(mutex);
      buffer.push_back(res);
      cond_nonempty.notify_all();
    }
  }
  exit = 2;
}

void BitBang::write(const unsigned char* data, int size, bool wait)
{
  std::unique_lock<std::mutex> lock(mutex);
  for (int i=0; i<size; ++i)
    buffer.push_back(data[i]);
  cond_nonempty.notify_all();
  if (wait)
    cond_empty.wait(lock, [&] { return buffer.empty();});
}

int BitBang::read(unsigned char* data, int size, bool wait)
{
  std::unique_lock<std::mutex> lock(mutex);
  int res = 0 ;
  while (true)
  {
    auto to_read = std::min(size, (signed)buffer.size());
    memcpy(data, &buffer[0], to_read);
    memmove(&buffer[0], &buffer[to_read], buffer.size() - to_read);
    buffer.resize(buffer.size() - to_read);
    if (!wait || size == to_read)
      return res + to_read;
    data += to_read;
    size -= to_read;
    res += to_read;
    cond_nonempty.wait(lock, [&] { return !buffer.empty();});
  }
}