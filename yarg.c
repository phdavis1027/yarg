#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <locale.h>

#include <mpv/client.h>

#include <stb/stb_ds.h>

#include "waybar_cffi_module.h"

#include "yarg.h"

const size_t wbcffi_version = 2; 

static int instance_count = 0;

static mpv_handle *mpv_ctx = NULL;
static pthread_mutex_t mpv_ctx_mutex = PTHREAD_MUTEX_INITIALIZER;

static char **stations;
static size_t current_station;
static pthread_mutex_t station_mutex = PTHREAD_MUTEX_INITIALIZER;

void onclicked(GtkButton* button) {
  char text[256];
  snprintf(text, 256, "Dice throw result: %d", rand() % 6 + 1);
  gtk_button_set_label(button, text);
}

int initialize_mpv() {
    int rc;
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
}


// Required API functions

void *wbcffi_init(
  const wbcffi_init_info* init_info,
  const wbcffi_config_entry* config_entries,
  size_t config_entries_len
) {
  int rc;

  FINFO("yarg initialized, %d instances\n", ++instance_count);

  Yarg *yarg = malloc(sizeof(yarg));
  if (yarg == NULL) {
    FFATAL("Failed to allocate yarg instance, %d instances\n", instance_count);
    return NULL;
  }

  yarg->waybar_module = init_info->obj;

  // Setup widgets
  GtkContainer *root = init_info->get_root_widget(init_info->obj);

  yarg->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
  gtk_container_add(GTK_CONTAINER(root), GTK_WIDGET(yarg->container));

  GtkLabel *label = GTK_LABEL(gtk_label_new("[Example C FFI Module:"));
  gtk_container_add(GTK_CONTAINER(yarg->container), GTK_WIDGET(label));

  // Add a button
  yarg->button = GTK_BUTTON(gtk_button_new_with_label("click me !"));
  g_signal_connect(yarg->button, "clicked", G_CALLBACK(onclicked), NULL);
  gtk_container_add(GTK_CONTAINER(yarg->container), GTK_WIDGET(yarg->button));


  // Add a label
  label = GTK_LABEL(gtk_label_new("]"));
  gtk_container_add(GTK_CONTAINER(yarg->container), GTK_WIDGET(label));

  setlocale(LC_NUMERIC, "C");

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
