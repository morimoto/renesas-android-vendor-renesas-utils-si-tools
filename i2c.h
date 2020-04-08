#ifndef _I2C_H_
#define _I2C_H_

int i2c_fd;

int i2c_parse_address(const char *address_arg);
int i2c_lookup_bus(const char *i2cbus_arg);
int i2c_io(unsigned char *out, int out_len, unsigned char *in, int in_len);
int i2c_init(char* bus, int addr);

#endif /* _I2C_H_ */
