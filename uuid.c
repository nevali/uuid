#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <uuid/uuid.h>

typedef enum
{
	UM_DEFAULT,
	UM_IDL,
	UM_C,
	UM_CANONICAL,
	UM_URN,
	UM_REGISTRY
} output_mode;

static const char *short_program_name;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [UUID ...]\n", short_program_name);
	fprintf(stderr, "\nOPTIONS is one or more of:\n");
	fprintf(stderr, "  -h       print this message and immediately exit\n");
	fprintf(stderr, "  -x       generate a time-based UUID\n");
	fprintf(stderr, "  -0       generate a nil UUID\n");
	fprintf(stderr, "  -n NUM   generate NUM new UUIDs\n");
	fprintf(stderr, "  -o FILE  write output to FILE\n");
	fprintf(stderr, "  -i       output an IDL template\n");
	fprintf(stderr, "  -s       output a C structure\n");
	fprintf(stderr, "  -m       output UUIDs in canonical form\n");
	fprintf(stderr, "  -u       output UUIDs as URNs\n");
	fprintf(stderr, "  -r       output UUIDs in Windows registry format\n");
	fprintf(stderr, "  -c       output UUIDs in uppercase\n");	
	fprintf(stderr, 
		"\nIf UUID is specified, it will be parsed and output instead of generating\n"
		"a new UUID.\n");
}

static int
output_uuid(FILE *outfile, uuid_t uuid, output_mode mode, int upper)
{
	char uuidbuf[128];
	const char *f;
	size_t c;
	
	if(mode == UM_CANONICAL)
	{
		f = upper ? "%02X" : "%02x";
		for(c = 0; c < 16; c++)
		{
			sprintf(&(uuidbuf[c * 2]), f, uuid[c]);				
		}
	}
	else if(mode == UM_C)
	{
		f = upper ? "0x%02X, " : "0x%02x, ";
		for(c = 0; c < 16; c++)
		{
			sprintf(&(uuidbuf[c * 6]), f, uuid[c]);
		}
		uuidbuf[94] = 0;
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
	case UM_CANONICAL:
		fprintf(outfile, "%s\n", uuidbuf);
		break;
	case UM_URN:
		fprintf(outfile, "urn:uuid:%s\n", uuidbuf);
		break;
	case UM_REGISTRY:
		fprintf(outfile, "{%s}\n", uuidbuf);
		break;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	const char *t, *outfile;
	int c, r, generate, generate_time, upper;
	output_mode mode;
	uuid_t uu;
	FILE *fout;
	
	generate = 0;
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
	while((c = getopt(argc, argv, "hn:o:xismurc0")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			return 0;
		case 'o':
			outfile = optarg;
			break;
		case 'n':
			generate = strtol(optarg, NULL, 0);
			if(generate < 1)
			{
				fprintf(stderr, "%s: warning: failed to parse '%s' as a positive integer, will generate one UUID\n", short_program_name, optarg);
				generate = 1;
			}
			break;
		case 'x':
			generate_time = 1;
			break;
		case 'i':
			mode = UM_IDL;
			break;
		case 's':
			mode = UM_C;
			break;
		case 'm':
			mode = UM_CANONICAL;
			break;
		case 'u':
			mode = UM_URN;
			break;
		case 'r':
			mode = UM_REGISTRY;
			break;
		case 'c':
			upper = 1;
			break;
		case '0':
			generate = -1;
			break;
		default:
			usage();
			return 1;
		}
	}
	argv += optind;
	argc -= optind;
	if((generate || generate_time) && argc)
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
	if(generate == -1)
	{
		uuid_clear(uu);
		output_uuid(fout, uu, mode, upper);
		generate = 0;
	}
	for(; generate; generate--)
	{
		if(generate_time)
		{
			uuid_generate_time(uu);
		}
		else
		{
			uuid_generate(uu);
		}
		output_uuid(fout, uu, mode, upper);
	}
	for(; argc; argc--, argv++)
	{
		if(uuid_parse(argv[0], uu))
		{
			fprintf(stderr, "%s: failed to parse '%s' as a valid UUID\n", short_program_name, argv[0]);
			r = 1;
		}
		else
		{
			output_uuid(fout, uu, mode, upper);
		}
	}
	if(fout != stdout)
	{
		fclose(fout);
	}
	return r;
}