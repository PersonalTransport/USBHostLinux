#ifndef USBQUEUE
#define USBQUEUE

#include <stdlib.h>
#include <assert.h>

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

#ifndef bool
#define bool int
#endif

typedef struct _USBMessage {
    int messageType;
    int messageLength;
    char *messageData;
} USBMessage;

typedef struct _USBQueueNode {
    struct _USBQueueNode *next;
    USBMessage *message;
} USBQueueNode;

typedef struct _USBQueue {
    USBQueueNode *head;
    USBQueueNode *tail;
    int length;
} USBQueue;

void USBQueue_Init(USBQueue *q);
void USBQueue_Destroy(USBQueue *q);
bool USBQueue_IsEmpty(USBQueue *q);
int USBQueue_Length(USBQueue *q);
void USBQueue_Enqueue(USBQueue *q, USBMessage *m);
USBMessage * USBQueue_Dequeue(USBQueue *q);

void USBMessage_Init(USBMessage *m, int messageType, int messageLength, char *messageData);
void USBMessage_Destroy(USBMessage *m);

void USBQueue_Init(USBQueue *q) {
    assert(q != NULL);
    q->head = NULL;
    q->tail = NULL;
    q->length = 0;
}

void USBQueue_Destroy(USBQueue *q) {
    assert(q != NULL);
    while (!USBQueue_IsEmpty(q)) {
        USBMessage *m = USBQueue_Dequeue(q);
        USBMessage_Destroy(m);
        free(m);
    }
}

bool USBQueue_IsEmpty(USBQueue *q) {
    assert(q != NULL);
    if (q->head == NULL) {
        assert(q->tail == NULL);
        return true;
    }
    assert(q->tail != NULL);
    return false;
}

int USBQueue_Length(USBQueue *q) {
    assert(q != NULL);
    return q->length;
}

void USBQueue_Enqueue(USBQueue *q, USBMessage *m) {
    assert(q != NULL);
    if (USBQueue_IsEmpty(q)) {
        assert(q->head == NULL);
        assert(q->tail == NULL);
        USBQueueNode *n = (USBQueueNode *) malloc(sizeof(USBQueueNode));
        n->next = NULL;
        n->message = m;
        q->head = n;
        q->tail = n;
    }
    else {
        assert(q->head != NULL);
        assert(q->tail != NULL);
        USBQueueNode *n = (USBQueueNode *) malloc(sizeof(USBQueueNode));
        n->next = NULL;
        n->message = m;
        q->head->next = n;
        q->head = n;
    }
    q->length += 1;
}

USBMessage * USBQueue_Dequeue(USBQueue *q) {
    assert(q != NULL);
    assert(!USBQueue_IsEmpty(q));
    if (q->head == q->tail) {
        USBQueueNode *n = q->tail;
        q->tail = NULL;
        q->head = NULL;
        USBMessage *m = n->message;
        free(n);
        q->length -= 1;
        return m;
    }
    else {
        USBQueueNode *n = q->tail;
        q->tail = q->tail->next;
        USBMessage *m = n->message;
        free(n);
        q->length -= 1;
        return m;
    }
}

void USBMessage_Init(USBMessage *m, int messageType, int messageLength, char *messageData) {
    assert(m != NULL);
    char *newData = (char *) malloc(messageLength);
    for (int i = 0; i < messageLength; i++)
        newData[i] = messageData[i];
    m->messageType = messageType;
    m->messageLength = messageLength;
    m->messageData = newData;
}

void USBMessage_Destroy(USBMessage *m) {
    assert(m != NULL);
    free(m->messageData);
}

#endif /* USBQUEUE */
