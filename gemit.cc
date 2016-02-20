#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>

int main(int argc, char** argv)
{
  int fd = open(argv[1], O_RDWR);
  for (int i=1; i<256; ++i)
  {
    unsigned char report[8] = {0,0,0,0,0,0,0,0};
    report[2] = i;
    write(fd, report, 8);
    report[2] = 0x2c; // space
    usleep(100000);
  }
}

// z h=44 g=29

29 44 z
40 28 enter    0x28