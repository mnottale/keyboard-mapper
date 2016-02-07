#include <boost/algorithm/string.hpp>
#include <set>
#include <map>
#include <sstream>
#include <fstream>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>

/*
int linux_to_x[] = {
  0, 9,
  10,11,12,13,14,15,16,17,18,19,20,21,   // 13: key_equal
  22,23,24,25,26,27,28,29,30,31,32,33,34,35, // 27: key_rightbrace
  36,37,38,39,40,41,42,43,44,45,46,47,48, // 40: key_apostr
  49,50,51,52,53,54,55,56,57,58,59,60,61,62, // 54  rshift
  63,64,65,66, // 58 caps
  67,68,69,70,71,72,73,74,75,76,  // 68 F10
  77,78,79,80,81,82,83,84,85,86,87,88,89,90,91, // 83: kpdot
  92,93,94,95,96, // 88: KEY_F12
  
};*/

int linux_to_x(int k)
{
  return k + 8;
}
int x_to_linux(int k)
{
  return k - 8;
}

enum class ModsL
{
  lshift = 42,
  rshift = 54,
  lctrl = 29,
  rctrl = 97,
  lalt = 56,
  ralt = 100,
};

std::set<int> lmodifiers = {42, 54, 29, 97, 56, 100};


enum class Action // code
{
  up = 0,
  down = 1,
  repeat = 2,
};

enum class Type
{
  padding = 0,
  keyEvent = 1,
  stamp = 4,
};

struct Event
{
  Action action;
  int lcode;
};

struct XModMap
{
  std::vector<std::vector<std::string>> keycodes;
  std::map<std::string, int> rev; // keyname -> keycode
  std::set<int> metas; // xcodes
  std::set<int> shifts;
  std::set<int> caps;
  std::string lookup(int code, int index) const
  {
    if (code >= keycodes.size())
      return {};
    auto const& line = keycodes[code];
    if (line.size() > index)
      return line[index];
    if (!index)
      return {};
    return lookup(code, index >= 2 ? (index - 2) : (index - 1));
  };
};


XModMap parse_xmodmap(std::istream& is)
{
  XModMap res;
  while (is.good())
  {
    std::string line;
    std::getline(is, line);
    if (line.empty())
      continue;
    boost::algorithm::trim(line);
    std::stringstream s(line);
    std::string cmd;
    s >> cmd;
    if (cmd == "keycode")
    {
      int kk;
      s >> kk >> cmd;
      std::vector<std::string> names;
      do
      {
        cmd.clear();
        s >> cmd;
        if (cmd.empty())
          break;
        if (cmd == "Caps_Lock")
        {
          res.caps.insert(kk);
        }
        if (cmd == "Mode_switch")
        {
          res.metas.insert(kk);
        }
        if (cmd == "Shift_L" || cmd == "Shift_R")
          res.shifts.insert(kk);
        names.push_back(cmd);
        res.rev[cmd] = kk;
      } while(true);
      if (names.empty())
        continue;
      if (res.keycodes.size() <= kk)
        res.keycodes.resize(kk+1);
      res.keycodes[kk] = names;
    }
  }
  return res;
}

struct PKey
{
  PKey(bool s, bool m, int lk, bool caps = false)
  : shift(s), mode(m), lkeycode(lk), caps(caps)
  {}
  bool shift;
  bool mode;
  int lkeycode; // kernel code
  bool caps;
};

PKey mapKey(PKey const& in, XModMap const& target, XModMap const& effective)
{
  int xkeycode = linux_to_x(in.lkeycode);
  int idx = (in.shift ? 1 : 0) + (in.mode ? 2 : 0);
  // lookup in target modmap
  std::string keyname = target.lookup(xkeycode, idx);
  if (keyname.empty())
    return PKey{false, false, -1};
  // lookup in effective mapping how to produce it
  auto it = effective.rev.find(keyname);
  bool caps = false;
  if (it == effective.rev.end())
  {
    // try caps lock trick
    keyname[0] = std::tolower(keyname[0]);
    it = effective.rev.find(keyname);
    if (it == effective.rev.end())
      return PKey{false, false, -2};
    caps = true;
  }
  auto const& keys = effective.keycodes[it->second];
  auto kit = std::find(keys.begin(), keys.end(), keyname);
  if (kit == keys.end())
    return PKey{false, false, -3};
  // check if preserving idx produces the same result
  if (keys.size() <= idx || keys[idx] != keyname)
    idx = kit - keys.begin();
  return PKey{idx%2, idx/2 > 0, x_to_linux(it->second), caps};
}

struct State
{
  int meta; // meta lcode or 0 if up
  int shift; // shift lcode or 0 if not down
  int dummyMeta;   // 0: no, -1: forced up, 1: forced down
  int dummyShift;
  int dummyCaps;
  int dummyMetaCode; // code used to emit dummy
  int dummyShiftCode;
  int dummyCapsCode;
  XModMap target, effective;
};

typedef std::function<void(Event)> Emitter;

void step(State& s, Event ev, Emitter emitter)
{
  bool ismod = false;
  if (lmodifiers.find(ev.lcode) != lmodifiers.end())
  {
    ismod = true;
    emitter(ev);
  }
  if (s.target.metas.find(linux_to_x(ev.lcode)) != s.target.metas.end())
  {
    s.meta = (ev.action == Action::up) ? 0 : ev.lcode;
  }
  if (ev.lcode == (int)ModsL::lshift || ev.lcode == (int)ModsL::rshift)
    s.shift = (ev.action == Action::up) ? 0 : ev.lcode;
  if (ismod)
    return;
  PKey res = mapKey(PKey{s.shift, s.meta, ev.lcode}, s.target, s.effective);
  std::cerr << ev.lcode << ',' << !!s.shift << ',' << !!s.meta
   << " => " << res.lkeycode << ',' << res.shift << ',' << res.mode << ',' << res.caps << std::endl;
  if (res.lkeycode > 0)
  {
    if (ev.action == Action::up)
    {
      emitter(Event{ev.action, res.lkeycode});
      if (s.dummyMeta)
      {
        emitter(Event{s.dummyMeta > 0 ? Action::up : Action::down, s.dummyMetaCode});
        s.dummyMeta = 0;
      }
      if (s.dummyShift)
      {
        emitter(Event{s.dummyShift > 0 ? Action::up : Action::down, s.dummyShiftCode});
        s.dummyShift = 0;
      }
      if (s.dummyCaps)
      {
        emitter(Event{Action::up, s.dummyCapsCode});
        s.dummyCaps = 0;
      }
    }
    else if (ev.action == Action::repeat)
    {
      emitter(Event{ev.action, res.lkeycode});
    }
    else
    { // down action
      if (res.caps)
      {
        int kk = x_to_linux(*s.target.caps.begin());
        s.dummyCapsCode = kk;
        s.dummyCaps = 1;
        emitter(Event{Action::down, kk});
      }
      if (res.shift && !s.shift)
      { // dummy shift down
        int kk = x_to_linux(*s.target.shifts.begin());
        s.dummyShift = 1;
        s.dummyShiftCode = kk;
        emitter(Event{Action::down, kk});
      }
      if (!res.shift && s.shift)
      { // dummy shift up
        s.dummyShift = -1;
        s.dummyShiftCode = s.shift;
        emitter(Event{Action::up, s.shift});
      }
      if (res.mode && !s.meta)
      { // dummy meta down
        int kk = x_to_linux(*s.target.metas.begin());
        s.dummyMeta = 1;
        s.dummyMetaCode = kk;
        emitter(Event{Action::down, kk});
      }
      if (!res.mode && s.meta)
      {
        s.dummyMeta = -1;
        s.dummyMetaCode = s.meta;
        emitter(Event{Action::up, s.meta});
      }
      emitter(Event{ev.action, res.lkeycode});
    }
  }
}


void output(input_event ev)
{
  if (ev.type == 1)
    std::cout << "=>" << (int)ev.type << ' ' << (int)ev.code << ' ' << ev.value << std::endl;
}

int main(int argc, char** argv)
{
  int fd = open(argv[1], O_RDONLY);
  char name[256];
  int size = sizeof (struct input_event);
  ioctl (fd, EVIOCGNAME (sizeof (name)), name);
  printf("Reading From : %s\n", name);
  XModMap target, effective;
  {
    std::ifstream ifs(argv[2]);
    target = parse_xmodmap(ifs);
    for (auto m: target.metas)
      std::cerr << "meta " << m << std::endl;
  }
  {
    std::ifstream ifs(argv[3]);
    effective = parse_xmodmap(ifs);
  }
  State s = State{0, 0, 0, 0, 0, 0, 0, 0, target, effective};
  while (true)
  {
    std::cerr << "running" << std::endl;
    struct input_event ev[64];
    int rd;
    if ((rd = read (fd, ev, size * 64)) < size)
    {
      perror("read()");
      exit(1);
    }
    for (int i=0; i*size < rd; ++i)
    {
      if (ev[i].type != (int)Type::keyEvent)
        output(ev[i]);
      else
      {
        std::cerr << "<=" << (int)ev[i].type << ' ' << (int)ev[i].code << ' ' << ev[i].value << std::endl;
        step(s, Event {(Action)ev[i].value, ev[i].code}, [&](Event ev) {
            input_event ie;
            ie.type = (int)Type::keyEvent;
            ie.code = ev.lcode;
            ie.value = (int)ev.action;
            output(ie);
        });
      }
    }
  }
}
