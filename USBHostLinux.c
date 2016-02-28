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

#include "USBQueue.h"
#include "USBMessage.h"

// Endpoints
//#define IN 0x81
//#define OUT 0x02

#define OUT 0x81
#define IN 0x02

// VID and PID when android is in host mode
#define VID 0x22b8
#define PID 0x2e76

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
    if(init() < 0)
        return -1;
    if(setupAccessory(
        "Manufacturer",
        "Model",
        "Description",
        "VersionName",
        "http://neuxs-computing.ch",
        "2254711SerialNo.") < 0){
        fprintf(stdout, "Error setting up accessory\n");
        deInit();
        return -1;
    };
    if(mainPhase() < 0){
        fprintf(stdout, "Error during main phase\n");
        deInit();
        return -1;
    }   
    deInit();
    fprintf(stdout, "Done, no errors\n");
    return 0;
}

void ReceiveUSBInput(USBQueue *inputQueue) {
        
        printf("ReceiveUSBInput\n");

        if (USBQueue_Length(inputQueue) >= MAX_QUEUE_LENGTH)
            return; // don't let our queue fill up too much

        int bytesTransferred;
        int response;
        
        char *buffer = (char *) malloc(262);
        
        // Get message data
        response = libusb_bulk_transfer(handle, OUT, buffer, 262,
                                        &bytesTransferred, 0);
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

    printf("SendUSBOutput\n");

    if (USBQueue_IsEmpty(outputQueue))
        return;
        
    int response;
    int bytesTransferred;
//    char buffer[262];
//    for (int i = 0; i < 262; i++)
//        buffer[i] = (char) 0;
    
    USBMessage *m = USBQueue_Dequeue(outputQueue);

    // Transfer message comm
    //buffer[0] = (char) m->comm;
    
    // Transfer sid
    //buffer[1] = (char) m->sid[0];
    //buffer[2] = (char) m->sid[1];
    //buffer[3] = (char) m->sid[2];
    //buffer[4] = (char) m->sid[3];

    // Transfer message length
    //buffer[5] = (char) m->length

    // Data
    //for (int i = 0; i < m->messageLength; i++)
    //    buffer[i + MESSAGE_DATA_OFFSET] = m->messageData[i];

    // Transfer message
    //response = libusb_bulk_transfer(handle, IN, buffer, m->messageLength + 8, &bytesTransferred, 1);
    
    // Could also do this (but I don't know if it will work)
//    response = libusb_bulk_transfer(handle, IN, (char *) m,
    response = libusb_bulk_transfer(handle, IN, (char *) m,
                                    m->length + MESSAGE_DATA_OFFSET,
                                    &bytesTransferred, 0);

    free(m);
}

void ProcessUSBInput(USBQueue *inputQueue, USBQueue *outputQueue) {
    int input_test;
    int input_data;
    char output_data[2];
    int output_sid;


    printf("ProcessUSBInput\n");

    // process data from receive queue
    if (!USBQueue_IsEmpty(inputQueue)) {
        USBMessage *m = USBQueue_Dequeue(inputQueue);

        printf("Received a message.\n");
        printf("Message comm:   %d\n", (int) m->comm);
        printf("Message sid:    %d\n", USBMessage_Get_SID(m));
        printf("Message length: %d\n", (int) m->length);
        printf("Message data: ");
        for (int i = 0; i < m->length; i++)
            printf("%c", m->data[i]);
        printf("\n");

        free(m);
    }
    
    // send some test data
    // TEST _ JOSEPH
    if (USBQueue_Length(outputQueue) < MAX_QUEUE_LENGTH) {
        printf("+++PLEASE ENTER DATA TO SEND+++\n>");
        input_test = true;
        while (input_test) {
            scanf("%d", &input_data);
            if (input_data < 99 && input_data >= 0) {
                sprintf(output_data, "%02d", input_data);
                input_test = false;
            } else {
                printf("++Please make sure the number is between 0 and 99\n>");
            }
        }
        
        printf("+=Here is your number to send = %s", output_data);
        
        printf("===Send to which object (sid)? (Enum{ lights = 1, turn_signal=2, battery=3, speed=4})===\n>");
        input_test = true;
        while (input_test) {
            scanf("%d", &input_data);
            if (input_data < 5 && input_data >= 0) {
                output_sid = input_data;    
                input_test = false;
            } else {
                printf("==Please make sure the number is between 0 and 4\n>");
            }
        
        }
        printf("+=Here is your sid to send = %d", output_sid);
        
        USBMessage *outputMessage = (USBMessage *) malloc(sizeof(USBMessage));

        //char data_send[20] = "abcd: this is a test";
        
        USBMessage_Init(outputMessage,
                        USBMESSAGE_COMM_SET_VAR,
                        output_sid,
                        //USBMESSAGE_SID_LIGHTS,
                        strlen(output_data), // length
                        output_data);
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
        fprintf(stdout, "Problem acquireing handle\n");
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
    fprintf(stdout,"Verion Code Device: %d\n", devVersion);
    
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

    fprintf(stdout,"Accessory Identification sent %d\n", devVersion);

    response = libusb_control_transfer(handle,0x40,53,0,0,NULL,0,0);
    if(response < 0){error(response);return -1;}

    fprintf(stdout,"Attempted to put device into accessory mode %d\n", devVersion);

    if(handle != NULL)
        libusb_release_interface (handle, 0);


    for(;;){//attempt to connect to new PID, if that doesn't work try ACCESSORY_PID_ALT
        tries--;
        if((handle = libusb_open_device_with_vid_pid(NULL, ACCESSORY_VID, ACCESSORY_PID)) == NULL){
            if(tries < 0){
                fprintf(stdout, "Something at that point...\n");
                return -1;
            }
        }else{
            break;
        }
        sleep(1);
    }
    libusb_claim_interface(handle, 0);
    fprintf(stdout, "Interface claimed, ready to transfer data\n");
    return 0;
}

static void error(int code){
    fprintf(stdout,"\n");
    switch(code){
    case LIBUSB_ERROR_IO:
        fprintf(stdout,"Error: LIBUSB_ERROR_IO\nInput/output error.\n");
        break;
    case LIBUSB_ERROR_INVALID_PARAM:
        fprintf(stdout,"Error: LIBUSB_ERROR_INVALID_PARAM\nInvalid parameter.\n");
        break;
    case LIBUSB_ERROR_ACCESS:
        fprintf(stdout,"Error: LIBUSB_ERROR_ACCESS\nAccess denied (insufficient permissions).\n");
        break;
    case LIBUSB_ERROR_NO_DEVICE:
        fprintf(stdout,"Error: LIBUSB_ERROR_NO_DEVICE\nNo such device (it may have been disconnected).\n");
        break;
    case LIBUSB_ERROR_NOT_FOUND:
        fprintf(stdout,"Error: LIBUSB_ERROR_NOT_FOUND\nEntity not found.\n");
        break;
    case LIBUSB_ERROR_BUSY:
        fprintf(stdout,"Error: LIBUSB_ERROR_BUSY\nResource busy.\n");
        break;
    case LIBUSB_ERROR_TIMEOUT:
        fprintf(stdout,"Error: LIBUSB_ERROR_TIMEOUT\nOperation timed out.\n");
        break;
    case LIBUSB_ERROR_OVERFLOW:
        fprintf(stdout,"Error: LIBUSB_ERROR_OVERFLOW\nOverflow.\n");
        break;
    case LIBUSB_ERROR_PIPE:
        fprintf(stdout,"Error: LIBUSB_ERROR_PIPE\nPipe error.\n");
        break;
    case LIBUSB_ERROR_INTERRUPTED:
        fprintf(stdout,"Error:LIBUSB_ERROR_INTERRUPTED\nSystem call interrupted (perhaps due to signal).\n");
        break;
    case LIBUSB_ERROR_NO_MEM:
        fprintf(stdout,"Error: LIBUSB_ERROR_NO_MEM\nInsufficient memory.\n");
        break;
    case LIBUSB_ERROR_NOT_SUPPORTED:
        fprintf(stdout,"Error: LIBUSB_ERROR_NOT_SUPPORTED\nOperation not supported or unimplemented on this platform.\n");
        break;
    case LIBUSB_ERROR_OTHER:
        fprintf(stdout,"Error: LIBUSB_ERROR_OTHER\nOther error.\n");
        break;
    default:
        fprintf(stdout, "Error: unkown error\n");
    }
}

static void status(int code){
    fprintf(stdout,"\n");
    switch(code){
        case LIBUSB_TRANSFER_COMPLETED:
            fprintf(stdout,"Success: LIBUSB_TRANSFER_COMPLETED\nTransfer completed.\n");
            break;
        case LIBUSB_TRANSFER_ERROR:
            fprintf(stdout,"Error: LIBUSB_TRANSFER_ERROR\nTransfer failed.\n");
            break;
        case LIBUSB_TRANSFER_TIMED_OUT:
            fprintf(stdout,"Error: LIBUSB_TRANSFER_TIMED_OUT\nTransfer timed out.\n");
            break;
        case LIBUSB_TRANSFER_CANCELLED:
            fprintf(stdout,"Error: LIBUSB_TRANSFER_CANCELLED\nTransfer was cancelled.\n");
            break;
        case LIBUSB_TRANSFER_STALL:
            fprintf(stdout,"Error: LIBUSB_TRANSFER_STALL\nFor bulk/interrupt endpoints: halt condition detected (endpoint stalled).\nFor control endpoints: control request not supported.\n");
            break;
        case LIBUSB_TRANSFER_NO_DEVICE:
            fprintf(stdout,"Error: LIBUSB_TRANSFER_NO_DEVICE\nDevice was disconnected.\n");
            break;
        case LIBUSB_TRANSFER_OVERFLOW:
            fprintf(stdout,"Error: LIBUSB_TRANSFER_OVERFLOW\nDevice sent more data than requested.\n");
            break;
        default:
            fprintf(stdout,"Error: unknown error\nTry again(?)\n");
            break;
    }
}
