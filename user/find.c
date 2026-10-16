#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "user/user.h"

int use_exec = 0;
char *cmd_args[MAXARG];
int cmd_count = 0;

void
run_command(char *path)
{
    //exec = 0 thì chi in duong dan cua part A
    if (use_exec == 0) {
        printf("%s\n", path);
        return;
    }

    //Tao process con
    int pid = fork();

    if (pid < 0) {
        fprintf(2, "find: fork failed\n");
        return;
    }

    if (pid == 0) {
        char *args[MAXARG];

        //Sao chep tham so cua lenh
        for (int i = 0; i < cmd_count; i++) {
            args[i] = cmd_args[i];
        }

        args[cmd_count] = path;
        args[cmd_count + 1] = 0;

        //Precess con chay chuong trinh duoc yeu cau
        exec(args[0], args);

        fprintf(2, "find: exec failed\n");
        exit(1);
    }

    wait(0);
}

void
find(char *path, char *target)
{
    int fd;
    struct stat st;
    struct dirent de;
    char buf[512];

    //Mo duong dan va lay thong tin file/thu muc
    fd = open(path, O_RDONLY);

    if (fd < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    //Khong phai thu muc thi kiem tra ten va return
    if (st.type != T_DIR) {
        char *name = path;

        for (char *p = path; *p != '\0'; p++) {
            if (*p == '/')
                name = p + 1;
        }

        if (strcmp(name, target) == 0) {
            run_command(path);
        }

        close(fd);
        return;
    }

    //Lay ten thuc muc hien tai de so sanh
    char *dirname = path;

    for (char *p = path; *p != '\0'; p++) {
        if (*p == '/')
            dirname = p + 1;
    }

    if (strcmp(dirname, target) == 0) {
        run_command(path);
    }

    //Doc cac muc ben trong thu muc
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)
            continue;

        char name[DIRSIZ + 1];

        memmove(name, de.name, DIRSIZ);
        name[DIRSIZ] = '\0';

        if (strcmp(name, ".") == 0 ||
            strcmp(name, "..") == 0) {
            continue;
        }

        int len = strlen(path);

        if (len + 1 + strlen(name) + 1 > sizeof(buf)) {
            fprintf(2, "find: path too long\n");
            continue;
        }

        strcpy(buf, path);

        if (len > 0 && path[len - 1] != '/') {
            buf[len] = '/';
            buf[len + 1] = '\0';
        }

        strcpy(buf + strlen(buf), name);

        //Goi de quy de tim kiem tron cac thu muc con
        find(buf, target);
    }

    close(fd);
}

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: find path filename [-exec cmd ...]\n");
        exit(1);
    }

    //Kiem tra tham so -exec
    if (argc > 3) {
        if (strcmp(argv[3], "-exec") != 0 || argc < 5) {
            fprintf(2, "Usage: find path filename [-exec cmd ...]\n");
            exit(1);
        }

        use_exec = 1;

        //Luu cac tham so cua lenh vao cmd_args
        for (int i = 4; i < argc; i++) {
            if (cmd_count >= MAXARG - 2) {
                fprintf(2, "find: too many arguments\n");
                exit(1);
            }

            cmd_args[cmd_count] = argv[i];
            cmd_count++;
        }
    }

    //Goi ham find de tim kiem file/thu muc
    find(argv[1], argv[2]);

    exit(0);
}
