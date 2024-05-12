#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <linux/msdos_fs.h>
#include <stdint.h>

#define FALSE 0
#define TRUE 1

#define SECTORSIZE 512   // bytes
#define CLUSTERSIZE  1024  // bytes
// Define FAT32 constants
#define BYTES_PER_SECTOR 512
#define ROOTDIR_CLUSTER 2
#define DIR_ENTRY_SIZE 32

// Struct for directory entry
typedef struct {
    char name[9];
    char ext[4];
    uint32_t size;
} DirectoryEntry;

int readsector (int fd, unsigned char *buf, unsigned int snum);
int writesector (int fd, unsigned char *buf, unsigned int snum);
void parse_directory_entry(char *entry, DirectoryEntry *dir_entry);
void list_directory_entries(const char *disk_image);
void display_ascii_content(const char *disk_image, const char *filename);
void display_binary_content(const char *disk_image, const char *filename);
void create_file(const char *disk_image, const char *filename);
void delete_file(const char *disk_image, const char *filename);
void write_data(const char *disk_image, const char *filename, int offset, int n, unsigned char data);
void display_help();

int main(int argc, char *argv[]) {
    // Check if correct number of arguments provided
    if (argc < 3) {
        fprintf(stderr, "Insufficient arguments.\n");
        fprintf(stderr, "Usage: %s DISKIMAGE OPTION FILENAME\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *disk_image = argv[1];
    char *option = argv[2];
    char *filename = argc > 3 ? argv[3] : NULL;

    if (strcmp(option, "-l") == 0) {
        // List directory entries
        list_directory_entries(disk_image);
    } else if (strcmp(option, "-r") == 0) {
        // Display file content
        if (argc < 5) {
            fprintf(stderr, "Insufficient arguments for -r option.\n");
            exit(EXIT_FAILURE);
        }
        if (strcmp(argv[3], "-a") == 0) {
            display_ascii_content(disk_image, filename);
        } else if (strcmp(argv[3], "-b") == 0) {
            display_binary_content(disk_image, filename);
        } else {
            fprintf(stderr, "Invalid option.\n");
            exit(EXIT_FAILURE);
        }
    } else if (strcmp(option, "-c") == 0) {
        // Create file
        create_file(disk_image, filename);
    } else if (strcmp(option, "-d") == 0) {
        // Delete file
        delete_file(disk_image, filename);
    } else if (strcmp(option, "-w") == 0) {
        // Write data
        if (argc != 7) {
            fprintf(stderr, "Insufficient arguments for -w option.\n");
            exit(EXIT_FAILURE);
        }
        int offset = atoi(argv[4]);
        int n = atoi(argv[5]);
        unsigned char data = atoi(argv[6]);
        write_data(disk_image, filename, offset, n, data);
    } else if (strcmp(option, "-h") == 0) {
        // Display help
        display_help();
    } else {
        fprintf(stderr, "Invalid option.\n");
        fprintf(stderr, "Usage: %s DISKIMAGE OPTION FILENAME\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    return 0;
}

int readsector (int fd, unsigned char *buf, unsigned int snum)
{
    off_t offset;
    int n;
    offset = snum * SECTORSIZE;
    lseek (fd, offset, SEEK_SET);
    n  = read (fd, buf, SECTORSIZE);
    if (n == SECTORSIZE)
        return (0);
    else
        return (-1);
}

int writesector (int fd, unsigned char *buf, unsigned int snum)
{
    off_t offset;
    int n;
    offset = snum * SECTORSIZE;
    lseek (fd, offset, SEEK_SET);
    n  = write (fd, buf, SECTORSIZE);
    fsync (fd);
    if (n == SECTORSIZE)
        return (0);
    else
        return (-1);
}

// Function to parse directory entry
void parse_directory_entry(char *entry, DirectoryEntry *dir_entry) {
    strncpy(dir_entry->name, entry, 8);
    dir_entry->name[8] = '\0';
    strncpy(dir_entry->ext, entry + 8, 3);
    dir_entry->ext[3] = '\0';
    dir_entry->size = *((uint32_t *)(entry + 28));
}

void display_ascii_content(const char *disk_image, const char *filename) {
    // Open the disk image file
    int fd = open(disk_image, O_RDONLY);
    if (fd == -1) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    // Find the file in the directory
    off_t offset = BYTES_PER_SECTOR * CLUSTERSIZE * (ROOTDIR_CLUSTER - 2); // Start of root directory
    DirectoryEntry dir_entry;
    char entry[DIR_ENTRY_SIZE];
    int found = FALSE;
    while (read(fd, entry, DIR_ENTRY_SIZE) == DIR_ENTRY_SIZE) {
        // Extract the filename and extension from the directory entry
        char entry_name[9];
        memcpy(entry_name, entry, 8);
        entry_name[8] = '\0';
        // Compare the filenames
        if (strcmp(entry_name, filename) == 0) {
            parse_directory_entry(entry, &dir_entry);
            found = TRUE;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "File not found.\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Seek to the offset of the file content
    offset = BYTES_PER_SECTOR * CLUSTERSIZE * (dir_entry.size - 2); // Start of file content
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Error seeking to file content");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Read and display the content of the file as ASCII text
    char buf[dir_entry.size];
    if (read(fd, buf, dir_entry.size) != dir_entry.size) {
        perror("Error reading file content");
        close(fd);
        exit(EXIT_FAILURE);
    }
    printf("%s", buf);

    // Close the disk image file
    close(fd);
}

void display_binary_content(const char *disk_image, const char *filename) {
    printf("now printing binary content\n");

    // Open the disk image file
    int fd = open(disk_image, O_RDONLY);
    if (fd == -1) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    // Find the file in the directory
    off_t offset = BYTES_PER_SECTOR * CLUSTERSIZE * (ROOTDIR_CLUSTER - 2); // Start of root directory
    DirectoryEntry dir_entry;
    char entry[DIR_ENTRY_SIZE];
    int found = FALSE;
    while (read(fd, entry, DIR_ENTRY_SIZE) == DIR_ENTRY_SIZE) {
        // Extract the filename and extension from the directory entry
        /*char entry_name[9];
        memcpy(entry_name, entry, 8);
        entry_name[8] = '\0';
        // Trim trailing spaces from the filename
        int i;
        for (i = 7; i >= 0; i--) {
            if (entry_name[i] != ' ')
                break;
        }
        entry_name[i + 1] = '\0';

        // Compare the filenames
        if (strcmp(entry_name, filename) == 0) {
            parse_directory_entry(entry, &dir_entry);
            found = TRUE;
            break;
        }*/
        if ((unsigned char)entry[0] != 0xE5 && (unsigned char)entry[11] != 0x0F) { // Not a special entry
            parse_directory_entry(entry, &dir_entry);
            //printf("%s \n", dir_entry.name);
            if (strcmp(dir_entry.name, filename) == 0) {
                parse_directory_entry(entry, &dir_entry);
                found = TRUE;
                break;
            }
        }

        /*while (read(fd, entry, DIR_ENTRY_SIZE) == DIR_ENTRY_SIZE) {
            if (entry[0] == 0x00) // Unused entry
                break;
            if ((unsigned char)entry[0] != 0xE5 && (unsigned char)entry[11] != 0x0F) { // Not a special entry
                parse_directory_entry(entry, &dir_entry);
                printf("%s %s %u\n", dir_entry.name, dir_entry.ext, dir_entry.size);
            }
        }*/
    }

    if (!found) {
        fprintf(stderr, "File not found.\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Seek to the offset of the file content
    offset = BYTES_PER_SECTOR * CLUSTERSIZE * (dir_entry.size - 2); // Start of file content
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Error seeking to file content");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Read and display the content of the file in binary form
    unsigned char buf[dir_entry.size];
    if (read(fd, buf, dir_entry.size) != dir_entry.size) {
        perror("Error reading file content");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Display the content in hexadecimal form
    printf("Hexadecimal representation of file content:\n");
    for (int i = 0; i < dir_entry.size; i += 16) {
        printf("%08x: ", i);
        for (int j = 0; j < 16 && i + j < dir_entry.size; j++) {
            printf("%02x ", buf[i + j]);
        }
        printf("\n");
    }

    // Close the disk image file
    close(fd);
}

void create_file(const char *disk_image, const char *filename) {
    // Open the disk image file
    int fd = open(disk_image, O_RDWR);
    if (fd == -1) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    // Find an empty directory entry
    off_t offset = BYTES_PER_SECTOR * CLUSTERSIZE * (ROOTDIR_CLUSTER - 2); // Start of root directory
    DirectoryEntry dir_entry;
    char entry[DIR_ENTRY_SIZE];
    int found = FALSE;
    while (read(fd, entry, DIR_ENTRY_SIZE) == DIR_ENTRY_SIZE) {
        if (entry[0] == 0x00 || entry[0] == 0xE5 || (entry[11] & 0x08) == 0) { // Unused or deleted entry
            memset(&dir_entry, 0, sizeof(DirectoryEntry));

            // Set filename and extension
            strncpy(dir_entry.name, filename, 8);
            dir_entry.name[8] = '\0';

            // Write the directory entry
            lseek(fd, -DIR_ENTRY_SIZE, SEEK_CUR);
            write(fd, &dir_entry, sizeof(DirectoryEntry));
            found = TRUE;
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "No free directory entry found.\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Close the disk image file
    close(fd);
}


void delete_file(const char *disk_image, const char *filename) {
    // Open the disk image file
    int fd = open(disk_image, O_RDWR);
    if (fd == -1) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    // Find the directory entry corresponding to the file
    off_t offset = BYTES_PER_SECTOR * (ROOTDIR_CLUSTER + 31); // 31 sectors reserved
    DirectoryEntry dir_entry;
    char entry[DIR_ENTRY_SIZE];
    int found = FALSE;
    while (read(fd, entry, DIR_ENTRY_SIZE) == DIR_ENTRY_SIZE) {
        if (memcmp(entry, filename, strlen(filename)) == 0) {
            // Mark the entry as deleted
            entry[0] = 0xE5;
            lseek(fd, -DIR_ENTRY_SIZE, SEEK_CUR);
            write(fd, entry, DIR_ENTRY_SIZE);
            found = TRUE;
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "File not found.\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Close the disk image file
    close(fd);
}


void write_data(const char *disk_image, const char *filename, int offset, int n, unsigned char data) {
    // Open the disk image file
    int fd = open(disk_image, O_RDWR);
    if (fd == -1) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    // Find the directory entry corresponding to the file
    off_t dir_offset = BYTES_PER_SECTOR * (ROOTDIR_CLUSTER + 31); // 31 sectors reserved
    DirectoryEntry dir_entry;
    char entry[DIR_ENTRY_SIZE];
    int found = FALSE;
    while (read(fd, entry, DIR_ENTRY_SIZE) == DIR_ENTRY_SIZE) {
        if (memcmp(entry, filename, strlen(filename)) == 0) {
            parse_directory_entry(entry, &dir_entry);
            found = TRUE;
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "File not found.\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Seek to the offset within the file
    off_t file_offset = (ROOTDIR_CLUSTER + 31) * BYTES_PER_SECTOR + dir_entry.size * DIR_ENTRY_SIZE + offset;
    if (lseek(fd, file_offset, SEEK_SET) == -1) {
        perror("Error seeking to file content");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Write data
    unsigned char buf[n];
    memset(buf, data, n);
    if (write(fd, buf, n) != n) {
        perror("Error writing data to file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Update file size in directory entry
    dir_entry.size += n;
    lseek(fd, dir_offset + dir_entry.size * DIR_ENTRY_SIZE, SEEK_SET);
    write(fd, &dir_entry, sizeof(DirectoryEntry));

    // Close the disk image file
    close(fd);
}

void list_directory_entries(const char *disk_image) {
    // Open the disk image file
    int fd = open(disk_image, O_RDONLY);
    if (fd == -1) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    // Move to the root directory cluster
    off_t offset = BYTES_PER_SECTOR * CLUSTERSIZE * (ROOTDIR_CLUSTER - 2); // Start of root directory
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Error seeking to root directory");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Read and parse directory entries
    DirectoryEntry dir_entry;
    char entry[DIR_ENTRY_SIZE];
    while (read(fd, entry, DIR_ENTRY_SIZE) == DIR_ENTRY_SIZE) {
        if (entry[0] == 0x00) // Unused entry
            break;
        if ((unsigned char)entry[0] != 0xE5 && (unsigned char)entry[11] != 0x0F) { // Not a special entry
            parse_directory_entry(entry, &dir_entry);
            printf("%s %s %u\n", dir_entry.name, dir_entry.ext, dir_entry.size);
        }
    }

    // Close the disk image file
    close(fd);
}

void display_help() {
    printf("Usage: fatmod DISKIMAGE OPTION FILENAME\n");
    printf("Options:\n");
    printf("  -l             List directory entries\n");
    printf("  -r -a FILENAME Display file content in ASCII form\n");
    printf("  -r -b FILENAME Display file content in binary form\n");
    printf("  -c FILENAME    Create a new file\n");
    printf("  -d FILENAME    Delete a file\n");
    printf("  -w FILENAME OFFSET N DATA\n");
    printf("                 Write data to a file\n");
    printf("  -h             Display help\n");
}