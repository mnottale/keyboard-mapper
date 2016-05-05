#include <boost/algorithm/string.hpp>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <bitbang/bitbang.hh>
#include <sys/types.h>
#include <dirent.h>
#include <linux/input.h>

#include <utils.cc>

int main(int argc, char** argv)
{
  std::string bbdriver = argv[1];
  std::string bbpins = argv[2];
  std::string bbspeed = argv[3];
  std::vector<std::string> vpins;
  boost::algorithm::split(vpins, bbpins, boost::is_any_of(","));
  BitBangConfig cfg;
  cfg.clock_pins[0] = std::stoi(vpins.at(0));
  cfg.clock_pins[1] = std::stoi(vpins.at(1));
  for (int i=0; i<8; ++i)
    cfg.data_pins.push_back(std::stoi(vpins.at(i+2)));
  cfg.speed = std::stoi(bbspeed);
  int fd = open_keyboard();
  BitBang bb(bbdriver, true, cfg);
  int size = sizeof (struct input_event);
  while (true)
  {
    struct input_event ev[64];
    int rd;
    while ((rd = read (fd, ev, size * 64)) < size)
    {
      perror("read()");
      fd = open_keyboard();
    }
    for (int i=0; i*size < rd; ++i)
    {
      if (ev[i].type != (int)Type::keyEvent)
        continue;
      unsigned char event[4] = {0xFF, (unsigned char)ev[i].value, (unsigned char)(ev[i].code >> 8), (unsigned char)ev[i].code};
      bb.write(event, 4, true);
    }
  }
}