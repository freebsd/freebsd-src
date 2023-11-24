/* Copyright (c) 2015, Cesanta Software
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ucl.h"

void usage(const char *name, FILE *out) {
  fprintf(out, "Usage: %s [--help] [-i|--in file] [-o|--out file]\n", name);
  fprintf(out, "    [-s|--schema file] [-f|--format format]\n\n");
  fprintf(out, "  --help   - print this message and exit\n");
  fprintf(out, "  --in     - specify input filename "
          "(default: standard input)\n");
  fprintf(out, "  --out    - specify output filename "
          "(default: standard output)\n");
  fprintf(out, "  --schema - specify schema file for validation\n");
  fprintf(out, "  --format - output format. Options: ucl (default), "
          "json, compact_json, yaml, msgpack\n");
}

int main(int argc, char **argv) {
  int i;
  char ch;
  FILE *in = stdin, *out = stdout;
  const char *schema = NULL, *parm, *val;
  unsigned char *buf = NULL;
  size_t size = 0, r = 0;
  struct ucl_parser *parser = NULL;
  ucl_object_t *obj = NULL;
  ucl_emitter_t emitter = UCL_EMIT_CONFIG;

  for (i = 1; i < argc; ++i) {
    parm = argv[i];
    val = ((i + 1) < argc) ? argv[++i] : NULL;

    if ((strcmp(parm, "--help") == 0) || (strcmp(parm, "-h") == 0)) {
      usage(argv[0], stdout);
      exit(0);

    } else if ((strcmp(parm, "--in") == 0) || (strcmp(parm, "-i") == 0)) {
      if (!val)
        goto err_val;

      in = fopen(val, "r");
      if (in == NULL) {
        perror("fopen on input file");
        exit(EXIT_FAILURE);
      }
    } else if ((strcmp(parm, "--out") == 0) || (strcmp(parm, "-o") == 0)) {
      if (!val)
        goto err_val;

      out = fopen(val, "w");
      if (out == NULL) {
        perror("fopen on output file");
        exit(EXIT_FAILURE);
      }
    } else if ((strcmp(parm, "--schema") == 0) || (strcmp(parm, "-s") == 0)) {
      if (!val)
        goto err_val;
      schema = val;

    } else if ((strcmp(parm, "--format") == 0) || (strcmp(parm, "-f") == 0)) {
        if (!val)
          goto err_val;

        if (strcmp(val, "ucl") == 0) {
          emitter = UCL_EMIT_CONFIG;
        } else if (strcmp(val, "json") == 0) {
          emitter = UCL_EMIT_JSON;
        } else if (strcmp(val, "yaml") == 0) {
          emitter = UCL_EMIT_YAML;
        } else if (strcmp(val, "compact_json") == 0) {
          emitter = UCL_EMIT_JSON_COMPACT;
        } else if (strcmp(val, "msgpack") == 0) {
          emitter = UCL_EMIT_MSGPACK;
        } else {
          fprintf(stderr, "Unknown output format: %s\n", val);
          exit(EXIT_FAILURE);
        }
    } else {
      usage(argv[0], stderr);
      exit(EXIT_FAILURE);
    }
  }

  parser = ucl_parser_new(0);
  buf = malloc(BUFSIZ);
  size = BUFSIZ;
  while (!feof(in) && !ferror(in)) {
    if (r == size) {
      buf = realloc(buf, size*2);
      size *= 2;
      if (buf == NULL) {
        perror("realloc");
        exit(EXIT_FAILURE);
      }
    }
    r += fread(buf + r, 1, size - r, in);
  }
  if (ferror(in)) {
    fprintf(stderr, "Failed to read the input file.\n");
    exit(EXIT_FAILURE);
  }
  fclose(in);
  if (!ucl_parser_add_chunk(parser, buf, r)) {
    fprintf(stderr, "Failed to parse input file: %s\n",
            ucl_parser_get_error(parser));
    exit(EXIT_FAILURE);
  }
  if ((obj = ucl_parser_get_object(parser)) == NULL) {
    fprintf(stderr, "Failed to get root object: %s\n",
            ucl_parser_get_error(parser));
    exit(EXIT_FAILURE);
  }
  if (schema != NULL) {
    struct ucl_parser *schema_parser = ucl_parser_new(0);
    ucl_object_t *schema_obj = NULL;
    struct ucl_schema_error error;

    if (!ucl_parser_add_file(schema_parser, schema)) {
      fprintf(stderr, "Failed to parse schema file: %s\n",
              ucl_parser_get_error(schema_parser));
      exit(EXIT_FAILURE);
    }
    if ((schema_obj = ucl_parser_get_object(schema_parser)) == NULL) {
      fprintf(stderr, "Failed to get root object: %s\n",
              ucl_parser_get_error(schema_parser));
      exit(EXIT_FAILURE);
    }
    if (!ucl_object_validate(schema_obj, obj, &error)) {
      fprintf(stderr, "Validation failed: %s\n", error.msg);
      exit(EXIT_FAILURE);
    }
  }

  if (emitter != UCL_EMIT_MSGPACK) {
    fprintf(out, "%s\n", ucl_object_emit(obj, emitter));
  } else {
    size_t len;
    unsigned char *res;

    res = ucl_object_emit_len(obj, emitter, &len);
    fwrite(res, 1, len, out);
  }

  return 0;

err_val:
    fprintf(stderr, "Parameter %s is missing mandatory value\n", parm);
    usage(argv[0], stderr);
    exit(EXIT_FAILURE);
}
