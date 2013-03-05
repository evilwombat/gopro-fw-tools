Unofficial firmware tools for my favorite sports video camera(s).

Everything here is completely unsupported. Use everything at your own risk.
There author takes no responsibility for anything that happens as a result
of using this software.

fwunpacker:
	A tool for extracting the firmware sections from HDxxx-firmware.bin
	and saving them as separate section_X files.

	Usage:
		fwunpacker firmware.bin

goprom:
	A tool for generating a script to split a romfs section into all the
	files found in it. This tool may also be used to generate a script to
	update the romfs section with modifications to previously-unpacked
	files, but this is a very dangerous procedure and can result in a dead
	camera if the resulting section is flashed.

	Usage:
		goprom --unpack romfs_section > unpack-romfs.sh
		chmod +x unpack-romfs.sh
		mkdir romfs
		cd romfs
		../unpack-romfs.sh ../romfs_section

fwparser:
	A tool for generating a script to split HDxxx-firmware.bin into
	separate sections found in it. This is deprecated in favor of
	fwunpacker, which will create the section files directly from the
	HD2-firmware.bin file.

	Usage:
		fwparser firmware.bin > unpack-firmware.sh
		chmod +x unpack-firmware.sh
		./unpack-firmware.sh firmware.bin
