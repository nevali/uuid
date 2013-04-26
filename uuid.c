/*
 * Copyright 2013 Mo McRoberts.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <uuid/uuid.h>

typedef enum
{
	UM_DEFAULT,
	UM_IDL,
	UM_C,
	UM_C_CF,
	UM_C_MS,
	UM_CANONICAL,
	UM_URN,
	UM_REGISTRY,
	UM_INFO,
	UM_JSON
} output_mode;

typedef enum
{
	UG_AUTO,
	UG_NIL,
	UG_TIME,
	UG_RANDOM
} generate_mode;

struct uuid_info_struct
{
	int variant;
	int type;
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_high_and_version;
	uint16_t clock_seq;
	uint8_t node[6];
};

static const char *short_program_name;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [UUID ...]\n", short_program_name);
	fprintf(stderr, "\nOPTIONS is one or more of:\n");
	fprintf(stderr, "  -h       print this message and immediately exit\n");
	fprintf(stderr, "  -r       generate a random UUID\n");
	fprintf(stderr, "  -x, -t   generate a time-based UUID\n");
	fprintf(stderr, "  -0       generate a nil UUID\n");
	fprintf(stderr, "  -n NUM   generate NUM new UUIDs\n");
	fprintf(stderr, "  -o FILE  write output to FILE\n");
	fprintf(stderr, "  -c       output UUIDs in upper-case\n");
	fprintf(stderr, "  -L       output UUIDs in lower-case\n");
	
	fprintf(stderr, "  -i       output an MSIDL template\n");
	fprintf(stderr, "  -u       output a libuuid C definition\n");
	fprintf(stderr, "  -s       output a COM C definition\n");
	fprintf(stderr, "  -F, -hdr output a CoreFoundation C definition\n");
	fprintf(stderr, "  -m       output UUIDs in minimised form\n");
	fprintf(stderr, "  -U       output UUIDs as URNs\n");
	fprintf(stderr, "  -r       output UUIDs in Windows registry format\n");
	fprintf(stderr, "  -I       output information about UUIDs\n");
	fprintf(stderr, "  -j       output a JSON structure\n");
	fprintf(stderr,
		"\nIf UUID is specified, it will be parsed and output instead of generating\n"
		"a new UUID.\n");
}

static int
unpack_uuid(uuid_t uu, struct uuid_info_struct *info)
{
	unsigned char *t;

	info->type = uuid_type(uu);
	info->variant = uuid_variant(uu);	
	t = uu;
	info->time_low = (t[0] << 24) | (t[1] << 16) | (t[2] << 8) | t[3];
	t += 4;
	info->time_mid = (t[0] << 8) | t[1];
	t += 2;
	info->time_high_and_version = (t[0] << 8) | t[1];
	t += 2;
	info->clock_seq = (t[0] << 8) | t[1];
	t += 2;
	memcpy(info->node, t, sizeof(info->node));
	return 0;
}
	
static int
output_uuid(FILE *outfile, uuid_t uuid, output_mode mode, int upper, int sequence)
{
	char uuidbuf[128];
	struct uuid_info_struct info;
	const char *f;
	size_t c;

	unpack_uuid(uuid, &info);
	if(mode == UM_CANONICAL)
	{
		f = upper ? "%02X" : "%02x";
		for(c = 0; c < 16; c++)
		{
			sprintf(&(uuidbuf[c * 2]), f, uuid[c]);
		}
	}
	else
	{
		if(upper)
		{
			uuid_unparse_upper(uuid, uuidbuf);
		}
		else
		{
			uuid_unparse_lower(uuid, uuidbuf);
		}
	}
	if(mode == UM_C || mode == UM_C_CF || mode == UM_C_MS)
	{
		if(!sequence)
		{
			fprintf(outfile, "#warning Change UUIDNAME to the name of your UUID\n");
		}
		fprintf(outfile, "/* %s */\n", uuidbuf);
	}
	if(mode == UM_C || mode == UM_C_CF)
	{
		f = upper ? "0x%02X, " : "0x%02x, ";
		for(c = 0; c < 16; c++)
		{
			sprintf(&(uuidbuf[c * 6]), f, uuid[c]);
		}
		uuidbuf[94] = 0;
	}
	else if(mode == UM_C_MS)
	{		
		if(upper)
		{
			f = "0x%02X%02X%02X%02XL, 0x%02X%02X, 0x%02X%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X";
		}
		else
		{
			f = "0x%02x%02x%02x%02xL, 0x%02x%02x, 0x%02x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x";
		}
		sprintf(uuidbuf, f,
				uuid[0], uuid[1], uuid[2], uuid[3],
				uuid[4], uuid[5],
				uuid[6], uuid[7],
				uuid[8], uuid[9], uuid[10], uuid[11],
				uuid[12], uuid[13], uuid[14], uuid[15]);
	}
	switch(mode)
	{
	case UM_DEFAULT:
		fprintf(outfile, "%s\n", uuidbuf);
		break;
	case UM_IDL:
		fprintf(outfile, "[\n\tuuid(%s),\n\tversion(1.0)\n]\ninterface INTERFACENAME\n{\n\n}\n", uuidbuf);
		break;
	case UM_C:
		fprintf(outfile, "UUID_DEFINE(UUIDNAME, %s);\n", uuidbuf);
		break;
	case UM_C_CF:
		fprintf(outfile, "#define UUIDNAME CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, %s)\n", uuidbuf);
		break;
	case UM_C_MS:
		fprintf(outfile, "DEFINE_GUID(UUIDNAME, %s);\n", uuidbuf);
		break;
	case UM_CANONICAL:
		fprintf(outfile, "%s\n", uuidbuf);
		break;
	case UM_URN:
		fprintf(outfile, "urn:uuid:%s\n", uuidbuf);
		break;
	case UM_REGISTRY:
		fprintf(outfile, "{%s}\n", uuidbuf);
		break;
	case UM_INFO:
		fprintf(outfile, "uuid: %s\n", uuidbuf);
		fprintf(outfile, "variant: %d\n", info.variant);
		fprintf(outfile, "type: %d\n", info.type);
		fprintf(outfile, "time_low: %04x\n", info.time_low);
		fprintf(outfile, "time_mid: %02x\n", info.time_mid);
		fprintf(outfile, "time_high_and_version: %02x\n", info.time_high_and_version);
		fprintf(outfile, "clock_seq: %02x\n", info.clock_seq);
		fprintf(outfile, "node: %02x:%02x:%02x:%02x:%02x:%02x\n",
				info.node[0], info.node[1], info.node[2],
				info.node[3], info.node[4], info.node[5]);
		break;
	case UM_JSON:
		fprintf(outfile, "{\n");
		fprintf(outfile, "  \"uuid\":\"%s\",\n", uuidbuf);
		fprintf(outfile, "  \"variant\":%d,\n", info.variant);
		fprintf(outfile, "  \"type\":%d,\n", info.type);
		fprintf(outfile, "  \"time_low\":0x%04x,\n", info.time_low);
		fprintf(outfile, "  \"time_mid\":0x%02x,\n", info.time_mid);
		fprintf(outfile, "  \"time_high_and_version\":0x%02x,\n", info.time_high_and_version);
		fprintf(outfile, "  \"clock_seq\":0x%02x,\n", info.clock_seq);
		fprintf(outfile, "  \"node\":\"%02x:%02x:%02x:%02x:%02x:%02x\"\n",
				info.node[0], info.node[1], info.node[2],
				info.node[3], info.node[4], info.node[5]);
		fprintf(outfile, "}\n");
		break;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	const char *t, *outfile;
	char buf[64];
	int c, r, generate, generate_time, upper;
	size_t l;
	generate_mode gmode;
	output_mode mode;
	uuid_t uu;
	FILE *fout;

	generate = 0;
	gmode = UG_AUTO;
	generate_time = 0;
	upper = 0;
	mode = UM_DEFAULT;
	outfile = NULL;
	
	t = strrchr(argv[0], '/');
	if(t)
	{
		short_program_name = t + 1;
	}
	else
	{
		short_program_name = argv[0];
	}
	while(1)
	{
		/* For compatibility with Apple's uuidgen, accept
		 * '-hdr' to generate a CoreFoundation #define
		 */
		if(argv[optind] && !strcmp(argv[optind], "-hdr"))
		{
			mode = UM_C_CF;
			optind++;
			continue;
		}
		c = getopt(argc, argv, "h-:n:o:txrsic0LDUmnwFuIj");
		if(c == -1)
		{
			break;
		}
		if(c == '-')
		{
			if(!strcmp(optarg, "help"))
			{
				usage();
				return 0;
			}
			else
			{
				fprintf(stderr, "%s: invalid long option -- '%s'\n", argv[0], optarg);
				return 1;
			}
		}
		switch(c)
		{
		case 'h':
			/* Print usage message and exit */
			usage();
			return 0;
		case 'o':
			/* Write output to the named file */
			outfile = optarg;
			break;
		case 'n':
			/* Generate the specified number of new UUIDs */
			generate = strtol(optarg, NULL, 0);
			if(generate < 1)
			{
				fprintf(stderr, "%s: warning: failed to parse '%s' as a positive integer, will generate one UUID\n", short_program_name, optarg);
				generate = 1;
			}
			break;
		case '0':
			/* Generate a nil UUID */
			gmode = UG_NIL;
			break;
		case 'x':
		case 't':
			/* Force generation of time-based UUIDs */
			gmode = UG_TIME;
			break;
		case 'r':
			/* Force generation of random UUIDs */
			gmode = UG_RANDOM;
			break;
		case 'D':
			/* Output in standard AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE form */
			mode = UM_DEFAULT;
			break;
		case 'i':
			/* Output a Microsoft/COM IDL template */
			mode = UM_IDL;
			break;
		case 'u':
			/* Output a libuuid UUID_DEFINE() statement */
			mode = UM_C;
			break;
		case 's':
			/* Output a Microsoft/COM C DEFINE_GUID statement */
			mode = UM_C_MS;
			break;
		case 'F':
			/* Output an Apple CFUUIDGetConstantUUIDWithBytes statement */
			mode = UM_C_CF;
			break;
		case 'm':
			/* Output canonical (minimal) form */
			mode = UM_CANONICAL;
			break;
		case 'U':
			/* Output a URN */
			mode = UM_URN;
			break;
		case 'w':
			/* Output in Windows Registry form */
			mode = UM_REGISTRY;
			break;
		case 'I':
			/* Output information about the UUID */
			mode = UM_INFO;
			break;
		case 'j':
			/* Output a JSON structure */
			mode = UM_JSON;
			break;
		case 'L':
			/* Output in lowercase */
			upper = 0;
			break;
		case 'c':
			/* Output in uppercase */
			upper = 1;
			break;
		default:
			usage();
			return 1;
		}
	}
	argv += optind;
	argc -= optind;
	if((generate || gmode != UG_AUTO) && argc)
	{
		fprintf(stderr, "%s: warning: cannot generate while also parsing UUIDs, generation options ignored\n", short_program_name);
		generate = 0;
	}
	if(outfile)
	{
		fout = fopen(outfile, "w");
		if(!fout)
		{
			fprintf(stderr, "%s: %s: %s\n", short_program_name, outfile, strerror(errno));
			return 1;
		}
	}
	else
	{
		fout = stdout;
	}
	if(!generate && !argc)
	{
		generate = 1;
	}
	r = 0;
	c = 0;
	for(; generate; generate--, c++)
	{
		switch(gmode)
		{
		case UG_AUTO:
			/* Generate according to whatever's available */
			uuid_generate(uu);
			break;
		case UG_NIL:
			/* Generate only a nil UUID */
			uuid_clear(uu);
			break;
		case UG_TIME:
			/* Force generation of a time-based UUID */
			uuid_generate_time(uu);
			break;
		case UG_RANDOM:
			/* Force generation of a random UUID */
			uuid_generate_random(uu);
			break;
		}
		output_uuid(fout, uu, mode, upper, c);
	}
	for(; argc; argc--, argv++, c++)
	{
		t = argv[0];
		l = strlen(t);
		if(!strncmp(t, "urn:uuid:", 9))
		{
			/* URN */
			t += 9;
		}
		else if(l == 38 && t[0] == '{' && t[37] == '}')
		{
			/* Windows Registry-style */
			strncpy(buf, t + 1, 36);
			buf[36] = 0;
			t = buf;
		}
		else if(l == 32)
		{
			/* Minimal form */
			sprintf(buf, "%.8s-%.4s-%.4s-%.4s-%s",
					&(t[0]), &(t[8]), &(t[12]), &(t[16]), &(t[20]));
			t = buf;
		}
		if(uuid_parse(t, uu))
		{
			fprintf(stderr, "%s: failed to parse '%s' as a valid UUID\n", short_program_name, argv[0]);
			r = 1;
		}
		else
		{
			output_uuid(fout, uu, mode, upper, c);
		}
	}
	if(fout != stdout)
	{
		fclose(fout);
	}
	return r;
}
