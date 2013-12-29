
#ifndef __CATSNATCH_GPIO_H__
#define __CATSNATCH_GPIO_H__

#define IN 1
#define OUT 0

int gpio_export(int pin);
int gpio_set_direction(int pin, int direction);
int gpio_write(int pin, int val);

#endif // __CATSNATCH_GPIO_H__
