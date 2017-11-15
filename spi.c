#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "spi.h"

extern int verbose;

#define VERBOSE()	(verbose > 2)

#define MIN(a,b)	(a > b ? b : a)

/*
 * The SPI bus parameters
 *	Variables as they need to be passed as pointers later on
 */

const static uint8_t     spiBPW   = 8 ;
const static uint16_t    spiDelay = 0 ;

int spi_fd = 0;
static int spi_speed;

int spi_io(unsigned char *out, unsigned char *in, int len, int deact)
{
	int ret;
	int i;
	struct spi_ioc_transfer spi[2];
	char *local_out = NULL;

	memset(spi, 0, sizeof(spi));

	spi[0].tx_buf        = (unsigned long)out;
	spi[0].rx_buf        = (unsigned long)in;
	spi[0].len           = len;
	spi[0].delay_usecs   = spiDelay;
	spi[0].speed_hz      = spi_speed;
	spi[0].bits_per_word = spiBPW;
	spi[0].cs_change     = 1;
	
	/* to deactivate CS */
	spi[1].tx_buf        = 0;
	spi[1].rx_buf        = 0;
	spi[1].len           = 0;
	spi[1].delay_usecs   = spiDelay;
	spi[1].speed_hz      = spi_speed;
	spi[1].bits_per_word = spiBPW;
	spi[1].cs_change     = 1;

	if (VERBOSE()) {
		if (out) {
			printf("(%04d)> ", len);
			for (i = 0; i < MIN(len, 16); i++)
				printf("%02x ", out[i]);
			if (i != len)
				printf("...");
			printf("\n");
		} else {
			printf("(%04d)> NULL\n", len);
		}
	}
	/* check if we need this */
	if (out == NULL) {
		local_out = malloc(len);
		memset(local_out, 0, len);
		spi[0].tx_buf = (unsigned long)local_out;
	}

	if (deact)
		ret = ioctl(spi_fd, SPI_IOC_MESSAGE(2), spi);
	else
		ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), spi);
		
	if (ret != len)
		printf("SPI write return %d instead of %d\n", ret, len);
	if (VERBOSE()) {
		if (in) {
			printf("(%04d)< ", len);
			for (i = 0; i < MIN(len, 16); i++)
				printf("%02x ", in[i]);
			if (i != len)
				printf("...");
			printf("\n");
		} else {
			printf("(%04d)< NULL\n", len);
		}
	}

	if (local_out)
		free(local_out);

	//return ret;
	return 0;
}

int spi_init(char *path, int speed, int mode)
{
	int fd ;
	int ret;
	
	mode &= 3;

	if ((fd = open (path, O_RDWR)) < 0) {
		printf("Unable to open SPI device %s: %s\n",
			path, strerror(errno)) ;
		return errno;
	}

	spi_fd = fd ;
	spi_speed = speed;

	/* Set SPI parameters */
	ret = ioctl (fd, SPI_IOC_WR_MODE, &mode);
	if (ret < 0) {
		printf("SPI Mode Change failure: %s\n", strerror (errno));
		return ret;
	}
  
	ret = ioctl (fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW);
	if (ret < 0) {
		printf("SPI BPW Change failure: %s\n", strerror (errno));
		return ret;
	}

	ret = ioctl (fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret < 0) {
		printf("SPI Speed Change failure: %s\n", strerror (errno));
		return ret;
	}
/*
	if (ioctl (fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) >= 0)
		printf("SPI speed %d\n", speed);
*/
	return 0;
}
