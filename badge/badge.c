#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdlib.h>
#include <string.h>

#define RESET       "\x1B[0m"
#define BOLD        "\x1B[1m"
#define ITALIC      "\x1B[3m"
#define CYAN        "\x1B[36m"
#define YELLOW      "\x1B[33m"
#define GREEN       "\x1B[32m"
#define RED         "\x1B[31m"
#define MAGENTA     "\x1B[35m"

void displaySysctlInfo(const char *name, const char *description) {
    char sysctlValue[256];
    size_t sysctlValueSize = sizeof(sysctlValue);

    if (sysctlbyname(name, sysctlValue, &sysctlValueSize, NULL, 0) == 0) {
        printf("  %s: %s\n", description, sysctlValue);
    } else {
        perror(name);
    }
}

int main() {
    struct utsname unameData;

    if (uname(&unameData) == -1) {
        perror("uname");
        return 1;
    }

    // Print first line just a space
    printf("\n");

    // Define ANSI text colors, bold, and italic formatting
    printf(BOLD YELLOW ITALIC "  System Information: \n", RESET);
    printf(BOLD CYAN "  --------------------------------------------------------" "\n" RESET);

    // Display User information (bold)
    printf(BOLD "  User: %s\n" RESET, getlogin());

    // Display OS information (bold)
    printf(BOLD "  OS: %s %s\n" RESET, unameData.sysname, unameData.release);

    // Display Architecture information (bold)
    printf(BOLD "  Architecture: %s\n" RESET, unameData.machine);

    // Get CPU information
    char cpuModel[256];
    size_t cpuModelSize = sizeof(cpuModel);
    if (sysctlbyname("hw.model", cpuModel, &cpuModelSize, NULL, 0) == 0) {
        printf("  CPU Model: %s\n", cpuModel);
    } else {
        perror("sysctl hw.model");
    }

    int cpuCores;
    size_t cpuCoresSize = sizeof(cpuCores);
    if (sysctlbyname("hw.ncpu", &cpuCores, &cpuCoresSize, NULL, 0) == 0) {
        printf("  CPU Cores: %d\n", cpuCores);
    } else {
        perror("sysctl hw.ncpu");
    }

    // Get memory information in megabytes
    long long totalMemory;
    size_t totalMemorySize = sizeof(totalMemory);
    if (sysctlbyname("hw.physmem", &totalMemory, &totalMemorySize, NULL, 0) == 0) {
        printf("  Total Physical Memory: %lld megabytes\n", totalMemory / 1048576);
    } else {
        perror("sysctl hw.physmem");
    }

    // Extract and display FreeBSD uptime using the 'uptime' command
    FILE *uptimePipe = popen("uptime", "r");
    if (uptimePipe != NULL) {
        char uptimeInfo[256];
        if (fgets(uptimeInfo, sizeof(uptimeInfo), uptimePipe) != NULL) {
            char *uptimeString = strstr(uptimeInfo, "up ");
            if (uptimeString != NULL) {
                char *end = strchr(uptimeString, ',');
                if (end != NULL) {
                    *end = '\0';
                }
                printf("  Uptime: %s\n", uptimeString + 3);
            }
        }
        pclose(uptimePipe);
    }

    printf(BOLD CYAN "  --------------------------------------------------------" "\n" RESET);
    printf("\n"); // Add a new line after all information

    return 0;
}
