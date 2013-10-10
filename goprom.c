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
#include <string.h>

struct inode {
	char name[0x73];
	int offset;
	int len;
	int magic;
};

static unsigned int read_word(FILE *fd);
static unsigned int read_word(FILE *fd)
{
	unsigned int r = 0;

	r |= fgetc(fd) <<  0;
	r |= fgetc(fd) <<  8;
	r |= fgetc(fd) << 16;
	r |= fgetc(fd) << 24;
	return r;
}

#define INODE_MAGIC 0x2387AB76

static void print_usage(void);
static void print_usage(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "	goprom --unpack romfs_section > unpack-romfs.sh\n");
	fprintf(stderr, "	Generate shell script to unpack a romfs section\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "	goprom --update romfs_section > update-romfs.sh\n");
	fprintf(stderr, "	Generate shell script to update files in an existing romfs section\n");
	fprintf(stderr, "	DO NOT DO THIS UNLESS YOU *REALLY* KNOW WHAT YOU ARE DOING.\n");
}

int main(int argc, char **argv)
{
	int		i, nfiles, update = 0;
	struct inode	d;
	FILE		*fd;

	if (argc != 3) {
		print_usage();
		exit(-1);
	}

	if (strcmp(argv[1], "--unpack") == 0) {
		fprintf(stderr, "Generating unpack script\n");
		update = 0;
	} else if (strcmp(argv[1], "--update") == 0) {
		fprintf(stderr, "Generating update script\n");
		fprintf(stderr, "You really better know what you are doing.\n");
		fprintf(stderr, "You can very easily destroy your camera if you misuse this!\n");
		update = 1;
	} else {
		print_usage();
		exit(-1);
	}
	
	fd = fopen(argv[2], "rb");

	if (!fd) {
		fprintf(stderr, "Could not open %s\n", argv[2]);
		exit(-1);
	}
	
	nfiles = read_word(fd);
	fprintf(stderr, "I see %d files\n", nfiles);

	if (nfiles < 0) {
		fprintf(stderr, "... and that certainly does not seem right\n");
		exit(-1);
	}
	
	fseek(fd, 0x800, SEEK_SET);
	
	for (i = 0; i < nfiles; i++) {
		fread(&d, sizeof(struct inode), 1, fd);

		if (d.magic != INODE_MAGIC) {
			fprintf(stderr, "Unknown inode magic: %08x\n", d.magic);
			exit(-1);
		}

		if (!update) {
			printf("mkdir -p `dirname %s`\n", d.name);
			printf("dd if=$1 bs=%d skip=1 | dd iflag=fullblock of=%s bs=%d count=1\n",
				d.offset, d.name, d.len);
		} else
			printf("dd if=%s of=$1 bs=1 seek=%d count=%d conv=notrunc\n",
				d.name, d.offset, d.len);
	}

	fclose(fd);

	return 0;
}
