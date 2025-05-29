#include "mbed.h"
#include "string.h"
#include "L2_FSMmain.h"
#include "L3_FSMmain.h"

//serial port interface
Serial pc(USBTX, USBRX);

//GLOBAL variables (DO NOT TOUCH!) ------------------------------------------

//source/destination ID
uint8_t input_thisId=1;
uint8_t input_destId=0;

//FSM operation implementation ------------------------------------------------
int main(void){

    //initialization
    pc.printf("\n\n------------------ Pop-in Protocol Stack starts! --------------------------\n");
    
    //source ID setting only (destination will be auto-detected)
    pc.printf(":: ID for this node : ");
    pc.scanf("%d", &input_thisId);
    pc.getc();
    
    pc.printf("You entered ID: %d\n", input_thisId);

    // For Pop-in protocol, we don't need to set destination manually
    // Admin nodes (1-3) will broadcast
    // User nodes (4+) will auto-detect booths
    if (input_thisId >= 1 && input_thisId <= 3) {
        pc.printf("Starting as ADMIN (Booth %d)\n", input_thisId);
        input_destId = 255;  // Broadcast for admins
    } else {
        pc.printf("Starting as USER (ID %d)\n", input_thisId);
        input_destId = 255;  // Start with broadcast to find booths
    }
    
    pc.printf("Initializing protocol stack...\n");
    
    //initialize lower layer stacks
    L2_initFSM(input_thisId);
    L3_initFSM(input_thisId);  // Only pass thisId to L3
    
    pc.printf("Starting main loop...\n");
    
    while(1)
    {
        L2_FSMrun();
        L3_FSMrun();
    }
}