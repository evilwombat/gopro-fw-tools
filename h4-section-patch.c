/*
 *  Copyright (c) 2013-20144, evilwombat
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>

#include "crc32.h"

#define GLOBAL_HEADER_SIZE	224

unsigned char *read_file(const char *fname, unsigned int *out_size);
unsigned char *read_file(const char *fname, unsigned int *out_size)
{
	unsigned char *buf;
	struct stat st;
	int size, ret;
	FILE *fd;

	ret = stat(fname, &st);
	if (ret) {
		printf("Error: Could not stat %s\n", fname);
		return NULL;
	}

	size = st.st_size;
	if (size <= 0) {
		printf("Bad file size of %s: %d\n", fname, size);
		return NULL;
	}

	fd = fopen(fname, "rb");
	if (!fd) {
		printf("Error opening file %s\n", fname);
		return NULL;
	}
		
	buf = malloc(size);
	if (!buf) {
		printf("Could not allocate %d bytes\n", size);
		fclose(fd);
		return NULL;
	}

	ret = fread(buf, size, 1, fd);
	
	if (ret != 1) {
		printf("Error reading file %s: %d\n", fname, ret);
		free(buf);
		fclose(fd);
		return NULL;
	}

	*out_size = size;
	fclose(fd);
	
	return buf;
}

/*
 * FW header appears to be big-endian ?
 */
static uint32_t read_word_le(unsigned char *buf, int offset);
static uint32_t read_word_le(unsigned char *buf, int offset)
{
	return (buf[offset+0] << 0) |
	       (buf[offset+1] << 8) |
	       (buf[offset+2] << 16) |
	       (buf[offset+3] << 24);
}

static void write_word_le(unsigned char *buf, int offset, uint32_t word);
static void write_word_le(unsigned char *buf, int offset, uint32_t word)
{
	buf[offset+0] = word >> 0;
	buf[offset+1] = word >> 8;
	buf[offset+2] = word >> 16;
	buf[offset+3] = word >> 24;
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
	printf("Usage: %s unpatched_firmware.bin section_filename section_number output_firmware.bin\n\n", name);
	printf("unpatched_firmware.bin - original, unpatched camera_firmware.bin file\n");
	printf("section_filename       - filename of replacement section being packed into the firmware\n");
	printf("section_number         - number of section to replace\n");
	printf("output_firmware.bin    - filename for where to write the modified camera_firmware.bin file\n");
}

static int find_magic(unsigned char *buf, int size, int start_offset);
static int find_magic(unsigned char *buf, int size, int start_offset)
{
	int c;
	int state = 0;
	int offset = start_offset;
	
	while (offset < size) {
		c = buf[offset++];
		if (c < 0)
			return -1;

		switch (state) {
			case 0:
				if (c == 0x90)
					state = 1;
				else
					state = 0;

				break;
			case 1:
				if (c == 0xEB)
					state = 2;
				else
					state = 0;

				break;

			case 2:
				if (c == 0x24)
					state = 3;
				else
					state = 0;
				break;

			case 3:
				if (c == 0xA3) {
					return offset;

				} else
					state = 0;
				break;

			default:
				fprintf(stderr, "Unexpected state %u in find_magic", state);
				return -1;
		}

		if (c == 0x90)
			state = 1;
	}

	return -1;
}

struct section_info {
	unsigned int header_crc;
	unsigned int actual_crc;
	unsigned int version;
	unsigned int build_date;
	unsigned int flags;
	unsigned int magic;
	unsigned int offset;
	unsigned int length;
};

void print_sections(struct section_info *sections, int num_sections);
void print_sections(struct section_info *sections, int num_sections)
{
	int i;
	printf("Section\t\t  Offset\t  Length\t     CRC\n");
	printf("========================================================\n");
	for (i = 0; i < num_sections; i++) {
		printf("section_%d\t%8d\t%8d\t%08x (%s)\n",
		       i, sections[i].offset, sections[i].length, sections[i].header_crc,
			(sections[i].header_crc == sections[i].actual_crc) ? "OK" : "MISMATCH!"
		);
	}
}

unsigned int get_global_crc(unsigned char *buf, int size);
unsigned int get_global_crc(unsigned char *buf, int size)
{
	if (size < GLOBAL_HEADER_SIZE) {
		printf("Invalid firmware size: %d\n", size);
		return 0;
	}
	
	return crc32(buf + GLOBAL_HEADER_SIZE, size - GLOBAL_HEADER_SIZE);
}

int parse_firmware(unsigned char *buf, int size, struct section_info *output, unsigned int max_sections);
int parse_firmware(unsigned char *buf, int size, struct section_info *output, unsigned int max_sections)
{
	unsigned int section_offset, num = 0;
	unsigned int global_header_crc, global_actual_crc;
	int length;
	int offset = 0;
	
	if (size < GLOBAL_HEADER_SIZE) {
		printf("Invalid firmware size: %d\n", size);
		return -1;
	}
	
	global_header_crc = read_word_le(buf, 0);
	global_actual_crc = get_global_crc(buf, size);
	
	printf("Global header CRC: %08x\n", global_header_crc);
	printf("Global actual CRC: %08x (%s)\n", global_actual_crc,
		(global_header_crc == global_actual_crc) ? "OK" : "MISMATCH!");
	
	if (global_header_crc != global_actual_crc) {
		printf("DANGER!!! Firmware global CRC does not match the CRC listed in the header!\n");
		printf("This is a bad thing. This firmware looks invalid.\n");
		return -1;
	}
	
	while (1) {
		if (num >= max_sections) {
			printf("Firmware contains more than %d sections. Something must be wrong.\n", max_sections);
			return -1;
		}
		
		offset = find_magic(buf, size, offset);
		if (offset < 0) {
			return num;
		}
	
		offset -= 28;
		output[num].header_crc = read_word_le(buf, offset);
		offset += 4;

		output[num].version = read_word_le(buf, offset);
		offset += 4;

		output[num].build_date = read_word_le(buf, offset);
		offset += 4;
		
		length = read_word_le(buf, offset);
		offset += 4;
		offset += 4;

		output[num].flags = read_word_le(buf, offset);
		offset += 4;

		output[num].magic = read_word_le(buf, offset);
		offset += 4;

		offset += 0x100 - 28;

		section_offset = offset;

		output[num].length = length;
		output[num].offset = section_offset;
		
		if (length < 0)
			continue;

		output[num].actual_crc = crc32(buf + section_offset, length);

		if (output[num].header_crc != output[num].actual_crc) {
			printf("WARNING!!! CRC MISMATCH WHILE PARSING SECTION %d\n", num);
			printf("Header CRC = %08x, Actual CRC = %08x\n",
			       output[num].header_crc, output[num].actual_crc);
			return -1;
		}
		
		offset += length;
		
		num++;
	}
}

#define MAX_SECTIONS	100

int main(int argc, char **argv)
{
	char *fname, *sname, *oname;
	int ret;
	int target_section;
	unsigned char *fw_buf, *replacement_buf;
	unsigned int fw_size, replacement_size;
	int num_sections;
	int old_num_sections;
	unsigned int new_section_crc, new_global_crc;
	struct section_info sections[MAX_SECTIONS];
	
	printf("evilwombat's magical firmware section patching tool.\n");
	printf("This program is incomplete, undocumented, and unfit for any purpose whatsoever.\n");
	printf("This program is provided entirely AS-IS without ANY warranty, and is unfit for ANY\n");
	printf("purpose. The author bears NO RESPONSIBILITY for ANY consequences of the use of this\n");
	printf("software, including, but not limited to, permanent to damage or destruction of\n");
	printf("your camera, someone else's camera, or any damage to anything, or any consequences\n");
	printf("whatsoever.\n");
	printf("\n");
	printf("Additionally, if you use this program, you are agreeing that your camera's warranty\n");
	printf("is VOIDED, and that you will not attempt to get any kind of warranty coverage or\n");
	printf("ask for any kind of warranty repair for your camera.\n");
	
	printf("IF YOU BRICK IT OR DAMAGE IT, IT'S YOUR FAULT. Don't try to get it replaced. We don't\n");
	printf("need the manufacturer complaining that hobby people are killing their cameras and\n");
	printf("getting mad at us for it.\n");
	printf("\nYOU HAVE BEEN WARNED\n");
	printf("\nMoreover, this program is INCOMPLETE and probably nonfunctional.\n");
	printf("DO NOT USE THIS PROGRAM!\n\n");
	
	if (argc != 5) {
		print_usage(argv[0]);
		return -1;
	}
	
	fname = argv[1];
	sname = argv[2];
	target_section = atoi(argv[3]);
	oname = argv[4];

	printf("Replacing section %d in file %s with file %s, and writing output to %s\n",
	       target_section, fname, sname, oname);

	fw_buf = read_file(fname, &fw_size);
	
	if (!fw_buf) {
		printf("Could not read in original firmware file %s. Exiting.\n", fname);
		return -1;
	}
	
	replacement_buf = read_file(sname, &replacement_size);
	
	if (!replacement_buf) {
		printf("Could not read in replacement section file %s. Exiting.\n", sname);
		return -1;
	}

	printf("\nDecoding contents of %s...\n", fname);
	num_sections = parse_firmware(fw_buf, fw_size, sections, MAX_SECTIONS);
	if (num_sections <= 0) {
		printf("This firmware looks invalid. Exiting.\n");
		return -1;
	}
	printf("\nFound %d sections in file %s:\n", num_sections, fname);
	print_sections(sections, num_sections);
	printf("\n");

	if (target_section >= num_sections) {
		printf("This firmware file (%s) only contains %d sections, and you are\n", fname, num_sections);
		printf("trying to replace section %d (and they are numbered from 0).\n", target_section);
		printf("Are you sure you know what you are doing?\n");
		return -1;
	}
	
	printf("Okay. Trying to replace section_%d with %s\n", target_section, sname);
	
	if (sections[target_section].length > replacement_size) {
		printf("\n******************************************************************************\n");
		printf("WARNING!! The replacement section is smaller than the section in the firmware.\n");
		printf("In the firmware, section_%d is %d bytes long.\n",
		       target_section, sections[target_section].length);
		printf("Your replacement file for section_%d is only %d bytes long.\n",
		       target_section, replacement_size);
		printf("Your replacement section is smaller than the target section by %d bytes.\n",
			sections[target_section].length - replacement_size);
		printf("\nThis might not necessarily be a bad thing, depending on what you are doing.\n");
		printf("If you continue, the section will be zero-padded to the expected length.\n");
		printf("******************************************************************************\n");
		printf("Would you like to proceed? (y/n)\n");
		
		if (getchar() != 'y') {
			printf("Operation aborted by user\n");
			return -1;
		}
	}
	
	if (sections[target_section].length < replacement_size) {
		printf("\n**************************************************************\n");
		printf("ERROR!! The replacement section will not fit into the firmware.\n");
		printf("Your replacement file for section_%d has length %d bytes.\n", target_section, replacement_size);
		printf("In the firmware, this section is only %d bytes long.\n", sections[target_section].length);
		printf("Your replacement section is too long by %d bytes.\n",
			replacement_size - sections[target_section].length);
		printf("This will not work.\n");
		return -1;
	}

	printf("\nReplacing target section...\n");
	/* Zero out replacement section in case we are zero-padding */
	memset(fw_buf + sections[target_section].offset, 0, sections[target_section].length);
	memcpy(fw_buf + sections[target_section].offset, replacement_buf, replacement_size);

	printf("Updating CRCs...\n");
	new_section_crc = crc32(fw_buf + sections[target_section].offset, sections[target_section].length);
	printf("New section CRC: %08x\n", new_section_crc);
	
	write_word_le(fw_buf, sections[target_section].offset - 0x100, new_section_crc);

	new_global_crc = get_global_crc(fw_buf, fw_size);
	printf("New global CRC: %08x\n", new_global_crc);
	
	write_word_le(fw_buf, 0, new_global_crc);
	
	old_num_sections = num_sections;

	printf("\nRescanning resulting firmware for sanity...\n");
	num_sections = parse_firmware(fw_buf, fw_size, sections, MAX_SECTIONS);
	if (num_sections <= 0) {
		printf("The new firmware looks invalid!!\nThis is definitely a bug in this program.\n");
		printf("Please contact evilwombat and report how this happened.\n");
		return -1;
	}
	printf("\nFound %d sections in the new firmware:\n", num_sections);
	print_sections(sections, num_sections);

	if (num_sections != old_num_sections) {
		printf("Old and new section count does not match!!\nThis is definitely a bug in this program.\n");
		printf("Please contact evilwombat and report how this happened.\n");
		return -1;
	}
	
	printf("\nSaving new firmware to file %s...\n", oname);
	ret = save_file(oname, fw_size, fw_buf);
	if (ret) {
		printf("Error saving file!\n");
		return -1;
	}
	printf("Done.\n");

	return 0;
}
