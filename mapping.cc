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
#include <signal.h>

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

std::string readBuffer;
bool enter_shell = false;
int block_output = 0;

static const char CTRL_C = 4;
static const char CTRL_D = 5;
static const char XA_LEFT = 128;
static const char XA_RIGHT = 129;
static const char XA_UP = 130;
static const char XA_DOWN = 131;
static const char XA_BACKSPACE = 8;

int linux_to_x(int k)
{
  return k + 8;
}
int x_to_linux(int k)
{
  return k - 8;
}

// usb gadget has its own mapping.
// letters: 'a' = 4, b=5..
// numbers: 1 = 0x1e ... 0 = 0x27
int lg_alphabet[] = { KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H,
  KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P,
KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};

std::map<int, int> ltog = {
  {KEY_ENTER, 0x28}, {KEY_ESC, 0x29}, {KEY_BACKSPACE, 0x2a}, {KEY_TAB, 0x2b},
  {KEY_SPACE, 0x2c}, { KEY_CAPSLOCK, 0x39},
  {KEY_F11, 0x44}, {KEY_F12, 0x45}, {KEY_INSERT, 0x49}, {KEY_HOME, 0x4a},
  {KEY_PAGEUP, 0x4b}, {KEY_DELETE, 0x4c}, { KEY_END, 0x4d}, { KEY_PAGEDOWN, 0x4e},
  {KEY_RIGHT, 0x4f}, {KEY_LEFT, 0x50}, {KEY_DOWN, 0x51}, {KEY_KPENTER, 0x58},
  {KEY_UP, 0x52},  {KEY_NUMLOCK, 0x53},

  {KEY_MINUS, 45}, {KEY_EQUAL, 46}, {KEY_LEFTBRACE, 47}, {KEY_RIGHTBRACE, 48},
  {KEY_BACKSLASH, 49}, {KEY_SEMICOLON, 51}, {KEY_APOSTROPHE, 52},
  {KEY_GRAVE, 53}, {KEY_COMMA, 54}, {KEY_DOT, 55}, {KEY_SLASH, 56},
  {KEY_SYSRQ, 70}, {KEY_SCROLLLOCK, 71}, {KEY_PAUSE, 72},
  {KEY_INSERT, 73}, {KEY_HOME, 74}, {KEY_END, 77},
  {KEY_KPSLASH, 84}, { KEY_KPASTERISK, 85}, {KEY_KPMINUS, 86},
  {KEY_KPPLUS, 87}, { KEY_KP1, 89}, {KEY_KP2, 90}, {KEY_KP3, 91},
  {KEY_KP4, 92}, { KEY_KP5, 93}, {KEY_KP6, 94}, {KEY_KP7, 95},
  {KEY_KP8, 96}, { KEY_KP9, 97}, {KEY_KP0, 98},
  {KEY_KPDOT, 99}, {KEY_102ND, 100}, {KEY_COMPOSE, 101},
  
  {KEY_LEFTCTRL, -0x01}, {KEY_RIGHTCTRL, -0x10},
  {KEY_LEFTSHIFT, -0x02}, { KEY_RIGHTSHIFT, -0x20},
  {KEY_LEFTALT, -0x04}, {KEY_RIGHTALT, -0x40},
  {KEY_LEFTMETA, -0x08}, {KEY_RIGHTMETA, -0x80},
};
static const int UNKNOWN_LKEY = 0x10000;
int linux_to_g(int i)
{
  auto letter = std::find(lg_alphabet, lg_alphabet+26, i);
  if (letter != lg_alphabet + 26)
    return 4 + (letter - lg_alphabet);
  if (i >= KEY_1 && i <= KEY_0)
    return i - KEY_1 + 0x1e;
  if (i >= KEY_F1 && i <= KEY_F10)
    return i - KEY_F1 + 0x3a;
  auto it = ltog.find(i);
  if (it != ltog.end())
    return it->second;
  return UNKNOWN_LKEY;
}

const char* ascii_to_xname[132] = {
"","","","","","","","","BackSpace","Tab",
"Return","","","","","","","","","",
"","","","","","","","","","",
"","","space", "exclam","quotedbl", "numbersign", "dollar", "percent", "ampersand", "apostrophe",
"parenleft", "parenright", "asterisk", "plus", "comma", "minus", "period", "slash", """", "1",
"2","3","4","5","6","7","8","9", "colon", "semicolon",
"less", "equal", "greater","question","at","A","B","C","D","E",
"F","G","H","I","J","K","L","M","N","O",
"P","Q","R","S","T","U","V","W","X","Y",
"Z", "bracketleft","backslash","bracketright", "dead_circunflex","underscore","backquote","a","b","c",
"d","e","f","g","h","i","j","k","l","m",
"n","o","p","q","r","s","t","u","v","w",
"x","y","z","braceleft","bar","braceright","asciitilde","",
// Extra stuff
"Left", "Right", "Up", "Down",
};

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
  std::set<int> controls;
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
        if (cmd == "Control_L" || cmd == "Control_R")
          res.controls.insert(kk);
        names.push_back(cmd);
        if (res.rev.find(cmd) == res.rev.end())
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
  PKey(bool s, bool m, int lk, bool caps = false, bool ctrl=false)
  : shift(s), mode(m), lkeycode(lk), caps(caps), ctrl(ctrl)
  {}
  bool shift;
  bool mode;
  int lkeycode; // kernel code
  bool caps;
  bool ctrl;
};

PKey mapKey(PKey const& in, XModMap const& target, XModMap const& effective, bool down)
{
  int xkeycode = linux_to_x(in.lkeycode);
  int idx = (in.shift ? 1 : 0) + (in.mode ? 2 : 0);
  // lookup in target modmap
  std::string keyname = target.lookup(xkeycode, idx);
  if (keyname.empty())
    return PKey{false, false, -1};
  // store into ascii buffer
  if (down)
  {
    auto ahit = std::find(ascii_to_xname, ascii_to_xname + 132, keyname);
    if (ahit != ascii_to_xname + 132)
    {
      char c = ahit - ascii_to_xname;
      std::cerr << "inserting ascii " << (int)c << " " << in.ctrl << std::endl;
      if (c == 'c' && in.ctrl)
      {
        readBuffer += CTRL_C;
      }
      else
        readBuffer += c;
    }
    else
      std::cerr << "could not askii-map " << keyname << std::endl;
  }
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
  int ctrl;
  int dummyMeta;   // 0: no, -1: forced up, 1: forced down
  int dummyShift;
  int dummyCaps;
  int dummyMetaCode; // code used to emit dummy
  int dummyShiftCode;
  int dummyCapsCode;
  XModMap target, effective;
};

typedef std::function<void(Event)> Emitter;

void step2(State& s, Event ev, Emitter emitter, PKey res);


void writeString(State& s, Emitter emitter, std::string const& str)
{
  for (unsigned char c: str)
  {
    if (c > 132)
    {
      std::cerr << "char out of range" << std::endl;
      continue;
    }
    if (strlen(ascii_to_xname[c]) == 0)
    {
      std::cerr << "unknown char " << (int)c << std::endl;
      continue;
    }
    std::string xname = ascii_to_xname[c];
    auto it = s.effective.rev.find(xname);
    if (it == s.effective.rev.end())
    {
      std::cerr << "no way to produce " << xname << std::endl;
      continue;
    }
    auto const& keys = s.effective.keycodes[it->second];
    auto kit = std::find(keys.begin(), keys.end(), xname);
    if (kit == keys.end())
    {
      std::cerr << "inconsistency 3" << std::endl;
      continue;
    }
    int idx = kit - keys.begin();
    PKey pk{idx%2, idx/2 > 0, x_to_linux(it->second)};
    step2(s, Event{Action::down, pk.lkeycode}, emitter, pk);
    step2(s, Event{Action::up, pk.lkeycode}, emitter, pk);
  }
}


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
  if (s.target.controls.find(linux_to_x(ev.lcode)) != s.target.controls.end())
  {
    s.ctrl = (ev.action == Action::up) ? 0 : ev.lcode;
  }
  if (ev.lcode == (int)ModsL::lshift || ev.lcode == (int)ModsL::rshift)
    s.shift = (ev.action == Action::up) ? 0 : ev.lcode;
  std::cerr << "mod state: " << s.meta << ' ' << s.shift << ' ' << s.ctrl << ' ' << std::endl;
  //if (ev.lcode == (int)ModsL::rctrl || ev.lcode == (int)ModsL::lctrl)
  //  s.ctrl = (ev.action == Action::up) ? 0 : ev.lcode;
  if (ismod)
    return;
  PKey res = mapKey(PKey{s.shift, s.meta, ev.lcode, false, s.ctrl}, s.target, s.effective,
                    ev.action == Action::down);
  std::cerr << ev.lcode << ',' << !!s.shift << ',' << !!s.meta
   << " => " << res.lkeycode << ',' << res.shift << ',' << res.mode << ',' << res.caps << std::endl;
  if (!block_output)
    step2(s, ev, emitter, res);
}
void step2(State& s, Event ev, Emitter emitter, PKey res)
{
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

class GHID
{
public:
  GHID(std::string const& dev);
  void output(input_event ev);
  unsigned char mods; // modifiers which are down
  std::set<int> keys; // keys which are down
  int fd;
};

GHID::GHID(std::string const& dev)
: mods(0)
{
  fd = open(dev.c_str(), O_RDWR);
  if (fd == -1)
    throw std::runtime_error("open ghid device failed");
}

void GHID::output(input_event ev)
{
  if (ev.type != (int) Type::keyEvent)
    return;
  Action a = (Action)ev.value;
  int key = ev.code;
  int gkey = linux_to_g(key);
  if (gkey == UNKNOWN_LKEY)
  {
    std::cerr << "unknown lkey " << key << std::endl;
    return;
  }
  // update state
  if (gkey < 0)
  {
    int gmod = -gkey;
    if (a == Action::up)
      mods &= ~gmod;
    else if (a == Action::down)
      mods |= gmod;
  }
  else
  {
    if (a == Action::up)
      keys.erase(gkey);
    else if (a == Action::down)
      keys.insert(gkey);
  }
  // generate report
  unsigned char report[8] = {0,0,0,0,0,0,0,0};
  report[0] = mods;
  int p = 2;
  for (auto const& g: keys)
  {
    if (p < 8)
      report[p++] = g;
  }
  int res = write(fd, report, 8);
  if (res != 8)
  {
    std::cerr << "report write failed" << std::endl;
  }
}

void output(input_event ev)
{
  if (ev.type == 1)
    std::cout << "=>" << (int)ev.type << ' ' << (int)ev.code << ' ' << ev.value << std::endl;
}
class Special
{
public:
  Special()
  {
    ++block_output;
  }
  virtual ~Special() {
    --block_output;
  }
  // true = finished
  virtual bool step(std::string& readBuffer, State&s, Emitter& emitter) = 0;
};

class Password: public Special
{
public:
  Password(std::function<void(std::string)> res)
  : rbPos(0)
  , started(false)
  , onResult(res)
  {}
  bool step(std::string& readBuffer, State&s, Emitter& emitter)
  {
    if (!started)
    {
      writeString(s, emitter, "password: ");
      started = true;
    }
    for (int i = rbPos; i< readBuffer.size(); ++i)
    {
      unsigned char c = readBuffer[i];
      if (c == XA_BACKSPACE)
      {
        if (i == 0)
        {
          readBuffer = readBuffer.substr(1);
          --i;
        }
        else
        {
          readBuffer = readBuffer.substr(0, i-1) + readBuffer.substr(i+1);
          i -= 2;
          writeString(s, emitter, {XA_BACKSPACE});
        }
      }
      else if (c == '\n')
      {
        onResult(readBuffer.substr(0, i));
        readBuffer.clear();
        return true;
      }
      else
      {
        writeString(s, emitter, "*");
      }
    }
    rbPos = readBuffer.size();
    return false;
  }
  int rbPos;
  bool started;
  std::string password;
  std::function<void(std::string)> onResult;
};

class Menu: public Special
{
public:
  Menu(std::vector<std::string> const& entries, std::function<void(int)> onRes)
  : displayed(false)
  , selection(0)
  , entries(entries)
  , onResult(onRes)
  {}
  // return -1 or selected entry
  bool step(std::string& readBuffer, State&s, Emitter& emitter)
  {
    std::string txt;
    if (!displayed)
    {
      displayed = true;
      txt = "\n";
      for (auto const& e: entries)
        txt += " " + e + "\n";
      txt += std::string(entries.size(), XA_UP);
      txt += XA_RIGHT;
      txt += XA_BACKSPACE;
      txt += '*';
      txt += XA_LEFT;
    }
    int res = false;
    for (auto c: readBuffer)
    {
      if (c == XA_UP && selection > 0)
      {
        txt += std::string(1, XA_RIGHT) + XA_BACKSPACE + ' ' + XA_UP + XA_BACKSPACE + '*' + XA_LEFT;
        selection--;
      }
      if (c == XA_DOWN && selection < entries.size()-1)
      {
        txt += std::string(1, XA_RIGHT) + XA_BACKSPACE + ' ' + XA_DOWN + XA_BACKSPACE + '*' + XA_LEFT;
        selection++;
      }
      if (c == '\n')
      {
        res = true;
        onResult(selection);
      }
    }
    readBuffer.clear();
    if (!txt.empty())
      writeString(s, emitter, txt);
    return res;
  }
  bool displayed;
  int selection;
  std::vector<std::string> entries;
  std::function<void(int)> onResult;
};

static std::shared_ptr<Special> liveSpecial;

void processCommands(State& s, Emitter emitter, std::string& readBuffer)
{
  if (liveSpecial)
  {
    auto ptr = liveSpecial.get();
    int res = liveSpecial->step(readBuffer, s, emitter);
    if (res)
    {
      std::cerr << "menu selected " << res << std::endl;
      if (liveSpecial.get() == ptr)
        liveSpecial.reset();
    }
    return;
  }
  std::cerr << "rb: " << readBuffer << std::endl;
  if (readBuffer.find("hkrshell\n") != readBuffer.npos)
  {
    writeString(s, emitter, "Entering shell mode...\n");
    readBuffer.clear();
    enter_shell = true;
  }
  if (readBuffer.find("hkrping\n") != readBuffer.npos)
  {
    writeString(s, emitter, "Hardware keyboard remapping is live!\n");
    readBuffer.clear();
  }
  if (readBuffer.find("hkrmenu\n") != readBuffer.npos)
  {
    readBuffer.clear();
    liveSpecial.reset(new Menu({"canard", "coin", "option 3"},
      [pstate=&s, emitter](int idx) {
        std::cerr << "index " << idx << " selected" << std::endl;
      }
      ));
  }
  if (readBuffer.find("hkrprompt\n") != readBuffer.npos)
  {
    readBuffer.clear();
    liveSpecial.reset(new Password([&](std::string p) {
        std::cerr << "password: '" << p << "'" << std::endl;
    }));
  }
  if (readBuffer.size() > 10)
    readBuffer = readBuffer.substr(readBuffer.size()-10);
}

int shell_pid = 0;
void shell_start(int res[2])
{
  int shellin[2];
  int shellout[2];
  // read, wrtie
  pipe(shellin);
  pipe(shellout);
  int pid = fork();
  if (!pid)
  {
    dup2(shellin[0], 0);
    dup2(shellout[1], 1);
    dup2(shellout[1], 2);
    close(shellin[1]);
    close(shellout[0]);
    execl("/bin/sh", "sh", 0);
    std::cerr << "something went wrong spamming shell" << std::endl;
    exit(0);
  }
  close(shellout[1]);
  close(shellin[0]);
  shell_pid = pid;
  res[0] = shellout[0];
  res[1] = shellin[1];
}

int do_select(int fd1, int fd2)
{
  fd_set rfds;
  struct timeval tv;
  int retval;
  FD_ZERO(&rfds);
  FD_SET(fd1, &rfds);
  FD_SET(fd2, &rfds);
  retval = select(std::max(fd1, fd2)+1, &rfds, 0,0,0);
  int res = 0;
  if (FD_ISSET(fd1, &rfds))
    res |= 1;
  if (FD_ISSET(fd2, &rfds))
    res |= 2;
  return res;
}

void shell_loop(int kbd_fd, GHID& gadget, State& s)
{
  Emitter emitter = [&](Event ev) {
    input_event ie;
    ie.type = (int)Type::keyEvent;
    ie.code = ev.lcode;
    ie.value = (int)ev.action;
    gadget.output(ie);
  };
  int shell_fds[2];
  shell_start(shell_fds);
  while (true)
  {
    std::cerr << "selecting" << std::endl;
    int sel = do_select(kbd_fd, shell_fds[0]);
    std::cerr << "select " << sel << std::endl;
    if (sel & 1)
    {
      struct input_event ev[64];
      int rd;
      if ((rd = read (kbd_fd, ev, sizeof(input_event) * 64)) < sizeof(input_event))
      {
        perror("read()");
        exit(1);
      }
      
      for (int i=0; i*sizeof(input_event) < rd; ++i)
      {
        if (ev[i].type != (int)Type::keyEvent)
          continue;
        step(s, Event {(Action)ev[i].value, ev[i].code}, [&](Event){});
        while (!readBuffer.empty())
        {
          char c = readBuffer[0];
          std::cerr << "to shell: " << (int)c << std::endl;
          if (c == CTRL_C)
          {
            std::cerr << "interrupting shell" << std::endl;
            system((std::string("egrep -H 'PPid:\\s")
              + std::to_string(shell_pid)
              + "' /proc/*/status |cut -d/ -f 3 |xargs kill 2").c_str());
          }
          else
            write(shell_fds[1], &c, 1);
          readBuffer = readBuffer.substr(1);
        }
      }
    }
    if (sel & 2)
    {
      char data[1024];
      int len = read(shell_fds[0], data, 1024);
      if (len <= 0)
      {
        std::cerr << "shell read error, terminating" << std::endl;
        return;
      }
      std::cerr << "from shell: " << std::string(data, len) << std::endl;
      writeString(s, emitter, std::string(data, len));
    }
  }
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
  GHID gadget(argv[4]);
  State s = State{0, 0, 0, 0, 0, 0, 0, 0, 0, target, effective};
  Emitter emitter = [&](Event ev) {
    input_event ie;
    ie.type = (int)Type::keyEvent;
    ie.code = ev.lcode;
    ie.value = (int)ev.action;
    gadget.output(ie);
  };
  while (true)
  {
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
        gadget.output(ev[i]);
      else
      {
        std::cerr << "<=" << (int)ev[i].type << ' ' << (int)ev[i].code << ' ' << ev[i].value << std::endl;
        step(s, Event {(Action)ev[i].value, ev[i].code}, emitter);
      }
      processCommands(s, emitter, readBuffer);
      if (enter_shell)
      {
        enter_shell = false;
        shell_loop(fd, gadget, s);
      }
    }
  }
}
