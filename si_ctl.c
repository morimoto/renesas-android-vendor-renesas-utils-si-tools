/*
 * si_ctl - utility to control SiLabs Si46xx radio
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
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include "si46xx.h"
#include "version.h"

int verbose = 0;
int i2s_master = 1;

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

uint32_t frequency_list_nrw[] = {	CHAN_5C,
					CHAN_11D};
uint32_t frequency_list_by[] = {	CHAN_5C,
					CHAN_12D,
					CHAN_11D,
					CHAN_9C,
					CHAN_10C,
					CHAN_11A,
					CHAN_11C,
					CHAN_12A,
					CHAN_6A};
uint32_t frequency_list_bw[] = {	CHAN_5C,
					CHAN_8D,
					CHAN_9D,
					CHAN_11B};
uint32_t frequency_list_bb[] = {	CHAN_5C,
					CHAN_7B,
					CHAN_7D};
uint32_t frequency_list_hb[] = {	CHAN_5C,
					CHAN_7B,
					CHAN_12A};
uint32_t frequency_list_hh[] = {	CHAN_5C,
					CHAN_7A};
uint32_t frequency_list_he[] = {	CHAN_5C,
					CHAN_7B,
					CHAN_11C};
uint32_t frequency_list_mv[] = {	CHAN_5C,
					CHAN_12B};
uint32_t frequency_list_ni[] = {	CHAN_5C,
					CHAN_6A,
					CHAN_6D,
					CHAN_11B,
					CHAN_12A};
uint32_t frequency_list_rp[] = {	CHAN_5C,
					CHAN_11A};
uint32_t frequency_list_sl[] = {	CHAN_5C,
					CHAN_9A};
uint32_t frequency_list_sn[] = {	CHAN_5C,
					CHAN_6C,
					CHAN_8D,
					CHAN_9A,
					CHAN_12A};
uint32_t frequency_list_st[] = {	CHAN_5C,
					CHAN_11C,
					CHAN_12C};
uint32_t frequency_list_sh[] = {	CHAN_5C,
					CHAN_9C};
uint32_t frequency_list_th[] = {	CHAN_5C,
					CHAN_7B,
					CHAN_9C,
					CHAN_12B};
uint32_t frequency_list_it_sue[] = {	CHAN_10B,
					CHAN_10C,
					CHAN_10D,
					CHAN_12A,
					CHAN_12B,
					CHAN_12C};
uint32_t frequency_list_ch[] = {	CHAN_12A,
					CHAN_12C,
					CHAN_12D,
					CHAN_7D,
					CHAN_7A,
					CHAN_9D,
					CHAN_8B};

int init_am(int offset)
{
	int ret;

	if (offset > 0)
		ret = si46xx_boot_flash(offset);
	else
		ret = si46xx_init_mode(SI46XX_MODE_AM);

	if (ret)
		return ret;
	/*
	 * enable I2S output
	 */
	si46xx_set_property(SI46XX_PIN_CONFIG_ENABLE, 0x0003);

	si46xx_set_property(SI46XX_AM_SEEK_FREQUENCY_SPACING, 1);
	si46xx_set_property(SI46XX_AM_SEEK_BAND_BOTTOM, 500);
	si46xx_set_property(SI46XX_AM_SEEK_BAND_TOP, 1700);
	si46xx_set_property(SI46XX_AM_VALID_RSSI_THRESHOLD, 15);
	si46xx_set_property(SI46XX_AM_VALID_SNR_THRESHOLD, 2);
	/*
	 * rate
	 */
	//si46xx_set_property(SI46XX_DIGITAL_IO_OUTPUT_SAMPLE_RATE, 48000);
	/*
	 * master or slave mode
	 */
	if (i2s_master)
		si46xx_set_property(SI46XX_DIGITAL_IO_OUTPUT_SELECT, 0x8000);
	else
		si46xx_set_property(SI46XX_DIGITAL_IO_OUTPUT_SELECT, 0x0);
	/*
	 * sample size = 16
	 * slot size = 32
	 */
	si46xx_set_property(SI46XX_DIGITAL_IO_OUTPUT_FORMAT,
		(16 << 8) |	//sample size 16
		(4 << 4) |	//slot size 16
		(0 << 0));	//right_j mode

	return 0;
}

int init_fm(int offset)
{
	int ret;

	if (offset > 0)
		ret = si46xx_boot_flash(offset);
	else
		ret = si46xx_init_mode(SI46XX_MODE_FM);

	if (ret)
		return ret;
	/*
	 * enable I2S output
	 */
	si46xx_set_property(SI46XX_PIN_CONFIG_ENABLE, 0x0003);
	//si46xx_set_property(SI46XX_FM_VALID_RSSI_THRESHOLD,0x0000);
	//si46xx_set_property(SI46XX_FM_VALID_SNR_THRESHOLD,0x0000);
	si46xx_set_property(SI46XX_FM_SOFTMUTE_SNR_LIMITS, 0x0000); // set the SNR limits for soft mute attenuation
	si46xx_set_property(SI46XX_FM_TUNE_FE_CFG, 0x0000); // front end switch open
	si46xx_set_property(SI46XX_FM_SEEK_BAND_BOTTOM, 88000 / 10);
	si46xx_set_property(SI46XX_FM_SEEK_BAND_TOP, 108000 / 10);
	/*
	 * rate
	 */
	//si46xx_set_property(SI46XX_DIGITAL_IO_OUTPUT_SAMPLE_RATE, 48000);
	/*
	 * master or slave mode
	 */
	if (i2s_master)
		si46xx_set_property(SI46XX_DIGITAL_IO_OUTPUT_SELECT, 0x8000);
	else
		si46xx_set_property(SI46XX_DIGITAL_IO_OUTPUT_SELECT, 0x0);
	/*
	 * sample size = 16
	 * slot size = 32
	 */
	si46xx_set_property(SI46XX_DIGITAL_IO_OUTPUT_FORMAT,
		(16 << 8) |	//sample size 16
		(4 << 4) |	//slot size 16
		(0 << 0));	//right_j mode
	si46xx_set_property(SI46XX_FM_RDS_CONFIG, 0x0001); // enable RDS
	si46xx_set_property(SI46XX_FM_AUDIO_DE_EMPHASIS, SI46XX_AUDIO_DE_EMPHASIS_EU); // set de-emphasis for Europe

	return 0;
}
int init_dab(int offset)
{
	int ret;

	if (offset > 0)
		ret = si46xx_boot_flash(offset);
	else
		ret = si46xx_init_mode(SI46XX_MODE_DAB);

	if (ret)
		return ret;
	si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_nrw),frequency_list_nrw);
	si46xx_set_property(SI46XX_DAB_CTRL_DAB_MUTE_SIGNAL_LEVEL_THRESHOLD,0);
	si46xx_set_property(SI46XX_DAB_CTRL_DAB_MUTE_SIGLOW_THRESHOLD,0);
	si46xx_set_property(SI46XX_DAB_CTRL_DAB_MUTE_ENABLE,0);
	si46xx_set_property(SI46XX_DIGITAL_SERVICE_INT_SOURCE,1); // enable DSRVPAKTINT interrupt ??
	si46xx_set_property(SI46XX_DAB_TUNE_FE_CFG,0x0001); // front end switch closed
	si46xx_set_property(SI46XX_DAB_TUNE_FE_VARM,0x1710); // Front End Varactor configuration (Changed from '10' to 0x1710 to improve receiver sensitivity - Bjoern 27.11.14)
	si46xx_set_property(SI46XX_DAB_TUNE_FE_VARB,0x1711); // Front End Varactor configuration (Changed from '10' to 0x1711 to improve receiver sensitivity - Bjoern 27.11.14)
	si46xx_set_property(SI46XX_PIN_CONFIG_ENABLE,0x0003); // enable I2S output
	return si46xx_dab_tune_freq(0,0);
}

//static struct global_args_t{
//	int dab;
//	int fm;
//	int dab_start_service;
//	int frequency;
//	int dab_service_list;
//} global_args;
//
//static struct option long_options[]=
//{
//	{"dab_start_service", required_argument, &global_args.dab_start_service,0},
//	{"frequency", required_argument,&global_args.frequency,0},
//	{"status", no_argument,0,'s'},
//	{"dab", no_argument,&global_args.dab, 'd'},
//	{"fm", no_argument,&global_args.fm, 'f'},
//	{"dab_service_list", no_argument, &global_args.dab_service_list,0},
//	{0,0,0,0}
//};
//

int output_help(char *prog_name)
{
	printf("usage: %s -a/b [am|fm|dab]\n",prog_name);
	printf("Init:\n");
	printf("  -a             init AM/FM/DAB mode (firmware from file)\n");
	printf("  -b             boot AM/FM/DAB image from flash\n");
	printf("  -s             get sys state (fm, dab, am...)\n");
	printf("Common AM/FM:\n");
	printf("  -c frequency   FM/AM tune KHz frequency\n");
	printf("  -l up|down     FM/AM seek next station\n");
	printf("  -d             FM/AM RSQ status\n");
	printf("  -m             FM rds status\n");
	printf("DAB only:\n");
	printf("  -e             dab status\n");
	printf("  -f service     start service of dab service list\n");
	printf("  -g             get dab service list\n");
	printf("  -i channel     tune to channel in dab frequency list\n");
	printf("  -j region      set frequency list (-v for list)\n");
	if (verbose) {
		printf("                    0   Baden-Wuertemberg\n");
		printf("                    1   Bayern\n");
		printf("                    2   Berlin-Brandenburg\n");
		printf("                    3   Bremen\n");
		printf("                    4   Hamburg\n");
		printf("                    5   Hessen\n");
		printf("                    6   Mecklenburg-Vorpommern\n");
		printf("                    7   Niedersachsen\n");
		printf("                    8   Nordrhein-Westfalen\n");
		printf("                    9   Rheinland-Pfalz\n");
		printf("                    10  Saarland\n");
		printf("                    11  Sachsen\n");
		printf("                    12  Sachsen-Anhalt\n");
		printf("                    13  Schleswig-Holstein\n");
		printf("                    14  Thueringen\n");
		printf("                    15  Suedtirol (Italien)\n");
		printf("                    16  Schweiz\n");
	}
	printf("  -k region      scan frequency list\n");
	printf("  -n             dab get audio info\n");
	printf("  -o             dab get subchannel info\n");
	printf("  -v(vvv)        verbose\n");
	printf("  -h             this help\n");
	if (verbose)
		printf("\nsi_ctl version %s\n", GIT_VERSION);

	return 0;
}

void load_regional_channel_list(uint8_t tmp)
{
	if(tmp == 0){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_bw),
					frequency_list_bw);
	}else if(tmp == 1){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_by),
					frequency_list_by);
	}else if(tmp == 2){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_bb),
					frequency_list_bb);
	}else if(tmp == 3){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_hb),
					frequency_list_hb);
	}else if(tmp == 4){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_hh),
					frequency_list_hh);
	}else if(tmp == 5){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_he),
					frequency_list_he);
	}else if(tmp == 6){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_mv),
					frequency_list_mv);
	}else if(tmp == 7){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_ni),
					frequency_list_ni);
	}else if(tmp == 8){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_nrw),
					frequency_list_nrw);
	}else if(tmp == 9){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_rp),
					frequency_list_rp);
	}else if(tmp == 10){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_sl),
					frequency_list_sl);
	}else if(tmp == 11){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_sn),
					frequency_list_sn);
	}else if(tmp == 12){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_st),
					frequency_list_st);
	}else if(tmp == 13){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_sh),
					frequency_list_sh);
	}else if(tmp == 14){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_th),
					frequency_list_th);
	}else if(tmp == 15){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_it_sue),
					frequency_list_it_sue);
	}else if(tmp == 16){
		si46xx_dab_set_freq_list(ARRAY_SIZE(frequency_list_ch),
					frequency_list_ch);
	}else{
		printf("Region %d not implemented\n",tmp);
	}
}

int mode_booted(int mode)
{
	return ((mode == SI46XX_MODE_AM) ||
		(mode == SI46XX_MODE_FM) ||
		(mode == SI46XX_MODE_DAB));
}

int main(int argc, char **argv)
{
	int ret = 0;
	int c;
	int frequency = -1;
	int offset = - 1;
	int mode;
	int tmp;
	struct dab_digrad_status_t dab_digrad_status;
	bool init = false;
	bool seek_up = false;
	bool seek_down = false;
	bool rsq_status = true;
	bool rds_status = false;
	bool sys_status = false;
	bool show_help = false;

	if (argc == 1)
		return output_help(argv[0]);

	ret = si46xx_init(argc, argv);
	if (ret < 0) {
		printf("Error opening interface to Si: %d\n", ret);
		return ret;
	}

	optind = 0;
	while (optind < argc) {
		if ((c = getopt(argc, argv, "a:b:c:def:ghi:j:k:l:mnosv")) != -1) {
			switch(c){
			/* init */
			case 'a':
			case 'b':
				init = true;
				if (strcmp(optarg, "dab") == 0) {
					mode = SI46XX_MODE_DAB;
					if (c == 'b')
						offset = FLASH_OFFSET_DAB;
				} else if (strcmp(optarg, "am") == 0) {
					mode = SI46XX_MODE_AM;
					if (c == 'b')
						offset = FLASH_OFFSET_AM;
				} else if (strcmp(optarg, "fm") == 0) {
					mode = SI46XX_MODE_FM;
					if (c == 'b')
						offset = FLASH_OFFSET_FM;
				} else {
					printf("Invalid mode: %s\n", optarg);
					return -EINVAL;
				}
				break;
			/* common for all modes */
			case 'v':
				verbose++;
				break;
			case 'h':
				show_help = true;
				break;
			case 's':
				sys_status = true;
				break;
			case 'd':
				rsq_status = true;
				break;
			case 'm':
				rds_status = true;
				break;
			case 'l':
				if (!strcmp(optarg, "down"))
					seek_down = true;
				else
					seek_up = true;
				break;
			case 'c':
				frequency = atoi(optarg);
				break;
			/* DAB stuff. TODO: rework */
			case 'e':
				si46xx_dab_digrad_status(&dab_digrad_status);
				si46xx_dab_digrad_status_print(&dab_digrad_status);
				break;
			case 'f':
				si46xx_dab_get_digital_service_list();
				si46xx_dab_print_service_list();
				si46xx_dab_start_digital_service_num(atoi(optarg));
				break;
			case 'g':
				si46xx_dab_get_digital_service_list();
				si46xx_dab_print_service_list();
				break;
			case 'i':
				si46xx_dab_tune_freq(atoi(optarg),0);
				break;
			case 'j':
				tmp = atoi(optarg);
				load_regional_channel_list(tmp);
				break;
			case 'k':
				tmp = atoi(optarg);
				load_regional_channel_list(tmp);
				si46xx_dab_scan();
				break;
			case 'n':
				si46xx_dab_get_audio_info();
				break;
			case 'o':
				si46xx_dab_get_subchannel_info();
				break;
			/* invalid option */
			default:
				return output_help(argv[0]);
			}
		} else {
			printf("unknown argument: %s\n", optarg);
			optind++;
		}
	}

	if (show_help)
		return output_help(argv[0]);

	/* Init */
	if (init) {
		switch (mode) {
			case SI46XX_MODE_FM:
				ret = init_fm(offset);
				if (frequency < 0)
					frequency = 105500;
			break;
			case SI46XX_MODE_AM:
				ret = init_am(offset);
				if (frequency < 0)
					frequency = 69470;
			break;
			case SI46XX_MODE_DAB:
				ret = init_dab(offset);
			break;
			default:
				printf("Invalid mode selected\n");
				ret = -EINVAL;
			break;
		}
		if (ret) {
			printf("Init failed %d\n", ret);
			return ret;
		}
	}

	/* Get current mode */
	mode = si46xx_get_sys_mode();
	if (mode < 0)
		return mode;

	/* Tune frequency */
	if (frequency > 0) {
		if (!mode_booted(mode)){
			printf("Invalid mode (no FW loaded?)\n");
			return -EINVAL;
		}
		ret = si46xx_tune_freq(mode, frequency, 0);
		if (ret) {
			printf("Tune failed %d\n", ret);
			return ret;
		}
		/* wait done */
		ret = si46xx_tune_wait(TIMEOUT_TUNE);
		if (ret) {
			printf("Tune wait failed: %d\n", ret);
			return ret;
		}
	}

	/* Seek */
	if ((seek_up) || (seek_down)) {
		if (!mode_booted(mode)){
			printf("Invalid mode (no FW loaded?)\n");
			return -EINVAL;
		}
		/* seek */
		ret = si46xx_seek_start(mode, seek_up, 1);
		if (ret) {
			printf("Seek start failed: %d\n", ret);
			return ret;
		}
		/* wait done */
		ret = si46xx_tune_wait(TIMEOUT_SEEK);
		if (ret) {
			printf("Seek wait failed: %d\n", ret);
			return ret;
		}
	}

	/* Sys status */
	if (sys_status) {
		ret = si46xx_get_sys_state();
		if (ret) {
			printf("Get Sys state failed: %d\n", ret);
			return ret;
		}
	}

	/* RSQ status */
	if (rsq_status) {
		if (!mode_booted(mode)){
			printf("Invalid mode (no FW loaded?)\n");
			return -EINVAL;
		}
		ret = si46xx_rsq_status(mode);
		if (ret) {
			printf("Get RSQ status failed %d\n", ret);
			return ret;
		}
	}

	/* RDS status */
	if (rds_status) {
		if (!mode_booted(mode)){
			printf("Invalid mode (no FW loaded?)\n");
			return -EINVAL;
		}
		ret = si46xx_fm_rds_status();
		if (ret) {
			printf("Get RDS status failed: %d\n", ret);
			return ret;
		}
		ret = si46xx_fm_rds_blockcount();
		if (ret) {
			printf("Get RDS blockcount failed: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

