#ifndef _YARG_H_
#define _YARG_H_

#define STB_DS_IMPLEMENTATION
#include <pthread.h>

#include <stb_ds.h>
#include <gtk/gtk.h>

#include "util.h"

typedef struct {
  wbcffi_module *waybar_module;

  GtkBox *container;
  GtkMenu *menu;
  GtkButton *button;

  HM_Station stations;
  int *current_station;
  pthread_mutex_t *station_mutex;

  int instance_no;
} Yarg;

#endif
