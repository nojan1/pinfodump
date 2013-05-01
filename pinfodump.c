
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/fs.h>

#include "file.h"

#define DRIVER_AUTHOR "Niklas Hedlund <nojan1989@gmail.com>"
#define DRIVER_DESC   "Tool to dump process info from running kernel"

char * logpath = 0;
module_param(logpath, charp, S_IRUGO);

static int __init init_pinfodump(void)
{
       char data[255];
	struct task_struct *task;
	struct file * outfile;
	unsigned long file_offset = 0;

	if(!logpath) {
	  //No path specified
	  return -EINVAL;
	}

	printk(KERN_INFO "Process info dumping now starts\n");

	outfile = file_open(logpath, O_WRONLY | O_CREAT, 0444);
	strcpy(data, "PID PARENT REAL_PARENT STATE");
	file_write(outfile, file_offset, data, strlen(data));

	rcu_read_lock();                                                    
	for_each_process(task) {                                             
	  task_lock(task);                                             

	  printk(KERN_INFO "%d", task->pid);

	  task_unlock(task);                                           
	}                                                                    
	rcu_read_unlock();           

	file_sync(outfile);
	file_close(outfile);

	return 0;
}

static void __exit close_pinfodump(void)
{
	printk(KERN_INFO "Goodbye, world \n");
}

module_init(init_pinfodump);
module_exit(close_pinfodump);

/* 
 * Get rid of taint message by declaring code as GPL. 
 */
MODULE_LICENSE("GPL");

/*
 * Or with defines, like this:
 */
MODULE_AUTHOR(DRIVER_AUTHOR);	/* Who wrote this module? */
MODULE_DESCRIPTION(DRIVER_DESC);	/* What does this module do */
