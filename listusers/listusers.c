#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>

void print_header() {
    printf("%-15s %-10s %-20s %-30s\n", "User", "ID", "Group", "Home Directory");
    printf("%-15s %-10s %-20s %-30s\n", "----", "----", "-----", "--------------");
}

int main() {
    struct passwd *pw;
    struct group *gr;
    
    print_header();

    // Open the passwd database
    setpwent();

    // Iterate over all user entries
    while ((pw = getpwent()) != NULL) {
        // Exclude users with UID less than 1000 and user "nobody"
        if (pw->pw_uid >= 1000 && strcmp(pw->pw_name, "nobody") != 0) {
            // Check if the home directory starts with /home or /tank
            if (strncmp(pw->pw_dir, "/home/", 6) == 0 || strncmp(pw->pw_dir, "/tank/", 6) == 0) {
                // Get group name
                gr = getgrgid(pw->pw_gid);
                
                // Print the user details
                printf("%-15s %-10d %-20s %-30s\n", pw->pw_name, pw->pw_uid, (gr != NULL ? gr->gr_name : "Unknown"), pw->pw_dir);
            }
        }
    }

    // Close the passwd database
    endpwent();

    return 0;
}
