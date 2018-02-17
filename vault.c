
// vault 
// detect crates in the vault and report the value
// uses i2c to read values from ultra-sonics
// write count inverted on 3 pair signal lines


// ultra-sonic's  pre-programmed at 0x70 0x71 0x72

// output count value on pairs on 0,5 6,13 19,26
//  p1  p0  
//   1   1   -> 0
//   1   0   -> 1
//   0   1   -> 2
//   0   0   -> 3


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
// include <i2c/smbus.h>
#include <linux/types.h>
#include <linux/i2c.h>

extern __s32 i2c_smbus_access(int file, char read_write, __u8 command, int size, union i2c_smbus_data *data);
extern __s32 i2c_smbus_write_quick(int file, __u8 value);
extern __s32 i2c_smbus_read_byte(int file);
extern __s32 i2c_smbus_write_byte(int file, __u8 value);
extern __s32 i2c_smbus_read_byte_data(int file, __u8 command);
extern __s32 i2c_smbus_write_byte_data(int file, __u8 command, __u8 value);
extern __s32 i2c_smbus_read_word_data(int file, __u8 command);
extern __s32 i2c_smbus_write_word_data(int file, __u8 command, __u16 value);
extern __s32 i2c_smbus_process_call(int file, __u8 command, __u16 value);

/* Returns the number of read bytes */
extern __s32 i2c_smbus_read_block_data(int file, __u8 command, __u8 *values);
extern __s32 i2c_smbus_write_block_data(int file, __u8 command, __u8 length, const __u8 *values);


int defgpio[] = { 0, 5, 6, 13, 19, 26 };        // gpio pins used for output (bcm pin name)
#define GPIOCNT 6

typedef struct _gpio {
    int gpio;           // pin-number for this gpio
    char *filepath;     // filepath to access gpio
} gpio_t;

int defuson[] = { 0x70, 0x71, 0x72 };           // default ultra-sonic bus-addr

typedef struct _usonic {
    int fd;             // file desc for this usonic
    int result;         // last result
    // write to addr 0    0x50 (inches) or 0x51 (cm)
    // read addr 2 (high) and addr 3 (low)  result
} usonic_t;

gpio_t *gpio;

void 
setgpio(gpio_t *xgpio, int value)
{
    int fd;

    if (0 > (fd = open(xgpio->filepath, O_WRONLY))) {
        int serr = errno;
        fprintf(stderr, "set: failed to open -- %s -- errno %d : %s\n", xgpio->filepath, serr, sys_errlist[serr]);
        return;
    }
    if (value)
        write(fd, "1", 1);
    else
        write(fd, "0", 1);
    close(fd);
}

usonic_t *
openus(int addr)
{
    usonic_t *us = (usonic_t *) malloc(sizeof(usonic_t));

    us->result = 0xffff;
    if (0 > (us->fd = open("/dev/i2c-1", O_RDWR))) {
        int serr = errno;
        fprintf(stderr, "openus: failed to open i2c -- errno %d : %s\n", serr, sys_errlist[serr]);
        return NULL;
    }

    if (0 > ioctl(us->fd, I2C_SLAVE, addr)) {
        int serr = errno;
        fprintf(stderr, "openus: failed to acquire bus i2c 0x%x -- errno %d : %s\n", addr, serr, sys_errlist[serr]);
        close(us->fd);
        us->fd = -1;
        return NULL;
    }

    return us;
}

void
readDistanceUS(usonic_t *us)
{
    int result;
    int high, low;

    us->result = 0;
    i2c_smbus_write_byte_data(us->fd, 0x0, 0x50);       // get distance in inches

    sleep(1);

    high = i2c_smbus_read_byte_data(us->fd, 0x02);
    low = i2c_smbus_read_byte_data(us->fd, 0x03);
    us->result = ((0x7f & high) << 8) | (0x7f & low);
}

void
main(int argc, char **argv)
{
    // initialize output lines

    // turn on export
    for (int i = 0; i < GPIOCNT; i++) {
        int fd;
        char buf[20];
        if (0 > (fd = open("/sys/class/gpio/export", O_WRONLY))) {
            int serr = errno;
            fprintf(stderr, "failed to open gpio/export -- errno %d : %s\n", serr, sys_errlist[serr]);
            exit(1);
        }

        sprintf(buf, "%d\n", defgpio[i]);
        write(fd, buf, strlen(buf));

        close(fd);
    }

    // set GPIO direction
    for (int i = 0; i < GPIOCNT; i++) {
        int fd;
        char buf[20];
        char dirf[80];
        sprintf(dirf, "/sys/class/gpio/gpio%d/direction", defgpio[i]);
        if (0 > (fd = open(dirf, O_WRONLY))) {
            int serr = errno;
            fprintf(stderr, "failed to open -- %s -- errno %d : %s\n", dirf, serr, sys_errlist[serr]);
            exit(2);
        }
        sprintf(buf, "out\n");
        write(fd, buf, strlen(buf));
        close(fd);
    }

    gpio = (gpio_t *) malloc(GPIOCNT * sizeof(gpio_t));

    // alloc gpio data and set default (high)
    for (int i = 0; i < GPIOCNT; i++) {
        int fd;
        char buf[20];
        char dirf[80];
        gpio_t * tgpio = gpio + i;
        sprintf(dirf, "/sys/class/gpio/gpio%d/value", defgpio[i]);
        tgpio->filepath = strdup(dirf);
        tgpio->gpio = defgpio[i];
        setgpio(tgpio, 1);
    }

    usonic_t *usc[3];

    usc[0] = openus(0x70);
    usc[1] = openus(0x71);
    usc[2] = openus(0x72);

    while (1) {
	for (int i = 0; i < 3; i++) {
	    readDistanceUS(usc[i]);
	    printf("read 0x%x : value %d\n", 0x70 + i, usc[i]->result);
	}
	puts("");
	sleep(3);
    }
}
