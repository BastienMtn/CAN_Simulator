#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <math.h>

#include "config.h"

#include "opel_omega_2001.h"

#include "LinuxCAN_API.h"

// TODO - Add all other nodes
// TODO - Add attack scenarios

struct print_param{
    char *msg;
    size_t size;
};

pthread_mutex_t write_mut, print_mut;
pthread_mutex_t m, dos_mut, flood_mut, fuzz_mut, replay_mut, suspend_mut;
pthread_cond_t c, dos_cond, flood_cond, fuzz_cond, replay_cond, suspend_cond;
bool dosOn = false, floodOn = false, fuzzON = false, replayOn = false, suspendOn = false;

//Global variables for update ECU...
static float acc_pedal = 0;
static int acc_duration = 1;
static int acc_count= 0;

GtkTextBuffer *buff;
CAN_MSG replay_msg; // Variable globale pour stocker dernière trame envoyée utile l'attaque replay

typedef enum
{
    Fast,
    Slow,
    OutOfRange
} tps_attack_mode;

gboolean gui_print(gpointer data){
    struct print_param* prmtrs = (struct print_param*)data;
    char * msg = prmtrs->msg;
    size_t size = prmtrs->size; 
    pthread_mutex_lock(&print_mut);
    GtkTextIter start, end;
    if(gtk_text_buffer_get_line_count(buff)>30){
        gtk_text_buffer_get_start_iter(buff, &start);
        gtk_text_buffer_get_start_iter(buff, &end);
        gtk_text_iter_forward_to_line_end(&end);
        gtk_text_buffer_delete(buff,&start, &end);
        gtk_text_buffer_get_end_iter(buff, &end);
        gtk_text_buffer_insert(buff, &end, msg, size);
    }else{
        // Get the end iterator and insert text
        gtk_text_buffer_get_end_iter(buff, &end);
        gtk_text_buffer_insert(buff, &end, msg, size);
    }
    pthread_mutex_unlock(&print_mut);
}

static void text_clear(GtkWidget *widget, gpointer data){
    pthread_mutex_lock(&print_mut);
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buff, &start);
    gtk_text_buffer_get_end_iter(buff, &end);
    gtk_text_buffer_delete(buff,&start, &end);
    pthread_mutex_unlock(&print_mut);
}

static void dos_bttn_cllbck(GtkWidget *widget, gpointer data)
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

static void flood_bttn_cllbck(GtkWidget *widget, gpointer data)
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

//attack fuzzy 
static void fuzz_bttn_cllbck(GtkWidget *widget, gpointer data)
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

//attack replay 
static void replay_bttn_cllbck(GtkWidget *widget, gpointer data)
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

//attack suspend 
static void suspend_bttn_cllbck(GtkWidget *widget, gpointer data)
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

static void activate(GtkApplication *app, gpointer user_data)
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

TCAN_HANDLE can_init()
{
    TCAN_HANDLE handle;
    TCAN_STATUS status;

    CHAR *comPort = "/dev/ttyUSB0";
    CHAR *szBitrate = "250";
    CHAR *acceptance_code = "1FFFFFFF";
    CHAR *acceptance_mask = "00000000";
    VOID *flags = CAN_TIMESTAMP_OFF;
    DWORD mode = OPERATION_MODE;
    char version[10];

    handle = -1;
    status = 0;

    handle = CAN_Open(comPort, szBitrate, acceptance_code, acceptance_mask, flags, mode);
    if (handle < 0)
        return -1;
    memset(version, 0, sizeof(char) * 10);
    status = CAN_Flush(handle);
    status = CAN_Version(handle, version);
    if (status == CAN_ERR_OK)
    {
        printf("Opened CAN Channel with handle= %d and version : %s \n", handle, version);
        char text[512];
        snprintf(text, sizeof(text), "Opened CAN Channel with handle= %d and version : %s \n", handle, version);
        struct print_param prmtrs;
        prmtrs.msg = text;
        prmtrs.size = strlen(text);
        gpointer p = &prmtrs;
        g_idle_add(gui_print, p);
    }

    return handle;
}

void *receive_routine(void *args)
{
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG recvMSG;
    TCAN_STATUS status;
    char text[512];
    while (1)
    {
        status = CAN_Read(handle, &recvMSG);
        if (status == CAN_ERR_OK)
        {
            snprintf(text, sizeof(text),"Read ID=0x%lx, Type=%s, DLC=%d, FrameType=%s, Data=",
                   recvMSG.Id, (recvMSG.Flags & CAN_FLAGS_STANDARD) ? "STD" : "EXT",
                   recvMSG.Size, (recvMSG.Flags & CAN_FLAGS_REMOTE) ? "REMOTE" : "DATA");
            // Find the length of the destination string
            int destLen = strlen(text);
            for (int i = 0; i < recvMSG.Size; i++)
            {
                if(i==(recvMSG.Size-1)){
                    snprintf(text+destLen*sizeof(char),10*sizeof(char),"%X \n", recvMSG.Data[i]);
                }else{
                    snprintf(text+destLen*sizeof(char),10*sizeof(char),"%X,", recvMSG.Data[i]);
                }                
                destLen = strlen(text);
            }
            //snprintf(text+destLen*sizeof(char),10,"\n");
            struct print_param prmtrs;
            prmtrs.msg = text;
            prmtrs.size = strlen(text);
            gpointer p = &prmtrs;

            g_idle_add(gui_print, p);
            //gui_print(text, strlen(text));
            //printf("\n");
            if (recvMSG.Id == 0x180)
            {
                struct opel_omega_2001_sas_data_t sas_msg;
                opel_omega_2001_sas_data_unpack(&sas_msg, recvMSG.Data, recvMSG.Size);
                //printf("Message is from SAS, Angle = %f and Speed = %f \n", opel_omega_2001_sas_data_steering_angle_decode(sas_msg.steering_angle), opel_omega_2001_sas_data_steering_speed_decode(sas_msg.steering_speed));
            }
            // break;
        }
    }
    return NULL;
}

void sas_data_update(double *angle, double *speed)
{
    static bool isTurning = false;
    static float turn_angle = 0;
    static int length = 1;
    static int count = 0;
    static float x = 0;
    if (isTurning == true)
    {
        if (count == length + 1)
        {
            isTurning = false;
            *angle = *speed = 0;
        }
        else
        {
            x = -1 + 2 * count / ((float)length + 1);
            *angle = (1 - pow(x, 2)) * turn_angle;
            *speed = (1 - pow(x, 2)) * SAS_SPEED_MAX;
            count++;
        }
    }
    else if (rand() < RAND_MAX / 4)
    {
        isTurning = true;
        // pick an angle
        turn_angle = -SAS_TURN_MAX + (float)rand() / ((float)RAND_MAX / (2 * SAS_TURN_MAX));
        // pick a duration
        length = (rand() % 5) + 1;
        printf("Turn angle = %f / Length = %d \n", turn_angle, length);
        count = 1;
        // prepare evolution of variables
        if (length == 1)
            *angle = turn_angle;
        else
        {
            x = -1 + 2 / ((float)length + 1);
            *angle = (1 - pow(x, 2)) * turn_angle;
            *speed = (1 - pow(x, 2)) * SAS_SPEED_MAX;
            count++;
        }
    }
}

void *sas_data_send_routine(void *args)
{
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG msg;
    struct timespec ts;

    msg.Flags = CAN_FLAGS_STANDARD;
    msg.Id = 0x180;
    msg.Size = 8;
    double angle = 0;
    double speed = 0;

    struct opel_omega_2001_sas_data_t msg_p;

    while (1) 
    {
        pthread_mutex_lock(&suspend_mut); // Lock pour venir lire l'état de variable
        while(suspendOn) // On check si attack suspend = 1
        {
            pthread_mutex_unlock(&suspend_mut); // Unlock pour laisser le bouton changer la variable pour stopper l'attack par ex
            pthread_mutex_lock(&suspend_mut); // On lock a nouveau pour lire l'état de la variable 
        }
        pthread_mutex_unlock(&suspend_mut); // Unlock une fois la variable lue

        opel_omega_2001_sas_data_init(&msg_p);
        msg_p.steering_angle = opel_omega_2001_sas_data_steering_angle_encode(angle);
        msg_p.steering_speed = opel_omega_2001_sas_data_steering_speed_encode(speed);
        opel_omega_2001_sas_data_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        
        //Stock chaque dernière trame recue pour utilisation dans attack replay
        pthread_mutex_lock(&write_mut);
        replay_msg = msg;
        pthread_mutex_unlock(&write_mut);

        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)SAS_DATA_PERIOD;
        ts.tv_nsec += (SAS_DATA_PERIOD - (int)SAS_DATA_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        //sleep(SAS_DATA_PERIOD);
        sas_data_update(&angle, &speed);
    }

    return NULL;
}

void ecu_data1_update(double *rpm, double *app, double *torque_req, double *torque_resp, double *torque_lost)
{
    static bool isMoving = false;
    static float x = 0;
    static float app_max = 0;
    if (isMoving == true)
    {
        if (acc_count == acc_duration + 1)
        {
            isMoving = false;
            *rpm = *app = *torque_req = *torque_lost = *torque_resp = 0;
        }
        else
        {
            x = -1 + 2 * acc_count / ((float)acc_duration + 1);
            *app = (1 - pow(x, 2)) * app_max;
            *rpm = (*app / 100) * ECU1_MAX_REAL_RPM * (0.8f + (float)rand() / ((float)RAND_MAX / (2 * 0.2f)));
            *torque_req = (*app / 100) * ECU1_MAX_TORQUE_REQ;
            *torque_resp = *torque_req * (0.8f + (float)rand() / ((float)RAND_MAX / (2 * 0.2f)));
            *torque_lost = abs(*torque_req - *torque_resp);
            acc_pedal = *app;
            acc_count++;
        }
    }
    else if (rand() > RAND_MAX / 4)
    {
        isMoving = true;
        // pick a speed
        app_max = (0.5f + ((float)rand() / (float)RAND_MAX) * 0.5f) * ECU1_APP_MAX;
        // pick a duration
        acc_duration = (rand() % 50) + 1;
        acc_count = 1;
        // prepare evolution of variables
        x = -1 + 2 / ((float)acc_duration + 1);
        *app = (1 - pow(x, 2)) * app_max;
        *rpm = (*app / 100) * ECU1_MAX_REAL_RPM * (0.8f + (float)rand() / ((float)RAND_MAX / (2 * 0.2f)));
        *torque_req = (*app / 100) * ECU1_MAX_TORQUE_REQ;
        *torque_resp = *torque_req * (0.8f + (float)rand() / ((float)RAND_MAX / (2 * 0.2f)));
        *torque_lost = abs(*torque_req - *torque_resp);
        acc_pedal = *app;
        acc_count++;
    }
}

void *ecu_data1_send_routine(void *args)
{
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG msg;
    struct timespec ts;

    msg.Flags = CAN_FLAGS_STANDARD;
    msg.Id = 0x1a0;
    msg.Size = 8;
    double rpm = 0, app = 0, torque_resp = 0, torque_req = 0, torque_lost = 0;

    struct opel_omega_2001_ecu_data1_t msg_p;
    while (1)
    {
        opel_omega_2001_ecu_data1_init(&msg_p);

        msg_p.rpm = opel_omega_2001_ecu_data1_rpm_encode(rpm);
        msg_p.app = opel_omega_2001_ecu_data1_app_encode(app);
        msg_p.torque_lost = opel_omega_2001_ecu_data1_torque_lost_encode(torque_lost);
        msg_p.torque_request = opel_omega_2001_ecu_data1_torque_request_encode(torque_req);
        msg_p.torque_response = opel_omega_2001_ecu_data1_torque_response_encode(torque_resp);

        opel_omega_2001_ecu_data1_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)ECU_DATA1_PERIOD;
        ts.tv_nsec += (ECU_DATA1_PERIOD - (int)ECU_DATA1_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        //sleep(ECU_DATA1_PERIOD);
        ecu_data1_update(&rpm, &app, &torque_req, &torque_resp, &torque_lost);
    }

    return NULL;
}

void *ecu_data2_update(double *pos)
{
    *pos = acc_pedal > ECU2_MAX_TPS ? 100 : (double)acc_pedal;
}

void *ecu_data2_send_routine(void *args)
{
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG msg;
    struct timespec ts;

    msg.Flags = CAN_FLAGS_STANDARD;
    msg.Id = 0x1c0;
    msg.Size = 8;
    double pos = 0;

    struct opel_omega_2001_ecu_data2_t msg_p;
    while (1)
    {
        opel_omega_2001_ecu_data2_init(&msg_p);
        msg_p.tps = opel_omega_2001_ecu_data2_tps_encode(pos);
        opel_omega_2001_ecu_data2_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)ECU_DATA2_PERIOD;
        ts.tv_nsec += (ECU_DATA2_PERIOD - (int)ECU_DATA2_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        //sleep(ECU_DATA2_PERIOD);
        ecu_data2_update(&pos);
    }

    return NULL;
}

void *ecu_data3_update(double *brake_active, double *kickdown_active, double *cruise_active)
{
    //Si on enfonce au moins 80% de la pédale d'accelération
    if(acc_pedal > 80)
    {
        *kickdown_active = 1;
    }
    else
    {
        *kickdown_active = 0;
    }

    // Si on a parcouru au moins 70%, alors on peut freiner
    if(acc_count > 0.7 * acc_duration)
    {
        *brake_active = 1;
    }
    else 
    {
        *brake_active = 0;
    }

    //Pour le moment on laisse le régulteur de vitesse toujours en OFF
    *cruise_active = 0;

}

void *ecu_data3_send_routine(void *args)
{
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG msg;
    struct timespec ts;

    msg.Flags = CAN_FLAGS_STANDARD;
    msg.Id = 0x280;
    msg.Size = 8;
    double brake_active = 0, kickdown_active = 0, cruise_active = 0 ;

    struct opel_omega_2001_ecu_data3_t msg_p;
    while (1)
    {
        opel_omega_2001_ecu_data3_init(&msg_p);

        msg_p.brake_active = opel_omega_2001_ecu_data3_brake_active_encode(brake_active);
        msg_p.kickdown_active = opel_omega_2001_ecu_data3_kickdown_active_encode(kickdown_active);
        msg_p.cruise_active = opel_omega_2001_ecu_data3_cruise_active_encode(cruise_active);

        opel_omega_2001_ecu_data3_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)ECU_DATA3_PERIOD;
        ts.tv_nsec += (ECU_DATA3_PERIOD - (int)ECU_DATA3_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        //sleep(ECU_DATA3_PERIOD);
        ecu_data3_update(&brake_active, &kickdown_active, &cruise_active);
    }

    return NULL;
}

void *ecu_data4_update(double *ect, double *iat)
{
    // Générer une valeur aléatoire pour ECT entre 60 et 100
    *ect = generate_random_value(20.0, 120.0);

    // Générer une valeur aléatoire pour IAT entre -10 et 50
    *iat = generate_random_value(-10.0, 50.0);
}

void *ecu_data4_send_routine(void *args)
{
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG msg;
    struct timespec ts;

    msg.Flags = CAN_FLAGS_STANDARD;
    msg.Id = 0x5c0;
    msg.Size = 8;
    double ect = 0; //Engine Coolant Temperature
    double iat = 0; //Intake Air Temperature

    struct opel_omega_2001_ecu_data4_t msg_p;
    while (1)
    {
        opel_omega_2001_ecu_data4_init(&msg_p);
        
        msg_p.ect = opel_omega_2001_ecu_data4_ect_encode(ect);
        msg_p.iat = opel_omega_2001_ecu_data4_iat_encode(iat);

        opel_omega_2001_ecu_data4_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)ECU_DATA4_PERIOD;
        ts.tv_nsec += (ECU_DATA4_PERIOD - (int)ECU_DATA4_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        //sleep(ECU_DATA4_PERIOD);
        ecu_data4_update(&ect, &iat);
    }

    return NULL;
}

void *tcu_data1_update(double *torque, double *oss)
{
    *torque = (double)(acc_pedal * 2, 55);
    // TODO - Add formula for oss depending on iss
    *oss = 0;
}

void *tcu_data1_send_routine(void *args)
{
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG msg;
    struct timespec ts;

    msg.Flags = CAN_FLAGS_STANDARD;
    msg.Id = 0x110;
    msg.Size = 8;

    struct opel_omega_2001_tcu_data1_t msg_p;
    double torque = 0, oss = 0;
    while (1)
    {
        opel_omega_2001_tcu_data1_init(&msg_p);

        msg_p.torque_request1 = opel_omega_2001_tcu_data1_torque_request1_encode(torque);
        msg_p.torque_request2 = opel_omega_2001_tcu_data1_torque_request2_encode(torque);
        msg_p.output_shaft_speed = opel_omega_2001_tcu_data1_output_shaft_speed_encode(oss);

        opel_omega_2001_tcu_data1_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)TCU_DATA1_PERIOD;
        ts.tv_nsec += (TCU_DATA1_PERIOD - (int)TCU_DATA1_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        //sleep(TCU_DATA1_PERIOD);
        tcu_data1_update(&torque, &oss);
    }

    return NULL;
}

void *tcu_data2_update(double *tot, double *shaft_speed)
{
    *tot = generate_random_value(20.0, 140.0);

    *shaft_speed = generate_random_value(0.0, 10000.0);
}

void *tcu_data2_send_routine(void *args)
{
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG msg;
    struct timespec ts;

    msg.Flags = CAN_FLAGS_STANDARD;
    msg.Id = 0x2E0;
    msg.Size = 8;

    struct opel_omega_2001_tcu_data2_t msg_p;
    double tot = 0; //Transmission Oil Temperature
    double shaft_speed = 0;

    while (1)
    {
        opel_omega_2001_tcu_data2_init(&msg_p);

        msg_p.tot = opel_omega_2001_tcu_data2_tot_encode(tot);
        msg_p.shaft_speed = opel_omega_2001_tcu_data2_input_shaft_speed_encode(shaft_speed);

        opel_omega_2001_tcu_data2_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)TCU_DATA2_PERIOD;
        ts.tv_nsec += (TCU_DATA2_PERIOD - (int)TCU_DATA2_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        //sleep(TCU_DATA2_PERIOD);
        tcu_data2_update(&tot, &shaft_speed);
    }

    return NULL;
}

// TODO - Make this more realistic
void *tcu_data3_update(double *gear, double *selector, double* tcc_state)
{
    *gear++;
    *selector++;
    *tcc_state = (*tcc_state == 2) ? 0 : 2;
}

void *tcu_data3_send_routine(void *args)
{
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG msg;
    struct timespec ts;

    msg.Flags = CAN_FLAGS_STANDARD;
    msg.Id = 0x3e0;
    msg.Size = 8;
    double gear = 0, selector = 0, sportMode = 0, winterMode = 0, autoNeutral = 1, tcc_state = 0;

    struct opel_omega_2001_tcu_data3_t msg_p;
    while (1)
    {
        opel_omega_2001_tcu_data3_init(&msg_p);

        msg_p.current_gear = opel_omega_2001_tcu_data3_current_gear_encode(gear);
        msg_p.selector_position = opel_omega_2001_tcu_data3_selector_position_encode(selector);
        msg_p.sport_mode_active = opel_omega_2001_tcu_data3_sport_mode_active_encode(sportMode);
        msg_p.winter_mode_active = opel_omega_2001_tcu_data3_winter_mode_active_encode(winterMode);
        msg_p.auto_neutral_active = opel_omega_2001_tcu_data3_auto_neutral_active_encode(autoNeutral);
        msg_p.tcc_state = opel_omega_2001_tcu_data3_tcc_state_encode(tcc_state);

        opel_omega_2001_tcu_data3_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)TCU_DATA3_PERIOD;
        ts.tv_nsec += (TCU_DATA3_PERIOD - (int)TCU_DATA3_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        //sleep(TCU_DATA3_PERIOD);
        tcu_data3_update(&gear, &selector, &tcc_state);
    }

    return NULL;
}

void *esp_data2_send_routine(void *args)
{
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG msg;
    struct timespec ts;

    msg.Flags = CAN_FLAGS_STANDARD;
    msg.Id = 0x318;
    msg.Size = 8;
    double pos = 0;

    struct opel_omega_2001_esp_data2_t msg_p;
    opel_omega_2001_esp_data2_init(&msg_p);
    msg_p.abs_active = opel_omega_2001_esp_data2_abs_active_encode(1);
    msg_p.esp_active = opel_omega_2001_esp_data2_esp_active_encode(1);
    msg_p.esp_off = opel_omega_2001_esp_data2_esp_off_encode(0);
    opel_omega_2001_esp_data2_pack(msg.Data, &msg_p, 8);
    while (1)
    {
        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)TCU_DATA3_PERIOD;
        ts.tv_nsec += (TCU_DATA3_PERIOD - (int)TCU_DATA3_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        //sleep(ESP_DATA2_PERIOD);
    }

    return NULL;
}

void *abs_wheel_speed_update(double *front_left_flag, double *front_left_speed, double *front_right_flag, double *front_right_speed, double *rear_left_flag, double *rear_left_speed, double *rear_right_flag, double *rear_right_speed) 
{

    *front_left_speed = generate_random_value(0.0, 255.0);
    
    // toutes les roues ont la même vitesse
    *front_right_speed = *front_left_speed;
    *rear_left_speed = *front_left_speed;
    *rear_right_speed = *front_left_speed;

    // Trigger du flag en fonction de la vitesse
    if (*front_left_speed == 0 || *front_left_speed >= 200.0) {
        *front_left_flag = 1;
        *front_right_flag = 1;
        *rear_right_flag = 1;
        *rear_left_flag = 1;
    } else {
        *front_left_flag = 0;
        *front_right_flag = 0;
        *rear_right_flag = 0;
        *rear_left_flag = 0; 
    }
}


void *abs_wheel_speed_routine(void *args)
{
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG msg;
    struct timespec ts;

    msg.Flags = CAN_FLAGS_STANDARD;
    msg.Id = 0x300;
    msg.Size = 8;

    struct opel_omega_2001_abs_wheel_speed_t msg_p;
    double front_left_flag, front_left_speed, front_right_flag, front_right_speed, rear_left_flag, rear_left_speed, rear_right_flag, rear_right_speed = 0; 
    
    while (1)
    {
        opel_omega_2001_abs_wheel_speed_init(&msg_p);

        msg_p.front_left_flag = opel_omega_2001_abs_wheel_speed_front_left_wheel_error_flag_encode(front_left_flag);
        msg_p.front_left_speed = opel_omega_2001_abs_wheel_speed_front_left_wheel_speed_encode(front_left_speed);
        msg_p.front_right_flag = opel_omega_2001_abs_wheel_speed_front_right_wheel_error_flag_encode(front_right_flag);
        msg_p.front_right_speed = opel_omega_2001_abs_wheel_speed_front_right_wheel_speed_encode(front_right_speed);
        msg_p.rear_left_flag = opel_omega_2001_abs_wheel_speed_rear_left_wheel_error_flag_encode(rear_left_flag);
        msg_p.rear_left_speed = opel_omega_2001_abs_wheel_speed_rear_left_wheel_speed_encode(rear_left_speed);
        msg_p.rear_right_flag = opel_omega_2001_abs_wheel_speed_rear_right_wheel_error_flag_encode(rear_right_flag);
        msg_p.rear_right_speed = opel_omega_2001_abs_wheel_speed_rear_right_wheel_speed_encode(rear_right_speed);

        opel_omega_2001_abs_wheel_speed_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)ABS_WHEEL_PERIOD;
        ts.tv_nsec += (ABS_WHEEL_PERIOD - (int)ABS_WHEEL_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        //sleep(ABS_WHEEL_PERIOD);
        abs_wheel_speed_update(&front_left_flag, &front_left_speed, &front_right_flag, &front_right_speed, &rear_left_flag, &rear_left_speed, &rear_right_flag, &rear_right_speed);
    }

    return NULL;
}

void *fake_ecu2_node(void *args)
{
    while (1)
    {
        pthread_mutex_lock(&flood_mut);
        pthread_cond_wait(&flood_cond, &flood_mut);
        pthread_mutex_unlock(&flood_mut);
        TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
        CAN_MSG msg;

        msg.Flags = CAN_FLAGS_STANDARD;
        msg.Id = 0x1c0;
        msg.Size = 8;
        double pos = 0;
        if (ATTACK_FAKE_TPS_MODE == Fast)
        {
            pos = 100;
        }
        else if (ATTACK_FAKE_TPS_MODE == Slow)
        {
            pos = 0;
        }
        else
        {
            pos = 2000;
        }

        TCAN_STATUS status;
        struct timespec ts;

        struct opel_omega_2001_ecu_data2_t msg_p;

        opel_omega_2001_ecu_data2_init(&msg_p);
        msg_p.tps = opel_omega_2001_ecu_data2_tps_encode(pos);
        opel_omega_2001_ecu_data2_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&flood_mut);
        while (floodOn)
        {
            pthread_mutex_unlock(&flood_mut);
            pthread_mutex_lock(&write_mut);
            status = CAN_Write(handle, &msg);
            pthread_mutex_unlock(&write_mut);
            if (status != CAN_ERR_OK)
                printf("error sending CAN frame \n");

            timespec_get(&ts, TIME_UTC);
            ts.tv_sec += (int)ATTACK_FAKE_TPS_PERIOD;
            ts.tv_nsec += (ATTACK_FAKE_TPS_PERIOD - (int)ATTACK_FAKE_TPS_PERIOD) * 10000000000;
            pthread_mutex_lock(&m);
            pthread_cond_timedwait(&c, &m, &ts);
            pthread_mutex_unlock(&m);
            pthread_mutex_lock(&flood_mut);
        }
        pthread_mutex_unlock(&flood_mut);
    }
    return NULL;
}

void *dos_attack_node(void *args)
{
    while (1)
    {
        pthread_mutex_lock(&dos_mut);
        pthread_cond_wait(&dos_cond, &dos_mut);
        pthread_mutex_unlock(&dos_mut);

        TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
        CAN_MSG msg;

        msg.Flags = CAN_FLAGS_STANDARD;
        msg.Id = ATTACK_DOS_ID;
        msg.Size = 8;
        for (int i = 0; i < 8; i++)
        {
            msg.Data[i] = 0;
        }

        struct timespec ts;
        TCAN_STATUS status;

        pthread_mutex_lock(&dos_mut);
        while (dosOn)
        {
            pthread_mutex_unlock(&dos_mut);
            pthread_mutex_lock(&write_mut);
            status = CAN_Write(handle, &msg);
            pthread_mutex_unlock(&write_mut);
            if (status != CAN_ERR_OK)
                printf("error sending CAN frame \n");
            timespec_get(&ts, TIME_UTC);
            ts.tv_sec += (int)ATTACK_DOS_PERIOD;
            ts.tv_nsec += (ATTACK_DOS_PERIOD - (int)ATTACK_DOS_PERIOD) * 10000000000;
            pthread_mutex_lock(&m);
            pthread_cond_timedwait(&c, &m, &ts);
            pthread_mutex_unlock(&m);
            pthread_mutex_lock(&dos_mut);
        }
        pthread_mutex_unlock(&dos_mut);
    }
    return NULL;
}
void *fuzz_ecu_data2_update(double *pos, CAN_MSG *msg) // Fonction qui génère nos valeurs aléatoires sur nos 8 octets de données
{
    if (ATTACK_FUZZ_MOD == 0) // fuzz uniquement la valeur pos
    {
        *pos = (double)(rand() % 101);
    }
    else if (ATTACK_FUZZ_MOD == 1) // fuzz les 8 octets de données
    {
        for (int i = 0; i < 8; i++)
        {
            msg->Data[i] = (unsigned char)(rand() % 256);
        }
    }
}


void *fuzz_ecu2_node(void *args) // Simulation de l'attaque fuzz sur le réseau CAN, envoi périodique des messages
{
    while (1)
    {
        pthread_mutex_lock(&fuzz_mut); // Verrouille le mutex fuzz_mut 
        pthread_cond_wait(&fuzz_cond, &fuzz_mut); // Attend un signal sur la condition fuzz_cond, relâche le mutex pendant l'attente
        pthread_mutex_unlock(&fuzz_mut); // Déverrouille le mutex fuzz_mut après avoir reçu le signal
        TCAN_HANDLE handle = *(TCAN_HANDLE *)args; // Récupère le handle CAN passé en argument
        CAN_MSG msg; // Déclare une structure de message CAN

        msg.Flags = CAN_FLAGS_STANDARD; // Initialise les flags du message à CAN standard
        msg.Id = 0x1c0; // Initialise l'ID à 0x1c0
        msg.Size = 8; // Définit la taille  à 8 octets
        static double pos = 0; 

        TCAN_STATUS status; // Déclare une variable pour stocker le statut des opérations CAN
        struct timespec ts; // Déclare une structure pour gérer les temps d'attente

        struct opel_omega_2001_ecu_data2_t msg_p; // Déclare une structure pour stocker les données spécifiques du message ECU

        pthread_mutex_lock(&fuzz_mut); // Verrouille le mutex fuzz_mut pour accéder à la boucle de fuzzing
        
        while (fuzzON) // Continue tant que fuzzOn est vrai
        {
            pthread_mutex_unlock(&fuzz_mut); // Déverrouille le mutex fuzz_mut pour permettre à d'autres threads de l'utiliser

            if (ATTACK_FUZZ_MOD == 0) // Encode le message uniquement si 
            {
            opel_omega_2001_ecu_data2_init(&msg_p); // Initialise la structure msg_p avec les données de l'ECU
            msg_p.tps = opel_omega_2001_ecu_data2_tps_encode(pos); // Encode la position dans le champ tps de msg_p
            opel_omega_2001_ecu_data2_pack(msg.Data, &msg_p, 8); // Pack les données msg_p dans le message CAN
            }

            pthread_mutex_lock(&write_mut); // Verrouille le mutex write_mut avant d'envoyer le message
            status = CAN_Write(handle, &msg); // Envoie le message CAN
            pthread_mutex_unlock(&write_mut); // Déverrouille le mutex write_mut après l'envoi

            if (status != CAN_ERR_OK) // Vérifie le statut de l'envoi
                printf("error sending CAN frame \n"); // Affiche un message d'erreur si l'envoi a échoué

            timespec_get(&ts, TIME_UTC); // Obtient le temps actuel en UTC
            ts.tv_sec += (int)ATTACK_FUZZ_PERIOD; // Ajoute la période d'attaque en secondes au temps actuel
            ts.tv_nsec += (ATTACK_FUZZ_PERIOD - (int)ATTACK_FUZZ_PERIOD) * 1000000000; // Ajoute la fraction de période en nanosecondes au temps actuel

            pthread_mutex_lock(&m); // Verrouille le mutex m pour gérer le temps d'attente
            pthread_cond_timedwait(&c, &m, &ts); // Attend la condition c ou le temps ts
            pthread_mutex_unlock(&m); // Déverrouille le mutex m après l'attente

            fuzz_ecu_data2_update(&pos, &msg); // Met à jour la position avec une nouvelle valeur aléatoire
            pthread_mutex_lock(&fuzz_mut); // Verrouille le mutex fuzz_mut pour la prochaine itération
        }
        pthread_mutex_unlock(&fuzz_mut); // Déverrouille le mutex fuzz_mut après la boucle de fuzzing
    }
    return NULL;
}

void *replay_attack_routine(void *args)
{
    while (1)
    {
        pthread_mutex_lock(&replay_mut); 
        pthread_cond_wait(&replay_cond, &replay_mut); 
        pthread_mutex_unlock(&replay_mut); 

        TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
        CAN_MSG msg;
        struct timespec ts;

        // Copie la trame stockée dans replay_msg la variable globale 
        pthread_mutex_lock(&write_mut);
        msg = replay_msg;
        pthread_mutex_unlock(&write_mut);

        while (replayOn)
        {
            // Envoi la trame CAN
            pthread_mutex_lock(&write_mut);
            TCAN_STATUS status = CAN_Write(handle, &msg);
            pthread_mutex_unlock(&write_mut);
            if (status != CAN_ERR_OK)
            {
                printf("error sending CAN frame \n");
            }

            timespec_get(&ts, TIME_UTC); // Obtient le temps actuel en UTC
            ts.tv_sec += (int)ATTACK_REPLAY_PERIOD; // Ajoute la période d'attaque en secondes au temps actuel
            ts.tv_nsec += (ATTACK_REPLAY_PERIOD - (int)ATTACK_REPLAY_PERIOD) * 1000000000; // Ajoute la fraction de période en nanosecondes au temps actuel

            pthread_mutex_lock(&m); // Verrouille le mutex m pour gérer le temps d'attente
            pthread_cond_timedwait(&c, &m, &ts); // Attend la condition c ou le temps ts
            pthread_mutex_unlock(&m); // Déverrouille le mutex m après l'attente
        }

    }
    
    return NULL;
}

int main(int argc, char *argv[])
{
    GtkApplication *app;
    int gui_status;

    app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    TCAN_HANDLE handle = can_init();
    if (handle == -1)
    {
        printf("Error initializing CAN Channel, Handle = -1 \n");
        //return 0;
    }

    TCAN_STATUS status;

    if (pthread_mutex_init(&print_mut, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }

    if (pthread_mutex_init(&write_mut, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }

    if (pthread_mutex_init(&m, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }

    if (pthread_cond_init(&c, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }

    if (pthread_mutex_init(&fuzz_mut, NULL) != 0)
    {
        printf("\n mutex fuzz init has failed\n");
        return 1;
    }

    pthread_t receive_thd;
    pthread_create(&receive_thd, NULL, receive_routine, &handle);

    pthread_t sas_thread;
    pthread_create(&sas_thread, NULL, sas_data_send_routine, &handle);

    pthread_t ecu_data1_thread;
    pthread_create(&ecu_data1_thread, NULL, ecu_data1_send_routine, &handle);

    pthread_t ecu_data2_thread;
    pthread_create(&ecu_data2_thread, NULL, ecu_data2_send_routine, &handle);

    pthread_t ecu_data3_thread;
    pthread_create(&ecu_data3_thread, NULL, ecu_data3_send_routine, &handle);

    pthread_t ecu_data4_thread;
    pthread_create(&ecu_data4_thread, NULL, ecu_data4_send_routine, &handle);

    pthread_t tcu_data1_thread;
    pthread_create(&tcu_data1_thread, NULL, tcu_data1_send_routine, &handle);

    pthread_t tcu_data2_thread;
    pthread_create(&tcu_data2_thread, NULL, tcu_data2_send_routine, &handle);

    pthread_t tcu_data3_thread;
    pthread_create(&tcu_data3_thread, NULL, tcu_data3_send_routine, &handle);

    pthread_t esp_data2_thread;
    pthread_create(&esp_data2_thread, NULL, esp_data2_send_routine, &handle);

    pthread_t abs_wheel_thread;
    pthread_create(&abs_wheel_thread, NULL, abs_wheel_speed_routine, &handle);

    pthread_t attack_tps_thread;
    pthread_create(&attack_tps_thread, NULL, fake_ecu2_node, &handle);

    pthread_t attack_dos_thread;
    pthread_create(&attack_dos_thread, NULL, dos_attack_node, &handle);

    pthread_t fuzz_thread;
    pthread_create(&fuzz_thread, NULL, fuzz_ecu2_node, &handle);

    pthread_t replay_thread;
    pthread_create(&replay_thread, NULL, replay_attack_routine, &handle);

    pthread_t suspend_thread;
    pthread_create(&suspend_thread, NULL, sas_data_send_routine, &handle);

    gui_status = g_application_run(G_APPLICATION(app), argc, argv);

    pthread_join(ecu_data1_thread, NULL);
    pthread_join(ecu_data2_thread, NULL);
    pthread_join(ecu_data3_thread, NULL);
    pthread_join(ecu_data4_thread, NULL);
    pthread_join(esp_data2_thread, NULL);
    pthread_join(tcu_data1_thread, NULL);
    pthread_join(tcu_data2_thread, NULL);
    pthread_join(tcu_data3_thread, NULL);
    pthread_join(abs_wheel_thread, NULL);
    pthread_join(sas_thread, NULL);
    pthread_join(receive_thd, NULL);
    pthread_join(fuzz_thread, NULL);
    pthread_join(replay_thread, NULL);
    pthread_join(suspend_thread, NULL);

    status = CAN_Close(handle);
    printf("Test finish\n");

    // Clean up
    g_object_unref(app);

    return 0;
}
