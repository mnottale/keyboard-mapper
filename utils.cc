#include <unordered_map>
#include <vector>


void split(std::vector<std::string>& res, std::string what,
           std::string const& sep)
{
  while (!what.empty())
  {
    auto p = what.find_first_of(sep);
    if (p == what.npos)
    {
      res.push_back(what);
      return;
    }
    res.push_back(what.substr(0, p));
    what = what.substr(p+1);
  }
}

void trim(std::string& what, bool left = true, bool right = true)
{
  if (left) while (!what.empty() && what[0] == ' ') what = what.substr(1);
  if (right) while (!what.empty() && what.back() == ' ') what = what.substr(0, what.size()-1);
}

enum class Type
{
  padding = 0,
  keyEvent = 1,
  stamp = 4,
};
enum class Action // code
{
  up = 0,
  down = 1,
  repeat = 2,
};

typedef std::unordered_map<std::string, std::string> Config;

std::vector<std::string> list_directory(std::string const& path)
{
  DIR* d = opendir(path.c_str());
  if (!d)
  {
    perror("opendir");
    return {};
  }
  std::vector<std::string> res;
  while (true)
  {
    auto ent = readdir(d);
    if (!ent)
      break;
    res.push_back(ent->d_name);
  }
  closedir(d);
  return res;
}

int open_keyboard()
{
  while (true)
  {
    auto inputs = list_directory("/dev/input/by-id");
    for (auto i: inputs)
    {
      if (i.size() >= 3 & i.find("kbd") == i.size() - 3)
      {
        auto f = "/dev/input/by-id/" + i;
        std::cerr << "found keyboard at " << f << std::endl;
        int fd = open(f.c_str(), O_RDONLY);
        if (fd >= 0)
          return fd;
      }
    }
    std::cerr << "waiting for keyboard..." << std::endl;
    usleep(500000);
  }
}

class Keyboard
{
public:
  Keyboard(Config const& config)
  : fd(-1)
  {
    if (config.find("keyboard") != config.end())
    {
      std::vector<std::string> c;
      split(c, config.at("keyboard"), " ,");
      BitBangConfig bconfig;
      std::string bbdriver = c.at(0);
      bconfig.speed = std::stoi(c.at(1));
      bconfig.clock_pins[0] = std::stoi(c.at(2));
      bconfig.clock_pins[1] = std::stoi(c.at(3));
      for (int i=0; i<8; ++i)
        bconfig.data_pins.push_back(std::stoi(c.at(4+i)));
      bb.reset(new BitBang(bbdriver, false, bconfig));
    }
    else
    {
      fd = open_keyboard();
      char name[256];
      ioctl (fd, EVIOCGNAME (sizeof (name)), name);
      printf("Reading From : %s\n", name);
    }
  }
  int read(input_event* evs, int size)
  {
    if (bb)
    {
      unsigned char data[3];
      while (true)
      {
        bb->read(data, 1, true);
        if (data[0] == 0xFF)
          break;
      }
      bb->read(data, 3, true);
      evs[0].type = (int)Type::keyEvent;
      evs[0].value = data[0];
      evs[0].code = (data[1] << 8) + data[2];
      return 1;
    }
    else
    {
      int evsize = sizeof(input_event);
      int rd;
      while ((rd = ::read (fd, evs, evsize * size)) < evsize)
      {
        perror("read()");
        fd = open_keyboard();
      }
      return rd / evsize;
    }
  }
  int fd;
  std::unique_ptr<BitBang> bb;
};
