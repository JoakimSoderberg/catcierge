#include "catcierge_args.h"
#include "cargo.h"
#include "catcierge_log.h"
#include "catcierge_util.h"

void print_cam_help(cargo_t cargo)
{
	int console_width = cargo_get_width(cargo, 0) - 1;
	print_line(stderr, console_width, "-");
	fprintf(stderr, "Raspberry Pi camera settings:\n");
	fprintf(stderr, "(Prepend these with --rpi. Example: --rpi--saturation or --rpi-sa)\n");
	print_line(stderr, console_width, "-");
	raspicamcontrol_display_help();
	print_line(stderr, console_width, "-");
	fprintf(stderr, 
		"\nNOTE! To use the above command line parameters "
		"you must prepend them with \"rpi-\".\n"
		"For example --rpi-saturation\n"
		"\n"
		"Also, these cannot be specified in the normal config "
		"instead use --rpi-config\n\n");
	print_line(stderr, console_width, "-");
}

char **catcierge_parse_rpi_args(int *argc, char **argv, catcierge_args_t *args)
{
	int i;
	int j;
	int rpi_count = 0;
	char **nargv;
	int nargc = 0;
	int used = 0;
	assert(argc);
	assert(argv);
	assert(args);

	if (!(nargv = calloc(*argc, sizeof(char *))))
	{
		return NULL;
	}

	for (i = 0, j = 0; i < *argc; i++)
	{
		if (!strncmp(argv[i], "--rpi-", sizeof("--rpi-") - 1))
		{
			int rpiret = 0;
			char *next_arg = NULL;
			const char *rpikey = argv[i] + sizeof("--rpi") - 1;

			if ((i + 1) < *argc)
			{
				next_arg = argv[i + 1];

				if (*next_arg == '-')
				{
					next_arg = NULL;
					i++;
				}
			}

			if ((used = raspicamcontrol_parse_cmdline(
				&args->rpi_settings.camera_parameters,
				rpikey, next_arg)) == 0)
			{
				CATERR("Error parsing rpi-cam option:\n  %s\n", rpikey);
				goto fail;
			}

			rpi_count += used;
		}
		else
		{
			nargv[j++] = strdup(argv[i]);
		}
	}

	*argc -= rpi_count;

	return nargv;
fail:
	if (nargv)
	{
		cargo_free_commandline(&nargv, nargc);
	}

	return NULL;
}

int catcierge_parse_rpi_config(const char *config_path, catcierge_args_t *args)
{
	char *buf = NULL;
	int ret = 0;
	char **argv = NULL;
	int argc = 0;
	int i;
	char *arg = NULL;
	char *next_arg = NULL;
	int used;
	char *s = NULL;

	CATLOG("Reading Raspberry Pi camera config: %s\n", config_path);
	
	if (!(buf = catcierge_read_file(config_path)))
	{
		CATERR("Could not read Raspberry Pi camera config\n");
		ret = -1; goto fail;
	}

	s = buf;

	// TODO: Remove comments #
	while (*s)
	{
		if (*s == '\n')
			*s = ' ';
		s++;
	}

	if (!(argv = cargo_split_commandline(0, buf, &argc)))
	{
		CATERR("Raspberry Pi camera: Failed to split input into command line:\n%s", buf);
		ret = -1; goto fail;
	}

	CATLOG("Found arguments: ");
	for (i = 0; i < argc; i++)
	{
		printf("%s ", argv[i]);
	}
	printf("\n");

	for (i = 0; i < argc;)
	{
		arg = argv[i];

		if ((i + 1) < argc)
		{
			next_arg = argv[i + 1];
		}

		if ((used = raspicamcontrol_parse_cmdline(
					&args->rpi_settings.camera_parameters,
					arg, next_arg)) == 0)
		{
			CATERR("Error parsing rpi-cam config option:\n  %s\n", arg);
			goto fail;
		}

		i += used;
	}

fail:
	cargo_free_commandline(&argv, argc);
	catcierge_xfree(&buf);

	return ret;
}
