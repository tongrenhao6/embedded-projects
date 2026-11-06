/* node_app.h - terminal firmware high-level app (node role) */
#ifndef NODE_APP_H
#define NODE_APP_H

void node_app_init(void);

/* FreeRTOS task entries (referenced by freertos.c) */
void NodeSensorTask(void *argument);
void NodeLoraTask(void *argument);
void NodeAlarmTask(void *argument);
void NodeDisplayTask(void *argument);

#endif
