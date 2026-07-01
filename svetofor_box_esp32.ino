#include "MyConfig.h"
#include "WC_Task.h"




void setup(){
   Serial.begin(115200);
   delay(1000);


    tasksStart();



}


void loop(){

   vTaskDelay(5000);

 
}