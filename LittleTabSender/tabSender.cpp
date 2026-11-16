#include <X11/X.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <thread>
#include <X11/Xatom.h>


struct TabletPacket{
  int32_t x;
  int32_t y;
  int32_t pressure;
  int32_t touch;
};

bool keep_running = True;

void UI(){
  Display *d = XOpenDisplay(NULL);
  if(!d) return;

  int s = DefaultScreen(d);
  int width = DisplayWidth(d, s);
  int height = DisplayHeight(d, s);

  Window w = XCreateSimpleWindow(d, RootWindow(d, s), 0, 0, width, height, 0, BlackPixel(d, s), BlackPixel(d, s));

  Atom wm_state = XInternAtom(d, "_NET_WM_STATE", False);
  Atom fullscreen = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False);
  XChangeProperty(d, w, wm_state, fullscreen, 32, PropModeReplace, (unsigned char *)&fullscreen, 1);

  Cursor no_cursor;
  Pixmap blank;
  XColor dummy;
  char data[1] = {0};
  blank = XCreateBitmapFromData(d, w, data, 1, 1);
  no_cursor = XCreatePixmapCursor(d, blank, blank, &dummy, &dummy, 0, 0);
  XDefineCursor(d, w, no_cursor);

  XMapWindow(d, w);
  XFlush(d);

  while (keep_running) {
    while (XPending(d)) {
      XEvent ev;
      XNextEvent(d, &ev);
    
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  }
  XCloseDisplay(d);
}



int main(){
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in windows_addr;
  windows_addr.sin_family = AF_INET;
  windows_addr.sin_port = htons(9000);
  windows_addr.sin_addr.s_addr = inet_addr("192.168.1.10");

  int fd = open("/dev/input/event8", O_RDONLY);
  if (fd < 0) {
    std::cerr << "could not open tablet file" << std::endl;
    return 1;
  }

  std::thread windowThread(UI);

  struct input_event ev;
  TabletPacket packet = {0, 0, 0, 0};

  while (true) {
    read(fd, &ev, sizeof(struct input_event));
    if (ev.type == EV_ABS) {
      if(ev.code == ABS_X) packet.x = ev.value;
      if(ev.code == ABS_Y) packet.y = ev.value;
      
    }

      if(ev.code == ABS_PRESSURE) packet.pressure = ev.value;


    if (ev.type == EV_KEY) {
      if(ev.code == BTN_TOUCH) packet.touch = ev.value;

    }


      if (ev.type == EV_SYN || ev.code == SYN_REPORT) {

        std::cout << "Pen -> X: " << packet.x 
              << " | Y: " << packet.y 
              << " | Pres: " << packet.pressure 
              << " | Touch: " << (packet.touch ? "DOWN" : "UP") 
              << std::endl;


        sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr*)&windows_addr, sizeof(windows_addr));

      }
  }
  close(fd);
  return 0;
}
