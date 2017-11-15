#ifndef _SPI_H_
#define _SPI_H_

int spi_fd;

int spi_io(unsigned char *out, unsigned char *in, int len, int deact);
int spi_init(char *path, int speed, int mode);

#endif /* _SPI_H_ */
