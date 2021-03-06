/********************************
   Copyright 2008 Google Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 ********************************/
#include <err.h>  /* warn() */
#include <crush/dbfr.h>
#include <crush/ffutils.h>
#include <crush/general.h>
#include "aggregate2_main.h"

struct agg_conf {
  int *key_fields;
  int nkeys;
  int *count_fields;
  int ncounts;
  int *sum_fields;
  int nsums;
  int *sum_precisions;
  int *join_fields;
  int njoins;
  /* averages not implemented in agg2 yet.
  int *average_fields; 
  size_t average_fields_sz;
  int naverages;
  int *average_precisions;
  */
};


int configure_aggregation(struct agg_conf *conf, struct cmdargs *args,
                          const char *header, const char *delim);

static void print_line(FILE * out,
                       const char *keys,
                       const char *delim,
                       const int *counts,
                       size_t ncounts,
                       const double *sums, int nsums, int *sum_precisions,
                       char **joins, size_t njoins);

static int extract_keys(char *target, const char *source, const char *delim,
                        int *keys, size_t nkeys, const char *suffix);

static int float_precision(char *n);

/** @brief  
  * 
  * @param args contains the parsed cmd-line options & arguments.
  * @param argc number of cmd-line arguments.
  * @param argv list of cmd-line arguments
  * @param optind index of the first non-option cmd-line argument.
  * 
  * @return exit status for main() to return.
  */
int aggregate2(struct cmdargs *args, int argc, char *argv[], int optind) {

  char default_delim[] = { 0xfe, 0x00 };
  char *default_join_str = ",";

  FILE *in, *out;
  dbfr_t *in_reader;

  struct agg_conf conf;

  int cur_precision;

  char *cur_keys = NULL, *prev_keys = NULL;
  int prev_keys_initialized = 0;
  size_t keybuf_sz = 0;

  int *cur_counts = NULL;
  double *cur_sums = NULL;
  char **cur_joins = NULL;
  int *cur_join_sizes = NULL;
  size_t join_str_len;
  int join_len;

  char field_buf[1024];         /* FIXME: should be dynamically resized */
  double f;                     /* numeric value of sum fields */
  int i;                        /* counter */

  if (! (args->keys || args->key_labels)) {
    fprintf(stderr, "%s: either -k or -K must be specified.\n", argv[0]);
    return EXIT_HELP;
  }

  /* individually these args are not required. */
  if (!(args->sums || args->sum_labels) &&
      !(args->counts || args->count_labels) &&
      !(args->joins || args->join_labels)) {
    fprintf(stderr,
            "%s: at least one of -s/-S, -c/-C, or -j/-J must be specified.\n",
            argv[0]);
    return EXIT_HELP;
  }

  if (!args->delim) {
    if ((args->delim = getenv("DELIMITER")) == NULL)
      args->delim = default_delim;
  }
  expand_chars(args->delim);

  if (!args->join_str) args->join_str = default_join_str;
  join_str_len = strlen(args->join_str);

  if (optind < argc) {
    in = nextfile(argc, argv, &optind, "r");
    if (! in)
      return EXIT_FILE_ERR;
  } else {
    in = stdin;
  }
  in_reader = dbfr_init(in);

  memset(&conf, 0, sizeof(conf));
  if (configure_aggregation(&conf, args, in_reader->next_line, args->delim)
      != 0) {
    fprintf(stderr, "%s: error parsing field arguments.\n", argv[0]);
    return EXIT_HELP;
  }

  if (args->outfile) {
    out = fopen(args->outfile, "w");
    if (!out) {
      warn("%s", args->outfile);
      return EXIT_FILE_ERR;
    }
  } else {
    out = stdout;
  }

  if (conf.ncounts > 0)
    cur_counts = calloc(conf.ncounts, sizeof(int));

  if (conf.nsums > 0)
    cur_sums = calloc(conf.nsums, sizeof(double));

  if (conf.njoins > 0) {
    cur_joins = calloc(conf.njoins, sizeof(char*));
    cur_join_sizes = calloc(conf.njoins, sizeof(int));
    /* these can be resized later */
    for (i = 0; i < conf.njoins; i++) {
      cur_join_sizes[i] = 1024;
      cur_joins[i]      = xmalloc(sizeof(char) * 1024);
      cur_joins[i][0]   = '\0';
    }
  }

  /* this can be resized later */
  cur_keys = xmalloc(sizeof(char) * 1024);
  prev_keys = xmalloc(sizeof(char) * 1024);
  keybuf_sz = 1024;

  if (args->labels || args->auto_label)
    args->preserve_header = 1;

  if (args->preserve_header) {
    if (dbfr_getline(in_reader) <= 0) {
      DIE("unexpected end of file");
    }

    chomp(in_reader->current_line);
    if (extract_keys(cur_keys, in_reader->current_line, args->delim,
                     conf.key_fields, conf.nkeys, NULL) != 0) {
      fprintf(stderr, "%s: malformatted input\n", argv[0]);
      return EXIT_FILE_ERR;
    }
    fputs(cur_keys, out);

    if (args->labels) {
      fprintf(out, "%s%s", args->delim, args->labels);
    } else {
      if (conf.nsums > 0) {
        if (extract_keys(cur_keys, in_reader->current_line, args->delim,
                         conf.sum_fields, conf.nsums,
                         args->auto_label ? "-Sum" : NULL) != 0) {
          fprintf(stderr, "%s: malformatted input for -s/-S\n", argv[0]);
          return EXIT_FILE_ERR;
        }
        fprintf(out, "%s%s", args->delim, cur_keys);
      }

      if (conf.ncounts > 0) {
        if (extract_keys(cur_keys, in_reader->current_line, args->delim,
                         conf.count_fields, conf.ncounts,
                         args->auto_label ? "-Count" : NULL) != 0) {
          fprintf(stderr, "%s: malformatted input for -c/-C\n", argv[0]);
          return EXIT_FILE_ERR;
        }
        fprintf(out, "%s%s", args->delim, cur_keys);
      }

      if (conf.njoins > 0) {
        if (extract_keys(cur_keys, in_reader->current_line, args->delim,
                         conf.join_fields, conf.njoins,
                         args->auto_label ? "-Join" : NULL) != 0) {
          fprintf(stderr, "%s: malformatted input for -j/-J\n", argv[0]);
          return EXIT_FILE_ERR;
        }
        fprintf(out, "%s%s", args->delim, cur_keys);
      }
    }
    fputs("\n", out);
  }

  cur_keys[0] = '\0';
  prev_keys[0] = '\0';

  while (in) {
    while (dbfr_getline(in_reader) > 0) {
      chomp(in_reader->current_line);
      if (in_reader->current_line_sz > keybuf_sz) {
        cur_keys = xrealloc(cur_keys, in_reader->current_line_sz);
        prev_keys = xrealloc(prev_keys, in_reader->current_line_sz);
        keybuf_sz = in_reader->current_line_sz;
      }

      if (extract_keys(cur_keys, in_reader->current_line, args->delim,
                       conf.key_fields, conf.nkeys, NULL) != 0) {
        fprintf(stderr, "%s: malformatted input\n", argv[0]);
        return EXIT_FILE_ERR;
      }

      if (prev_keys_initialized && !str_eq(cur_keys, prev_keys)) {
        print_line(out, prev_keys, args->delim, cur_counts,
                   conf.ncounts, cur_sums,
                   conf.nsums, conf.sum_precisions, cur_joins, conf.njoins);

        memset(cur_counts, 0, conf.ncounts * sizeof(int));
        memset(cur_sums, 0, conf.nsums * sizeof(double));
        for (i = 0; i < conf.njoins; i++) cur_joins[i][0] = '\0';
      }

      for (i = 0; i < conf.ncounts; i++) {
        get_line_field(field_buf, in_reader->current_line, 1023,
                       conf.count_fields[i], args->delim);
        if (field_buf[0] != '\0') {
          cur_counts[i]++;
        }
      }

      for (i = 0; i < conf.nsums; i++) {
        get_line_field(field_buf, in_reader->current_line, 1023,
                       conf.sum_fields[i], args->delim);
        f = atof(field_buf);
        cur_sums[i] += f;

        cur_precision = float_precision(field_buf);

        if (cur_precision > conf.sum_precisions[i])
          conf.sum_precisions[i] = cur_precision;
      }

      for (i = 0; i < conf.njoins; i++) {
        get_line_field(field_buf, in_reader->current_line, 1023,
                       conf.join_fields[i], args->delim);
        join_len = strlen(field_buf) + join_str_len + strlen(cur_joins[i]);
        if (join_len >= cur_join_sizes[i])
          cur_joins[i] = xrealloc(cur_joins[i], cur_join_sizes[i] = join_len+1);
        if (cur_joins[i][0] == '\0')
          strcpy(cur_joins[i], field_buf);
        else {
          strcat(cur_joins[i], args->join_str);
          strcat(cur_joins[i], field_buf);
        }
      }

      strcpy(prev_keys, cur_keys);
      prev_keys_initialized = 1;
    }
    dbfr_close(in_reader);
    in = nextfile(argc, argv, &optind, "r");
    if (in) {
      in_reader = dbfr_init(in);
      /* reconfigure fields (needed if labels were used) */
      if (configure_aggregation(&conf, args, in_reader->next_line, args->delim)
          != 0) {
        fprintf(stderr, "%s: error parsing field arguments.\n", argv[0]);
        return EXIT_HELP;
      }
      if (args->preserve_header)
        dbfr_getline(in_reader);
    }
  }

  print_line(out, prev_keys, args->delim, cur_counts, conf.ncounts,
             cur_sums, conf.nsums, conf.sum_precisions, cur_joins, conf.njoins);

  free(cur_counts);
  free(cur_sums);
  free(cur_keys);
  free(prev_keys);

  return EXIT_OKAY;
}

static void decrement_values(int *array, size_t sz) {
  int j;
  if (array == NULL || sz == 0)
    return;
  for (j = 0; j < sz; j++) {
    array[j]--;
  }
}

int configure_aggregation(struct agg_conf *conf, struct cmdargs *args,
                          const char *header, const char *delim) {
  size_t ignore_sz = 0;
  if (args->keys) {
    conf->nkeys = expand_nums(args->keys, &(conf->key_fields), &ignore_sz);
  } else if (args->key_labels) {
    conf->nkeys = expand_label_list(args->key_labels, header,
                                    delim, &(conf->key_fields), &ignore_sz);
    args->preserve_header = 1;
  }
  if (conf->nkeys < 0)
    return conf->nkeys;
  decrement_values(conf->key_fields, conf->nkeys);

  ignore_sz = 0;
  if (args->sums) {
    conf->nsums = expand_nums(args->sums, &(conf->sum_fields), &ignore_sz);
  } else if (args->sum_labels) {
    conf->nsums = expand_label_list(args->sum_labels, header,
                                    delim, &(conf->sum_fields), &ignore_sz);
    args->preserve_header = 1;
  }
  if (conf->nsums < 0) {
    return conf->nsums;
  } else if (conf->nsums > 0) {
    decrement_values(conf->sum_fields, conf->nsums);
    conf->sum_precisions = xmalloc(sizeof(int) * conf->nsums);
    memset(conf->sum_precisions, 0, sizeof(int) * conf->nsums);
  }

  ignore_sz = 0;
  if (args->counts) {
    conf->ncounts = expand_nums(args->counts, &(conf->count_fields),&ignore_sz);
  } else if (args->count_labels) {
    conf->ncounts = expand_label_list(args->count_labels, header,
                                      delim, &(conf->count_fields), &ignore_sz);
    args->preserve_header = 1;
  }
  if (conf->ncounts < 0)
    return conf->ncounts;
  else if (conf->ncounts > 0)
    decrement_values(conf->count_fields, conf->ncounts);

  ignore_sz = 0;
  if (args->joins) {
    conf->njoins = expand_nums(args->joins, &(conf->join_fields), &ignore_sz);
  } else if (args->join_labels) {
    conf->njoins = expand_label_list(args->join_labels, header,
                                     delim, &(conf->join_fields), &ignore_sz);
    args->preserve_header = 1;
  }
  if (conf->njoins < 0)
    return conf->njoins;
  else if (conf->njoins > 0)
    decrement_values(conf->join_fields, conf->njoins);
/*
  if (args->averages) {
    conf->naverages = expand_nums(args->averages, &(conf->average_fields),
                                  &(conf->average_fields_sz));
  } else if (args->average_labels) {
    conf->naverages = expand_label_list(args->average_labels, header,
                                        delim, &(conf->average_fields),
                                        &(conf->average_fields_sz));
    args->preserve_header = 1;
  }
  if (conf->naverages < 0) {
    return conf->naverages;
  } else if (conf->naverages > 0) {
    decrement_values(conf->average_fields, conf->naverages);
    conf->average_precisions = xmalloc(sizeof(int) * conf->naverages);
    memset(conf->average_precisions, 0, sizeof(int) * conf->naverages);
  }
*/
  return 0;
}

/* assumption: target is at least as big as source, so it cannot be
   overflowed.
   TODO(jhinds): this is faulty if the same field is specified multiple times;
   and when auto_labeling, the suffix might cause an overflow.
 */
static int extract_keys(char *target, const char *source, const char *delim,
                        int *keys, size_t nkeys, const char *suffix) {
  int i, s, e; /* iter, start, end */
  int field_len;

  target[0] = '\0';
  for (i = 0; i < nkeys; i++) {
  	field_len = get_line_pos(source, keys[i], delim, &s, &e);
    if (field_len < 0)
      return 1;
    if (field_len > 0)
      strncat(target, source + s, e - s + 1);
    if (suffix)
      strcat(target, suffix);
    if (i != nkeys - 1)
      strcat(target, delim);
  }
  return 0;
}



static void print_line(FILE * out,
                       const char *keys,
                       const char *delim,
                       const int *counts,
                       size_t ncounts,
                       const double *sums, int nsums, int *sum_precisions,
                       char **joins,
                       size_t njoins) {
  int i;
  fputs(keys, out);

  for (i = 0; i < nsums; i++) {
    fprintf(out, "%s%.*f", delim, sum_precisions[i], sums[i]);
  }

  for (i = 0; i < ncounts; i++) {
    fprintf(out, "%s%d", delim, counts[i]);
  }

  for (i = 0; i < njoins; i++) {
    fputs(delim, out);
    fputs(joins[i], out);
  }

  fputs("\n", out);
}


static int float_precision(char *n) {
  char *dot;
  if (n == NULL || n[0] == '\0')
    return 0;
  dot = strchr(n, '.');
  if (dot == NULL)
    return 0;

  return strlen(dot) - 1;
}
