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

#define MAX_BLOCK_LEN		128

#define GLOBAL_TARGET_DIR	"/mnt"
#define GLOBAL_ROOT_DIR		"/newroot"
#define GLOBAL_PROC		"/proc"
#define GLOBAL_SYS		"/sys"
#define GLOBAL_DEV		"/dev"

#define FILE_ROOTFS		"rootfs.squashfs" 
#define FILE_KERNEL_MODULE	"kmod.squashfs"
#define FILE_DAO		"dao.squashfs"
#define FILE_ENV		"env.txt"

int main(int argc, char *argv[])
{
	char chunk[128]={0};
	FILE *fp = fopen("./env.txt", "r");
	while(fgets(chunk, sizeof(chunk), fp) != NULL) {
		printf("%s", chunk);
	}
	fclose(fp);
}

