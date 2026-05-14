#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <locale.h>

#include <mpv/client.h>

#include "waybar_cffi_module.h"

#include "station.h"
#include "yarg.h"

const size_t wbcffi_version = 2; 

static int instance_count = 0;

static mpv_handle *mpv_ctx = NULL;
static pthread_mutex_t mpv_ctx_mutex = PTHREAD_MUTEX_INITIALIZER;

static HM_Station stations;
static int current_station = -1;
static pthread_mutex_t station_mutex = PTHREAD_MUTEX_INITIALIZER;

int initialize_mpv() {
    int rc;
    pthread_mutex_lock(&mpv_ctx_mutex);
    setlocale(LC_NUMERIC, "C");
    assert(mpv_ctx == NULL);
    mpv_ctx = mpv_create();
    if (mpv_ctx == NULL) {
      return 1;
    }
    if ((rc = mpv_request_log_messages(mpv_ctx, "debug")) < 0) {
      return rc;
    }
    if ((rc = mpv_initialize(mpv_ctx)) < 0) {
      return rc;
    }
    pthread_mutex_unlock(&mpv_ctx_mutex);
    return 0;
}

static const char STATION_SUBKEY[] = "station";

void load_stations(
  const wbcffi_config_entry* config_entries,
  size_t config_entries_len
) {
  const size_t station_prefix_len = strlen(STATION_SUBKEY);
  for (int i = 0; i < config_entries_len; ++i) {
      const char *key = config_entries[i].key;
      if (strncmp(key, STATION_SUBKEY, station_prefix_len) == 0
          && key[station_prefix_len] == '/') {
          const char *station = key + station_prefix_len + 1;
	  const char *url = config_entries[i].value;
	  hmput(stations, station, url);
      }
  }
}

void setup_menu(Yarg *yarg) {
  GtkContainer *root = init_info->get_root_widget(init_info->obj);

  yarg->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
  gtk_container_add(GTK_CONTAINER(root), GTK_WIDGET(yarg->container));

  yarg->menu = GTK_MENU(gtk_menu_new());
  for (ptrdiff_t i = 0; i < hmlen(yarg->stations); ++i) {
    GtkWidget *item = gtk_menu_item_new_with_label(yarg->stations[i].key);
    gtk_menu_shell_append(GTK_MENU_SHELL(yarg->menu), item);
  }
  gtk_widget_show_all(GTK_WIDGET(yarg->menu));

  yarg->button = GTK_BUTTON(gtk_button_new_with_label("RADIO"));
  gtk_menu_attach_to_widget(yarg->menu, GTK_WIDGET(yarg->button), NULL);
  g_signal_connect_swapped(
    yarg->button,
    "button_press_event",
    G_CALLBACK(popup_menu),
    yarg->menu
  );
  gtk_container_add(GTK_CONTAINER(yarg->container), GTK_WIDGET(yarg->button));
}

static gint popup_menu(GtkWidget *widget, GdkEvent *event) {
  GtkMenu *menu;
  GdkEventButton *event_button;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  // The "widget" is the menu that was supplied when
  // `g_signal_connect_swapped()` was called.
  menu = GTK_MENU (widget);
  if (event->type == GDK_BUTTON_PRESS)
    {
      event_button = (GdkEventButton *) event;
      if (event_button->button == GDK_BUTTON_PRIMARY)
        {
          gtk_menu_popup_at_pointer(menu, event);
          return TRUE;
        }
    }

  return FALSE;
}

// Required API functions

void *wbcffi_init(
  const wbcffi_init_info* init_info,
  const wbcffi_config_entry* config_entries,
  size_t config_entries_len
) {
  int rc;

  load_stations(config_entries, config_entries_len);

  Yarg *yarg = malloc(sizeof(*yarg));
  if (yarg == NULL) {
    return NULL;
  }

  yarg->waybar_module = init_info->obj;
  yarg->stations = stations;
  yarg->current_station = &current_station;
  yarg->station_mutex = &station_mutex;

  setup_menu(yarg);

  if (mpv_ctx == NULL && (rc = initialize_mpv()) != 0) {
    exit(rc);
  }

  return yarg;
}

void wbcffi_deinit(void* instance) {
  printf("cffi_example inst=%p: free memory\n", instance);
  free(instance);
}

void wbcffi_update(void* instance) { printf("cffi_example inst=%p: Update request\n", instance); }

void wbcffi_refresh(void* instance, int signal) {
  printf("cffi_example inst=%p: Received refresh signal %d\n", instance, signal);
}

void wbcffi_doaction(void* instance, const char* name) {
  printf("cffi_example inst=%p: doAction(%s)\n", instance, name);
}
