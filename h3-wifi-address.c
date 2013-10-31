/*
 *  Copyright (c) 2013, evilwombat
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>

#include "crc32.h"

#define SIZE_OFFSET 0x3f8
#define CRC_OFFSET 0x3fc

static int h3b_v300_wifi_addr_patch[] = {
	0x4164,
	0x44b0,
	0x45c0,
	0x225f2,
	0x2287a,
	0x6a38a,
	-1
};

static int h3pb_v104_wifi_addr_patch[] = {
	0x42b8,
	0x4644,
	0x47b6,
	0x2342c,
	0x236fc,
	0x6a59a,
	-1
};

struct wifi_fw_type {
	const char *name;
	unsigned int size;
	unsigned int crc;
	unsigned char old_val;
	int *patch;
};

struct wifi_fw_type wifi_fw_list[] = {
	{
		.name	 = "Hero3 (White/Silver/Black) v300 Wifi Firmware",
		.size	 = 0x0006aecc,
		.crc	 = 0x815f3add,
		.old_val = 5,
		.patch	 = h3b_v300_wifi_addr_patch,
	},
	{
		.name	 = "Hero3 Plus Black (and Silver?) v104 Wifi Firmware",
		.size	 = 0x0006b5d4,
		.crc	 = 0x95d70245,
		.old_val = 5,
		.patch	 = h3pb_v104_wifi_addr_patch,
	},
	{ },	/* End of list */
};

static int read_file(FILE *fd, unsigned char *buf, int size);
static int read_file(FILE *fd, unsigned char *buf, int size)
{
	int ret;
	ret = fread(buf, size, 1, fd);
	return ret == 1 ? 0 : -1;
}

/*
 * FW header appears to be big-endian ?
 */
static uint32_t read_word(unsigned char *buf, int offset);
static uint32_t read_word(unsigned char *buf, int offset)
{
	return (buf[offset] << 24) |
	       (buf[offset+1] << 16) |
	       (buf[offset+2] << 8) |
	       (buf[offset+3]);
}

static void write_word(unsigned char *buf, int offset, uint32_t word);
static void write_word(unsigned char *buf, int offset, uint32_t word)
{
	buf[offset] = word >> 24;
	buf[offset+1] = word >> 16;
	buf[offset+2] = word >> 8;
	buf[offset+3] = word;
}

static int patch_buffer(unsigned char *buf, int *offsets, unsigned char val, unsigned char old_val);
static int patch_buffer(unsigned char *buf, int *offsets, unsigned char val, unsigned char old_val)
{
	int i;

	for (i = 0; offsets[i] != -1; i++) {
		if (buf[offsets[i]] != old_val) {
			printf("Patching mismatch at offset %08x, expected %02x, got %02x\n",
			       offsets[i], old_val, buf[offsets[i]]);
			return -1;
		}

		buf[offsets[i]] = val;
	}

	return 0;
}

static int save_file(const char *output_name, int size, unsigned char *buf);
static int save_file(const char *output_name, int size, unsigned char *buf)
{
	FILE *ofd;
	int ret;
	ofd = fopen(output_name, "wb+");
	if (!ofd) {
		printf("Could not write to %s\n", output_name);
		return -1;
	}
	ret = fwrite(buf, size, 1, ofd);
	fclose(ofd);

	return ret == 1 ? 0 : -1;
}

static void print_usage(const char *name);
static void print_usage(const char *name)
{
	printf("Usage: %s [firmware file] [address digit] [output filename]\n", name);

	printf("Patch Gopro Wifi Firmware with custom IP address\n");
	printf("New camera address will be 10.X.5.9\n");
	printf("New computer address will be 10.X.5.109\n");
	printf("Where X is the address digit specified\n");
}

int main(int argc, char **argv)
{
	FILE *in_fd;
	char *fname, *output_name;
	struct stat st;
	int ret;
	unsigned int size, hdr_size;
	unsigned char *buf;
	uint32_t crc, hdr_crc;
	int patch_byte = 8;
	struct wifi_fw_type *wifi_fw = wifi_fw_list;

	printf("MAKE SURE YOU HAVE READ THE INSTRUCTIONS!\n");
	printf("The author makes absolutely NO GUARANTEES of the correctness of this program\n");
	printf("and takes absolutely NO RESPONSIBILITY OR LIABILITY for any consequences that\n");
	printf("arise from its use. This program could SEVERELY mess up your camera, totally\n");
	printf("destroy it, cause it to catch fire. It could also destroy your computer, burn\n");
	printf("down your house, etc. The author takes no responsibility for the consequences\n");
	printf("of using this program. Use it at your own risk! You have been warned.\n");
	printf("\n");

	if (argc != 4) {
		print_usage(argv[0]);
		return -1;
	}

	fname = argv[1];

	patch_byte = atoi(argv[2]);

	if (patch_byte < 0 || patch_byte > 255) {
		printf("Bad patching value: %d\n", patch_byte);
		return -1;
	}

	printf("Patching camera address to 10.%d.5.9\n", patch_byte);
	printf("Patching computer address to 10.%d.5.109\n", patch_byte);
	printf("\n");

	output_name = argv[3];

	ret = stat(fname, &st);
	if (ret) {
		printf("Error: Could not stat %s\n", fname);
		return -1;
	}

	size = st.st_size;
	if (size <= 0) {
		printf("Bad file size: %d\n", size);
		return -1;
	}

	buf = malloc(size);
	if (!buf) {
		printf("Could not allocate %d bytes\n", size);
		return -1;
	}

	in_fd = fopen(fname, "rb");

	ret = read_file(in_fd, buf, size);
	if (ret) {
		printf("Could not read file: %d\n", ret);
		return -1;
	}
	fclose(in_fd);

	hdr_size = read_word(buf, SIZE_OFFSET);
	hdr_crc = read_word(buf, CRC_OFFSET);

	printf("Wifi FW header reports size: %08x\n", hdr_size);
	printf("Wifi FW header reports CRC : %08x\n", hdr_crc);

	printf("Actual file size: %08x\n", size);
	if (size != hdr_size) {
		printf("File size does not match size reported in header.\n");
		goto fail;
	}

	write_word(buf, CRC_OFFSET, 0x00000000);
	crc = crc32(buf, size);

	printf("Actual file CRC : %08x\n", crc);
	if (crc != hdr_crc) {
		printf("File CRC does not match CRC reported in header.\n");
		goto fail;
	}

	/*
	 * Checking both the size and CRC is probably redundant, but we may
	 * as well be careful
	 */
	while (wifi_fw->name) {
		if (wifi_fw->size == size && wifi_fw->crc == crc)
			break;
		wifi_fw++;
	}

	if (wifi_fw->name == NULL) {
		printf("\nUnrecognized firmware file!\n");
		printf("This only works on the following firmware files:\n");

		wifi_fw = wifi_fw_list;
		while (wifi_fw->name) {
			printf("\t * %s\n", wifi_fw->name);
			wifi_fw++;
		}

		goto fail;
	}

	printf("Detected firmware type: \"%s\"\n", wifi_fw->name);

	printf("Patching firmware...\n");
	ret = patch_buffer(buf, wifi_fw->patch, patch_byte, wifi_fw->old_val);
	if (ret) {
		printf("Error patching buffer: %d\n", ret);
		goto fail;
	}

	crc = crc32(buf, size);
	printf("New CRC: %08x\n", crc);
	write_word(buf, CRC_OFFSET, crc);

	printf("Saving output file: %s\n", output_name);
	ret = save_file(output_name, size, buf);
	if (ret) {
		printf("Error saving file: %d\n", ret);
		goto fail;
	}
	printf("Done.\n");

fail:
	free(buf);
	return 0;
}
