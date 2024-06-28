#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>

extern GtkTextBuffer *buff;
extern pthread_mutex_t print_mut;
extern pthread_mutex_t dos_mut;
extern pthread_mutex_t flood_mut;
extern pthread_mutex_t fuzz_mut;
extern pthread_mutex_t replay_mut;
extern pthread_mutex_t suspend_mut;
extern pthread_cond_t dos_cond;
extern pthread_cond_t flood_cond;
extern pthread_cond_t fuzz_cond;
extern pthread_cond_t replay_cond;
extern pthread_cond_t suspend_cond;
extern bool dosOn;
extern bool floodOn;
extern bool fuzzON;
extern bool replayOn;
extern bool suspendOn;

struct print_param
{
    char *msg;
    size_t size;
};

gboolean gui_print(gpointer data);
void text_clear(GtkWidget *widget, gpointer data);
void dos_bttn_cllbck(GtkWidget *widget, gpointer data);
void flood_bttn_cllbck(GtkWidget *widget, gpointer data);
void fuzz_bttn_cllbck(GtkWidget *widget, gpointer data);
void replay_bttn_cllbck(GtkWidget *widget, gpointer data);
void suspend_bttn_cllbck(GtkWidget *widget, gpointer data);
void activate(GtkApplication *app, gpointer user_data);

#endif