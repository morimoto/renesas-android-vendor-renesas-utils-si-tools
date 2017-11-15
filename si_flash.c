/*
 * si_flash - utility to flash SiLabs Si46xx radio
 * Copyright (C) 2016 CogentEmbedded, Inc
 * Andrey Gusakov <andrey.gusakov@cogentembedded.com>
 *
 * Based on
 * dabpi_ctl - raspberry pi fm/fmhd/dab receiver board control interface
 * Copyright (C) 2014  Bjoern Biesenbach <bjoern@bjoern-b.de>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <errno.h>
#include "si46xx.h"
#include "si46xx_props.h"
#include "version.h"

extern uint32_t crc32(uint32_t crc, const void *buf, size_t size);

int verbose = 0;

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#define FLASH_WRITE_BLOCK_SIZE	2048

void show_help(char *prog_name)
{
	printf("usage: %s\n", prog_name);
	printf("  -i             init chip (bootloader mode)\n");
	printf("  -e             erase chip\n");
	printf("  -w <file>      write file\n");
	printf("  -o <offset>    offset to read/write\n");
	printf("  -d             dump propertyes\n");
	printf("  -b             boot from flash\n");
	printf("  -v(vvv)        verbose\n");
	printf("  -h             this help\n");
	printf(" Standart flash offsets:\n");
	printf(" 0x%06x          patch 016\n", FLASH_OFFSET_PATCH_016);
	printf(" 0x%06x          FM firmware\n", FLASH_OFFSET_FM);
	printf(" 0x%06x          DAB firmware\n", FLASH_OFFSET_DAB);
	printf(" 0x%06x          AM firmware\n", FLASH_OFFSET_AM);
}

int main(int argc, char **argv)
{
	int ret = 0;
	int mode;
	int c;
	bool init = false;
	bool erase = false;
	bool dump = false;
	bool boot = false;
	int offset = -1;
	char *filename = NULL;

	printf("si_flash version %s\n", GIT_VERSION);

	if(argc == 1){
		show_help(argv[0]);
		exit(0);
	}

	ret = si46xx_init(argc, argv);
	if (ret < 0)
		goto exit;

	while (optind < argc) {
		if ((c=getopt(argc, argv, "iew:o:dbv")) != -1) {
			switch(c){
			case 'i':
				init = true;
				break;
			case 'e':
				erase = true;
				break;
			case 'w':
				filename = optarg;
				break;
			case 'o':
				offset = strtoul(optarg, NULL, 16);
				break;
			case 'd':
				dump = true;
				break;
			case 'b':
				boot = true;
				break;
			case 'v':
				verbose++;
				break;
			case 'h':
			default:
				show_help(argv[0]);
				exit(0);
				break;
			}
		} else {
			printf("unknown argument (%d of %d): %s\n", optind, argc, optarg);
			optind++;
		}
	}

	/* init */
	if (init) {
		mode = si46xx_get_sys_mode();
		/* skip init? */
		if (mode != SI46XX_MODE_BOOT) {
			printf("Booting to bootloader mode\n");
			ret = si46xx_init_mode(SI46XX_MODE_BOOT);
			if (ret)
				goto exit;
		} else {
			printf("Allready in bootloader mode\n");
		}
	}

	mode = si46xx_get_sys_mode();
	if (mode != SI46XX_MODE_BOOT) {
		printf("Not in bootloader mode!\n");
		goto exit;
	}

	/* erase chip */
	if (erase) {
		printf("Erasing while chip\n");
		ret = si46xx_flash_erase_chip();
		if (ret) {
			printf("Erase failed\n");
			goto exit;
		}
	}

	/* dump propertyes */
	if (dump) {
		int i;
		int val;
		char *name;
		int prop_list[] = {
			BL_SPI_CLOCK_FREQ_KHZ,
			BL_SPI_MODE,
			BL_READ_CMD,
			BL_HIGH_SPEED_READ_CMD,
			BL_HIGH_SPEED_READ_MAX_FREQ_MHZ,
			BL_WRITE_CMD,
			BL_ERASE_SECTOR_CMD,
			BL_ERASE_CHIP_CMD};

		printf("Bootloader props dump:\n");
		for (i = 0; i < ARRAY_SIZE(prop_list); i++) {
			name = si46xx_property_name(prop_list[i], SI46XX_MODE_BOOT);
			ret = si46xx_flash_property_get(prop_list[i], &val);
			if (ret) {
				printf("Property 0x%04x read error: %d\n",
					prop_list[i], ret);
				goto exit;
			}
			if (name)
				printf("%s: 0x%04x\n", name, val);
			else
				printf("0x%04x: 0x%04x\n", prop_list[i], val);
		}
	}

	/* flash */
	if (filename) {
		FILE *f;
		long size;
		char *buffer;
		int i = 0;

		if (offset < 0) {
			printf("Invalid offset\n");
			ret = -EINVAL;
			goto exit;
		}
		f = fopen(filename, "rb");
		if (f == NULL) {
			printf("File error: %d\n", errno);
			ret = -EIO;
			goto exit;
		}

		/* obtain file size */
		fseek (f , 0 , SEEK_END);
		size = ftell (f);
		rewind (f);

		printf("Flashing %d bytes @0x%08x\n", size, offset);

		buffer = malloc(size);
		if (buffer == NULL) {
			printf("Failed to allocate buffer\n");
			fclose(f);
			ret = -ENOMEM;
			goto exit;
		}

		if (size != fread(buffer, 1, size, f)) {
			printf("Reading error\n");
			fclose(f);
			ret = -EIO;
			goto exit;
		}
		fclose(f);

		/* write */
		while (i < size) {
			uint32_t crc;

			printf("Writing @0x%08x\n", offset + i);
			crc = crc32(0, buffer + i, FLASH_WRITE_BLOCK_SIZE);
			ret = si46xx_flash_write(offset + i, buffer + i,
				FLASH_WRITE_BLOCK_SIZE, crc + 1, 1);
			if (ret) {
				printf("Write error @0x%08x: %d\n", offset, ret);
				free(buffer);
				goto exit;
			}
			i += FLASH_WRITE_BLOCK_SIZE;
		}
		free(buffer);
	}

	/* boot */
	if (boot) {
		if (offset < 0) {
			printf("Invalid offset\n");
			ret = -EINVAL;
			goto exit;
		}

		printf("Booting from flash@0x%06x\n", offset);
		ret = si46xx_boot_flash(offset);
		if (ret) {
			printf("boot from flash failed\n");
			goto exit;
		}
	}
exit:
	printf("Operation done: %d\n", ret);

	return 0;
}

