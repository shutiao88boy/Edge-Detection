#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "signal_edge_ui_api.h"

int main() {
    printf("=== Signal Edge UI API Demo ===\n\n");
    
    printf("1. Testing ZD_GetAPIVersion()...\n");
    char *version = ZD_GetAPIVersion();
    printf("   API Version: %s\n\n", version);
    
    printf("2. Testing ZD_ServerCreate()...\n");
    int ret = ZD_ServerCreate("SRP-Jeston", "Localhost");
    if (ret == 0) {
        printf("   Server created successfully!\n\n");
    } else {
        printf("   Server create failed: %s\n\n", ZD_GetLastError());
        return 1;
    }
    
    printf("3. Testing ZD_GetGnssData()...\n");
    char gnssData[1024] = {0};
    char gnssInfo[1024] = {0};
    ret = ZD_GetGnssData(0, gnssData, 1024, gnssInfo);
    if (ret == 0) {
        printf("   GNSS data acquired: %s\n\n", gnssInfo);
    } else {
        printf("   GNSS data acquire failed: %s\n\n", ZD_GetLastError());
    }
    
    printf("4. Testing ZD_GetInsData()...\n");
    char insData[1024] = {0};
    char insInfo[1024] = {0};
    ret = ZD_GetInsData(0, insData, 1024, insInfo);
    if (ret == 0) {
        printf("   INS data acquired: %s\n\n", insInfo);
    } else {
        printf("   INS data acquire failed: %s\n\n", ZD_GetLastError());
    }
    
    printf("5. Testing ZD_GetImageData()...\n");
    char imageData[1024] = {0};
    char imageInfo[1024] = {0};
    ret = ZD_GetImageData(0, imageData, 1024, imageInfo);
    if (ret == 0) {
        printf("   Image data acquired: %s\n\n", imageInfo);
    } else {
        printf("   Image data acquire failed: %s\n\n", ZD_GetLastError());
    }
    
    printf("6. Testing ZD_ServerDestroy()...\n");
    ZD_ServerDestroy();
    printf("   Server destroyed successfully!\n\n");
    
    printf("=== Demo completed successfully! ===\n");
    
    return 0;
}
