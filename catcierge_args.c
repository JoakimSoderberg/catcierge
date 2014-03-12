
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "catcierge_args.h"

#ifdef WITH_INI
#include "alini/alini.h"
#endif

int catcierge_parse_setting(catcierge_args_t *args, const char *key, char **values, size_t value_count)
{
	size_t i;

	if (!strcmp(key, "show"))
	{
		args->show = 1;
		if (value_count == 1) args->show = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "save"))
	{
		args->saveimg = 1;
		if (value_count == 1) args->saveimg = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "highlight"))
	{
		args->highlight_match = 1;
		if (value_count == 1) args->highlight_match = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "show_fps"))
	{
		args->show_fps = 1;
		if (value_count == 1) args->show_fps = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "lockout"))
	{
		if (value_count == 1)
		{
			args->lockout_time = atoi(values[0]);
		}
		else
		{
			args->lockout_time = DEFAULT_LOCKOUT_TIME;
		}

		return 0;
	}

	if (!strcmp(key, "lockout_error"))
	{
		if (value_count == 1)
		{
			args->max_consecutive_lockout_count = atoi(values[0]);
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "lockout_dummy"))
	{
		if (value_count ==1)
		{
			args->lockout_dummy = atoi(values[0]);
		}
		else
		{
			args->lockout_dummy = 1;
		}

		return 0;
	}

	if (!strcmp(key, "matchtime"))
	{
		if (value_count == 1)
		{
			args->match_time = atoi(values[0]);
		}
		else
		{
			args->match_time = DEFAULT_MATCH_WAIT;
		}

		return 0;
	}

	if (!strcmp(key, "match_flipped"))
	{
		if (value_count == 1)
		{
			args->match_flipped = atoi(values[0]);
		}
		else
		{
			args->match_flipped = 1;
		}

		return 0;
	}

	if (!strcmp(key, "output"))
	{
		if (value_count == 1)
		{
			args->output_path = values[0];
			return 0;
		}

		return -1;
	}

	#ifdef WITH_RFID
	if (!strcmp(key, "rfid_in"))
	{
		if (value_count == 1)
		{
			args->rfid_inner_path = values[0];
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "rfid_out"))
	{
		if (value_count == 1)
		{
			args->rfid_outer_path = values[0];
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "rfid_allowed"))
	{
		if (value_count == 1)
		{
			if (create_rfid_allowed_list(values[0]))
			{
				CATERR("Failed to create RFID allowed list\n");
				return -1;
			}

			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "rfid_time"))
	{
		if (value_count == 1)
		{
			printf("val: %s\n", values[0]);
			args->rfid_lock_time = (double)atof(values[0]);
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "rfid_lock"))
	{
		args->lock_on_invalid_rfid = 1;
		if (value_count == 1) args->lock_on_invalid_rfid = atoi(values[0]);
		return 0;
	}
	#endif // WITH_RFID

	// On the command line you can specify multiple snouts like this:
	// --snout_paths <path1> <path2> <path3>
	// or 
	// --snout_path <path1> --snout_path <path2>
	// In the config file you do
	// snout_path=<path1>
	// snout_path=<path2>
	if (!strcmp(key, "snout") || 
		!strcmp(key, "snout_paths") || 
		!strcmp(key, "snout_path"))
	{
		if (value_count == 0)
			return -1;

		for (i = 0; i < value_count; i++)
		{
			if (args->snout_count >= MAX_SNOUT_COUNT) return -1;
			args->snout_paths[args->snout_count] = values[i];
			args->snout_count++;
		}

		return 0;
	}

	if (!strcmp(key, "threshold"))
	{
		if (value_count == 1)
		{
			args->match_threshold = atof(values[0]);
		}
		else
		{
			args->match_threshold = DEFAULT_MATCH_THRESH;
		}

		return 0;
	}

	if (!strcmp(key, "log"))
	{
		if (value_count == 1)
		{
			args->log_path = values[0];
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "match_cmd"))
	{
		if (value_count == 1)
		{
			args->match_cmd = values[0];
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "save_img_cmd"))
	{
		if (value_count == 1)
		{
			args->save_img_cmd = values[0];
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "save_imgs_cmd"))
	{
		if (value_count == 1)
		{
			args->save_imgs_cmd = values[0];
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "match_done_cmd"))
	{
		if (value_count == 1)
		{
			args->match_done_cmd = values[0];
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "do_lockout_cmd"))
	{
		if (value_count == 1)
		{
			args->do_lockout_cmd = values[0];
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "do_unlock_cmd"))
	{
		if (value_count == 1)
		{
			args->do_unlock_cmd = values[0];
			return 0;
		}

		return -1;
	}

	#ifdef WITH_RFID
	if (!strcmp(key, "rfid_detect_cmd"))
	{
		if (value_count == 1)
		{
			args->rfid_detect_cmd = values[0];
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "rfid_match_cmd"))
	{
		if (value_count == 1)
		{
			args->rfid_match_cmd = values[0];
			return 0;
		}

		return -1;
	}
	#endif // WITH_RFID

	return -1;
}
