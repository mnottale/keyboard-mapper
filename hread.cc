#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <iostream>


int main(int argc, char** argv)
{
  int fd = open(argv[1], O_RDWR);
  while (true)
  {
    struct input_event ev[64];
    int rd = read(fd, ev, 64 * sizeof(input_event));
    for (int i=0; i*sizeof(input_event) < rd; ++i)
    {
      if (ev[i].type != 1 || ev[i].value != 1)
        continue;
      struct timeval tv;
      gettimeofday(&tv, 0);
      std::cerr << (tv.tv_usec/1000) << ' ' << (int)ev[i].code << std::endl;
    }
  }
}