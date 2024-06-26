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
#include <sys/time.h>
#include <errno.h>

#include "config.h"

#include "opel_omega_2001.h"

#include "LinuxCAN_API.h"

// TODO - Add all other nodes
// TODO - Add attack scenarios for fuzzing, replay, spoof, suspensions
// TODO - Add aperiodic messages
// TODO - Add latency check with remote request frames

struct print_param{
    char *msg;
    size_t size;
};

pthread_mutex_t write_mut, print_mut;
pthread_mutex_t m, dos_mut, flood_mut;
pthread_cond_t c, dos_cond, flood_cond;
bool stop_threads = false;
bool dosOn = false, floodOn = false;
static float acc_pedal = 0;
GtkTextBuffer *buff;

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

    /* Place the second button in the grid cell (1, 0), and make it fill
     * just 1 cell horizontally and vertically (ie no spanning)
     */
    gtk_grid_attach(GTK_GRID(grid), button, 1, 0, 1, 1);

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

void can_print_message(CAN_MSG message, struct timeval tval_timestp, int dir) {
	// Print the JSON representation of the CAN message
	    printf("{\n");
	    printf("    \"timestamp\": %ld.%3ld,\n", tval_timestp.tv_sec,tval_timestp.tv_usec);
        printf("    \"direction\": %s,\n", dir==0?"\"received\"":"\"sent\"");
	    printf("    \"can_id\": \"0x%03X\",\n", message.Id>>18);
	    printf("    \"extended_id\": \"0x%05lX\",\n", message.Id & 0x3FFFF);
	    printf("    \"ide\": %u,\n", message.Flags & CAN_FLAGS_STANDARD);
	    printf("    \"rtr\": %u,\n", message.Flags & CAN_FLAGS_REMOTE);
	    printf("    \"dlc\": %u,\n", message.Size);
	    printf("    \"data\": [");
	    for (int i = 0; i < message.Size; i++) {
	    	printf("\"0x%02X\"", message.Data[i]);
	        if (i < message.Size - 1) {
	        	printf(", ");
	        }
	    }
	    printf("]\n");
	    printf("},\n");
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
    while (!stop_threads)
    {
        status = CAN_Read(handle, &recvMSG);
        if (status == CAN_ERR_OK)
        {
            struct timeval tval_timestp;
            gettimeofday(&tval_timestp, NULL);
            can_print_message(recvMSG, tval_timestp, 0);
            snprintf(text, sizeof(text),"Time = %ld.%3ld | Read ID=0x%lx, Type=%s, DLC=%d, FrameType=%s, Data=", tval_timestp.tv_sec,tval_timestp.tv_usec,
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
        //printf("Turn angle = %f / Length = %d \n", turn_angle, length);
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
    struct timeval tval_start, tval_end;
    while (!stop_threads)
    {
        opel_omega_2001_sas_data_init(&msg_p);
        msg_p.steering_angle = opel_omega_2001_sas_data_steering_angle_encode(angle);
        msg_p.steering_speed = opel_omega_2001_sas_data_steering_speed_encode(speed);
        opel_omega_2001_sas_data_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        struct timeval tval_timestp;
        gettimeofday(&tval_timestp, NULL);
        can_print_message(msg, tval_timestp, 1);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        else //printf("Sent frame 0x180\n");
        gettimeofday(&tval_end, NULL);
        //printf("180 time elapsed between 2 sends : %ld us", (tval_end.tv_usec - tval_start.tv_usec));
        gettimeofday(&tval_start, NULL);
        /*timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)SAS_DATA_PERIOD;
        ts.tv_nsec += (SAS_DATA_PERIOD - (int)SAS_DATA_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);*/
        usleep(SAS_DATA_PERIOD);
        sas_data_update(&angle, &speed);
    }

    return NULL;
}

void ecu_data1_update(double *rpm, double *app, double *torque_req, double *torque_resp, double *torque_lost)
{
    static bool isMoving = false;
    static int length = 1;
    static int count = 0;
    static float x = 0;
    static float app_max = 0;
    if (isMoving == true)
    {
        if (count == length + 1)
        {
            isMoving = false;
            *rpm = *app = *torque_req = *torque_lost = *torque_resp = 0;
        }
        else
        {
            x = -1 + 2 * count / ((float)length + 1);
            *app = (1 - pow(x, 2)) * app_max;
            *rpm = (*app / 100) * ECU1_MAX_REAL_RPM * (0.8f + (float)rand() / ((float)RAND_MAX / (2 * 0.2f)));
            *torque_req = (*app / 100) * ECU1_MAX_TORQUE_REQ;
            *torque_resp = *torque_req * (0.8f + (float)rand() / ((float)RAND_MAX / (2 * 0.2f)));
            *torque_lost = abs(*torque_req - *torque_resp);
            acc_pedal = *app;
            count++;
        }
    }
    else if (rand() > RAND_MAX / 4)
    {
        isMoving = true;
        // pick a speed
        app_max = (0.5f + ((float)rand() / (float)RAND_MAX) * 0.5f) * ECU1_APP_MAX;
        // pick a duration
        length = (rand() % 50) + 1;
        count = 1;
        // prepare evolution of variables
        x = -1 + 2 / ((float)length + 1);
        *app = (1 - pow(x, 2)) * app_max;
        *rpm = (*app / 100) * ECU1_MAX_REAL_RPM * (0.8f + (float)rand() / ((float)RAND_MAX / (2 * 0.2f)));
        *torque_req = (*app / 100) * ECU1_MAX_TORQUE_REQ;
        *torque_resp = *torque_req * (0.8f + (float)rand() / ((float)RAND_MAX / (2 * 0.2f)));
        *torque_lost = abs(*torque_req - *torque_resp);
        acc_pedal = *app;
        count++;
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
    struct timeval tval_start, tval_end;
    while (!stop_threads)
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
        struct timeval tval_timestp;
        gettimeofday(&tval_timestp, NULL);
        can_print_message(msg, tval_timestp, 1);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        else //printf("Sent frame 0x1a0\n");
        gettimeofday(&tval_end, NULL);
        //printf("1a0 time elapsed between 2 sends : %ld us", (tval_end.tv_usec - tval_start.tv_usec));
        gettimeofday(&tval_start, NULL);
        /*
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)ECU_DATA1_PERIOD;
        ts.tv_nsec += (ECU_DATA1_PERIOD - (int)ECU_DATA1_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        */
        usleep(ECU_DATA1_PERIOD);
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
    struct timeval tval_start, tval_end;
    while (!stop_threads)
    {
        opel_omega_2001_ecu_data2_init(&msg_p);
        msg_p.tps = opel_omega_2001_ecu_data2_tps_encode(pos);
        opel_omega_2001_ecu_data2_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        struct timeval tval_timestp;
        gettimeofday(&tval_timestp, NULL);
        can_print_message(msg, tval_timestp, 1);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        else //printf("Sent frame 0x1c0\n");
        gettimeofday(&tval_end, NULL);
        //printf("1c0 time elapsed between 2 sends : %ld us", (tval_end.tv_usec - tval_start.tv_usec));
        gettimeofday(&tval_start, NULL);
        /*
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)ECU_DATA2_PERIOD;
        ts.tv_nsec += (ECU_DATA2_PERIOD - (int)ECU_DATA2_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        */
        usleep(ECU_DATA2_PERIOD);
        ecu_data2_update(&pos);
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
    struct timeval tval_start, tval_end;
    double torque = 0, oss = 0;
    while (!stop_threads)
    {
        opel_omega_2001_tcu_data1_init(&msg_p);

        msg_p.torque_request1 = opel_omega_2001_tcu_data1_torque_request1_encode(torque);
        msg_p.torque_request2 = opel_omega_2001_tcu_data1_torque_request2_encode(torque);
        msg_p.output_shaft_speed = opel_omega_2001_tcu_data1_output_shaft_speed_encode(oss);

        opel_omega_2001_tcu_data1_pack(msg.Data, &msg_p, 8);

        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        struct timeval tval_timestp;
        gettimeofday(&tval_timestp, NULL);
        can_print_message(msg, tval_timestp, 1);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        else //printf("Sent frame 0x110\n");
        gettimeofday(&tval_end, NULL);
        //printf("110 time elapsed between 2 sends : %ld us", (tval_end.tv_usec - tval_start.tv_usec));
        gettimeofday(&tval_start, NULL);
        /*
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)TCU_DATA1_PERIOD;
        ts.tv_nsec += (TCU_DATA1_PERIOD - (int)TCU_DATA1_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        */
        usleep(TCU_DATA1_PERIOD);
        tcu_data1_update(&torque, &oss);
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
    struct timeval tval_start, tval_end;
    while (!stop_threads)
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
        struct timeval tval_timestp;
        gettimeofday(&tval_timestp, NULL);
        can_print_message(msg, tval_timestp, 1);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        else //printf("Sent frame 0x3e0\n");
        gettimeofday(&tval_end, NULL);
        //printf("3e0 time elapsed between 2 sends : %ld us", (tval_end.tv_usec - tval_start.tv_usec));
        gettimeofday(&tval_start, NULL);
        /* 
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)TCU_DATA3_PERIOD;
        ts.tv_nsec += (TCU_DATA3_PERIOD - (int)TCU_DATA3_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        */
        usleep(TCU_DATA3_PERIOD);
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
    struct timeval tval_start, tval_end;
    opel_omega_2001_esp_data2_init(&msg_p);
    msg_p.abs_active = opel_omega_2001_esp_data2_abs_active_encode(1);
    msg_p.esp_active = opel_omega_2001_esp_data2_esp_active_encode(1);
    msg_p.esp_off = opel_omega_2001_esp_data2_esp_off_encode(0);
    opel_omega_2001_esp_data2_pack(msg.Data, &msg_p, 8);
    while (!stop_threads)
    {
        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        struct timeval tval_timestp;
        gettimeofday(&tval_timestp, NULL);
        can_print_message(msg, tval_timestp, 1);
        if (status != CAN_ERR_OK)
            printf("error sending CAN frame \n");
        else //printf("Sent frame 0x318\n");
        gettimeofday(&tval_end, NULL);
        //printf("318 time elapsed between 2 sends : %ld us", (tval_end.tv_usec - tval_start.tv_usec));
        gettimeofday(&tval_start, NULL);
        /*
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += (int)ESP_DATA2_PERIOD;
        ts.tv_nsec += (ESP_DATA2_PERIOD - (int)ESP_DATA2_PERIOD) * 10000000000;
        pthread_mutex_lock(&m);
        pthread_cond_timedwait(&c, &m, &ts);
        pthread_mutex_unlock(&m);
        */
        usleep(ESP_DATA2_PERIOD);
    }

    return NULL;
}

void *fake_ecu2_node(void *args)
{
    while (!stop_threads)
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
            struct timeval tval_timestp;
            gettimeofday(&tval_timestp, NULL);
            can_print_message(msg, tval_timestp, 1);
            if (status != CAN_ERR_OK)
                printf("error sending CAN frame \n");
            else //printf("Sent frame 0x1c0\n");

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
    while (!stop_threads)
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
            struct timeval tval_timestp;
            gettimeofday(&tval_timestp, NULL);
            can_print_message(msg, tval_timestp, 1);
            if (status != CAN_ERR_OK)
                printf("error sending CAN frame \n");
            else //printf("Sent frame 0x1\n");
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

void *delay_msrmnt_routine(void* args){
    TCAN_HANDLE handle = *(TCAN_HANDLE *)args;
    CAN_MSG msg = {
        .Id = 0,
        .Data = {0,0,0,0,0,0,0,0},
        .Size = 8,
        .Flags = CAN_FLAGS_STANDARD,
    };
    while (!stop_threads)
    {
        int wait = ((float)rand() / RAND_MAX) * 1000000;
        usleep(wait);
        pthread_mutex_lock(&write_mut);
        TCAN_STATUS status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        struct timeval tval_timestp;
        gettimeofday(&tval_timestp, NULL);
        can_print_message(msg, tval_timestp, 1);
        usleep(1000000-wait);
        
        msg.Id = 0x200;
        wait = ((float)rand() / RAND_MAX) * 1000000;
        usleep(wait);
        pthread_mutex_lock(&write_mut);
        status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        tval_timestp;
        gettimeofday(&tval_timestp, NULL);
        can_print_message(msg, tval_timestp, 1);
        usleep(1000000-wait);

        msg.Id = 0x400;
        wait = ((float)rand() / RAND_MAX) * 1000000;
        usleep(wait);
        pthread_mutex_lock(&write_mut);
        status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        tval_timestp;
        gettimeofday(&tval_timestp, NULL);
        can_print_message(msg, tval_timestp, 1);
        usleep(1000000-wait);

        msg.Id = 0x600;
        wait = ((float)rand() / RAND_MAX) * 1000000;
        usleep(wait);
        pthread_mutex_lock(&write_mut);
        status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        tval_timestp;
        gettimeofday(&tval_timestp, NULL);
        can_print_message(msg, tval_timestp, 1);
        usleep(1000000-wait);

        msg.Id = 0x7FF;
        wait = ((float)rand() / RAND_MAX) * 1000000;
        usleep(wait);
        pthread_mutex_lock(&write_mut);
        status = CAN_Write(handle, &msg);
        pthread_mutex_unlock(&write_mut);
        tval_timestp;
        gettimeofday(&tval_timestp, NULL);
        can_print_message(msg, tval_timestp, 1);
        usleep(1000000-wait);
    }
}

void *stop_system_routine(){
    usleep(10000000);
    stop_threads = true;
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
        return 0;
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

    pthread_t receive_thd;
    pthread_create(&receive_thd, NULL, receive_routine, &handle);

    pthread_t sas_thread;
    pthread_create(&sas_thread, NULL, sas_data_send_routine, &handle);

    usleep(30000);

    pthread_t ecu_data1_thread;
    pthread_create(&ecu_data1_thread, NULL, ecu_data1_send_routine, &handle);

    usleep(30000);

    pthread_t ecu_data2_thread;
    pthread_create(&ecu_data2_thread, NULL, ecu_data2_send_routine, &handle);

    usleep(30000);

    pthread_t tcu_data1_thread;
    pthread_create(&tcu_data1_thread, NULL, tcu_data1_send_routine, &handle);

    usleep(30000);

    pthread_t tcu_data3_thread;
    pthread_create(&tcu_data3_thread, NULL, tcu_data3_send_routine, &handle);

    usleep(30000);

    pthread_t esp_data2_thread;
    pthread_create(&esp_data2_thread, NULL, esp_data2_send_routine, &handle);

    usleep(30000);

    pthread_t attack_tps_thread;
    pthread_create(&attack_tps_thread, NULL, fake_ecu2_node, &handle);
    
    usleep(30000);
    
    pthread_t attack_dos_thread;
    pthread_create(&attack_dos_thread, NULL, dos_attack_node, &handle);

    pthread_t delay_msrmnt_thread;
    pthread_create(&delay_msrmnt_thread, NULL, delay_msrmnt_routine, &handle);

    pthread_t stop_thread;
    pthread_create(&stop_thread, NULL, stop_system_routine, NULL);

    gui_status = g_application_run(G_APPLICATION(app), argc, argv);

    pthread_join(ecu_data1_thread, NULL);
    pthread_join(ecu_data2_thread, NULL);
    pthread_join(tcu_data1_thread, NULL);
    pthread_join(tcu_data3_thread, NULL);
    pthread_join(esp_data2_thread, NULL);
    pthread_join(attack_dos_thread, NULL);
    pthread_join(attack_tps_thread, NULL);
    pthread_join(sas_thread, NULL);
    pthread_join(receive_thd, NULL);
    status = CAN_Close(handle);
    printf("Test finish\n");

    // Clean up
    g_object_unref(app);

    return 0;
}