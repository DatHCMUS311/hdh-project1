#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data, int len);

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    printf("Example 1:\n");
    int a[2] = {61810, 2026};
    memdump("ii", (char *)a, sizeof(a));

    printf("Example 2:\n");
    memdump("S", "a string", sizeof("a string"));

    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *)&s, sizeof(s));

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;

    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");

    printf("Example 4:\n");
    memdump("pihcS", (char *)&example, sizeof(example));

    printf("Example 5:\n");
    memdump("sccccc", (char *)&example, sizeof(example));
  } else if (argc == 2) {
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while (n < sizeof(data)) {
      int nn = read(0, data + n, sizeof(data) - n);
      if (nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data, n);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void
memdump(char *fmt, char *data, int len)
{
  int offset = 0;

  for (int j = 0; fmt[j] != '\0'; j++) {

    // i: Doc 4 byte, in so nguyen 32-bit
    if (fmt[j] == 'i') {
      if (offset + 4 > len) {
        printf("memdump: not enough data for 'i'\n");
        return;
      }

      int value;
      memmove(&value, data + offset, 4);
      printf("%d\n", value);

      offset += 4;
    }

    // p: Doc 8 byte, in so hexa 64-bit
    else if (fmt[j] == 'p') {
      if (offset + 8 > len) {
        printf("memdump: not enough data for 'p'\n");
        return;
      }

      uint64 value;
      memmove(&value, data + offset, 8);

      // In hexa khong co tien to 0x
      char hex[] = "0123456789abcdef";
      int started = 0;

      for (int k = 15; k >= 0; k--) {
        int digit = (value >> (k * 4)) & 0xf;

        if (digit != 0 || started || k == 0) {
          printf("%c", hex[digit]);
          started = 1;
        }
      }
      printf("\n");

      offset += 8;
    }

    // h: Doc 2 byte, in so nguyen 16-bit
    else if (fmt[j] == 'h') {
      if (offset + 2 > len) {
        printf("memdump: not enough data for 'h'\n");
        return;
      }

      short value;
      memmove(&value, data + offset, 2);
      printf("%d\n", (int)value);

      offset += 2;
    }

    // c: Doc 1 byte, in ky tu
    else if (fmt[j] == 'c') {
      if (offset + 1 > len) {
        printf("memdump: not enough data for 'c'\n");
        return;
      }

      char value = data[offset];
      printf("%c\n", value);

      offset += 1;
    }

    // s: Doc 8 byte chua con tro den chuoi
    else if (fmt[j] == 's') {
      if (offset + 8 > len) {
        printf("memdump: not enough data for 's'\n");
        return;
      }

      char *str;
      memmove(&str, data + offset, 8);
      printf("%s\n", str);

      offset += 8;
    }

    // S: Doc chuoi truc tiep trong data
    else if (fmt[j] == 'S') {
      while (offset < len && data[offset] != '\0') {
        printf("%c", data[offset]);
        offset++;
      }

      printf("\n");

      if (offset < len && data[offset] == '\0') {
        offset++;
      }
    }
  }
}