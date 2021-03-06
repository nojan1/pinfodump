#ifndef __FILE_H_
#define __FILE_H_

#include <linux/fs.h>

struct file* file_open(const char* path, int flags, int rights);

void file_close(struct file* file);

int file_read(struct file* file, unsigned long long offset, void * data, unsigned int size);

int file_write(struct file* file, unsigned long * offset, void * data, unsigned int size);

int file_sync(struct file* file);

#endif
