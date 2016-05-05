

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