#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>  /* for NAME_MAX */
#include <sys/ioctl.h>
#include <string.h>
#include <strings.h>    /* for strcasecmp() */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
//#include "i2cbusses.h"
#include <linux/i2c-dev.h>

#include "i2c.h"

extern int verbose;

#define VERBOSE()	(verbose > 2)

int i2c_fd = 0;

int i2c_parse_address(const char *address_arg)
{
	long address;
	char *end;

	address = strtol(address_arg, &end, 0);
	if (*end || !*address_arg) {
		fprintf(stderr, "Error: Chip address is not a number!\n");
		return -1;
	}
	if (address < 0x03 || address > 0x77) {
		fprintf(stderr, "Error: Chip address out of range "
			"(0x03-0x77)!\n");
		return -2;
	}

	return address;
}

int i2c_open_dev(char *filename)
{
	int file;

	file = open(filename, O_RDWR);

	if (file < 0) {
		if (errno == ENOENT) {
			printf("Error: Could not open file "
				"%s: %s\n",
				filename, strerror(ENOENT));
		} else {
			fprintf(stderr, "Error: Could not open file "
				"%s: %s\n", filename, strerror(errno));
			if (errno == EACCES)
				fprintf(stderr, "Run as root?\n");
		}
	}

	return file;
}

int set_slave_addr(int file, int address, int force)
{
	/* With force, let the user read from/write to the registers
	   even when a driver is also running */
	if (ioctl(file, force ? I2C_SLAVE_FORCE : I2C_SLAVE, address) < 0) {
		fprintf(stderr,
			"Error: Could not set address to 0x%02x: %s\n",
			address, strerror(errno));
		return -errno;
	}

	return 0;
}

int i2c_io(unsigned char *out, int out_len, unsigned char *in, int in_len)
{
	int i;
	int ret;

	if (VERBOSE()) {
		if (out) {
			printf("(%04d)> ", out_len);
			for (i = 0; i < MIN(out_len, 16); i++)
				printf("%02x ", out[i]);
			if (i != out_len)
				printf("...");
			printf("\n");
		} else {
			printf("(%04d)> NULL\n", out_len);
		}
	}

	if ((out != NULL) && (out_len != 0)) {
		ret = write(i2c_fd, out, out_len);
		if (ret != out_len) {
			printf("I2C write error %d: %s\n", errno, strerror(errno));
			/* return -errno; */
		}
	}

	if ((in != NULL) && (in_len != 0)) {
		memset(in, 0, in_len);
		ret = read(i2c_fd, in, in_len);
		if (ret != in_len) {
			printf("I2C read error %d: %s\n", errno, strerror(errno));
			/* return -errno; */
		}
	}
	if (VERBOSE()) {
		if (in) {
			printf("(%04d)< ", in_len);
			for (i = 0; i < MIN(in_len, 16); i++)
				printf("%02x ", in[i]);
			if (i != in_len)
				printf("...");
			printf("\n");
		} else {
			printf("(%04d)< NULL\n", in_len);
		}
	}
	return 0;
}

int i2c_init(char* bus, int addr)
{
	int fd ;
	int ret;
	char filename[20];

	fd = i2c_open_dev(bus);
	if (fd < 0) {
		printf("Unable to open i2c device %s: %s\n",
			filename, strerror(errno)) ;
		return errno;
	}

	ret = set_slave_addr(fd, addr, 0);
	if (ret < 0) {
		printf("Unable to set i2c address 0x%02x: %d\n",
			addr, ret) ;
		return ret;
	}

	i2c_fd = fd;

	return 0;
}
