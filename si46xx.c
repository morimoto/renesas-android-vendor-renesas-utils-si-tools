/* 
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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "spi.h"
#include "i2c.h"
#include "si46xx.h"
#include "si46xx_props.h"

#define msleep(x) usleep(x*1000)

#define RESET_GPIO	780
#define MODE_GPIO	781

#define STORE_U8(a) do {		\
	data[i++] = (a);		\
	} while (0)
#define STORE_U16(a) do {		\
	data[i++] = (a) & 0xff; 	\
	data[i++] = ((a) >> 8) & 0xff;	\
	} while (0)
#define STORE_U32(a) do {		\
	data[i++] = (a) & 0xff; 	\
	data[i++] = ((a) >> 8) & 0xff;	\
	data[i++] = ((a) >> 16) & 0xff;	\
	data[i++] = ((a) >> 24) & 0xff;	\
	} while (0)

#define CS_LOW()
#define CS_HIGH()
#define RESET_LOW()
#define RESET_HIGH()

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

uint8_t dab_num_channels;
int wait = 0;

int SPI_Write(char *out, int out_len, char *in, int in_len, int deact)
{
	if (spi_fd)
		return spi_io(out, in, MAX(in_len, out_len), deact);
	else if (i2c_fd) {
		/* some shitty workaround */
		if ((out != NULL) && (out == in))
			return i2c_io(out, out_len, in + out_len, in_len - 1);
		return i2c_io(out, out_len, in, in_len);
	} else {
		printf("No interface to Si provided\n");
		return -EINVAL;
	}
}

void print_hex_str(uint8_t *str, uint16_t len)
{
	uint16_t i;
	for(i = 0;i < len; i++){
		if ((i % 16) == 0)
			printf("%03x: ", i);
		printf("%02x ",(int)str[i]);
		if ((i % 16) == 15)
			printf("\n");
	}
	printf("\n");
}

static int si46xx_write_host_load_data(uint8_t cmd,
		const uint8_t *ptr,
		uint16_t len)
{
	int ret;
	uint8_t *data = malloc(len + 4);

	data[0] = cmd;
	data[1] = 0;
	data[2] = 0;
	data[3] = 0;
	memcpy(data + 4, ptr, len);
	ret = SPI_Write(data, len + 4, NULL, 0, 1);
	free(data);
	return ret;
}

static int si46xx_read(uint8_t *ptr, uint8_t cnt)
{
	int ret;
	int timeout;
	uint8_t *data = malloc(cnt + 1);

	timeout = 1000; // wait for CTS
	usleep(20);
	while(--timeout) {
		//msleep(1); // make sure cs is high for 20us
		data[0] = SI46XX_RD_REPLY;
		ret = SPI_Write(data, 1, data, cnt + 1, 1); // read status register (we are working without interrupts)
		if (ret < 0)
			return ret;
		if (data[1] & 0x80) {
			if (ptr)
				memcpy(ptr, data + 1, cnt);
			free(data);
			return 0;
		}
		if (data[1] & 0x40) {
			if (ptr)
				memcpy(ptr, data + 1, cnt);
			free(data);
			return -EIO;
		}
		usleep(20); // make sure cs is high for 20us
	}
	free(data);
	printf("Timeout waiting for CTS\n");
	return -ETIME;
}

/*
 * REWORK THIS SHIT:
 * avoid memcpy
 * let caller set cmd, or at least avoid 1 byte offset
 */
static int si46xx_write_data(uint8_t cmd,
		uint8_t *ptr,
		uint16_t len)
{
	int ret;
	uint8_t timeout;
	uint8_t *data;

	/* check busy */
	ret = si46xx_read(NULL, 4);
	if (ret)
		return ret;
/*
	timeout = 100; // wait for CTS
	while(--timeout){
		if (si46xx_read(buf, 4))
			continue;
		if(buf[0] & 0x80)
			break;
	}
	if (timeout == 0) {
		printf("Busy wait timeout\n");
		return -ETIME;
	}
*/
	data = malloc(len + 1);
	data[0] = cmd;
	if (data && len)
		memcpy(data + 1, ptr, len);
	ret = SPI_Write(data, len + 1, NULL, 0, 1);
	free(data);
	return ret;
}

static uint16_t si46xx_read_dynamic__(uint8_t *data)
{
	int ret;
	uint8_t zero = 0;
	uint16_t cnt;

	CS_HIGH();
	msleep(1); // make sure cs is high for 20us
	CS_LOW();
	ret = SPI_Write(&zero, 1, NULL, 0, 0);
	if (ret < 0)
		goto exit;
	ret = SPI_Write(NULL, 0, data, 6, 0);
	if (ret < 0)
		goto exit;
	cnt = ((uint16_t)data[5] << 8) | (uint16_t)data[4];
	if(cnt > 3000)
		cnt = 0;
	ret = SPI_Write(NULL, 0, &data[6], cnt, 1);
exit:
	CS_HIGH();
	msleep(1); // make sure cs is high for 20us
	if (ret < 0)
		return ret;
	return cnt + 6;
}

static uint16_t si46xx_read_dynamic(uint8_t *data)
{
	uint8_t zero = 0;
	uint16_t cnt;

	SPI_Write(&zero, 1, NULL, 0, 0);
	SPI_Write(NULL, 0, data, 6, 0);
	cnt = ((uint16_t)data[5] << 8) | (uint16_t)data[4];
	printf("cnt = %d\n", cnt);
	if (cnt > 3000)
		cnt = 0;
	SPI_Write(NULL, 0, &data[6], cnt, 1);

	return cnt + 6;
}

static char *pup_states_names[] = {
	"out of reset",
	"reserved",
	"bootloader",
	"application"
};

int si46xx_check_reply(char *buf)
{
	int ret = 0;
	if (buf[0] & (1 << 7))
		wait = 1;
	else
		wait = 0;
	if (buf[0] & (1 << 6)) {
		printf("ERR_CMD\n");
		print_hex_str(buf, 4);
		printf("PUP_STATE: %s\n", pup_states_names[buf[3] >> 6]);
		if (buf[3] & (1 << 3)) {
			printf("REPOFERR (reply too fast)\n");
			ret = -EBUSY;
		}
		if (buf[3] & (1 << 2)) {
			printf("CMDOFERR (CMD too fast)\n");
			ret = -EAGAIN;
		}
		if (buf[3] & (1 << 1)) {
			printf("ARBERR (arbiter error)\n");
			ret = -EIO;
		}
		if (buf[3] & (1 << 0)) {
			printf("ERRNR (non-recoverable error)\n");
			ret = -EINVAL;
		}
	}
	return ret;
}

int si46xx_read_reply(char *buf, int len)
{
	int ret;

	ret = si46xx_read(buf, len);
	if (ret)
		return ret;

	/* check basic errors */
	return si46xx_check_reply(buf);
}

int si46xx_get_sys_state(void)
{
	int ret;
	uint8_t zero = 0;
	char buf[6];
	uint8_t mode;

	si46xx_write_data(SI46XX_GET_SYS_STATE, &zero, 1);
	ret = si46xx_read_reply(buf, sizeof(buf));
	if (ret)
		return ret;
	mode = buf[4];
	printf("Current mode:\n");
	switch(mode)
	{
		case 0: printf("Bootloader is active\n"); break;
		case 1: printf("FMHD is active\n"); break;
		case 2: printf("DAB is active\n"); break;
		case 3: printf("TDMB or data only DAB image is active\n"); break;
		case 4: printf("FMHD is active\n"); break;
		case 5: printf("AMHD is active\n"); break;
		case 6: printf("AMHD Demod is active\n"); break;
		default: printf("UNKNOWN\n");break;
	}
	return ret;
}

int si46xx_get_sys_mode(void)
{
	int ret;
	uint8_t zero = 0;
	char buf[6];
	uint8_t mode;

	si46xx_write_data(SI46XX_GET_SYS_STATE, &zero, 1);
	ret = si46xx_read_reply(buf, sizeof(buf));
	if (ret)
		return ret;
	mode = buf[4];
	switch(mode)
	{
		case 0:
			return SI46XX_MODE_BOOT;
		case 1:
		case 4:
			return SI46XX_MODE_FM;
		case 2:
		case 3:
			return SI46XX_MODE_DAB;
		case 5:
		case 6:
			return SI46XX_MODE_AM;
		default:
			return SI46XX_MODE_UNK;
	}
}

static int si46xx_get_part_info()
{
	int ret;
	uint8_t zero = 0;
	char buf[22];

	si46xx_write_data(SI46XX_GET_PART_INFO,&zero,1);
	ret = si46xx_read_reply(buf, sizeof(buf));
	if(ret)
		return ret;

	printf("si46xx_get_part_info answer:\n");
	printf("CHIPREV:\t0x%02x\n", buf[4]);
	printf("ROMID:\t0x%02x\n", buf[5]);
	printf("PART:\t%04d\n", (buf[9] << 8) | buf[8]);

	return ret;
}

void si46xx_periodic()
{
	si46xx_read(NULL, 4);
}


int si46xx_dab_start_digital_service(uint32_t service_id,
		uint32_t comp_id)
{
	uint8_t data[11];

	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = service_id & 0xFF;
	data[4] = (service_id >>8) & 0xFF;
	data[5] = (service_id >>16) & 0xFF;
	data[6] = (service_id >>24) & 0xFF;
	data[7] = comp_id & 0xFF;
	data[8] = (comp_id >> 8) & 0xFF;
	data[9] = (comp_id >> 16) & 0xFF;
	data[10] = (comp_id >> 24) & 0xFF;

	si46xx_write_data(SI46XX_DAB_START_DIGITAL_SERVICE,data,11);
	return si46xx_read(NULL, 4);
}

static void si46xx_swap_services(uint8_t first, uint8_t second)
{
	struct dab_service_t tmp;

	memcpy(&tmp,&dab_service_list.services[first],sizeof(tmp));
	memcpy(&dab_service_list.services[first],
			&dab_service_list.services[second] ,sizeof(tmp));
	memcpy(&dab_service_list.services[second],&tmp ,sizeof(tmp));
}

static void si46xx_sort_service_list(void)
{
	uint8_t i,p,swapped;

	swapped = 0;
	for(i = dab_service_list.num_services; i > 1; i--){
		for(p = 0;p < i - 1; p++){
			if(dab_service_list.services[p].service_id >
					dab_service_list.services[p+1].service_id){
				si46xx_swap_services(p, p + 1);
				swapped = 1;
			}
		}
		if(!swapped)
			break;
	}
}

static void si46xx_dab_parse_service_list(uint8_t *data, uint16_t len)
{
	uint16_t remaining_bytes;
	uint16_t pos;
	uint8_t service_num;
	uint8_t component_num;
	uint8_t i;

	if(len<6)
		return; // no list available? exit
	if(len >= 9){
		dab_service_list.list_size = data[5]<<8 | data[4];
		dab_service_list.version = data[7]<<8 | data[6];
		dab_service_list.num_services = data[8];
	}
	// 9,10,11 are align pad
	pos = 12;
	if(len <= pos)
		return; // no services? exit

	remaining_bytes = len - pos;
	service_num = 0;
	// size of one service with zero component: 24 byte
	// every component + 4 byte
	while(service_num < dab_service_list.num_services){
		dab_service_list.services[service_num].service_id =
			data[pos+3]<<24 |
			data[pos+2]<<16 |
			data[pos+1]<<8 |
			data[pos];
		component_num = data[pos+5] & 0x0F;
		dab_service_list.services[service_num].num_components = component_num;
		memcpy(dab_service_list.services[service_num].service_label,
				&data[pos+8],16);
		dab_service_list.services[service_num].service_label[16] = '\0';
		for(i=0;i<component_num;i++){
			dab_service_list.services[service_num].component_id[i] =
				data[pos+25] << 8 |
				data[pos+24];
			pos += 4;
		}
		pos +=24;
		service_num++;
	}
	si46xx_sort_service_list();
}

void si46xx_dab_get_ensemble_info()
{
	char buf[22];
	char data;
	uint8_t timeout;
	char label[17];

	//data[0] = (1<<4) | (1<<0); // force_wb, low side injection
	data = 0;

	si46xx_write_data(SI46XX_DAB_GET_ENSEMBLE_INFO, &data, 1);
	timeout = 10;
	while(--timeout){ // completed with CTS
		si46xx_read(buf, 22);
		if(buf[0] & 0x80)
			break;
	}
	memcpy(label, &buf[6], 16);
	label[16] = '\0';
	printf("Name: %s",label);
}

void si46xx_dab_print_service_list()
{
	uint8_t i,p;

	printf("List size:     %d\n",dab_service_list.list_size);
	printf("List version:  %d\n",dab_service_list.version);
	printf("Services:      %d\n",dab_service_list.num_services);

	for(i=0;i<dab_service_list.num_services;i++){
		printf("Num: %2u  Service ID: %8x  Service Name: %s  Component ID: %d\n",
				i,
				dab_service_list.services[i].service_id,
				dab_service_list.services[i].service_label,
				dab_service_list.services[i].component_id[0]
		      );
		for(p=0;p<dab_service_list.services[i].num_components;p++){
			printf("                                                               Component ID: %d\n",
					dab_service_list.services[i].component_id[i]
			      );
		}
	}
}

int si46xx_dab_start_digital_service_num(uint32_t num)
{
	printf("Starting service %s %x %x\n", dab_service_list.services[num].service_label,
			dab_service_list.services[num].service_id,
			dab_service_list.services[num].component_id[0]);
	return si46xx_dab_start_digital_service(dab_service_list.services[num].service_id,
			dab_service_list.services[num].component_id[0]);
}

int si46xx_dab_get_digital_service_list()
{
	uint8_t zero = 0;
	uint16_t len;
	uint16_t timeout;
	char buf[2047+6];

	printf("si46xx_dab_get_digital_service_list()\n");
	timeout = 100;
	while(timeout--){
		si46xx_write_data(SI46XX_DAB_GET_DIGITAL_SERVICE_LIST,&zero,1);
		if((len = si46xx_read_dynamic(buf)) > 6)
			break;
	}
	si46xx_dab_parse_service_list(buf,len);
	return len;
}

int si46xx_dab_get_audio_info(void)
{
	int ret = 0;
	uint8_t zero = 0;
	char buf[9];

	printf("si46xx_dab_get_audio_info()\n");
	si46xx_write_data(SI46XX_DAB_GET_AUDIO_INFO, &zero, 1);
	ret = si46xx_read(buf, sizeof(buf));
	if (ret)
		return ret;
	printf("Bit rate: %dkbps\n",buf[4] + (buf[5]<<8));
	printf("Sample rate: %dHz\n",buf[6] + (buf[7]<<8));
	if((buf[8]& 0x03) == 0) {
		printf("Audio Mode = Dual Mono\n");
	}
	if((buf[8]& 0x03) == 1) {
		printf("Audio Mode = Mono\n");
	}
	if((buf[8]& 0x03) == 2) {
		printf("Audio Mode = Stereo\n");
	}
	if((buf[8]& 0x03) == 3) {
		printf("Audio Mode = Joint Stereo\n");
	}
	printf("SBR: %d\n", (buf[8] & 0x04) ? 1:0);
	printf("PS: %d\n", (buf[8] & 0x08) ? 1:0);

	return ret;
}

void si46xx_dab_get_subchannel_info(void)
{
	uint8_t zero = 0;
	char buf[12];
	printf("si46xx_dab_get_subchannel_info()\n");
	si46xx_write_data(SI46XX_DAB_GET_SUBCHAN_INFO, &zero, 1);
	si46xx_read(buf, sizeof(buf));
	if(buf[4] == 0) {
		printf("Service Mode = Audio Stream Service\n");
	}
	if(buf[4] == 1) {
		printf("Service Mode = Data Stream Service\n");
	}
	if(buf[4] == 2) {
		printf("Service Mode = FIDC Service\n");
	}
	if(buf[4] == 3) {
		printf("Service Mode = MSC Data Packet Service\n");
	}
	if(buf[4] == 4) {
		printf("Service Mode = DAB+\n");
	}
	if(buf[4] == 5) {
		printf("Service Mode = DAB\n");
	}
	if(buf[4] == 6) {
		printf("Service Mode = FIC Service\n");
	}
	if(buf[4] == 7) {
		printf("Service Mode = XPAD Data\n");
	}
	if(buf[4] == 8) {
		printf("Service Mode = No Media\n");
	}
	if(buf[5] == 1) {
		printf("Protection Mode UEP-1\n");
	}
	if(buf[5] == 2) {
		printf("Protection Mode UEP-2\n");
	}
	if(buf[5] == 3) {
		printf("Protection Mode UEP-3\n");
	}
	if(buf[5] == 4) {
		printf("Protection Mode UEP-4\n");
	}
	if(buf[5] == 5) {
		printf("Protection Mode UEP-5\n");
	}
	if(buf[5] == 6) {
		printf("Protection Mode EEP-1A\n");
	}
	if(buf[5] == 7) {
		printf("Protection Mode EEP-2A\n");
	}
	if(buf[5] == 8) {
		printf("Protection Mode EEP-3A\n");
	}
	if(buf[5] == 9) {
		printf("Protection Mode EEP-4A\n");
	}
	if(buf[5] == 10) {
		printf("Protection Mode EEP-1B\n");
	}
	if(buf[5] == 11) {
		printf("Protection Mode EEP-2B\n");
	}
	if(buf[5] == 12) {
		printf("Protection Mode EEP-3B\n");
	}
	if(buf[5] == 13) {
		printf("Protection Mode EEP-4B\n");
	}
	printf("Subchannel Bitrate: %dkbps\n",buf[6] + (buf[7]<<8));
	printf("Capacity Units: %d CU\n",buf[8] + (buf[9]<<8));
	printf("CU Starting Adress: %d\n",buf[10] + (buf[11]<<8));
}


int si46xx_dab_set_freq_list(uint8_t num, uint32_t *freq_list)
{
	uint8_t data[3 + 4 * 48]; // max 48 frequencies
	uint8_t i;

	dab_num_channels = num;

	printf("si46xx_dab_set_freq_list(): ");
	if(num == 0 || num > 48){
		printf("num must be between 1 and 48\n");
		return -EINVAL;
	}

	data[0] = num; // NUM_FREQS 1-48
	data[1] = 0;
	data[2] = 0;

	for(i=0;i<num;i++){
		data[3+4*i] = freq_list[i] & 0xFF;
		data[4+4*i] = freq_list[i] >> 8;
		data[5+4*i] = freq_list[i] >> 16;
		data[6+4*i] = freq_list[i] >> 24;
	}
	si46xx_write_data(SI46XX_DAB_SET_FREQ_LIST, data, 3 + 4 * num);

	return si46xx_read(NULL, 4);
}

int si46xx_tune_wait(int timeout)
{
	int ret;
	char buf[5];

	do {
		ret = si46xx_read(buf, sizeof(buf));
		if (ret)
			return ret;
		if (buf[0] & (1 << 0))
			return 0;
		msleep(1);
	} while (--timeout > 0);

	return -ETIME;
}

int si46xx_dab_tune_freq(uint8_t index, uint8_t antcap)
{
	int ret;
	uint8_t data[5];
	char buf[4];
	uint8_t timeout;

	printf("si46xx_dab_tune_freq(%d): ",index);

	//data[0] = (1<<4) | (1<<0); // force_wb, low side injection
	data[0] = 0;
	data[1] = index;
	data[2] = 0;
	data[3] = antcap;
	data[4] = 0;

	si46xx_write_data(SI46XX_DAB_TUNE_FREQ, data, sizeof(data));
	timeout = 20;
	while(--timeout){ // wait for tune to complete
		ret = si46xx_read(buf, sizeof(buf));
		if (ret)
			return ret;
		if(buf[0] & 0x01)
			break;
		msleep(100);
	}
	return ret;
}

int si46xx_fm_tune_freq(uint32_t khz, uint16_t antcap)
{
	uint8_t data[5];

	printf("si46xx_fm_tune_freq(%d)\n", khz);

	//data[0] = (1<<4) | (1<<0); // force_wb, low side injection
	//data[0] = (1<<4)| (1<<3); // force_wb, tune_mode=2
	data[0] = 0;
	data[1] = ((khz/10) & 0xFF);
	data[2] = ((khz/10) >> 8) & 0xFF;
	data[3] = antcap & 0xFF;
	data[4] = 0;
	si46xx_write_data(SI46XX_FM_TUNE_FREQ, data, sizeof(data));

	return si46xx_read(NULL, 4);
}

int si46xx_am_tune_freq(uint32_t khz, uint16_t antcap)
{
	uint8_t data[5];

	printf("si46xx_am_tune_freq(%d)\n", khz);

	//data[0] = (1<<4) | (1<<0); // force_wb, low side injection
	//data[0] = (1<<4)| (1<<3); // force_wb, tune_mode=2
	data[0] = 0;
	data[1] = khz & 0xFF;
	data[2] = (khz >> 8) & 0xFF;
	data[3] = antcap & 0xFF;
	data[4] = (antcap >> 8) & 0xFF;
	si46xx_write_data(SI46XX_AM_TUNE_FREQ, data, sizeof(data));

	return si46xx_read(NULL, 4);
}

int si46xx_tune_freq(int mode, uint32_t khz, uint16_t antcap)
{
	int ret;

	if (mode == SI46XX_MODE_AM)
		ret = si46xx_am_tune_freq(khz, antcap);
	else if (mode == SI46XX_MODE_FM)
		ret = si46xx_fm_tune_freq(khz, antcap);
	else
		ret = -EINVAL;

	return ret;
}

int si46xx_fm_seek_start(uint8_t up, uint8_t wrap)
{
	uint8_t data[5];

	//printf("si46xx_fm_seek_start()\n");

	data[0] = 0;
	data[1] = (up&0x01)<<1 | (wrap&0x01);
	data[2] = 0;
	data[3] = 0;
	data[4] = 0;
	si46xx_write_data(SI46XX_FM_SEEK_START, data, 5);

	return si46xx_read(NULL, 4);
}

int si46xx_seek_start(int mode, uint8_t up, uint8_t wrap)
{
	uint8_t data[5];

	//printf("si46xx_seek_start()\n");

	data[0] = 0;
	data[1] = (up & 0x01)<<1 | (wrap & 0x01);
	data[2] = 0;
	data[3] = 0;
	data[4] = 0;
	if (mode == SI46XX_MODE_AM)
		si46xx_write_data(SI46XX_AM_SEEK_START, data, 5);
	else if (mode == SI46XX_MODE_FM)
		si46xx_write_data(SI46XX_FM_SEEK_START, data, 5);
	else
		return -EINVAL;

	return si46xx_read(NULL, 4);
}

static int si46xx_load_init()
{
	uint8_t data = 0;
	char buf[4];

	printf("si46xx_load_init()\n");

	si46xx_write_data(SI46XX_LOAD_INIT, &data, 1);
	msleep(4); // wait 4ms (datasheet)
	return si46xx_read_reply(buf, sizeof(buf));
}

static int store_image(const uint8_t *data, uint32_t len, uint8_t wait_for_int)
{
	int ret;
	uint32_t remaining_bytes = len;
	uint32_t count_to;

	ret = si46xx_load_init();
	if (ret) {
		printf("LOAD_INIT failed: %d\n", ret);
		return ret;
	}
	while(remaining_bytes){
		if(remaining_bytes >= 2048){
			count_to = 2048;
		}else{
			count_to = remaining_bytes;
		}

		si46xx_write_host_load_data(SI46XX_HOST_LOAD,
			data + (len - remaining_bytes), count_to);
		remaining_bytes -= count_to;
		msleep(1);
	}
	msleep(4); // wait 4ms (datasheet)
	ret = si46xx_read(NULL, 4);
	msleep(4); // wait 4ms (datasheet)
	return ret;
}

//#define FW_LOAD_BUF	256
#define FW_LOAD_BUF	4096
static int store_image_from_file(char *filename, uint8_t wait_for_int)
{
	int ret = 0;
	long remaining_bytes;
	long len;
	uint32_t count_to;
	FILE *fp;
	uint8_t buffer[FW_LOAD_BUF];
	size_t result;
	char buf[4];

	fp = fopen(filename, "rb");
	if(fp == NULL){
		printf("file error %s: %d\n", filename, errno);
		return -errno;
	}

	fseek(fp,0, SEEK_END);
	len = ftell(fp);
	remaining_bytes = len;
	rewind(fp);

	ret = si46xx_load_init();
	if (ret) {
		printf("LOAD_INIT failed\n");
		goto fail;
	}

	printf("Loading: %s (%d bytes)\n", filename, len);

	while (remaining_bytes) {
		if (remaining_bytes >= FW_LOAD_BUF)
			count_to = FW_LOAD_BUF;
		else
			count_to = remaining_bytes;

		result = fread(buffer, 1, count_to, fp);
		if (result != count_to) {
			printf("file error %s\n", filename);
			ret = -EIO;
			goto fail;
		}

		si46xx_write_host_load_data(SI46XX_HOST_LOAD, buffer, count_to);
		remaining_bytes -= count_to;
		//msleep(1);
	}
	msleep(4); // wait 4ms (datasheet)
	ret = si46xx_read_reply(buf, sizeof(buf));
	if (ret)
		printf("Load firmware failed\n");
	msleep(4); // wait 4ms (datasheet)
fail:
	fclose(fp);
	return ret;
}

static int si46xx_powerup(void)
{
	int ret;
	uint8_t data[15];
	char buf[4];

	data[0] = 0x80; // ARG1
	data[1] = (1<<4) | (7<<0); // ARG2 CLK_MODE=0x1 TR_SIZE=0x7
	//data[2] = 0x28; // ARG3 IBIAS=0x28
	data[2] = 0x48; // ARG3 IBIAS=0x28
	data[3] = 0x00; // ARG4 XTAL
	data[4] = 0xF9; // ARG5 XTAL // F8
	data[5] = 0x24; // ARG6 XTAL
	data[6] = 0x01; // ARG7 XTAL 19.2MHz
	data[7] = 0x1F; // ARG8 CTUN
	data[8] = 0x00 | (1<<4); // ARG9
	data[9] = 0x00; // ARG10
	data[10] = 0x00; // ARG11
	data[11] = 0x00; // ARG12
	data[12] = 0x00; // ARG13 IBIAS_RUN
	data[13] = 0x00; // ARG14
	data[14] = 0x00; // ARG15

	ret = si46xx_write_data(SI46XX_POWER_UP, data, 15);
	if (ret)
		return ret;
	msleep(1); // wait 20us after powerup (datasheet)
	return si46xx_read_reply(buf, sizeof(buf));
}

static int si46xx_boot(void)
{
	int ret;
	int i = 5;
	uint8_t data = 0;
	char buf[4];

	printf("si46xx_boot()\n");

	do {
		si46xx_write_data(SI46XX_BOOT, &data, 1);
		msleep(300); // 63ms at analog fm, 198ms at DAB
		ret = si46xx_read_reply(buf, sizeof(buf));
	} while ((i--) && (ret));
	return ret;
}

int si46xx_rsq_status(int mode)
{
	int khz;
	int ret;
	uint8_t data = 0;
	char buf[20];

	//printf("si46xx_fm_rsq_status(%d)\n", mode);

	if (mode == SI46XX_MODE_AM) {
		si46xx_write_data(SI46XX_AM_RSQ_STATUS, &data, 1);
		ret = si46xx_read(buf, 16);
	}
	else if (mode == SI46XX_MODE_FM) {
		si46xx_write_data(SI46XX_FM_RSQ_STATUS, &data, 1);
		ret = si46xx_read(buf, 20);
	}
	else
		return -EINVAL;
	if (ret)
		return ret;

	printf("SNR:        %d dB\n", (signed char) buf[10]);
	printf("RSSI:       %d dBuV\n", (signed char) buf[9]);
	khz = (buf[7] << 8) | buf[6];
	if (mode == SI46XX_MODE_FM)
		khz *= 10;
	printf("Frequency:  %dkHz\n", khz);
	printf("FREQOFF:    %d\n", buf[8]*2);
	printf("READANTCAP: %d\n", (buf[12] | (buf[13] << 8)));

	if (mode == SI46XX_MODE_AM) {
		printf(" AM modulaton: %d%%\n", buf[11]);
		printf(" HDLEVEL:      %d%%\n", buf[15]);
	}
	return 0;
}

int si46xx_fm_rds_blockcount(void)
{
	int ret;
	//uint8_t data = 1; // clears block counts if set
	uint8_t data = 0; // clears block counts if set
	char buf[10];

	printf("si46xx_rds_blockcount()\n");
	si46xx_write_data(SI46XX_FM_RDS_BLOCKCOUNT,&data,1);
	ret = si46xx_read(buf, sizeof(buf));
	if (ret)
		return ret;
	printf("Expected: %d\n",buf[4] | (buf[5]<<8));
	printf("Received: %d\n",buf[6] | (buf[7]<<8));
	printf("Uncorrectable: %d\n",buf[8] | (buf[9]<<8));
	return 0;
}

static uint8_t si46xx_rds_parse(uint16_t *block)
{
	uint8_t addr;
	fm_rds_data.pi = block[0];
	if((block[1] & 0xF800) == 0x00){ // group 0A
		addr = block[1] & 0x03;
		fm_rds_data.ps_name[addr*2] = (block[3] & 0xFF00)>>8;
		fm_rds_data.ps_name[addr*2+1] = block[3] & 0xFF;
		fm_rds_data.group_0a_flags |= (1<<addr);
	}else if((block[1] & 0xF800)>>11 == 0x04){ // group 2A
		addr = block[1] & 0x0F;
		if((block[1] & 0x10) == 0x00){ // parse only string A
			fm_rds_data.radiotext[addr*4] = (block[2] & 0xFF00)>>8;
			fm_rds_data.radiotext[addr*4+1] = (block[2] & 0xFF);
			fm_rds_data.radiotext[addr*4+2] = (block[3] & 0xFF00)>>8;
			fm_rds_data.radiotext[addr*4+3] = (block[3] & 0xFF);

			if(fm_rds_data.radiotext[addr*4] == '\r'){
				fm_rds_data.radiotext[addr*4] = 0;
				fm_rds_data.group_2a_flags = 0xFFFF;
			}
			if(fm_rds_data.radiotext[addr*4+1] == '\r'){
				fm_rds_data.radiotext[addr*4+1] = 0;
				fm_rds_data.group_2a_flags = 0xFFFF;
			}
			if(fm_rds_data.radiotext[addr*4+2] == '\r'){
				fm_rds_data.radiotext[addr*4+2] = 0;
				fm_rds_data.group_2a_flags = 0xFFFF;
			}
			if(fm_rds_data.radiotext[addr*4+3] == '\r'){
				fm_rds_data.radiotext[addr*4+3] = 0;
				fm_rds_data.group_2a_flags = 0xFFFF;
			}
			fm_rds_data.group_2a_flags |= (1<<addr);
		}
	}
	if(fm_rds_data.group_0a_flags == 0x0F &&
			fm_rds_data.group_2a_flags == 0xFFFF){
		fm_rds_data.ps_name[8] = 0;
		fm_rds_data.radiotext[128] = 0;
		return 1;
	}
	return 0;
}

int si46xx_fm_rds_status(void)
{
	int ret;
	uint8_t data = 0;
	char buf[20];
	uint16_t timeout;
	uint16_t blocks[4];

	//printf("si46xx_rds_status()\n");

	timeout = 5000; // work on 1000 rds blocks max
	while(--timeout){
		data = 1;
		si46xx_write_data(SI46XX_FM_RDS_STATUS, &data, 1);
		ret = si46xx_read(buf, sizeof(buf));
		if (ret)
			return ret;
		blocks[0] = buf[12] + (buf[13]<<8);
		blocks[1] = buf[14] + (buf[15]<<8);
		blocks[2] = buf[16] + (buf[17]<<8);
		blocks[3] = buf[18] + (buf[19]<<8);
		fm_rds_data.sync = (buf[5] & 0x02) ? 1 : 0;
		if(!fm_rds_data.sync)
			break;
		if(si46xx_rds_parse(blocks))
			break;
		if(fm_rds_data.group_0a_flags == 0x0F) // stop at ps_name complete
			break;
	}
	if(!timeout) {
		printf("Timeout\n");
		return -ETIME;
	}
	printf("RDSSYNC: %u\n", (buf[5] & 0x02) ? 1 : 0);
	printf("PI: %d  Name:%s\nRadiotext: %s\n",
			fm_rds_data.pi,
			fm_rds_data.ps_name,
			fm_rds_data.radiotext);

	return 0;
}

void si46xx_dab_get_service_linking_info(uint32_t service_id)
{
	uint8_t data[7];
	char buf[24];

	printf("si46xx_dab_get_service_linking_info()\n");
	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = (service_id) & 0xFF;
	data[4] = (service_id>>8) & 0xFF;
	data[5] = (service_id>>16) & 0xFF;
	data[6] = (service_id>>24) & 0xFF;
	si46xx_write_data(SI46XX_DAB_GET_SERVICE_LINKING_INFO, data, sizeof(data));
	si46xx_read(buf, 24);
}

void si46xx_dab_digrad_status_print(struct dab_digrad_status_t *status)
{
	printf("ACQ: %d\n",status->acq);
	printf("VALID: %d\n",status->valid);
	printf("RSSI: %d\n",status->rssi);
	printf("SNR: %d\n",status->snr);
	printf("FIC_QUALITY: %d\n",status->fic_quality);
	printf("CNR %d\n",status->cnr);
	printf("FFT_OFFSET %d\n",status->fft_offset);
	printf("Tuned frequency %dkHz\n",status->frequency);
	printf("Tuned index %d\n",status->tuned_index);

	printf("ANTCAP: %d\n",status->read_ant_cap);
}

void si46xx_dab_digrad_status(struct dab_digrad_status_t *status)
{
	uint8_t data = (1<<3) | 1; // set digrad_ack and stc_ack
	char buf[22];
	uint8_t timeout = 100;

	printf("si46xx_dab_digrad_status():\n");
	timeout = 10;
	while(--timeout){
		data = (1<<3) | 1; // set digrad_ack and stc_ack
		si46xx_write_data(SI46XX_DAB_DIGRAD_STATUS,&data,1);
		si46xx_read(buf, sizeof(buf));
		if(buf[0] & 0x81)
			break;
	}
	if(!timeout){
		printf("si46xx_dab_digrad_status() timeout reached\n");
		return;
	}
	if(!status)
		return;

	status->acq = (buf[5] & 0x04) ? 1:0;
	status->valid = buf[5] & 0x01;
	status->rssi = (int8_t)buf[6];
	status->snr = (int8_t)buf[7];
	status->fic_quality = buf[8];
	status->cnr = buf[9];
	status->fft_offset = (int8_t)buf[17];
	status->frequency = buf[12] |
		buf[13]<<8 |
		buf[14]<<16 |
		buf[15]<<24;
	status->tuned_index = buf[16];
	status->read_ant_cap = buf[18] | buf[19]<<8;

}

void si46xx_dab_scan()
{
	uint8_t i;
	struct dab_digrad_status_t status;

	for(i=0;i<dab_num_channels;i++){
		si46xx_dab_tune_freq(i,0);
		si46xx_dab_digrad_status(&status);
		printf("Channel %d: ACQ: %d RSSI: %d SNR: %d ", i,
				status.acq,
				status.rssi,
				status.snr);
		if(status.acq){
			msleep(1000);
			si46xx_dab_get_ensemble_info();
		};
		printf("\n");
	}
}

int si46xx_set_property(uint16_t property_id, uint16_t value)
{
	uint8_t data[5];
	char buf[4];
	char *name;

	/* fix this */
	name = si46xx_property_name(property_id, SI46XX_MODE_FM);
	if (name)
		printf("si46xx_set_property(%s, 0x%02X)\n", name, value);
	else
		printf("si46xx_set_property(0x%02X,0x%02X)\n", property_id, value);
	
	data[0] = 0;
	data[1] = property_id & 0xFF;
	data[2] = (property_id >> 8) & 0xFF;
	data[3] = value & 0xFF;
	data[4] = (value >> 8) & 0xFF;
	si46xx_write_data(SI46XX_SET_PROPERTY, data, 5);
	return si46xx_read_reply(buf, sizeof(buf));
}

/*
 * Flash managment commands
 */

int si46xx_flash_erase_chip(void)
{
	int i = 0;
	uint8_t data[3];

	printf("si46xx_flash_erase_chip()\n");
	STORE_U8(0xFF);
	STORE_U8(0xDE);
	STORE_U8(0xC0);

	si46xx_write_data(SI46XX_FLASH_LOAD, data, sizeof(data));
	return si46xx_read(NULL, 4);
}

int si46xx_flash_erase_sector(int addr)
{
	int i = 0;
	uint8_t data[7];

	printf("si46xx_flash_erase_sector()\n");
	STORE_U8(0xFE);
	STORE_U8(0xDE);
	STORE_U8(0xC0);
	STORE_U32(addr);

	si46xx_write_data(SI46XX_FLASH_LOAD, data, sizeof(data));
	return si46xx_read(NULL, 4);
}

int si46xx_flash_property_get(int prop, int *value)
{
	int i = 0;
	int ret;
	uint8_t data[3];
	char buf[6];

	printf("si46xx_flash_property_get(0x%04x)\n", prop);
	STORE_U8(0x11);
	STORE_U16(prop);

	si46xx_write_data(SI46XX_FLASH_LOAD, data, sizeof(data));
	ret = si46xx_read(buf, sizeof(buf));
	if (ret)
		return ret;

	*value = (buf[4] | (buf[5] << 8));
	return 0;
}

#define MAX_BLOCK_SIZE	4084
int si46xx_flash_write(int offset, char *ptr, int size, uint32_t crc, int verify)
{
	int i = 0;
	int ret;
	uint8_t data[MAX_BLOCK_SIZE + 16]; //header

	if (size > MAX_BLOCK_SIZE)
		return -EINVAL;

	/* header */
	if (verify)
		STORE_U8(0xF1);
	else
		STORE_U8(0xF0);
	STORE_U8(0x0C);
	STORE_U8(0xED);
	/* CRC */
	STORE_U32(verify ? crc : 0);
	/* offset */
	STORE_U32(offset);
	/* size */
	STORE_U32(size);
	/* data */
	memcpy(&data[i], ptr, size);
	i += size;

	si46xx_write_data(SI46XX_FLASH_LOAD, data, i);
	return si46xx_read(NULL, ret);
}

int si46xx_flash_load(int offset)
{
	int i = 0;
	int ret;
	uint8_t data[12];
	printf("si46xx_flash_load(0x%08x)\n", offset);

	STORE_U8(0);
	STORE_U8(0);
	STORE_U8(0);

	/* offset */
	STORE_U32(offset);
	/*  */
	STORE_U32(0);

	si46xx_write_data(SI46XX_FLASH_LOAD, data, i);
	return si46xx_read(NULL, 4);
}

int si46xx_init(int argc, char **argv)
{
	int ret;

	if (argc < 2)
		return 0;

	/* check interface */
	if (strstr(argv[1], "spi")) {
		/* 10 MHz, mode = 0 */
		ret = spi_init(argv[1], SPI_DEV_SPEED, 0);

		if (ret) {
			printf("Setup SPI error: %d\n", ret);
			return ret;
		}
		/* used arguments */
	} else if (strstr(argv[1], "i2c")) {
		int addr;

		if (argc < 3) {
			printf("No i2c address provided\n");
			return -1;
		}

		addr = i2c_parse_address(argv[2]);
		if (addr < 0) {
			printf("Can not parse addr %s: %d\n", argv[2], addr);
			return addr;
		}

		ret = i2c_init(argv[1], addr, 0 /* don't care now */);
		if (ret) {
			printf("Setup I2C error: %d\n", ret);
			return ret;
		}

		/* used arguments */
		return 2;
	}
	return 0;
}

int si46xx_init_patch(void)
{
	int ret;
	int mode;

	printf("si46xx_init_patch()\n");

	mode = si46xx_get_sys_mode();

	if (mode != SI46XX_MODE_BOOT) {
		if (mode == SI46XX_MODE_UNK) {
			ret = si46xx_powerup();
			if (ret) {
				printf("Power up failed\n");
				return ret;
			}
		}

		ret = store_image_from_file(FIRMWARE_PATH "patch.bin", 0);
		if (ret) {
			printf("Patch load failed\n");
			return ret;
		}
	}
	return 0;
}

int si46xx_init_mode(int mode)
{
	int ret;
	int cur_mode;
	uint8_t read_data[30];
	printf("si46xx_init_mode(%d)\n", mode);
#if 0
	/* reset si46xx  */
	RESET_LOW();
	msleep(10);
	RESET_HIGH();
	msleep(10);
#endif
	if (mode == si46xx_get_sys_mode()) {
		printf("skip!\n");
		return 0;
	}

	ret = si46xx_init_patch();
	if (ret)
		return ret;

	if (mode == SI46XX_MODE_BOOT) {
		ret = si46xx_load_init();
		if (ret)
			printf("LOAD_INIT failed\n");
		return ret;
	}

	if (mode == SI46XX_MODE_FM) {
		ret = store_image_from_file(FIRMWARE_PATH "fm.bif", 0);
	} else if (mode == SI46XX_MODE_DAB) {
		ret = store_image_from_file(FIRMWARE_PATH "dab.bif", 0);
	} else if (mode == SI46XX_MODE_AM) {
		ret = store_image_from_file(FIRMWARE_PATH "am.bif", 0);
	} else {
		printf("Mode %d not supported\n", mode);
	}
	if (ret) {
		printf("Firmware load failed\n");
		return ret;
	}

	ret = si46xx_boot();
	if (ret) {
		printf("BOOT failed\n");
		return ret;
	}
	ret = si46xx_get_sys_state();
	if (ret) {
		printf("Get sys state failed\n");
		return ret;
	}
	ret = si46xx_get_part_info();
	if (ret) {
		printf("Get part info failed\n");
		return ret;
	}

	return 0;
}


int si46xx_boot_flash(int offset)
{
	int ret;

	printf("si46xx_boot_flash(0x%08x)\n", offset);

	ret = si46xx_init_patch();
	if (ret)
		return ret;

	ret = si46xx_load_init();
	if (ret) {
		printf("LOAD_INIT failed\n");
		return ret;
	}

	ret = si46xx_flash_load(offset);
	if (ret) {
		printf("FLASH_LOAD failed\n");
		return ret;
	}

	ret = si46xx_boot();
	if (ret) {
		printf("BOOT failed\n");
		return ret;
	}

	return ret;
}
