/*
 *  Copyright (c) 2012-2013, evilwombat
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

static FILE *fd;

/* Magic is 0xA3 0x24 0xEB 0x90 */

static int find_magic(void);
static int find_magic(void)
{
	int c;
	int state = 0;
	
	while (!feof(fd)) {
		c = fgetc(fd);
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
					return 0;

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

static unsigned int read_word(void);
static unsigned int read_word(void)
{
	unsigned int r = 0;
	r |= fgetc(fd) <<  0;
	r |= fgetc(fd) <<  8;
	r |= fgetc(fd) << 16;
	r |= fgetc(fd) << 24;
	return r;
}

static int save_section(const char *output_name, int length);
static int save_section(const char *output_name, int length)
{
	FILE *ofd;
	int i;
	char t;
	ofd = fopen(output_name, "wb+");

	if (!ofd) {
		printf("Could not write to %s\n", output_name);
		return -1;
	}

	for (i = 0; i < length; i++) {
		fread(&t, 1, 1, fd);
		fwrite(&t, 1, 1, ofd);
	}
	fclose(ofd);
	return 0;
}

/*
 * Thanks to this guy for info on the header format:
 * https://gist.github.com/2394361
 */
int main(int argc, char **argv)
{
	int verbose = 0;
	int ret = 0;
	unsigned int crc, version, build_date, flags, magic;
	unsigned int section_offset, num = 0;
	int length;
	char *fname;
	char name_buf[20];

	if (argc != 2) {
		printf("Usage: %s [firmware_file]\n", argv[0]);
		return -1;
	}

	fname = argv[1];
	
	fd = fopen(fname, "rb");
	if (!fd) {
		printf("Could not open %s\n", fname);
		return -1;
	}

	while (1) {
		ret = find_magic();
		if (ret < 0) {
			printf("End of file reached.\n");
			fclose(fd);
			return 0;
		}
	
		fseek(fd, -28, SEEK_CUR);
		crc = read_word();
		version = read_word();
		build_date = read_word();
		length = read_word();
		read_word();
		flags = read_word();
		magic = read_word();
		fseek(fd, 0x100-28, SEEK_CUR);
		section_offset = ftell(fd);

		if (length < 0)
			continue;

		if (verbose)
		{
			fprintf(stderr, "Section found\n");
			fprintf(stderr, "\tCRC\t= %08x\n", crc);
			fprintf(stderr, "\tVersion = %08x\n", version);
			fprintf(stderr, "\tBuild\t= %08x\n", build_date);
			fprintf(stderr, "\tLength\t= %08x\n", length);
			fprintf(stderr, "\tFlags\t= %08x\n", flags);
			fprintf(stderr, "\tMagic\t= %08x\n", magic);
		}

		snprintf(name_buf, 20, "section_%d", num);
		printf("Saving section %d to %s at offset %d len %d CRC 0x%08x\n",
			num, name_buf, section_offset, length, crc);

		save_section(name_buf, length);
		
		num++;
	}

	fclose(fd);
	return 0;
}
