#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "parser.h"
#include "ev_process.h"

#define NAME_LENGTH 256

static struct option long_opts[] = {
  {"template", required_argument, NULL, 't'},
  {"device", required_argument, NULL, 'd'},
  {"config", required_argument, NULL, 'c'},
  {0, 0, 0, 0}
};

#define DEF2INIT(D) \
          { (D), #D }

typedef struct {
  int ctrl;
  char *desc;
} t_map;

t_map axis_map[] = {
  DEF2INIT(ABS_X),
  DEF2INIT(ABS_Y),
  DEF2INIT(ABS_Z),
  DEF2INIT(ABS_RX),
  DEF2INIT(ABS_RY),
  DEF2INIT(ABS_RZ),
  DEF2INIT(ABS_THROTTLE),
  DEF2INIT(ABS_RUDDER),
  DEF2INIT(ABS_WHEEL),
  DEF2INIT(ABS_GAS),
  DEF2INIT(ABS_BRAKE),
  DEF2INIT(ABS_HAT0X),
  DEF2INIT(ABS_HAT0Y),
  DEF2INIT(ABS_HAT1X),
  DEF2INIT(ABS_HAT1Y),
  DEF2INIT(ABS_HAT2X),
  DEF2INIT(ABS_HAT2Y),
  DEF2INIT(ABS_HAT3X),
  DEF2INIT(ABS_HAT3Y),
  DEF2INIT(ABS_PRESSURE),
  DEF2INIT(ABS_DISTANCE),
  DEF2INIT(ABS_TILT_X),
  DEF2INIT(ABS_TILT_Y),
  DEF2INIT(ABS_TOOL_WIDTH),
  DEF2INIT(ABS_VOLUME),
  DEF2INIT(ABS_MISC),
  {-1, NULL}
};

t_map button_map[] = {
  DEF2INIT(BTN_MISC),
  DEF2INIT(BTN_0),
  DEF2INIT(BTN_1),
  DEF2INIT(BTN_2),
  DEF2INIT(BTN_3),
  DEF2INIT(BTN_4),
  DEF2INIT(BTN_5),
  DEF2INIT(BTN_6),
  DEF2INIT(BTN_7),
  DEF2INIT(BTN_8),
  DEF2INIT(BTN_9),
  DEF2INIT(BTN_MOUSE),
  DEF2INIT(BTN_LEFT),
  DEF2INIT(BTN_RIGHT),
  DEF2INIT(BTN_MIDDLE),
  DEF2INIT(BTN_SIDE),
  DEF2INIT(BTN_EXTRA),
  DEF2INIT(BTN_FORWARD),
  DEF2INIT(BTN_BACK),
  DEF2INIT(BTN_TASK),
  DEF2INIT(BTN_TRIGGER),
  DEF2INIT(BTN_THUMB),
  DEF2INIT(BTN_THUMB2),
  DEF2INIT(BTN_TOP),
  DEF2INIT(BTN_TOP2),
  DEF2INIT(BTN_PINKIE),
  DEF2INIT(BTN_BASE),
  DEF2INIT(BTN_BASE2),
  DEF2INIT(BTN_BASE3),
  DEF2INIT(BTN_BASE4),
  DEF2INIT(BTN_BASE5),
  DEF2INIT(BTN_BASE6),
  DEF2INIT(BTN_DEAD),
  DEF2INIT(BTN_GAMEPAD),
  DEF2INIT(BTN_A),
  DEF2INIT(BTN_B),
  DEF2INIT(BTN_C),
  DEF2INIT(BTN_X),
  DEF2INIT(BTN_Y),
  DEF2INIT(BTN_Z),
  DEF2INIT(BTN_TL),
  DEF2INIT(BTN_TR),
  DEF2INIT(BTN_TL2),
  DEF2INIT(BTN_TR2),
  DEF2INIT(BTN_SELECT),
  DEF2INIT(BTN_START),
  DEF2INIT(BTN_MODE),
  DEF2INIT(BTN_THUMBL),
  DEF2INIT(BTN_THUMBR),
  DEF2INIT(BTN_DIGI),
  DEF2INIT(BTN_TOOL_PEN),
  DEF2INIT(BTN_TOOL_RUBBER),
  DEF2INIT(BTN_TOOL_BRUSH),
  DEF2INIT(BTN_TOOL_PENCIL),
  DEF2INIT(BTN_TOOL_AIRBRUSH),
  DEF2INIT(BTN_TOOL_FINGER),
  DEF2INIT(BTN_TOOL_MOUSE),
  DEF2INIT(BTN_TOOL_LENS),
  DEF2INIT(BTN_TOOL_QUINTTAP),
  DEF2INIT(BTN_TOUCH),
  DEF2INIT(BTN_STYLUS),
  DEF2INIT(BTN_STYLUS2),
  DEF2INIT(BTN_TOOL_DOUBLETAP),
  DEF2INIT(BTN_TOOL_TRIPLETAP),
  DEF2INIT(BTN_TOOL_QUADTAP),
  DEF2INIT(BTN_WHEEL),
  DEF2INIT(BTN_GEAR_DOWN),
  DEF2INIT(BTN_GEAR_UP),
  {-1, NULL}
};

char *find_axis_name(int ctrl)
{
  int i = 0;
  while(axis_map[i].ctrl >= 0){
    if(axis_map[i].ctrl == ctrl){
      return axis_map[i].desc;
    }
    ++i;
  }
  return NULL;
}

char *find_button_name(int ctrl)
{
  int i = 0;
  while(button_map[i].ctrl >= 0){
    if(button_map[i].ctrl == ctrl){
      return button_map[i].desc;
    }
    ++i;
  }
  return NULL;
}


int find_device_by_name(const char *path, const char *devName)
{
  char *nameStart = "event";
  DIR *input = opendir(path);
  if(input == NULL){
    perror("opendir");
    return -1;
  }

  struct dirent *de;
  int fd = -1;
  while((de = readdir(input)) != NULL){
    if(strncmp(nameStart, de->d_name, strlen(nameStart)) != 0){
      continue;
    }
    char *fname = NULL;
    if(asprintf(&fname, "%s/%s", path, de->d_name) < 0){
      continue;
    }

    fd = open(fname, O_RDONLY | O_NONBLOCK);
    if(fd < 0){
      perror("open");
      //printf("Problem opening '%s'.\n", fname);
      free(fname);
      fname = NULL;
      continue;
    }
    //printf("Opened '%s'.\n", fname);
    free(fname);
    fname = NULL;

    char name[NAME_LENGTH];
    if(ioctl(fd, EVIOCGNAME(NAME_LENGTH), name) < 0){
      perror("ioctl EVIOCGNAME");
      return 1;
    }
    if(strncmp(name, devName, strlen(devName)) == 0){
      break;
    }

    close(fd);
    fd = -1;
  }
  closedir(input);
  return fd;
}

int explore_device(int fd, FILE *templ_file)
{
  int i;
  //Initialize the button array (all buttons are marked free)
  for(i = 0; i < BUTTON_ARRAY_LEN; ++i){
    config.real_btn_array[i] = 0;
  }
  //Initialize all axes as ignored
  for(i = 0; i < ABS_MAX; ++i){
    config.axes[i].ignore = true;
  }

  char name[NAME_LENGTH];
  if(ioctl(fd, EVIOCGNAME(NAME_LENGTH), name) < 0){
    perror("ioctl EVIOCGNAME");
    return 1;
  }
  if(templ_file){
    fprintf(templ_file, "//Automaticaly generated section\n");
    fprintf(templ_file, "device \"%s\"\n\n", name);
  }

  if(config.grab){
    int grab = ioctl(fd, EVIOCGRAB, (void*)1);
    if(grab < 0){
      config.grabbed = false;
    }else{
      config.grabbed = true;
    }
  }else{
    config.grabbed = false;
  }

  struct input_id id;
  if(ioctl(fd, EVIOCGID, &id) < 0){
    perror("ioctl EVIOCGID");
    return 1;
  }
  //No need to record the bustype as it will be virtual anyway
  config.vendor = id.vendor;
  config.product = id.product;
  config.version = id.version;

  uint8_t ev_bit[KEY_MAX / 8 + 1];
  uint8_t bit[KEY_MAX / 8 + 1];
  memset(ev_bit, 0, sizeof(ev_bit));
  if(ioctl(fd, EVIOCGBIT(0, EV_MAX), ev_bit) < 0){
    perror("ioctl EVIOCGBIT");
    return 1;
  }

  int ev, code;

  for(ev = 0; ev < EV_MAX; ++ev){
    if(ev_bit[0] & (1 << ev)){
      if(ev == EV_SYN){
        continue;
      }
      uint8_t state[KEY_MAX / 8 + 1];
      if(ev == EV_KEY){
        memset(state, 0, sizeof(state));
        if(ioctl(fd, EVIOCGKEY(KEY_MAX), state) < 0){
          perror("ioctl EVIOCGKEY");
          return 1;
        }
      }
      memset(bit, 0, sizeof(bit));
      if(ioctl(fd, EVIOCGBIT(ev, KEY_MAX), bit) < 0){
        perror("ioctl EVIOCGBIT");
        return 1;
      }
      for(code = 0; code < KEY_MAX; ++code){
        if(bit[code / 8] & (1 << (code % 8))){//event is valid
          if(ev == EV_KEY){
            add_used_button(config.real_btn_array, code, true);
            if(templ_file){
              char *name = find_button_name(code);
              if(name){
                fprintf(templ_file, "  button %s = %d\n", name, code);
              }else{
                printf("Unknown button Id %d.\n", code);
              }
            }
          }
          if(ev == EV_ABS){
            if(templ_file){
              char *name = find_axis_name(code);
              if(name){
                fprintf(templ_file, "  axis %s = %d\n", name, code);
                struct input_absinfo absinfo;
                if(ioctl(fd, EVIOCGABS(code), &absinfo) < 0){
                  perror("ioctl EVIOCGABS");
                  return 1;
                }
                config.axes[code].center = (absinfo.minimum + absinfo.maximum) / 2.0;
                //Hysteresis 1/4 of the full scale
                config.axes[code].hysteresis = (absinfo.maximum - config.axes[code].center) / 2.0;
                config.axes[code].current_state = INACTIVE;
                config.axes[code].ignore = false;
                config.axes[code].minimum = absinfo.minimum;
                config.axes[code].maximum = absinfo.maximum;
                config.axes[code].fuzz = absinfo.fuzz;
                config.axes[code].flat = absinfo.flat;
                config.axes[code].res = absinfo.resolution;
              }else{
                printf("Unknown axis Id %d.\n", code);
              }
            }
          }
        }
      }
    }
  }

  if(templ_file){
    fprintf(templ_file, "\n//Your configuration follows\n");
  }

  close(fd);
  return 0;
}


static int virt_dev = -1;

int send_event(struct input_event *ev)
{
  if(virt_dev < 0){
    printf("NULL virt_dev.\n");
    return -1;
  }
  int res = write(virt_dev, &ev, sizeof(struct input_event));
  if(res < 0){
    printf("Problem sending out an event!\n");
  }
  return res;
}


int main(int argc, char *argv[])
{
  int c;
  int index;
  char *template = NULL;
  char *dev = NULL;
  char *conf_file = NULL;

  while(1){
    c = getopt_long(argc, argv, "t:d:", long_opts, &index);
    if(c < 0){
      break;
    }
    switch(c){
      case 't':
        if(optarg != NULL){
          template = optarg;
        }
        break;
      case 'd':
        if(optarg != NULL){
          dev = optarg;
        }
        break;
      case 'c':
        if(optarg != NULL){
          conf_file = optarg;
        }
        break;
    }
  }

  int fd;

  if(conf_file){
    parse_config(conf_file);
  }

  if(dev){
    fd = open(dev, O_RDWR);
    if(fd < 0){
      perror("open");
      printf("Device '%s' NOT opened.\n", dev);
      return 1;
    }
  }else{
    fd = find_device_by_name("/dev/input", config.device);
    if(fd < 0){
      perror("open");
      printf("Device named '%s' NOT opened.\n", config.device);
      return 1;
    }
  }

  FILE *templ_file = NULL;
  if(template != NULL){
    if((templ_file = fopen(template, "w")) == NULL){
      printf("Can't open template file '%s'.\n", template);
      return 1;
    }
  }

  explore_device(fd, templ_file);

  sort_out_buttons();

  print_config();

#define VIRTUAL_DEVICE
#ifdef VIRTUAL_DEVICE
  int ui = open("/dev/uinput", O_RDWR);
  if(ui < 0){
    perror("open");
    return 1;
  }
  printf("File '%s' opened.\n", argv[2]);

  int res = 0;
  struct uinput_user_dev ud;
  memset(&ud, 0, sizeof(ud));
  ud.id.bustype = BUS_VIRTUAL;
  ud.id.vendor = config.vendor;
  ud.id.product = config.product;
  ud.id.version = config.version;
  snprintf(ud.name, UINPUT_MAX_NAME_SIZE,
                   "evdevshift: %s", config.device);

  res |= (ioctl(ui, UI_SET_EVBIT, EV_SYN) == -1);
  res |= (ioctl(ui, UI_SET_EVBIT, EV_KEY) == -1);
  int i;
  for(i = 0; i < BUTTON_ARRAY_LEN; ++i){
    if(config.virtual_btn_array[i] == -1){
      res |= (ioctl(ui, UI_SET_KEYBIT, i) == -1);
    }
  }
  res |= (ioctl(ui, UI_SET_EVBIT, EV_ABS) == -1);
  for(i = 0; i < ABS_CNT; ++i){
    if(!config.axes[i].ignore){
      res |= (ioctl(ui, UI_SET_ABSBIT, i) == -1);
      ud.absmin[i] = config.axes[i].minimum;
      ud.absmax[i] = config.axes[i].maximum;
      ud.absfuzz[i] = config.axes[i].fuzz;
      ud.absflat[i] = config.axes[i].flat;
    }
  }
  res |= (write(ui, &ud, sizeof(ud)) == -1);

  if(ioctl(ui, UI_DEV_CREATE, 0) < 0){
    perror("ioctl UI_DEV_CREATE");
    return 1;
  }

  printf("New device created.\n");
  struct input_event event[16];
  ssize_t read_in;

  for(i = 0; i < BUTTON_ARRAY_LEN; ++i){
    config.real_btn_array[i] = 0;
    config.virtual_btn_array[i] = 0;
  }
  virt_dev = ui;
  while(1){
    read_in = read(fd, &event, sizeof(event));
    if((read_in < 0) || (read_in % sizeof(struct input_event) != 0)){
      printf("Read wrong number of bytes!\n");
      return 1;
    }
    size_t n;
    for(n = 0; n < read_in / sizeof(struct input_event); ++n){
      process_event(&(event[n]));
    }
  }
#endif

  if(config.grabbed){
    ioctl(fd, EVIOCGRAB, (void*)1);
  }
  clean_up_config();

  return 0;
}

