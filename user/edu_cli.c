#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#define EDU_IOCTL_CARD_IDENTIFICATION _IOR('e', 1, uint32_t)
#define EDU_IOCTL_CARD_LIVENESS _IOWR('e', 2, uint32_t)
#define EDU_IOCTL_CARD_FACTORIAL _IOWR('e', 3, uint32_t)
#define EDU_IOCTL_CARD_FACTORIAL_IRQ _IOWR('e', 4, uint32_t)


#define EDU_DEVICE "/dev/edu"


typedef enum {
    CMD_ID,
    CMD_LIVENESS,
    CMD_FACTORIAL,
    CMD_FACTORIAL_IRQ,
    CMD_UNKNOWN
} command_t;

typedef struct {
    const char *name;
    command_t cmd;
} command_entry_t;

static const command_entry_t command_table[] = {
    { "id",       CMD_ID },
    { "liveness", CMD_LIVENESS },
    { "factorial", CMD_FACTORIAL },
    { "factorial_irq", CMD_FACTORIAL_IRQ },
    { NULL,       CMD_UNKNOWN }
};

command_t parse_command(const char *arg) {
    for (int i = 0; command_table[i].name != NULL; i++) {
        if (strcmp(arg, command_table[i].name) == 0)
            return command_table[i].cmd;
    }
    return CMD_UNKNOWN;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [value]\n", argv[0]);
        return 1;
    }

    int fd = open(EDU_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    command_t cmd = parse_command(argv[1]);
    uint32_t val;

    switch (cmd) {
    case CMD_ID:
        if (ioctl(fd, EDU_IOCTL_CARD_IDENTIFICATION, &val) < 0) {
            perror("ioctl: card_id");
            close(fd);
            return 1;
        }
        printf("Card ID: 0x%08X\n", val);
        break;

    case CMD_LIVENESS:
        if (argc < 3) {
            fprintf(stderr, "Usage: %s liveness <value>\n", argv[0]);
            close(fd);
            return 1;
        }
        val = (uint32_t)strtoul(argv[2], NULL, 0);
        if (ioctl(fd, EDU_IOCTL_CARD_LIVENESS, &val) < 0) {
            perror("ioctl: liveness");
            close(fd);
            return 1;
        }
        printf("Inverted: 0x%08X\n", val);
        break;
    case CMD_FACTORIAL:
        if (argc < 3) {
            fprintf(stderr, "Usage: %s factorial <value>\n", argv[0]);
            close(fd);
            return 1;
        }
        val = (uint32_t)strtoul(argv[2], NULL, 0);
        if (ioctl(fd, EDU_IOCTL_CARD_FACTORIAL, &val) < 0) {
            perror("ioctl: factorial");
            close(fd);
            return 1;
        }
        printf("Factorial: %u (0x%08X)\n", val, val);
    break;
    case CMD_FACTORIAL_IRQ:
        if (argc < 3) {
            fprintf(stderr, "Usage: %s factorial_irq <value>\n", argv[0]);
            close(fd);
            return 1;
        }
        val = (uint32_t)strtoul(argv[2], NULL, 0);
        if (ioctl(fd, EDU_IOCTL_CARD_FACTORIAL_IRQ, &val) < 0) {
            perror("ioctl: factorial_irq");
            close(fd);
            return 1;
        }
        printf("Factorial IRQ: %u (0x%08X)\n", val, val);
    break;
    default:
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}