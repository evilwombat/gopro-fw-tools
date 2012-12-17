/*
 *  Copyright (c) 2012, evilwombat
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

FILE *fd;

/* Magic is 0xA3 0x24 0xEB 0x90 */

int find_magic(void)
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
		}

		if (c == 0x90)
			state = 1;
	}
}

unsigned int read_word(void)
{
	unsigned int r = 0;
	r |= fgetc(fd) <<  0;
	r |= fgetc(fd) <<  8;
	r |= fgetc(fd) << 16;
	r |= fgetc(fd) << 24;
	return r;
}

/*
 * Thanks to this guy for info on the header format:
 * https://gist.github.com/2394361
 */
int main(int argc, char **argv)
{
	int ret = 0;
	unsigned int crc, version, build_date, length, flags, magic;
	unsigned int section_offset, num = 0;
	char *fname;

	if (argc != 2) {
		printf("Usage: %s [firmware_file]\n", argv[0]);
		return -1;
	}

	fname = argv[1];
	
	fd = fopen(fname, "r");
	if (!fd) {
		printf("Could not open %s\n", fname);
		return -1;
	}

	while (1) {
		ret = find_magic();
		if (ret < 0) {
			printf("# End of file reached.\n");
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
		fseek(fd, length, SEEK_CUR);
/*
		printf("Section found\n");
		printf("\tCRC\t= %08x\n", crc);
		printf("\tVersion = %08x\n", version);
		printf("\tBuild\t= %08x\n", build_date);
		printf("\tLength\t= %08x\n", length);
		printf("\tFlags\t= %08x\n", flags);
		printf("\tMagic\t= %08x\n", magic);
*/

		printf("# section_%d at offset %d length %d CRC 0x%08x\n",
			num, section_offset, length, crc);
		printf("dd if=$1 bs=%d skip=1 | dd iflag=fullblock of=section_%d bs=%d count=1\n",
			section_offset, num, length);
		printf("\n");
		num++;
	}

	fclose(fd);
	return 0;
}
