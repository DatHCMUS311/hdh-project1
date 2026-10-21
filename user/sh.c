// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void); // Fork but panics on failure.
void panic(char *);
struct cmd *parsecmd(char *);
void runcmd(struct cmd *) __attribute__((noreturn));

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    exit(1);

  switch (cmd->type) {
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd *)cmd;
    if (ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    close(rcmd->fd);
    if (open(rcmd->file, rcmd->mode) < 0) {
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    if (fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    if (pipe(p) < 0)
      panic("pipe");
    if (fork1() == 0) {
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if (fork1() == 0) {
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    if (fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

#define MAX_HIST  32
#define HIST_SIZE 128

static char history[MAX_HIST][HIST_SIZE];
static int hist_count = 0;

static int
mystrncmp(const char *p, const char *q, uint n)
{
  while (n > 0 && *p && *p == *q) {
    n--;
    p++;
    q++;
  }
  if (n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

static void
add_history(const char *s)
{
  int len = strlen(s);
  while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
    len--;

  int start = 0;
  while (start < len && (s[start] == ' ' || s[start] == '\t'))
    start++;

  if (start >= len)
    return;

  if (s[start] == '!')
    return;

  if (hist_count > 0 && strcmp(history[hist_count - 1], s + start) == 0)
    return;

  char clean[HIST_SIZE];
  int i = 0;
  while (start < len && i + 1 < HIST_SIZE) {
    clean[i++] = s[start++];
  }
  clean[i] = 0;

  if (hist_count < MAX_HIST) {
    strcpy(history[hist_count++], clean);
  } else {
    for (int j = 0; j < MAX_HIST - 1; j++)
      strcpy(history[j], history[j + 1]);
    strcpy(history[MAX_HIST - 1], clean);
  }
}

static void
tab_complete(char *buf, int *len, int nbuf)
{
  int cur_len = *len;
  int word_start = cur_len;
  while (word_start > 0 && buf[word_start - 1] != ' ')
    word_start--;

  int prefix_len = cur_len - word_start;

  int is_first = 1;
  for (int i = 0; i < word_start; i++) {
    if (buf[i] != ' ' && buf[i] != '\t') {
      is_first = 0;
      break;
    }
  }

  char *dir = is_first ? "/" : ".";
  int fd = open(dir, O_RDONLY);
  if (fd < 0)
    return;

  struct dirent de;
  char matches[64][DIRSIZ + 1];
  int nmatches = 0;

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    char name[DIRSIZ + 1];
    memmove(name, de.name, DIRSIZ);
    name[DIRSIZ] = 0;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    if (mystrncmp(name, &buf[word_start], prefix_len) == 0) {
      if (nmatches < 64) {
        strcpy(matches[nmatches++], name);
      }
    }
  }
  close(fd);

  if (nmatches == 0)
    return;

  for (int i = 0; i < nmatches - 1; i++) {
    for (int j = i + 1; j < nmatches; j++) {
      if (strcmp(matches[i], matches[j]) > 0) {
        char tmp[DIRSIZ + 1];
        strcpy(tmp, matches[i]);
        strcpy(matches[i], matches[j]);
        strcpy(matches[j], tmp);
      }
    }
  }

  if (nmatches == 1) {
    char *suffix = matches[0] + prefix_len;
    while (*suffix && cur_len + 1 < nbuf) {
      buf[cur_len++] = *suffix;
      write(1, suffix, 1);
      suffix++;
    }
    if (cur_len + 1 < nbuf) {
      buf[cur_len++] = ' ';
      write(1, " ", 1);
    }
    buf[cur_len] = 0;
    *len = cur_len;
  } else {
    write(1, "\n", 1);
    for (int i = 0; i < nmatches; i++) {
      if (i > 0)
        write(1, " ", 1);
      write(1, matches[i], strlen(matches[i]));
    }
    write(1, "\n$ ", 3);
    write(1, buf, cur_len);
  }
}

int
getcmd(char *buf, int nbuf)
{
  struct stat st;
  if (fstat(0, &st) < 0 || st.type != T_DEVICE) {
    int i = 0;
    char c;
    memset(buf, 0, nbuf);
    while (i + 1 < nbuf) {
      int cc = read(0, &c, 1);
      if (cc < 1)
        break;
      buf[i++] = c;
      if (c == '\n' || c == '\r')
        break;
    }
    buf[i] = 0;
    if (buf[0] == 0)
      return -1;
    return 0;
  }

  write(2, "$ ", 2);
  memset(buf, 0, nbuf);

  int len = 0;
  int hist_idx = hist_count;

  while (1) {
    char c;
    int cc = read(0, &c, 1);
    if (cc < 1) {
      if (len == 0)
        return -1;
      break;
    }

    if (c == 4) {
      if (len == 0)
        return -1;
      continue;
    }

    if (c == '\r' || c == '\n') {
      write(1, "\n", 1);
      buf[len++] = '\n';
      buf[len] = 0;
      break;
    }

    if (c == '\b' || c == 0x7f) {
      if (len > 0) {
        len--;
        buf[len] = 0;
        write(1, "\b \b", 3);
      }
      continue;
    }

    if (c == 21) {
      while (len > 0) {
        len--;
        write(1, "\b \b", 3);
      }
      buf[0] = 0;
      continue;
    }

    if (c == '\t') {
      tab_complete(buf, &len, nbuf);
      continue;
    }

    if (c == 27) {
      char c1, c2;
      if (read(0, &c1, 1) == 1 && c1 == '[') {
        if (read(0, &c2, 1) == 1) {
          if (c2 == 'A') {
            if (hist_idx > 0) {
              hist_idx--;
              while (len > 0) {
                len--;
                write(1, "\b \b", 3);
              }
              int hlen = strlen(history[hist_idx]);
              for (int i = 0; i < hlen && len + 1 < nbuf; i++)
                buf[len++] = history[hist_idx][i];
              buf[len] = 0;
              write(1, buf, len);
            }
          } else if (c2 == 'B') {
            if (hist_idx < hist_count) {
              hist_idx++;
              while (len > 0) {
                len--;
                write(1, "\b \b", 3);
              }
              if (hist_idx < hist_count) {
                int hlen = strlen(history[hist_idx]);
                for (int i = 0; i < hlen && len + 1 < nbuf; i++)
                  buf[len++] = history[hist_idx][i];
                buf[len] = 0;
                write(1, buf, len);
              } else {
                len = 0;
                buf[0] = 0;
              }
            }
          }
        }
      }
      continue;
    }

    if (c >= 32 && c <= 126) {
      if (len + 2 < nbuf) {
        buf[len++] = c;
        buf[len] = 0;
        write(1, &c, 1);
      }
    }
  }

  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd;

  while ((fd = open("console", O_RDWR)) >= 0) {
    if (fd >= 3) {
      close(fd);
      break;
    }
  }

  while (getcmd(buf, sizeof(buf)) >= 0) {
    char *cmd = buf;
    while (*cmd == ' ' || *cmd == '\t')
      cmd++;
    if (*cmd == '\n' || *cmd == '\0')
      continue;

    if (cmd[0] == '!') {
      if (cmd[1] == '!') {
        if (hist_count == 0) {
          fprintf(2, "no commands in history\n");
          continue;
        }
        cmd = history[hist_count - 1];
        printf("%s\n", cmd);
      } else if (cmd[1] >= '0' && cmd[1] <= '9') {
        int n = atoi(cmd + 1);
        if (n >= 1 && n <= hist_count) {
          cmd = history[n - 1];
          printf("%s\n", cmd);
        } else {
          fprintf(2, "event not found: %s", cmd);
          continue;
        }
      }
    }

    add_history(cmd);

    char clean_name[16];
    int k = 0;
    while (cmd[k] && cmd[k] != ' ' && cmd[k] != '\t' && cmd[k] != '\n' &&
           cmd[k] != '\r' && k < 15) {
      clean_name[k] = cmd[k];
      k++;
    }
    clean_name[k] = 0;

    if (strcmp(clean_name, "history") == 0) {
      for (int i = 0; i < hist_count; i++) {
        printf("%d %s\n", i + 1, history[i]);
      }
      continue;
    }

    if (cmd[0] == 'c' && cmd[1] == 'd' &&
        (cmd[2] == ' ' || cmd[2] == '\t' || cmd[2] == '\n' || cmd[2] == '\0')) {
      char *p = cmd + 2;
      while (*p == ' ' || *p == '\t')
        p++;
      char *end = p;
      while (*end && *end != '\n' && *end != '\r')
        end++;
      *end = 0;
      if (chdir(p) < 0)
        fprintf(2, "cannot cd %s\n", p);
      continue;
    }

    if (fork1() == 0)
      runcmd(parsecmd(cmd));
    wait(0);
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if (pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd *
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd *)cmd;
}

struct cmd *
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd *)cmd;
}

struct cmd *
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd *)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  if (q)
    *q = s;
  ret = *s;
  switch (*s) {
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if (*s == '>') {
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if (eq)
    *eq = s;

  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if (s != es) {
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd *
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while (peek(ps, es, "&")) {
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if (peek(ps, es, ";")) {
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd *
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if (peek(ps, es, "|")) {
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd *
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while (peek(ps, es, "<>")) {
    tok = gettoken(ps, es, 0, 0);
    if (gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch (tok) {
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE | O_TRUNC, 1);
      break;
    case '+': // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd *
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if (!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if (!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd *
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if (peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd *)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while (!peek(ps, es, "|)&;")) {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0)
      break;
    if (tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if (argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd *
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    return 0;

  switch (cmd->type) {
  case EXEC:
    ecmd = (struct execcmd *)cmd;
    for (i = 0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
