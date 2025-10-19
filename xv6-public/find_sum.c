#include "types.h"
#include "user.h"
#include "fcntl.h"

typedef struct {
  int building;
  int curval;
  int total;
} AccCtx;

static inline int is_digit(char c) {
  return (c >= '0' && c <= '9');
}

static void acc_sep(AccCtx *ctx) {
  if (ctx->building) {
    ctx->total += ctx->curval;
    ctx->curval = 0;
    ctx->building = 0;
  }
}

static void acc_push(AccCtx *ctx, char c) {
  if (is_digit(c)) {
    ctx->building = 1;
    ctx->curval = ctx->curval * 10 + (c - '0');
  } else {
    acc_sep(ctx);
  }
}

static void acc_feed(AccCtx *ctx, const char *buf, int nbytes) {
  for (int i = 0; i < nbytes; i++)
    acc_push(ctx, buf[i]);
}

static int write_number_ln(int fd, int value) {
  char tmp[32];
  int t = 0;

  if (value == 0) {
    tmp[t++] = '0';
  } else {
    while (value > 0 && t < (int)sizeof(tmp)) {
      tmp[t++] = '0' + (value % 10);
      value /= 10;
    }
    for (int i = 0; i < t / 2; i++) {
      char z = tmp[i];
      tmp[i] = tmp[t - 1 - i];
      tmp[t - 1 - i] = z;
    }
  }

  tmp[t++] = '\n';
  return write(fd, tmp, t);
}

int
main(int argc, char *argv[])
{
  AccCtx ctx;
  ctx.building = 0;
  ctx.curval   = 0;
  ctx.total    = 0;

  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      const char *s = argv[i];
      acc_feed(&ctx, s, strlen(s));
      acc_sep(&ctx);
    }
  } else {
    char buf[256];
    int n;
    while ((n = read(0, buf, sizeof(buf))) > 0) {
      acc_feed(&ctx, buf, n);
    }
  }

  acc_sep(&ctx);

  int fd = open("result.txt", O_CREATE | O_WRONLY);
  if (fd < 0) {
    printf(2, "find_sum: cannot open result.txt\n");
    exit();
  }
  if (write_number_ln(fd, ctx.total) < 0) {
    printf(2, "find_sum: write failed\n");
    close(fd);
    exit();
  }
  close(fd);
  exit();
}
