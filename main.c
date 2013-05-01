
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/pfn.h>
#include <linux/highmem.h>

#include "file.h"

#define DRIVER_AUTHOR "Niklas Hedlund <nojan1989@gmail.com>"
#define DRIVER_DESC   "Tool to dump process info from running kernel"

//#undef DEBUG
#define DEBUG

#ifdef DEBUG
#define DBG(fmt, args...) do { printk("[pinfodump] "fmt"\n", ## args); } while (0)
#else
#define DBG(fmt, args...) do {} while(0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
#define get_task_uid(task) task->uid
#define get_task_parent(task) task->parent
#else
#define get_task_uid(task) task->cred->uid
#define get_task_parent(task) task->real_parent
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25)
#define tpe_d_path(file, buf, len) d_path(file->f_dentry, file->f_vfsmnt, buf, len);
#else
#define tpe_d_path(file, buf, len) d_path(&file->f_path, buf, len);
#endif

char * path = 0;
int dumpmem = 1;

module_param(path, charp, S_IRUGO);
module_param(dumpmem, int, S_IRUGO);

char *exe_from_mm(struct mm_struct *mm, char *buf, int len) {
  struct vm_area_struct *vma;
  char *p = NULL;

  down_read(&mm->mmap_sem);

  vma = mm->mmap;

  while (vma) {
    if ((vma->vm_flags & VM_EXEC) && vma->vm_file)
      break;
    vma = vma->vm_next;
  }

  if (vma && vma->vm_file)
    p = tpe_d_path(vma->vm_file, buf, len);

up_read(&mm->mmap_sem);

return p;
}

void getStateDesc(long state, char *buffer){
  switch(state){
  case 0:
    strcpy(buffer, "TASK_RUNNING");
    break;
  case 1:
    strcpy(buffer, "TASK_INTERRUPTIBLE");
    break;
  case 2:
    strcpy(buffer, "TASK_UNINTERRUPTIBLE");
    break;
  case 4:
    strcpy(buffer, "TASK_STOPPED");
    break;
  case 8:
    strcpy(buffer, "TASK_TRACED");
    break;
  case 16:
    strcpy(buffer, "EXIT_ZOMBIE");
    break;
  case 32:
    strcpy(buffer, "EXIT_DEAD");
    break;
  case 64:
    strcpy(buffer, "TASK_DEAD");
    break;
  case 128:
    strcpy(buffer, "TASK_WAKEKILL");
    break;
  default:
    strcpy(buffer, "Unknown");
  }
}

static void write_null_mem(struct file* file, unsigned long* offset){
  int i;
  unsigned long bla = 0;
  unsigned char sep = '\xFF';

  for(i = 0;i<3;i++){
    //Write length of segment
    file_write(file, *offset, ((unsigned char*)&bla), sizeof(bla));
    file_write(file, *offset, &sep, 1);
    DBG("%lu %lu", bla, sizeof(bla));
  }
}

static int write_mem(unsigned long start, unsigned long stop,  struct file* file, unsigned long* offset){
  #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
        resource_size_t i, is;
 #else
        __PTRDIFF_TYPE__ i, is;
 #endif

	struct page * p;
	void * v;
	unsigned long s = 0;
	unsigned char sep = '\xFF';
	unsigned long totlength = stop - start;
	
	//Write length of segment
        file_write(file, *offset, ((unsigned char*)&totlength), sizeof(totlength));
	file_write(file, *offset, &sep, 1);

	DBG("%lu %lu", totlength, sizeof(totlength));

	for (i = start; i <= stop; i+= PAGE_SIZE){
	  p = pfn_to_page((i) >> PAGE_SHIFT);
	  is = min((size_t) PAGE_SIZE, (size_t) (stop - i + 1));

	  if(s + is > totlength){
	    is -= (totlength - s);
	  }

	  v = kmap(p);
	  s += file_write(file, *offset, (unsigned char*)v, is);
	  kunmap(p);
	}

	if(s != totlength){
	  DBG("Warning: Wrote %lu out of %lu!", s, totlength);
	}

	return 0;
}

static int __init init_pinfodump(void)
{
  char data[255], stateBuffer[255], command[255];
	struct task_struct *task;
	struct file * outfile;
	unsigned long file_offset = 0;

	if(!path) {
	  //No path specified
	  DBG("No path specified");
	  return -EINVAL;
	}

	DBG("Process info dumping now starts\n");

	outfile = file_open(path, O_WRONLY | O_CREAT, 0444);

	rcu_read_lock();                                                    
	for_each_process(task) {                                             
	  task_lock(task);                                             

	  //DBG("%d %d", task->pid, task->parent->pid);

	  getStateDesc(task->state, stateBuffer);

	  //if(task->mm){
	  //exe_from_mm(task->mm, command, 255);
	  //}else{
	    strcpy(command, task->comm);
	    //}

	  sprintf(data, "%i %s %i %i %s\xFF", task->pid, command, get_task_uid(task), get_task_parent(task)->pid, stateBuffer);
	  file_write(outfile, file_offset, data, strlen(data));
	  file_offset += strlen(data);

	  if(task->mm && dumpmem){
	    DBG("Dumping memory");
	    write_mem(task->mm->start_code, task->mm->end_code, outfile, &file_offset);
	    write_mem(task->mm->start_data, task->mm->end_data, outfile, &file_offset);
	    write_mem(task->mm->start_brk, task->mm->brk, outfile, &file_offset);
	  }else{
	    write_null_mem(outfile, &file_offset);
	  }

	  task_unlock(task);                                           
	}                                                                    
	rcu_read_unlock();           

	file_sync(outfile);
	file_close(outfile);

	DBG("Dump completed!");

	return 0;
}

static void __exit close_pinfodump(void)
{
  DBG("Goodbye, world \n");
}

module_init(init_pinfodump);
module_exit(close_pinfodump);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
