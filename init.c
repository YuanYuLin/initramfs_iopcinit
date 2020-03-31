#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/mount.h>
#include <errno.h>
#include <linux/loop.h>

#define BOOTDEV_TAG_OFFSET	0x100000
#define STR_BOOT_DEV		"BOOT_DEV="
#define STR_BOOT_DELAY		"BOOT_DELAY="
#define STR_BOOT_PART		"BOOT_PART="

#define MAX_BLOCK_LEN		128

//#define GLOBAL_TARGET_DIR	"/mnt"
#define GLOBAL_ROOT_DIR		"/newroot"
#define GLOBAL_ROOT_MODULES_DIR	"/newroot/lib/modules"
#define GLOBAL_ROOT_DAO_DIR	"/newroot/lib/iopcdao"
#define GLOBAL_PROC		"/proc"
#define GLOBAL_SYS		"/sys"
#define GLOBAL_DEV		"/dev"

#define LOOP_ROOTFS		"/dev/loop0"
#define DISK_FILE_ROOTFS	"/newroot/rootfs.squashfs" 
#define MEM_FILE_ROOTFS		"/mnt/rootfs.squashfs" 

#define LOOP_KERNEL_MODULE	"/dev/loop1"
#define DISK_FILE_KERNEL_MODULE	"/newroot/kmod.squashfs"
#define MEM_FILE_KERNEL_MODULE	"/mnt/kmod.squashfs"

#define LOOP_DAO		"/dev/loop2"
#define DISK_FILE_DAO		"/newroot/dao.squashfs"
#define MEM_FILE_DAO		"/mnt/dao.squashfs"

#define DISK_FILE_ENV		"/newroot/env.txt"
#define MEM_FILE_ENV		"/mnt/env.txt"

#define MEM_FILE_EXPORT		"/mnt/export.txt"

#define MOUNT_OPTS		NULL

uint8_t g_boot_dev[MAX_BLOCK_LEN] = {0};
uint8_t g_boot_part[MAX_BLOCK_LEN] = {0};
uint8_t g_boot_delay[MAX_BLOCK_LEN] = {0};

static int loopdev_setup_device(const char * file, const char * device) {
    int file_fd = open(file, O_RDWR);
    int device_fd = -1; 

    struct loop_info64 info;

    if(file_fd < 0) {
        fprintf(stderr, "Failed to open backing file (%s).\n", file);
        goto error;
    }

    if((device_fd = open(device, O_RDWR)) < 0) {
        fprintf(stderr, "Failed to open device (%s).\n", device);
        goto error;
    }

    if(ioctl(device_fd, LOOP_SET_FD, file_fd) < 0) {
        fprintf(stderr, "Failed to set fd.\n");
        goto error;
    }

    close(file_fd);
    file_fd = -1; 

    memset(&info, 0, sizeof(struct loop_info64)); /* Is this necessary? */
    info.lo_offset = 0;
    /* info.lo_sizelimit = 0 => max avilable */
    /* info.lo_encrypt_type = 0 => none */

    if(ioctl(device_fd, LOOP_SET_STATUS64, &info)) {
         fprintf(stderr, "Failed to set info.\n");
         goto error;
     }

     close(device_fd);
     device_fd = -1; 

    return 0;

error:
    if(file_fd >= 0) {
        close(file_fd);
    }   
    if(device_fd >= 0) {
        ioctl(device_fd, LOOP_CLR_FD, 0); 
        close(device_fd);
    }   
    return 1;
}

static void setup()
{
    // mount proc
    mount("proc", GLOBAL_PROC, "proc", 0, MOUNT_OPTS);
    // export env
    // mount devtmpfs
    mount("devtmpfs", GLOBAL_DEV, "devtmpfs", 0, MOUNT_OPTS);
    // mount sysfs
    mount("sysfs", GLOBAL_SYS, "sysfs", 0, MOUNT_OPTS);
}

static void copyto(uint8_t *src_file, uint8_t *dst_file)
{
    int src_fd = -1;
    int dst_fd = -1;
    int rd_len = -1;
    int wr_len = -1;
    uint8_t buffer[MAX_BLOCK_LEN];
    src_fd = open(src_file, O_RDONLY);
    if(src_fd <0) {
        printf("NOT found src %s\n", src_file);
        return;
    }

    dst_fd = open(dst_file, O_CREAT | O_WRONLY);
    if(dst_fd <0) {
        printf("NOT found dst %s\n", dst_file);
        return;
    }
    while (1) {
        rd_len = read(src_fd, buffer, MAX_BLOCK_LEN);
	if(rd_len <= 0) break;
        wr_len = write(dst_fd, buffer, rd_len);
	if(wr_len <= 0) break;
    }
    close(src_fd);
    close(dst_fd);
}


#define HEAD_TAG_SIZE	8

struct boot_info_t {
    uint8_t tag[HEAD_TAG_SIZE]; // [IOPC]
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t rsv[54];
}__attribute__((packed)); // MAX size = 64 bytes

static int hdrcmp(uint8_t* dev_name)
{
    int fd = -1;
    uint32_t offset = BOOTDEV_TAG_OFFSET;
    uint8_t magic[] = { '[', '*', 'I', 'O', 'P', 'C', '*', ']'};
    struct boot_info_t boot_info;
    fd = open(dev_name, O_RDONLY, 0644);
    if(fd < 0) {
        printf("open [%s] failed\n", dev_name);
        return 0;
    }
    lseek(fd, offset, SEEK_SET);
    read(fd, &boot_info, 64);
    close(fd);
    if(memcmp(&boot_info.tag[0], &magic[0], HEAD_TAG_SIZE) == 0) {
        return 1;
    }
    printf("[%x, %x, %x, %x, %x, %x, %x, %x]\n", boot_info.tag[0], boot_info.tag[1], boot_info.tag[2], boot_info.tag[3], boot_info.tag[4], boot_info.tag[5], boot_info.tag[6], boot_info.tag[7]);
    printf("[%x, %x, %x, %x, %x, %x, %x, %x]\n", magic[0], magic[1], magic[2], magic[3], magic[4], magic[5], magic[6], magic[7]);
    return 0;
}

static void setenv_to_file(uint8_t *key, uint8_t *val)
{
    //char chunk[MAX_BLOCK_LEN]={0};
    FILE *fp = fopen(MEM_FILE_EXPORT, "aw");
    if(fp == NULL) {
        return;
    }
    fprintf(fp, "%s=%s\n", key, val);
    /*
    while(fgets(chunk, MAX_BLOCK_LEN, fp) != NULL) {
        printf("env : %s\n", chunk);
        //putenv(chunk);
    }
    */
    fclose(fp);
}

static int scan_dev_for_bootdev(uint8_t *boot_dev)
{
    uint32_t str_boot_dev_len = 0;
    uint32_t boot_delay = 1;
    uint8_t isFound = 0;
    boot_delay = strtoul(g_boot_delay, NULL, 10);
    sleep(boot_delay);

    str_boot_dev_len = strlen(g_boot_dev);
    printf("%s, %d\n", g_boot_dev, str_boot_dev_len);
    struct dirent *de;
    DIR* dir = opendir("/sys/block");
    if(dir == NULL) {
        return 0;
    }
    while((de = readdir(dir)) != NULL) {
        if(str_boot_dev_len == 0){
            printf("BOOT_DEV env NOT set\n");
            break;
        }
	if(strncmp(de->d_name, g_boot_dev, str_boot_dev_len) == 0) {
	    sprintf(boot_dev, "/dev/%s", de->d_name);
            printf("%s == %s[%s]\n", de->d_name, g_boot_dev, boot_dev);
	    if(hdrcmp(boot_dev)) {
		setenv_to_file("BOOT_DEV", boot_dev);
	        sprintf(boot_dev, "/dev/%s%s", de->d_name, g_boot_part);
		setenv_to_file("BOOT_PART", boot_dev);
		isFound = 1;
		printf("isFound[%s]\n", boot_dev);
	        break;
            }
	    memset(&boot_dev[0], 0, strlen(boot_dev));
        }
    }

    closedir(dir);
    if(isFound)
        return 1;
    return 0;
}

#define CH_SPACE	0x20
#define CH_RETURN	0xA
static void parse_proc_cmdline()
{
    uint32_t line_len = MAX_BLOCK_LEN * 4;
    uint8_t line[line_len]; 
    int rd_len = -1;
    int env_idx = 0;
    uint8_t env[MAX_BLOCK_LEN] = { 0 };
    int fd = open("/proc/cmdline", O_RDONLY);
    memset(line, 0, line_len);
    rd_len = read(fd, line, line_len); 
    close(fd);
    for(int idx=0;idx<rd_len;idx++) {
        int str_len = 0;
        if( (line[idx] == CH_SPACE) || (line[idx] == CH_RETURN) ) { // SPACE
            str_len = strlen(STR_BOOT_DEV);
            if(memcmp(env, STR_BOOT_DEV, str_len) == 0) {
                int envlen = strlen(env);
                memcpy(&g_boot_dev[0], &env[str_len], (envlen - str_len));
	    }
            str_len = strlen(STR_BOOT_PART);
            if(memcmp(env, STR_BOOT_PART, str_len) == 0) {
                int envlen = strlen(env);
                memcpy(&g_boot_part[0], &env[str_len], (envlen - str_len));
	    }
            str_len = strlen(STR_BOOT_DELAY);
            if(memcmp(env, STR_BOOT_DELAY, str_len) == 0) {
                int envlen = strlen(env);
                memcpy(&g_boot_delay[0], &env[str_len], (envlen - str_len));
	    }
	    //putenv(env);
            env_idx=0;
	    memset(env, 0, MAX_BLOCK_LEN);
	} else {
            env[env_idx++] = line[idx];
        }
    }
    printf("%s, %s, %s\n", g_boot_dev, g_boot_part, g_boot_delay);
}

int main(int argc, char *argv[])
{
    uint8_t boot_dev[MAX_BLOCK_LEN] = {0};
    int result = -1;
    setup();
    parse_proc_cmdline();
    // mount tmpfs
    //mount("tmpfs", GLOBAL_TARGET_DIR, "tmpfs", 0, MOUNT_OPTS);

    printf("Scan for bootdev\n");
    // mount disk to new_root
    if(scan_dev_for_bootdev(&boot_dev[0])) {
        printf("Mounting %s to %s\n", boot_dev, GLOBAL_ROOT_DIR);
        result = mount(boot_dev, GLOBAL_ROOT_DIR, "vfat", 0, MOUNT_OPTS);
        if(result < 0)
            printf("Mount Failed[%s] %s to %s\n", strerror(errno), boot_dev, GLOBAL_ROOT_DIR);

        // copy files to tmpfs
	printf("Copying %s to %s\n", DISK_FILE_ROOTFS, MEM_FILE_ROOTFS);
	copyto(DISK_FILE_ROOTFS, MEM_FILE_ROOTFS);

	printf("Copying %s to %s\n", DISK_FILE_KERNEL_MODULE, MEM_FILE_KERNEL_MODULE);
        copyto(DISK_FILE_KERNEL_MODULE, MEM_FILE_KERNEL_MODULE);

	printf("Copying %s to %s\n", DISK_FILE_DAO, MEM_FILE_DAO);
        copyto(DISK_FILE_DAO, MEM_FILE_DAO);

	printf("Copying %s to %s\n", DISK_FILE_ENV, MEM_FILE_ENV);
        copyto(DISK_FILE_ENV, MEM_FILE_ENV);

        // umount new_root
	printf("Umount %s\n", GLOBAL_ROOT_DIR);
        umount(GLOBAL_ROOT_DIR);

        // if env.txt exist, then export envroments
	printf("Export ENV %s\n", MEM_FILE_ENV);
        setenv_to_file("BOOT2ROOT", "1");

        // mount rootfs
	loopdev_setup_device(MEM_FILE_ROOTFS, LOOP_ROOTFS);
	printf("Mount %s to %s\n", LOOP_ROOTFS, GLOBAL_ROOT_DIR);
        result = mount(LOOP_ROOTFS, GLOBAL_ROOT_DIR, "squashfs", 0, MOUNT_OPTS);
        if(result < 0)
            printf("Mount Failed[%s] \n", strerror(errno));

        // mount kmod
	loopdev_setup_device(MEM_FILE_KERNEL_MODULE, LOOP_KERNEL_MODULE);
	printf("Mount %s to %s\n", LOOP_KERNEL_MODULE, GLOBAL_ROOT_MODULES_DIR);
        result = mount(LOOP_KERNEL_MODULE, GLOBAL_ROOT_MODULES_DIR, "squashfs", 0, MOUNT_OPTS);
        if(result < 0)
            printf("Mount Failed[%s] \n", strerror(errno));

        // mount dao
	loopdev_setup_device(MEM_FILE_DAO, LOOP_DAO);
	printf("Mount %s to %s\n", LOOP_DAO, GLOBAL_ROOT_DAO_DIR);
        result = mount(LOOP_DAO, GLOBAL_ROOT_DAO_DIR, "squashfs", 0, MOUNT_OPTS);
        if(result < 0)
            printf("Mount Failed[%s] \n", strerror(errno));

	printf("Umount [%s, %s]\n", GLOBAL_PROC, GLOBAL_SYS);
        // umount proc, sys
        umount(GLOBAL_PROC);
        umount(GLOBAL_SYS);
        //umount(GLOBAL_TARGET_DIR);

    } else {
	system("/bin/sh");
    }
}

