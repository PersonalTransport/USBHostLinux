/*
 * USBHostLinux.c
 *
 * Richard Barella Jr. (C) 2016
 *
 * Credit to Manuel Di Cerbo for original code (2011) "simplectrl.c"
 *
 * OsciPrime is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * OsciPrime is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OsciPrime; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <usb.h>
#include <libusb.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "USBQueue.h"

// Endpoints
//#define IN 0x81
//#define OUT 0x02

#define OUT 0x81
#define IN 0x02

// VID and PID when android is in host mode
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
        char buffer[100];
        // Get message type
        int messageType;
        response = libusb_bulk_transfer(handle, OUT, buffer, 100,
                                        &bytesTransferred, 1);
        if (response < 0) {
            if (response == LIBUSB_ERROR_TIMEOUT)
                return; // NO MESSAGE SO RETURN
            error(response);
        }
        messageType = ((((int) buffer[0]) << 24) & 0xFF000000)
                    | ((((int) buffer[1]) << 16) & 0x00FF0000)
                    | ((((int) buffer[2]) << 8) & 0x0000FF00)
                    | (((int) buffer[3]) & 0x000000FF);
        // Get message length
        int messageLength;
        messageLength = ((((int) buffer[4]) << 24) & 0xFF000000)
                      | ((((int) buffer[5]) << 16) & 0x00FF0000)
                      | ((((int) buffer[6]) << 8) & 0x0000FF00)
                      | (((int) buffer[7]) & 0x000000FF);
        
        assert(bytesTransferred == messageLength + 8);

        char *messageData = buffer + 8;

        // enqueue message
        USBMessage *m = (USBMessage *) malloc(sizeof(USBMessage));
        USBMessage_Init(m, messageType, messageLength, messageData);
        USBQueue_Enqueue(inputQueue, m);
}

void SendUSBOutput(USBQueue *outputQueue) {

    printf("SendUSBOutput\n");

    if (USBQueue_IsEmpty(outputQueue))
        return;
    int response;
    int bytesTransferred;
    char buffer[100];
    
    USBMessage *m = USBQueue_Dequeue(outputQueue);

    // Transfer message type
    buffer[0] = (char) ((m->messageType >> 24) & 0xFF);
    buffer[1] = (char) ((m->messageType >> 16) & 0xFF);
    buffer[2] = (char) ((m->messageType >> 8) & 0xFF);
    buffer[3] = (char) ((m->messageType) & 0xFF);

    // Transfer message length
    buffer[4] = (char) ((m->messageLength >> 24) & 0xFF);
    buffer[5] = (char) ((m->messageLength >> 16) & 0xFF);
    buffer[6] = (char) ((m->messageLength >> 8) & 0xFF);
    buffer[7] = (char) ((m->messageLength) & 0xFF);

    // Data
    for (int i = 0; i < m->messageLength; i++)
        buffer[i + 8] = m->messageData[i];

    // Transfer message
    response = libusb_bulk_transfer(handle, IN, buffer, m->messageLength + 8, &bytesTransferred, 1);

    USBMessage_Destroy(m);
    free(m);
}

void ProcessUSBInput(USBQueue *inputQueue, USBQueue *outputQueue) {

    printf("ProcessUSBInput\n");

    if (!USBQueue_IsEmpty(inputQueue)) {
        USBMessage *m = USBQueue_Dequeue(inputQueue);

        printf("Received a message.\n");
        printf("Message type:   %d\n", m->messageType);
        printf("Message length: %d\n", m->messageLength);
        printf("Message data: ");
        for (int i = 0; i < m->messageLength; i++)
            printf("%c", m->messageData[i]);
        printf("\n");

        USBMessage_Destroy(m);
        free(m);
    }
    if (USBQueue_Length(outputQueue) < MAX_QUEUE_LENGTH) {
        USBMessage *outputMessage = (USBMessage *) malloc(sizeof(USBMessage *));
        USBMessage_Init(outputMessage, 10, 4, "abcd");
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

        ReceiveUSBInput(&inputQueue);
        ProcessUSBInput(&inputQueue, &outputQueue);
        SendUSBOutput(&outputQueue);
        
        //int bytesTransferred;
        //int response;
        //char buffer[12];

        //for (int i = 0; i < 12; i++)
        //    buffer[i] = 'a'+i;

        //// Send message
        //response = libusb_bulk_transfer(handle, IN, buffer, 12,
        //                                &bytesTransferred, 0);
        //if (response < 0) {
        //    error(response);
        //}
        //printf("%d bytes transferred:\n", bytesTransferred);
        //printf("\"");
        //for (int i = 0; i < bytesTransferred; i++)
        //    printf("%c", buffer[i]);
        //printf("\"\n");

        //response = libusb_bulk_transfer(handle, OUT, buffer, 12,
        //                                &bytesTransferred, 0);

        //printf("%d bytes received:\n", bytesTransferred);
        //printf("\"");
        //for (int i = 0; i < bytesTransferred; i++)
        //    printf("%c", buffer[i]);
        //printf("\"\n");

    }
    
    
        //printf("%c\n", buffer);

//	unsigned char buffer[500000];
//	int response = 0;
//	static int transferred;
//
//	response = libusb_bulk_transfer(handle,IN,buffer,16384, &transferred,0);
//	if(response < 0){error(response);return -1;}
//
//	response = libusb_bulk_transfer(handle,IN,buffer,500000, &transferred,0);
//	if(response < 0){error(response);return -1;}

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
	//	libusb_free_transfer(ctrlTransfer);
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
