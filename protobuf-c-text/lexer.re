/*
 * vim: set filetype=c:
 *
 * Compile with: re2c -s -o protobuf-c-text/lexer.re.c protobuf-c-text/lexer.re
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"

static ProtobufCBinaryData *
unesc_str(unsigned char *src)
{
  ProtobufCBinaryData *dst_pbbd;
  unsigned char *dst;
  int i = 0, dst_len = 0;
  unsigned char oct[4];

  dst_pbbd = malloc(sizeof(ProtobufCBinaryData));
  dst = malloc(strlen(src) + 1);
  if (!dst_pbbd || !dst) {
    goto unesc_str_error;
  }
  oct[3] = '\0';

  while (src[i]) {
    if (src[i] != '\\') {
      dst[dst_len++] = src[i++];
    } else {
      i++;
      switch (src[i]) {
        case '0':
          if ((src[i+1] >= '0' && src[i+1] <= '7')
              && (src[i+2] >= '0' && src[i+2] <= '7')) {
            memcpy(oct, src + i, 3);
            printf("x: '%s'\n", oct);
            dst[dst_len++] = (unsigned char)strtoul(oct, NULL, 8);
            i += 2;  /* Gets incremented again down below. */
          } else {
            goto unesc_str_error;
          }
          break;
        case '\'':
          dst[dst_len++] = '\'';
          break;
        case '\"':
          dst[dst_len++] = '\"';
          break;
        case '\\':
          dst[dst_len++] = '\\';
          break;
        case 'n':
          dst[dst_len++] = '\n';
          break;
        case 'r':
          dst[dst_len++] = '\r';
          break;
        case 't':
          dst[dst_len++] = '\t';
          break;
        default:
          goto unesc_str_error;
          break;
      }
      i++;
    }
  }

  dst_pbbd->data = dst;
  dst_pbbd->len = dst_len;
  return dst_pbbd;

unesc_str_error:
  free(dst);
  free(dst_pbbd);
  return NULL;
}

#define CHUNK 4096

static int
fill(Scanner *s)
{
  char *buf;
  int len, oldlen, nmemb;

  if (s->token > s->limit) {
    /* this shouldn't happen */
    return 0;
  }
  if (s->f && !feof(s->f)) {
    oldlen = s->limit - s->token;
    buf = malloc(CHUNK + oldlen);
    memcpy(buf, s->token, oldlen);
    nmemb = fread(buf + oldlen, 1, CHUNK, s->f);
    if (nmemb != CHUNK) {
      /* Short read.  eof.  Append nul. */
      len = oldlen + nmemb;
      buf[len] = '\0';
    }
    /* Reset the world to use buf. */
    s->cursor = &buf[s->cursor - s->token];
    s->limit = buf + len;
    s->token = buf;
    free(s->buffer);
    s->buffer = buf;
  }

  return s->limit >= s->cursor;
}

#define YYFILL(n) { if (!fill(s)) return 0; }
#define RETURN(t) { return t; }

int
scan(Scanner *s)
{
token_start:
  s->token = s->cursor;

  /*!re2c
  re2c:define:YYCTYPE   = "unsigned char";
  re2c:define:YYCURSOR  = s->cursor;
  re2c:define:YYLIMIT   = s->limit;
  re2c:define:YYMARKER  = s->buffer;

  I = [-]? [0-9]+;
  F = [-]? [0-9]* "." [0-9]+;
  BW = [a-zA-Z0-9_]+;
  EQ = [\\] ["];
  NL = "\n";
  QS = ["] (EQ|[^"]|NL)* ["];
  WS = [ \t\n];

  I | F           { RETURN(NUMBER); }
  "true"          { s->boolean=true; RETURN(BOOLEAN); }
  "false"         { s->boolean=false; RETURN(BOOLEAN); }
  BW              { RETURN(BAREWORD); }
  QS              { RETURN(QUOTED); }
  "{"             { RETURN(OBRACE); }
  "}"             { RETURN(CBRACE); }
  ":"             { RETURN(COLON); }
  WS              { goto token_start; }
  "\000"          { return 0; }
  */
}

void
scanner_init_file(Scanner *s, FILE *f)
{
  memset(s, 0, sizeof(Scanner));
  s->f = f;
}

void
scanner_init_string(Scanner *s, char *buf)
{
  memset(s, 0, sizeof(Scanner));
  s->buffer = buf;
  s->cursor = buf;
  s->limit = &buf[strlen(buf)];
}
