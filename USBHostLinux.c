/*
 * USBHostLinux.c
 *
 * Richard Barella Jr. (C) 2016
 *
 * Credit to Manuel Di Cerbo for original code (2011) "simplectrl.c"
 *
 * USBHostLinux.c is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * USBHostLinux.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this source file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <usb.h>
#include <libusb.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ncurses.h>

#include "USBQueue.h"
#include "USBMessage.h"

// Endpoints
#define OUT 0x81
#define IN 0x02

//#define OUT 0x83
//#define IN 0x02

// VID and PID when android is in host mode

//      Joseph's VID and PID
//#define VID 0x22b8
//#define PID 0x2e76

//      Ricky's VID and PID
#define VID 0x04e8
#define PID 0x6860

// VID and PID after android is turned into accessory mode
#define ACCESSORY_VID 0x18d1
#define ACCESSORY_PID 0x2d01

#define LEN 2

#define MAX_QUEUE_LENGTH 10

/*
If you are on Ubuntu you will require libusb as well as the headers...
We installed the headers with "apt-get source libusb"
gcc simplectrl.c -I/usr/include/ -o simplectrl -lusb-1.0 -I/usr/include/ -I/usr/include/libusb-1.0

Tested for Nexus S with Gingerbread 2.3.4
*/

static int mainPhase();
static int init(void);
static int deInit(void);
static void error(int code);
static void status(int code);
static int setupAccessory(
    const char* manufacturer,
    const char* modelName,
    const char* description,
    const char* version,
    const char* uri,
    const char* serialNumber);

//static
static struct libusb_device_handle* handle;
static char stop;
static char success = 0;

int main (int argc, char *argv[]){
    initscr();
    if(init() < 0) {
        endwin();
        return -1;
    }
    if(setupAccessory(
        "Personal Transportation Solutions",
        "EvMaster",
        "Description",
        "0.1",
        "http://neuxs-computing.ch",
        "2254711SerialNo.") < 0){
        printw( "Error setting up accessory\n");
        refresh();
        deInit();
        return -1;
    }
    if(mainPhase() < 0){
        printw( "Error during main phase\n");
        refresh();
        deInit();
        endwin();
        return -1;
    }   
    deInit();
    printw( "Done, no errors\n");
    refresh();
    endwin();
    return 0;
}

void ReceiveUSBInput(USBQueue *inputQueue) {
        
        //printw("ReceiveUSBInput\n");

        if (USBQueue_Length(inputQueue) >= MAX_QUEUE_LENGTH)
            return; // don't let our queue fill up too much

        int bytesTransferred;
        int response;
        
        char *buffer = (char *) malloc(262);
        
        // Get message data
        response = libusb_bulk_transfer(handle, OUT, buffer, 262,
                                        &bytesTransferred, 10);
        if (response < 0) {
            if (response == LIBUSB_ERROR_TIMEOUT){
                free(buffer);
                return; // NO MESSAGE SO RETURN
            }
            error(response);
        }
        
        USBMessage *usbMessage = (USBMessage*) buffer;
        
        assert(bytesTransferred == usbMessage->length + MESSAGE_DATA_OFFSET);

        // enqueue message
        USBQueue_Enqueue(inputQueue, usbMessage);
}

void SendUSBOutput(USBQueue *outputQueue) {

    //printw("SendUSBOutput\n");

    if (USBQueue_IsEmpty(outputQueue))
        return;
        
    int response;
    int bytesTransferred;
    
    USBMessage *m = USBQueue_Dequeue(outputQueue);

    response = libusb_bulk_transfer(handle, IN, (char *) m,
                                    m->length + MESSAGE_DATA_OFFSET,
                                    &bytesTransferred, 10);

    free(m);
}

void int_to_bytes(char *out, int in) {
    out[0] = (char) ((in >> 24) & 0xFF);
    out[1] = (char) ((in >> 16) & 0xFF);
    out[2] = (char) ((in >> 8) & 0xFF);
    out[3] = (char) ((in) & 0xFF);
}

void ProcessUSBInput(USBQueue *inputQueue, USBQueue *outputQueue) {
    int input_test;
    int input_data;
    char output_data_1[4]; int_to_bytes(output_data_1, 0);
    char output_data_2[4]; int_to_bytes(output_data_2, 11);
    char output_data_3[4]; int_to_bytes(output_data_3, 23);
    char output_data_4[4]; int_to_bytes(output_data_4, 35);
    char output_data_5[4]; int_to_bytes(output_data_5, 47);
    char output_data_6[4]; int_to_bytes(output_data_6, 52);
    char output_data_7[4]; int_to_bytes(output_data_7, 60);
    char output_data_8[4]; int_to_bytes(output_data_8, 74);
    char output_data_9[4]; int_to_bytes(output_data_9, 82);
    char output_data_0[4]; int_to_bytes(output_data_0, 99);
    char output_data[4];
    
    int output_sid;
    char output_sid_str[20];

    char lights_sid_str[20]      = "LIGHTS";
    char turn_signal_sid_str[20] = "TURN_SIGNAL";
    char battery_sid_str[20]     = "BATTERY";
    char speed_sid_str[20]       = "SPEED";
    char hazard_sid_str[20]      = "HAZARD";
    char defrost_sid_str[20]     = "DEFROST";
    char wipers_sid_str[20]      = "WIPERS";

    //printw("ProcessUSBInput\n");

    // process data from receive queue
    if (!USBQueue_IsEmpty(inputQueue)) {
        USBMessage *m = USBQueue_Dequeue(inputQueue);

        printw("Received a message.\n");
        printw("Message comm:   %d\n", (int) m->comm);
        printw("Message sid:    %d\n", USBMessage_Get_SID(m));
        printw("Message length: %d\n", (int) m->length);
        printw("Message data: %d\n", (int) char_Array_to_Int(m->data));

        refresh();

        free(m);
    }
    
    // send some test data
    // TEST _ JOSEPH
    if (USBQueue_Length(outputQueue) < MAX_QUEUE_LENGTH) {
        
        char c = getch();
        switch (c) {
            case '1':
                int_to_bytes(output_data, 0);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = speed_sid_str[i];
                break;
            case '2':
                int_to_bytes(output_data, 11);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = speed_sid_str[i];
                break;
            case '3':
                int_to_bytes(output_data, 22);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = speed_sid_str[i];
                break;
            case '4':
                int_to_bytes(output_data, 33);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = speed_sid_str[i];
                break;
            case '5':
                int_to_bytes(output_data, 44);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = speed_sid_str[i];
                break;
            case '6':
                int_to_bytes(output_data, 55);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = speed_sid_str[i];
                break;
            case '7':
                int_to_bytes(output_data, 66);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = speed_sid_str[i];
                break;
            case '8':
                int_to_bytes(output_data, 77);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = speed_sid_str[i];
                break;
            case '9':
                int_to_bytes(output_data, 88);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = speed_sid_str[i];
                break;
            case '0':
                int_to_bytes(output_data, 99);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = speed_sid_str[i];
                break;
            case 'q':
                int_to_bytes(output_data, 0);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = battery_sid_str[i];
                break;
            case 'w':
                int_to_bytes(output_data, 11);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = battery_sid_str[i];
                break;
            case 'e':
                int_to_bytes(output_data, 22);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = battery_sid_str[i];
                break;
            case 'r':
                int_to_bytes(output_data, 33);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = battery_sid_str[i];
                break;
            case 't':
                int_to_bytes(output_data, 44);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = battery_sid_str[i];
                break;
            case 'y':
                int_to_bytes(output_data, 55);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = battery_sid_str[i];
                break;
            case 'u':
                int_to_bytes(output_data, 66);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = battery_sid_str[i];
                break;
            case 'i':
                int_to_bytes(output_data, 77);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = battery_sid_str[i];
                break;
            case 'o':
                int_to_bytes(output_data, 88);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = battery_sid_str[i];
                break;
            case 'p':
                int_to_bytes(output_data, 99);
                for (int i = 0; i < 20; i++)
                    output_sid_str[i] = battery_sid_str[i];
                break;
            case ERR:
            default:
                return;
                break;
        }

        
        USBMessage *outputMessage = (USBMessage *) malloc(sizeof(USBMessage));
        USBMessage_Init(outputMessage,
                        USBMESSAGE_COMM_SET_VAR,
                        output_sid_str,
                        4, // length
                        output_data);
        //printw("+=Here is your sid to send = %d\n", USBMessage_Get_SID(outputMessage));
        USBQueue_Enqueue(outputQueue, outputMessage);
    }
}

static int mainPhase(){

    char buffer;
    int response;
    static int transferred;

    USBQueue inputQueue;
    USBQueue_Init(&inputQueue);
    USBQueue outputQueue;
    USBQueue_Init(&outputQueue);

    while (1) {

        ProcessUSBInput(&inputQueue, &outputQueue);
        SendUSBOutput(&outputQueue);
        ReceiveUSBInput(&inputQueue);
    }
}



static int init(){
    libusb_init(NULL);
    if((handle = libusb_open_device_with_vid_pid(NULL, VID, PID)) == NULL){
        printw( "Problem acquireing handle\n");
        refresh();
        return -1;
    }
    libusb_claim_interface(handle, 0);
    return 0;
}

static int deInit(){
    //TODO free all transfers individually...
    //if(ctrlTransfer != NULL)
    //  libusb_free_transfer(ctrlTransfer);
    if(handle != NULL)
        libusb_release_interface (handle, 0);
    libusb_exit(NULL);
    return 0;
}

static int setupAccessory(
    const char* manufacturer,
    const char* modelName,
    const char* description,
    const char* version,
    const char* uri,
    const char* serialNumber){

    unsigned char ioBuffer[2];
    int devVersion;
    int response;
    int tries = 5;

    response = libusb_control_transfer(
        handle, //handle
        0xC0, //bmRequestType
        51, //bRequest
        0, //wValue
        0, //wIndex
        ioBuffer, //data
        2, //wLength
        0 //timeout
    );

    if(response < 0){error(response);return-1;}

    devVersion = ioBuffer[1] << 8 | ioBuffer[0];
    printw("Verion Code Device: %d\n", devVersion);
    refresh();
    
    sleep(1);//sometimes hangs on the next transfer :(

    response = libusb_control_transfer(handle,0x40,52,0,0,(char*)manufacturer,strlen(manufacturer),0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,0x40,52,0,1,(char*)modelName,strlen(modelName)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,0x40,52,0,2,(char*)description,strlen(description)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,0x40,52,0,3,(char*)version,strlen(version)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,0x40,52,0,4,(char*)uri,strlen(uri)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,0x40,52,0,5,(char*)serialNumber,strlen(serialNumber)+1,0);
    if(response < 0){error(response);return -1;}

    printw("Accessory Identification sent %d\n", devVersion);
    refresh();

    response = libusb_control_transfer(handle,0x40,53,0,0,NULL,0,0);
    if(response < 0){error(response);return -1;}

    printw("Attempted to put device into accessory mode %d\n", devVersion);
    refresh();

    if(handle != NULL)
        libusb_release_interface (handle, 0);


    for(;;){//attempt to connect to new PID, if that doesn't work try ACCESSORY_PID_ALT
        tries--;
        if((handle = libusb_open_device_with_vid_pid(NULL, ACCESSORY_VID, ACCESSORY_PID)) == NULL){
            if(tries < 0){
                printw( "Something at that point...\n");
                refresh();
                return -1;
            }
        }else{
            break;
        }
        sleep(1);
    }
    libusb_claim_interface(handle, 0);
    printw( "Interface claimed, ready to transfer data\n");
    refresh();
    return 0;
}

static void error(int code){
    printw("\n");
    refresh();
    switch(code){
    case LIBUSB_ERROR_IO:
        printw("Error: LIBUSB_ERROR_IO\nInput/output error.\n");
        refresh();
        break;
    case LIBUSB_ERROR_INVALID_PARAM:
        printw("Error: LIBUSB_ERROR_INVALID_PARAM\nInvalid parameter.\n");
        refresh();
        break;
    case LIBUSB_ERROR_ACCESS:
        printw("Error: LIBUSB_ERROR_ACCESS\nAccess denied (insufficient permissions).\n");
        refresh();
        break;
    case LIBUSB_ERROR_NO_DEVICE:
        printw("Error: LIBUSB_ERROR_NO_DEVICE\nNo such device (it may have been disconnected).\n");
        refresh();
        break;
    case LIBUSB_ERROR_NOT_FOUND:
        printw("Error: LIBUSB_ERROR_NOT_FOUND\nEntity not found.\n");
        refresh();
        break;
    case LIBUSB_ERROR_BUSY:
        printw("Error: LIBUSB_ERROR_BUSY\nResource busy.\n");
        refresh();
        break;
    case LIBUSB_ERROR_TIMEOUT:
        printw("Error: LIBUSB_ERROR_TIMEOUT\nOperation timed out.\n");
        refresh();
        break;
    case LIBUSB_ERROR_OVERFLOW:
        printw("Error: LIBUSB_ERROR_OVERFLOW\nOverflow.\n");
        refresh();
        break;
    case LIBUSB_ERROR_PIPE:
        printw("Error: LIBUSB_ERROR_PIPE\nPipe error.\n");
        refresh();
        break;
    case LIBUSB_ERROR_INTERRUPTED:
        printw("Error:LIBUSB_ERROR_INTERRUPTED\nSystem call interrupted (perhaps due to signal).\n");
        refresh();
        break;
    case LIBUSB_ERROR_NO_MEM:
        printw("Error: LIBUSB_ERROR_NO_MEM\nInsufficient memory.\n");
        refresh();
        break;
    case LIBUSB_ERROR_NOT_SUPPORTED:
        printw("Error: LIBUSB_ERROR_NOT_SUPPORTED\nOperation not supported or unimplemented on this platform.\n");
        refresh();
        break;
    case LIBUSB_ERROR_OTHER:
        printw("Error: LIBUSB_ERROR_OTHER\nOther error.\n");
        refresh();
        break;
    default:
        printw( "Error: unkown error\n");
        refresh();
    }
}

static void status(int code){
    printw("\n");
    switch(code){
        case LIBUSB_TRANSFER_COMPLETED:
            printw("Success: LIBUSB_TRANSFER_COMPLETED\nTransfer completed.\n");
            refresh();
            break;
        case LIBUSB_TRANSFER_ERROR:
            printw("Error: LIBUSB_TRANSFER_ERROR\nTransfer failed.\n");
            refresh();
            break;
        case LIBUSB_TRANSFER_TIMED_OUT:
            printw("Error: LIBUSB_TRANSFER_TIMED_OUT\nTransfer timed out.\n");
            refresh();
            break;
        case LIBUSB_TRANSFER_CANCELLED:
            printw("Error: LIBUSB_TRANSFER_CANCELLED\nTransfer was cancelled.\n");
            refresh();
            break;
        case LIBUSB_TRANSFER_STALL:
            printw("Error: LIBUSB_TRANSFER_STALL\nFor bulk/interrupt endpoints: halt condition detected (endpoint stalled).\nFor control endpoints: control request not supported.\n");
            refresh();
            break;
        case LIBUSB_TRANSFER_NO_DEVICE:
            printw("Error: LIBUSB_TRANSFER_NO_DEVICE\nDevice was disconnected.\n");
            refresh();
            break;
        case LIBUSB_TRANSFER_OVERFLOW:
            printw("Error: LIBUSB_TRANSFER_OVERFLOW\nDevice sent more data than requested.\n");
            refresh();
            break;
        default:
            printw("Error: unknown error\nTry again(?)\n");
            refresh();
            break;
    }
}
