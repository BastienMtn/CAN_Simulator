#include "ui.h"

gboolean gui_print(gpointer data)
{
    struct print_param *prmtrs = (struct print_param *)data;
    char *msg = prmtrs->msg;
    size_t size = prmtrs->size;
    pthread_mutex_lock(&print_mut);
    GtkTextIter start, end;
    if (gtk_text_buffer_get_line_count(buff) > 30)
    {
        gtk_text_buffer_get_start_iter(buff, &start);
        gtk_text_buffer_get_start_iter(buff, &end);
        gtk_text_iter_forward_to_line_end(&end);
        gtk_text_buffer_delete(buff, &start, &end);
        gtk_text_buffer_get_end_iter(buff, &end);
        gtk_text_buffer_insert(buff, &end, msg, size);
    }
    else
    {
        // Get the end iterator and insert text
        gtk_text_buffer_get_end_iter(buff, &end);
        gtk_text_buffer_insert(buff, &end, msg, size);
    }
    pthread_mutex_unlock(&print_mut);
}

void text_clear(GtkWidget *widget, gpointer data)
{
    pthread_mutex_lock(&print_mut);
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buff, &start);
    gtk_text_buffer_get_end_iter(buff, &end);
    gtk_text_buffer_delete(buff, &start, &end);
    pthread_mutex_unlock(&print_mut);
}

void dos_bttn_cllbck(GtkWidget *widget, gpointer data)
{
    pthread_mutex_lock(&dos_mut);
    dosOn = !dosOn;
    if (dosOn)
    {
        pthread_cond_signal(&dos_cond);
        printf("DOS is On\n");
    }
    else
    {
        printf("DOS is Off\n");
    }
    pthread_mutex_unlock(&dos_mut);
}

void flood_bttn_cllbck(GtkWidget *widget, gpointer data)
{
    pthread_mutex_lock(&flood_mut);
    floodOn = !floodOn;
    if (floodOn)
    {
        pthread_cond_signal(&flood_cond);
        printf("Flooding is On\n");
    }
    else
    {
        printf("Flooding is Off\n");
    }
    pthread_mutex_unlock(&flood_mut);
}

// attack fuzzy
void fuzz_bttn_cllbck(GtkWidget *widget, gpointer data)
{
    pthread_mutex_lock(&fuzz_mut);
    fuzzON = !fuzzON;
    if (fuzzON)
    {
        pthread_cond_signal(&fuzz_cond);
        printf("Fuzzy Attack is On\n");
    }
    else
    {
        printf("Fuzzy Attack is Off\n");
    }
    pthread_mutex_unlock(&fuzz_mut);
}

// attack replay
void replay_bttn_cllbck(GtkWidget *widget, gpointer data)
{
    pthread_mutex_lock(&replay_mut);
    replayOn = !replayOn;
    if (replayOn)
    {
        pthread_cond_signal(&replay_cond);
        printf("Replay Attack is On\n");
    }
    else
    {
        printf("Replay Attack is Off\n");
    }
    pthread_mutex_unlock(&replay_mut);
}

// attack suspend
void suspend_bttn_cllbck(GtkWidget *widget, gpointer data)
{
    pthread_mutex_lock(&suspend_mut);
    suspendOn = !suspendOn;
    if (suspendOn)
    {
        pthread_cond_signal(&suspend_cond);
        printf("Suspend Attack is On\n");
    }
    else
    {
        printf("Suspend Attack is Off\n");
    }
    pthread_mutex_unlock(&suspend_mut);
}

void activate(GtkApplication *app, gpointer user_data)
{
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *button;
    GtkWidget *text_view;
    GtkWidget *scrolled_window;

    /* create a new window, and set its title */
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Window");

    /* Here we construct the container that is going pack our buttons */
    grid = gtk_grid_new();

    /* Pack the container in the window */
    gtk_window_set_child(GTK_WINDOW(window), grid);

    button = gtk_toggle_button_new_with_label("DOS");
    g_signal_connect(button, "clicked", G_CALLBACK(dos_bttn_cllbck), NULL);

    /* Place the first button in the grid cell (0, 0), and make it fill
     * just 1 cell horizontally and vertically (ie no spanning)
     */
    gtk_grid_attach(GTK_GRID(grid), button, 0, 0, 1, 1);

    button = gtk_toggle_button_new_with_label("Flood");
    g_signal_connect(button, "clicked", G_CALLBACK(flood_bttn_cllbck), NULL);
    gtk_grid_attach(GTK_GRID(grid), button, 1, 0, 1, 1);

    // Fuzzy button
    button = gtk_toggle_button_new_with_label("Fuzzy");
    g_signal_connect(button, "clicked", G_CALLBACK(fuzz_bttn_cllbck), NULL);
    gtk_grid_attach(GTK_GRID(grid), button, 2, 0, 1, 1);

    // Replay button
    button = gtk_toggle_button_new_with_label("Replay");
    g_signal_connect(button, "clicked", G_CALLBACK(replay_bttn_cllbck), NULL);
    gtk_grid_attach(GTK_GRID(grid), button, 3, 0, 1, 1);

    // Suspend button
    button = gtk_toggle_button_new_with_label("Suspend");
    g_signal_connect(button, "clicked", G_CALLBACK(suspend_bttn_cllbck), NULL);
    gtk_grid_attach(GTK_GRID(grid), button, 4, 0, 1, 1);

    /* The text buffer represents the text being edited */
    buff = gtk_text_buffer_new(NULL);

    /* Text view is a widget in which can display the text buffer.
     * The line wrapping is set to break lines in between words.
     */
    text_view = gtk_text_view_new_with_buffer(buff);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);

    /* Create the scrolled window. Usually NULL is passed for both parameters so
     * that it creates the horizontal/vertical adjustments automatically. Setting
     * the scrollbar policy to automatic allows the scrollbars to only show up
     * when needed.
     */
    scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scrolled_window), 800);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled_window), 500);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), text_view);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 0, 1, 2, 2);

    button = gtk_button_new_with_label("Clear");
    g_signal_connect(button, "clicked", G_CALLBACK(text_clear), NULL);

    /* Place the Quit button in the grid cell (0, 1), and make it
     * span 2 columns.
     */
    gtk_grid_attach(GTK_GRID(grid), button, 0, 3, 1, 1);

    button = gtk_button_new_with_label("Quit");
    g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_window_destroy), window);

    /* Place the Quit button in the grid cell (0, 1), and make it
     * span 2 columns.
     */
    gtk_grid_attach(GTK_GRID(grid), button, 1, 3, 1, 1);

    gtk_window_present(GTK_WINDOW(window));
}