#include "L3_FSMevent.h"
#include "L3_msg.h"
#include "L3_types.h"
#include "L3_timer.h"
#include "L3_LLinterface.h"
#include "protocol_parameters.h"
#include "mbed.h"

// FSM States for Pop-in Protocol
#define L3STATE_SCANNING        0
#define L3STATE_CONNECTED       1
#define L3STATE_WAITING         2
#define L3STATE_IN_USE          3

// State variables
static uint8_t main_state = L3STATE_SCANNING;
static uint8_t prev_state = main_state;

// User and Booth Management
static uint8_t myId;
static uint8_t currentBoothId = 0;
static uint8_t isAdmin = 0;  // 1: admin, 0: user
static Booth_t myBooth;  // For admin use
static uint32_t scanningTimer = 0;
static uint8_t waitingNumber = 0;
static uint8_t registeredCount = 0;  // Track total registered users

// Message buffers
static uint8_t txBuffer[L3_MAXDATASIZE];
static uint8_t rxBuffer[L3_MAXDATASIZE];

// Serial port interface
static Serial pc(USBTX, USBRX);

// Function prototypes
static void sendMessage(uint8_t msgType, uint8_t* data, uint8_t dataLen, uint8_t destId);
static void handleConnectRequest(uint8_t srcId);
static void handleBoothInfo(uint8_t* data, uint8_t size);
static void handleRegisterResponse(uint8_t* data);
static void handleQueueInfo(uint8_t* data);
static uint8_t checkUserInList(User_t* list, uint8_t listSize, uint8_t userId);
static uint8_t checkUserInRegisteredList(uint8_t userId);
static uint8_t addUserToList(User_t* list, uint8_t* listSize, uint8_t userId);
static void removeUserFromList(User_t* list, uint8_t* count, uint8_t userId);
static void removeUserFromActiveList(uint8_t userId);
static void displayBoothInfo(void);
static void scanForBooths(void);
static void L3service_processKeyboardInput(void);

// Initialize FSM
void L3_initFSM(uint8_t id) {
    myId = id;
    isAdmin = (id >= ADMIN_ID_START && id <= ADMIN_ID_END) ? 1 : 0;
    
    // Initialize booth if admin
    if (isAdmin) {
        myBooth.boothId = id;
        myBooth.capacity = MAX_BOOTH_CAPACITY;
        myBooth.currentCount = 0;
        myBooth.waitingCount = 0;
        sprintf(myBooth.description, "Booth %d - Pop-up Store Experience", id);
        
        // Initialize registered user IDs array
        for (uint8_t i = 0; i < MAX_USERS; i++) {
            myBooth.registeredUserIds[i] = 0;
        }
        
        pc.printf("\n=== ADMIN MODE: Booth %d ===\n", id);
        pc.printf("Booth Description: %s\n", myBooth.description);
        pc.printf("Max Capacity: %d\n", myBooth.capacity);
        
        main_state = L3STATE_IN_USE;  // Admin always in USE state
        
        // Send initial broadcast
        uint8_t announceData[10];
        uint8_t msgSize = L3_msg_encodeBoothAnnounce(announceData, myBooth.boothId, 
                                                      myBooth.currentCount, myBooth.capacity, 
                                                      myBooth.waitingCount);
        
        pc.printf("Booth initialized. Waiting for users...\n");
        pc.printf("Sending initial broadcast...\n");
        L3_LLI_dataReqFunc(announceData, msgSize, BROADCAST_ID);
        
    } else {
        pc.printf("\n=== USER MODE: ID %d ===\n", id);
        pc.printf("Starting booth scanning...\n");
        pc.printf("Press 'e' to exit booth when inside\n");
        main_state = L3STATE_SCANNING;
        
        // Set up keyboard interrupt for exit command
        pc.attach(&L3service_processKeyboardInput, Serial::RxIrq);
        
        // Send initial scan request immediately
        pc.printf("Sending initial scan requests to booths 1-3...\n");
        for (uint8_t i = ADMIN_ID_START; i <= ADMIN_ID_END; i++) {
            uint8_t scanMsg[1] = {MSG_TYPE_BOOTH_SCAN};
            L3_LLI_dataReqFunc(scanMsg, 1, i);
        }
    }
    
    L3_event_clearAllEventFlag();
}

// Main FSM Run
void L3_FSMrun(void) {
    if (prev_state != main_state) {
        debug_if(DBGMSG_L3, "[L3] State transition from %i to %i\n", prev_state, main_state);
        prev_state = main_state;
    }
    
    // Admin booth announcement (periodic)
    if (isAdmin) {
        scanningTimer++;
        if (scanningTimer > 1000000) {  // Much longer period
            scanningTimer = 0;
            uint8_t announceData[10];
            uint8_t msgSize = L3_msg_encodeBoothAnnounce(announceData, myBooth.boothId,
                                                          myBooth.currentCount, myBooth.capacity,
                                                          myBooth.waitingCount);
            
            pc.printf("\n[Admin] Broadcasting booth info (Users: %d/%d)...\n", 
                     myBooth.currentCount, myBooth.capacity);
            L3_LLI_dataReqFunc(announceData, msgSize, BROADCAST_ID);
        }
    }
    
    // Handle received messages
    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        uint8_t* dataPtr = L3_LLI_getMsgPtr();
        uint8_t size = L3_LLI_getSize();
        uint8_t srcId = L3_LLI_getSrcId();
        uint8_t msgType = L3_msg_getType(dataPtr);
        
        // Filter out broadcast messages in certain states
        if (msgType == MSG_TYPE_BOOTH_ANNOUNCE && !isAdmin && main_state != L3STATE_SCANNING) {
            L3_event_clearEventFlag(L3_event_msgRcvd);
            return;
        }
        
        // Only show debug for non-periodic messages
        if (msgType != MSG_TYPE_BOOTH_ANNOUNCE || !isAdmin) {
            pc.printf("[DEBUG] Received message type 0x%02X from ID %d (size: %d)\n", msgType, srcId, size);
        }
        
        switch (msgType) {
            case MSG_TYPE_BOOTH_SCAN:
                if (isAdmin) {
                    pc.printf("Received scan request from User %d\n", srcId);
                    // Respond with booth info
                    uint8_t announceData[10];
                    uint8_t msgSize = L3_msg_encodeBoothAnnounce(announceData, myBooth.boothId,
                                                                  myBooth.currentCount, myBooth.capacity,
                                                                  myBooth.waitingCount);
                    L3_LLI_dataReqFunc(announceData, msgSize, srcId);
                }
                break;
                
            case MSG_TYPE_EXIT_REQUEST:
                if (isAdmin) {
                    pc.printf("\n[Admin] Received exit request from User %d\n", srcId);
                    
                    // Check if user is actually in the booth
                    if (checkUserInList(myBooth.activeList, myBooth.currentCount, srcId)) {
                        // Remove from active list only (keep in registered list)
                        removeUserFromActiveList(srcId);
                        
                        pc.printf("User %d has exited. Current users: %d/%d\n", 
                                 srcId, myBooth.currentCount, myBooth.capacity);
                        
                        // Send exit confirmation
                        uint8_t exitResp[2];
                        exitResp[0] = MSG_TYPE_EXIT_RESPONSE;
                        exitResp[1] = 1;  // Success
                        L3_LLI_dataReqFunc(exitResp, 2, srcId);
                        
                        // TODO: Check waiting queue and admit next user
                    } else {
                        pc.printf("[Admin] Warning: User %d not in active list\n", srcId);
                    }
                }
                break;
                
            case MSG_TYPE_EXIT_RESPONSE:
                if (!isAdmin) {
                    pc.printf("\n[User] Exit confirmed by booth.\n");
                }
                break;
                
            case MSG_TYPE_BOOTH_ANNOUNCE:
                if (!isAdmin && main_state == L3STATE_SCANNING) {
                    uint8_t* msgData = L3_msg_getData(dataPtr);
                    uint8_t boothId = msgData[0];
                    uint8_t currentUsers = msgData[1];
                    uint8_t capacity = msgData[2];
                    uint8_t waitingUsers = msgData[3];
                    
                    pc.printf("\nBooth %d detected! (Users: %d/%d, Waiting: %d)\n", 
                             boothId, currentUsers, capacity, waitingUsers);
                    
                    // Auto-connect to first detected booth
                    currentBoothId = boothId;
                    pc.printf("Connecting to Booth %d...\n", currentBoothId);
                    
                    sendMessage(MSG_TYPE_CONNECT_REQUEST, NULL, 0, currentBoothId);
                    main_state = L3STATE_CONNECTED;
                }
                break;
                
            case MSG_TYPE_CONNECT_REQUEST:
                if (isAdmin) {
                    pc.printf("\n[Admin] Received connect request from User %d\n", srcId);
                    handleConnectRequest(srcId);
                }
                break;
                
            case MSG_TYPE_BOOTH_INFO:
                if (!isAdmin && main_state == L3STATE_CONNECTED) {
                    pc.printf("\n[User] Received booth info from Booth %d\n", srcId);
                    handleBoothInfo(L3_msg_getData(dataPtr), size - 1);
                }
                break;
                
            case MSG_TYPE_REGISTER_REQUEST:
                if (isAdmin) {
                    pc.printf("\n[Admin] Received registration request from User %d\n", srcId);
                    pc.printf("[DEBUG] Checking registration status...\n");
                    
                    uint8_t response[3];
                    uint8_t msgSize;
                    
                    // Check if user is already in registered list (C2 condition)
                    uint8_t isRegistered = checkUserInRegisteredList(srcId);
                    
                    if (isRegistered) {
                        // IC2: Already registered - reject
                        msgSize = L3_msg_encodeRegisterResponse(response, 0, REGISTER_REASON_ALREADY_USED);
                        pc.printf("User %d registration rejected - already experienced this booth!\n", srcId);
                    } else if (myBooth.currentCount >= myBooth.capacity) {
                        // C1: Capacity full - add to waiting
                        if (!checkUserInList(myBooth.waitingQueue, myBooth.waitingCount, srcId)) {
                            addUserToList(myBooth.waitingQueue, &myBooth.waitingCount, srcId);
                            myBooth.waitingQueue[myBooth.waitingCount - 1].waitingNumber = ++waitingNumber;
                        }
                        msgSize = L3_msg_encodeRegisterResponse(response, 0, REGISTER_REASON_FULL_WAITING);
                        pc.printf("User %d added to waiting queue (number: %d)\n", srcId, waitingNumber);
                        
                        // Send queue info
                        uint8_t queueData[3];
                        uint8_t queueMsgSize = L3_msg_encodeQueueInfo(queueData, waitingNumber, myBooth.waitingCount);
                        L3_LLI_dataReqFunc(queueData, queueMsgSize, srcId);
                    } else {
                        // IC1 & C2: Success - add to both lists
                        // Add to permanent registered list
                        myBooth.registeredUserIds[registeredCount] = srcId;
                        myBooth.registeredList[registeredCount].userId = srcId;
                        myBooth.registeredList[registeredCount].isRegistered = 1;
                        registeredCount++;
                        
                        // Add to active list
                        addUserToList(myBooth.activeList, &myBooth.currentCount, srcId);
                        
                        msgSize = L3_msg_encodeRegisterResponse(response, 1, REGISTER_REASON_SUCCESS);
                        pc.printf("User %d successfully registered and entered booth\n", srcId);
                        pc.printf("Current booth status: %d/%d users (Total registered: %d)\n", 
                                 myBooth.currentCount, myBooth.capacity, registeredCount);
                    }
                    
                    L3_LLI_dataReqFunc(response, msgSize, srcId);
                }
                break;
                
            case MSG_TYPE_REGISTER_RESPONSE:
                if (!isAdmin) {
                    handleRegisterResponse(L3_msg_getData(dataPtr));
                }
                break;
                
            case MSG_TYPE_QUEUE_INFO:
                if (!isAdmin) {
                    handleQueueInfo(L3_msg_getData(dataPtr));
                }
                break;
        }
        
        L3_event_clearEventFlag(L3_event_msgRcvd);
    }
    
    // User-specific state machine
    if (!isAdmin) {
        static uint32_t userScanTimer = 0;
        
        switch (main_state) {
            case L3STATE_SCANNING:
                // Periodic scanning - send scan request
                userScanTimer++;
                if (userScanTimer > 500000) {
                    userScanTimer = 0;
                    pc.printf("\n[User] Scanning for booths...\n");
                    // Send scan request to all possible booth IDs
                    for (uint8_t i = ADMIN_ID_START; i <= ADMIN_ID_END; i++) {
                        sendMessage(MSG_TYPE_BOOTH_SCAN, NULL, 0, i);
                    }
                }
                break;
                
            case L3STATE_CONNECTED:
                // Waiting for booth info and user decision
                break;
                
            case L3STATE_WAITING:
                // In waiting queue
                pc.printf(".");  // Show waiting status
                break;
                
            case L3STATE_IN_USE:
                // Using booth - show status periodically
                static uint32_t statusTimer = 0;
                statusTimer++;
                if (statusTimer > 2000000) {
                    statusTimer = 0;
                    pc.printf(".");  // Show user is still in booth
                }
                break;
        }
    }
}

// Helper Functions
static void sendMessage(uint8_t msgType, uint8_t* data, uint8_t dataLen, uint8_t destId) {
    txBuffer[0] = msgType;
    if (data && dataLen > 0) {
        memcpy(txBuffer + 1, data, dataLen);
    }
    
    // Only show debug for non-periodic messages
    if (msgType != MSG_TYPE_BOOTH_ANNOUNCE) {
        pc.printf("[DEBUG] Sending message type 0x%02X to ID %d (size: %d)\n", msgType, destId, dataLen + 1);
    }
    
    L3_LLI_dataReqFunc(txBuffer, dataLen + 1, destId);
}

static void handleConnectRequest(uint8_t srcId) {
    // Send booth info
    uint8_t infoData[60];
    uint8_t msgSize = L3_msg_encodeBoothInfo(infoData, myBooth.currentCount, myBooth.capacity,
                                              myBooth.waitingCount, myBooth.description);
    
    pc.printf("[Admin] Sending booth info to user %d\n", srcId);
    L3_LLI_dataReqFunc(infoData, msgSize, srcId);
}

static void handleBoothInfo(uint8_t* data, uint8_t size) {
    uint8_t currentUsers = data[0];
    uint8_t capacity = data[1];
    uint8_t waitingUsers = data[2];
    char* description = (char*)(data + 3);
    
    pc.printf("\n=== BOOTH INFORMATION ===\n");
    pc.printf("Description: %s\n", description);
    pc.printf("Current users: %d/%d\n", currentUsers, capacity);
    pc.printf("Waiting users: %d\n", waitingUsers);
    pc.printf("\nWould you like to experience this booth? (y/n): ");
    
    // Auto-respond YES for demo
    pc.printf("y (auto-response)\n");
    
    // Send registration request immediately
    pc.printf("Sending registration request...\n");
    sendMessage(MSG_TYPE_REGISTER_REQUEST, NULL, 0, currentBoothId);
}

static void handleRegisterResponse(uint8_t* data) {
    uint8_t success = data[0];
    uint8_t reason = data[1];
    
    if (success) {
        pc.printf("\nRegistration successful! Welcome to the booth!\n");
        pc.printf("Your experience session has started.\n");
        pc.printf("Press 'e' to exit the booth.\n");
        main_state = L3STATE_IN_USE;
    } else {
        if (reason == REGISTER_REASON_ALREADY_USED) {
            pc.printf("\nRegistration failed: You have already experienced this booth.\n");
            pc.printf("Each user can only experience a booth once.\n");
            pc.printf("Returning to scanning mode...\n");
            main_state = L3STATE_SCANNING;
            currentBoothId = 0;
        } else if (reason == REGISTER_REASON_FULL_WAITING) {
            pc.printf("\n⏳ Booth is full. You have been added to the waiting queue.\n");
            main_state = L3STATE_WAITING;
        }
    }
}

static void handleQueueInfo(uint8_t* data) {
    uint8_t queueNumber = data[0];
    uint8_t totalWaiting = data[1];
    
    pc.printf("Your waiting number: %d (Total waiting: %d)\n", queueNumber, totalWaiting);
}

static uint8_t checkUserInList(User_t* list, uint8_t listSize, uint8_t userId) {
    for (uint8_t i = 0; i < listSize; i++) {
        if (list[i].userId == userId) {
            return 1;
        }
    }
    return 0;
}

static uint8_t addUserToList(User_t* list, uint8_t* count, uint8_t userId) {
    // First check if already in list
    for (uint8_t i = 0; i < *count; i++) {
        if (list[i].userId == userId) {
            return 1;  // Already in list
        }
    }
    
    // Add new user
    if (*count < MAX_USERS) {
        list[*count].userId = userId;
        list[*count].isActive = 1;
        (*count)++;
        return 1;
    }
    return 0;
}

static void removeUserFromList(User_t* list, uint8_t* count, uint8_t userId) {
    for (uint8_t i = 0; i < *count; i++) {
        if (list[i].userId == userId) {
            // Shift remaining elements
            for (uint8_t j = i; j < *count - 1; j++) {
                list[j] = list[j + 1];
            }
            (*count)--;
            break;
        }
    }
}

// Check if user is in registered list (permanent record)
static uint8_t checkUserInRegisteredList(uint8_t userId) {
    for (uint8_t i = 0; i < registeredCount; i++) {
        if (myBooth.registeredUserIds[i] == userId) {
            pc.printf("[DEBUG] User %d found in registered list at index %d\n", userId, i);
            return 1;
        }
    }
    pc.printf("[DEBUG] User %d not found in registered list (count: %d)\n", userId, registeredCount);
    return 0;
}

// Remove user from active list only
static void removeUserFromActiveList(uint8_t userId) {
    removeUserFromList(myBooth.activeList, &myBooth.currentCount, userId);
}

// Keyboard input handler for user commands
static void L3service_processKeyboardInput(void) {
    char c = pc.getc();
    
    if (c == 'e' || c == 'E') {
        if (!isAdmin && main_state == L3STATE_IN_USE) {
            pc.printf("\nExiting booth...\n");
            sendMessage(MSG_TYPE_EXIT_REQUEST, NULL, 0, currentBoothId);
            
            // Don't wait for response, immediately return to scanning
            pc.printf("Exited the booth.\n");
            pc.printf("Returning to scanning mode...\n");
            main_state = L3STATE_SCANNING;
            currentBoothId = 0;
        }
    }
}