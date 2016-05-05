#include <bitbang.hh>
#include <iostream>

int main(int argc, char** argv)
{
  if (argc != 14)
  {
    std::cerr << "Usage: " << argv[0]
      << " r|w driver clock pinctrl1 pinctrl2 pindata1 ... pindata8"
      << std::endl;
    return 1;
  }
  BitBangConfig config;
  config.clock_pins[0] = std::stoi(argv[4]);
  config.clock_pins[1] = std::stoi(argv[5]);
  config.data_pins.resize(8);
  for (int i=0; i<8; ++i)
    config.data_pins[i] = std::stoi(argv[6+i]);
  config.speed = std::stoi(argv[3]);
  bool writer = argv[1][0] == 'w';
  BitBang bb(argv[2], writer, config);
  if (writer)
  {
    while (std::cin.good() && ! std::cin.eof())
    {
      //std::string line;
      //std::getline(std::cin, line);
      //line += '\n';
      //bb.write((const unsigned char*)line.data(), line.size(), true);
      unsigned char c = std::cin.get();
      bb.write(&c, 1, true);
    }
  }
  else
  {
    while (true)
    {
      unsigned char c;
      bb.read(&c, 1, true);
      std::cout.write((char*)&c, 1);
    }
  }
  exit(0);
}