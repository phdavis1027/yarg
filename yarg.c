#include <gtk/gtk.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <locale.h>

#include <mpv/client.h>

#include "waybar_cffi_module.h"

#include "station.h"
#include "yarg.h"

const size_t wbcffi_version = 2; 

static int instance_no = 0;

static mpv_handle *mpv_ctx = NULL;
static pthread_mutex_t mpv_ctx_mutex = PTHREAD_MUTEX_INITIALIZER;

static HM_Station stations;
static int current_station = -1;

static pthread_mutex_t station_mutex;
static pthread_once_t station_mutex_once = PTHREAD_ONCE_INIT;

static int station_init_rc;
static void initialize_station_mutex() {
  int rc;
  pthread_mutexattr_t attr;

  if ((rc = pthread_mutexattr_init(&attr)) != 0) {
    station_init_rc = rc;
    return;
  }

  if ((rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK)) != 0) {
    pthread_mutexattr_destroy(&attr);
    station_init_rc = rc;
    return;
  }

  if ((rc = pthread_mutex_init(&station_mutex, &attr)) != 0) {
    pthread_mutexattr_destroy(&attr);
    station_init_rc = rc;
    return;
  }

  if ((rc = pthread_mutexattr_destroy(&attr)) != 0) {
    station_init_rc = rc;
    return;
  }

  station_init_rc = 0;
}

static int lock_station_mutex() {
  int rc;

  if ((rc = pthread_once(&station_mutex_once, initialize_station_mutex)) != 0) {
    return rc;
  }

  if (station_init_rc != 0) {
    return station_init_rc;
  }

  if ((rc = pthread_mutex_lock(&station_mutex)) != 0) {
    return rc;
  }

  return 0;
}

static int unlock_station_mutex() {
  return pthread_mutex_unlock(&station_mutex);
}

int initialize_mpv() {
    int rc;
    pthread_mutex_lock(&mpv_ctx_mutex);
    if (mpv_ctx != NULL) {
      pthread_mutex_unlock(&mpv_ctx_mutex);
      return 0;
    }
    setlocale(LC_NUMERIC, "C");
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
// This function's *only* job is to make sure that the static `stations`
// instance is correct.
void load_stations(
  const Yarg *yarg,
  const wbcffi_config_entry* config_entries,
  size_t config_entries_len
) {
  assert(lock_station_mutex() == 0);
  const size_t station_prefix_len = strlen(STATION_SUBKEY);
  if (stations == NULL) {
    sh_new_strdup(stations);
  }
  for (int i = 0; i < config_entries_len; ++i) {
      const char *key = config_entries[i].key;
      printf("[yarg %d]: loading key %s\n", yarg->instance_no, key);
      if (strncmp(key, STATION_SUBKEY, station_prefix_len) == 0
          && key[station_prefix_len] == '/') {

          const char *station = key + station_prefix_len + 1;
	  printf("[yarg %d]: Loading station %s\n", yarg->instance_no, station);
	  const char *url = config_entries[i].value;
	  int station_idx = shgeti(stations, station);
	  char *url_copy = strdup(url);
	  assert(url_copy != NULL);

	  if (station_idx >= 0) {
	    free(stations[station_idx].value);
	  }
	  shput(stations, station, url_copy);
      }
  }
  assert(unlock_station_mutex() == 0);
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

// NOTE: This function assumes you hold the 
void set_station(const int station_idx) {
}

// Returns the last index of the substring `out` of `in`
// such that trailing whitespace has been trimmed
// from `out`
int trim_trailing_whitespace(const char *in) {
    char *out = in;
    int len = strlen(in);
    assert(len > 0);
    while (len > 0 && in[len - 1] == '\n') --len;
    return len;
}

char *unwrap_quotes(const char * in) {
    char *out = malloc(strlen(in) + 1);
    strncpy(out, in + 1, strlen(in) - 2);
    return out;
}

static gint play_station(GtkWidget *widget, const char *station_key) {
  assert(lock_station_mutex() == 0);

  int station_idx = shgeti(stations, station_key);
  if (station_idx < 0 || !GTK_IS_MENU_ITEM(widget)) {
    assert(unlock_station_mutex() == 0);
    return FALSE;
  }

  const Station station = stations[station_idx];
  if (station.key == NULL || station.value == NULL) {
    assert(unlock_station_mutex() == 0);
    return FALSE;
  }

  const char url[128];
  // TODO: Parse, don't validate
  int trimmed = trim_trailing_whitespace(station.value);
  int written = snprintf(url, trimmed + 1, "%s", station.value);
  const char *unwrapped = unwrap_quotes(url);

  const char *command[] = {"loadfile", unwrapped, NULL}; 

  const char *stop[] = {"stop",  NULL};

  if (current_station == station_idx) {
    mpv_command(mpv_ctx, (const char**) stop);
    current_station = -1;
  } else {
    if (current_station > 0) {
	mpv_command(mpv_ctx, (const char**) stop);
    }
    mpv_command(mpv_ctx, (const char**) command);
    current_station = station_idx;
  }

  free(unwrapped);

  
  // TODO: Tighten this critical section, if possible
  assert(unlock_station_mutex() == 0);

  return TRUE;
}

void setup_menu(Yarg *yarg, const wbcffi_init_info* init_info) {
  // TODO: tighten this critical section
  printf("[yarg %d] setup_menu, locking station\n", yarg->instance_no);
  assert(lock_station_mutex() == 0);
  GtkContainer *root = init_info->get_root_widget(init_info->obj);

  yarg->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
  gtk_container_add(GTK_CONTAINER(root), GTK_WIDGET(yarg->container));

  yarg->menu = GTK_MENU(gtk_menu_new());
  // Build menu items from `stations`
  int station_len = shlen(yarg->stations);
  printf("[yarg: %d] found HM_Station with len %d\n", yarg->instance_no, station_len);
  for (int i = 0; i < station_len; i++) {
    GtkWidget *item = gtk_menu_item_new_with_label(yarg->stations[i].key);
    // Attach them to the parent menu
    gtk_menu_shell_append(GTK_MENU_SHELL(yarg->menu), item);
    // Wire them up to the "activate" signal,
    // which is apparently what happens when the user
    // clicks the menu item.
    g_signal_connect(
      item,
      "activate",
      G_CALLBACK(play_station),
      yarg->stations[i].key
    );
    printf("[yarg %d]: wired callback to station %s\n", yarg->instance_no, yarg->stations[i].key);
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
  printf("[yarg: %d] menu setup complete, unlocking station\n", yarg->instance_no);
  assert(unlock_station_mutex() == 0);
}

// Required API functions

void *wbcffi_init(
  const wbcffi_init_info* init_info,
  const wbcffi_config_entry* config_entries,
  size_t config_entries_len
) {
  int rc;

  Yarg *yarg = malloc(sizeof(*yarg));
  yarg->instance_no = instance_no++; 
  
  if (yarg == NULL) {
    return NULL;
  }

  load_stations(yarg, config_entries, config_entries_len);

  yarg->waybar_module = init_info->obj;
  yarg->stations = stations;
  yarg->current_station = &current_station;
  yarg->station_mutex = &station_mutex;

  setup_menu(yarg, init_info);

  if ((rc = initialize_mpv()) != 0) {
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
