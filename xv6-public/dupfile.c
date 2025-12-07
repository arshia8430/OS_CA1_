#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    printf(1, "usage: dupfile <src_path>\n");
    exit();
  }

  int r = make_duplicate(argv[1]);
  if(r == 0){
    printf(1, "OK: created %s_copy\n", argv[1]);
  } else if(r == -1){
    printf(1, "ERR: source not found: %s\n", argv[1]);
  } else {
    printf(1, "ERR: could not duplicate: %s\n", argv[1]);
  }
  exit();
}
