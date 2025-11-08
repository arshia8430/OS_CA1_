#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

void create_test_file(void)
{
  int fd = open("grep_test_file.txt", O_CREATE | O_WRONLY);
  if(fd < 0){
    printf(1, "Failed to create test file\n");
    exit();
  }

  write(fd, "hello hello ha ha ha ha\n", sizeof("hello hello ha ha ha ha\n") - 1);
  write(fd, "cat dog os os \n", sizeof("cat dog os os \n") - 1);
  write(fd, "Another line, no keyword.\n", sizeof("Another line, no keyword.\n") - 1);
  write(fd, "here is key CA2\n", sizeof("here is key CA2\n") - 1);
  write(fd, "The end.\n", sizeof("The end.\n") - 1);

  close(fd);
}

int main(int argc, char *argv[])
{
  char buf[512];
  int len;
  create_test_file();
  printf(1, "--- testing---\n");
  len = grep_syscall("CA2", "grep_test_file.txt", buf, sizeof(buf));
  printf(1, "search hello\n");
  printf(1, "syscall returned: %d\n", len);
  if(len > 0) {
    printf(1, "Found line: %s", buf);
  } else {
    printf(1, "Keyword not found.\n");
  }
  memset(buf, 0, sizeof(buf));

  len = grep_syscall("CA1", "grep_test_file.txt", buf, sizeof(buf));
  printf(1, "\nsearch CA1\n");
  printf(1, "syscall returned: %d\n", len);
  if(len > 0) {
    printf(1, "Found line: %s", buf);
  } else {
    printf(1, "error\n");
  }
  memset(buf, 0, sizeof(buf));
  len = grep_syscall("end", "grep_test_file.txt", buf, sizeof(buf));
  printf(1, "search end\n");
  printf(1, "syscall returned: %d\n", len);
  if(len > 0) {
    printf(1, "Found line: %s", buf);
  } else {
    printf(1, "Keyword not found.\n");
  }




  unlink("grep_test_file.txt");
  exit();
}
