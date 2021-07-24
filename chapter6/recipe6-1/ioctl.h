#ifndef IOCTL_H_
#define IOCTL_H_

struct __attribute__((packed)) ioctl_info {
       char led_red, led_green, led_blue;
};

#define             IOCTL_MAGIC         'R'
#define             SET_DATA            _IOW(IOCTL_MAGIC, 2, struct ioctl_info )
#define             GET_DATA            _IOR(IOCTL_MAGIC, 3, struct ioctl_info )

#endif /* IOCTL_H_ */
