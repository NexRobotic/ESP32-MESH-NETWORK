/*Mesh V 1.0

This code serves as a basic framework for creating an ESP-NOW mesh network where nodes can communicate through announcements and messages, 
allowing for easy scalability and interaction among multiple ESP32 devices.

Usage
Upload this code to multiple ESP32 devices to establish communication.
Open the serial monitor to view announcements and messages.
To send a message, type 'h' to list known nodes, select one by number, and a message will be sent to the selected node.

TO DO 

I need to develop a message structure like
                        {sender addr + reciever addr + message + TTL(Time To live)}
so that i can announce this message into the network and it will hopefully went to its owner. 
 
*/


#include <esp_now.h>
#include <WiFi.h>

// Define a static name for this ESP32 node
const char staticName[] = "Delhi";

// Structure to send/receive node data (MAC address + Name)
typedef struct struct_message {
  char nodeName[32];   // Static name of the ESP32
  uint8_t macAddr[6];  // MAC address of the ESP32
} struct_message;

// Create a struct_message called announcement for announcements
struct_message announcement;

// Node table to keep track of connected nodes
#define MAX_NODES 20  // Max number of nodes to store
struct_message nodeTable[MAX_NODES];
int nodeCount = 0;

esp_now_peer_info_t peerInfo;
uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Function to send announcements
void sendAnnouncement() {
  // Set the static name
  strcpy(announcement.nodeName, staticName);

  // Get the device MAC address
  WiFi.macAddress(announcement.macAddr);

  // Send the announcement
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&announcement, sizeof(announcement));

  if (result == ESP_OK) {
    Serial.println("A01");  // Announcement sent successfully
  } else {
    Serial.println("A00");  // Failed to send announcement
  }
}

// Function to broadcast node table to all nodes
void broadcastNodeTable() { 
  for (int i = 0; i < nodeCount; i++) {
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&nodeTable[i], sizeof(nodeTable[i]));
    if (result == ESP_OK) {
      Serial.printf("Broadcasting node %d: %s\n", i + 1, nodeTable[i].nodeName);
    } else {
      Serial.println("Failed to broadcast node");
    }
  }
}

// Function to check if the MAC address is all zeros
bool isInvalidMAC(const uint8_t *macAddr) {
  for (int i = 0; i < 6; i++) {
    if (macAddr[i] != 0x00) {
      return false;  // If any byte is non-zero, it's valid
    }
  }
  return true;  // All bytes are zero, so it's invalid
}

// Callback function for when data is received
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  struct_message receivedData;
  memcpy(&receivedData, incomingData, sizeof(receivedData));

  // Check if the received MAC address is valid (non-zero)
  if (isInvalidMAC(receivedData.macAddr)) {
    Serial.println("Received node with invalid MAC address. Ignoring.");
    return;
  }

  // Check if the node name is already in the table
  bool nameDuplicate = false;
  bool macDuplicate = false;
  
  for (int i = 0; i < nodeCount; i++) {
    if (strcmp(nodeTable[i].nodeName, receivedData.nodeName) == 0) {
      nameDuplicate = true;
    }
    if (memcmp(nodeTable[i].macAddr, receivedData.macAddr, 6) == 0) {
      macDuplicate = true;
    }
    if (nameDuplicate || macDuplicate) {
      break;
    }
  }

  // If there is a duplicate name or MAC address, don't add the node
  if (nameDuplicate) {
    Serial.println("Duplicate node name. Ignoring.");
    return;
  }
  
  if (macDuplicate) {
    Serial.println("Duplicate MAC address. Ignoring.");
    return;
  }

  // Add new node to the node table if not already added
  if (nodeCount < MAX_NODES) {
    memcpy(&nodeTable[nodeCount], &receivedData, sizeof(receivedData));
    nodeCount++;
    Serial.println("New node added:");
    Serial.print("Name: ");
    Serial.println(receivedData.nodeName);
    Serial.print("MAC Address: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", receivedData.macAddr[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.println();

    // Re-announce the updated node table to ensure every node is up-to-date
    broadcastNodeTable();
  } else {
    Serial.println("Node table is full. Cannot add more nodes.");
  }
}

// Function to print the list of nodes
void printNodeTable() {
  Serial.println("Node Table:");
  for (int i = 0; i < nodeCount; i++) {
    Serial.print("Node ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(nodeTable[i].nodeName);
  }
}

// Function to send a message to a specific node
void sendMessageToNode(int nodeIndex, const char *message) {
  if (nodeIndex >= 0 && nodeIndex < nodeCount) {
    esp_err_t result = esp_now_send(nodeTable[nodeIndex].macAddr, (uint8_t *)message, strlen(message) + 1);
    if (result == ESP_OK) {
      Serial.println("M01.");  // Message sent successfully
    } else {
      Serial.println("M00");  // Failed to send message
    }
  } else {
    Serial.println("Invalid node index.");
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callback function for data received
  esp_now_register_recv_cb(OnDataRecv);

  // Register callback for send status
  esp_now_register_send_cb([](const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "D1" : "D0");
  });

  // Register broadcast peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Announce the node at startup
  sendAnnouncement();
}

void loop() {
  // Periodically re-announce node
  static unsigned long lastAnnounce = 0;
  if (millis() - lastAnnounce > 10000) {
    sendAnnouncement();
    lastAnnounce = millis();
  }
}

void serialEvent(){
  char input = Serial.read();
    if (input == 'h') {
      // Display the node table when 'h' is pressed
      printNodeTable();
    }
}
