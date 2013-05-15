
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
#include <linux/slab.h>

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

static void write_null_mem(struct file* file){
  u64 bla = 0;
  unsigned char sep = '\xFF';

  file_write(file, NULL, &bla, sizeof(bla));
  file_write(file, NULL, &sep, 1);
}

static int write_mem(struct mm_struct * mem,  struct file* file){
  struct vm_area_struct * vma;
  struct page * p;
  struct file * memdumpfile;
  void * v;
  unsigned long i, is, end_a, start_a, s = 0;
  char name[50], memdumppath[255];
  u64 flags;
  u64 length;
  u64 address;
  unsigned char sep = '\xFF';

  down_read(&mem->mmap_sem);

  for(vma = mem->mmap; vma != NULL; vma = vma->vm_next){
    memset(name, '\0', 50);

    if(vma->vm_file)
      tpe_d_path(vma->vm_file, name, 50);

    flags = (u64)vma->vm_flags;

    end_a = PAGE_ALIGN(vma->vm_end);
    start_a = vma->vm_start & PAGE_MASK;
    length = end_a - start_a;
    address = (u64)start_a;

    //Write header
    file_write(file, NULL, &length, sizeof(length));
    file_write(file, NULL, &sep, 1);
    file_write(file, NULL, &address, sizeof(address));
    file_write(file, NULL, &sep, 1);
    file_write(file, NULL, &flags, sizeof(flags));
    file_write(file, NULL, &sep, 1);
    file_write(file, NULL, name, 50);
    file_write(file, NULL, &sep, 1);

    s = 0;

    strcpy(memdumppath, path);
    if(vma->vm_file)
      strcat(memdumppath, name);
    
    sprintf(name, "%lu", address);
    strcat(memdumppath, name);
    strcat(memdumppath, ".dump");
    memdumpfile = file_open(memdumppath, O_WRONLY | O_CREAT, 0444);

    if(memdumpfile == NULL){
      DBG("ERROR OPENING FILE; %s", memdumppath);
      break;
    }

    for(i = start_a; i < end_a; i += PAGE_SIZE){
      p = pfn_to_page((i) >> PAGE_SHIFT);
      is = min((size_t) PAGE_SIZE, (size_t) (end_a - i + 1));

      if(s + is > length){
      	is -= (length - s);
      }
     
      v = kmap(p);
      file_write(memdumpfile, NULL, v, is);
      //DBG("%lu", is);
      s+= is;
      kunmap(p);
    }

    if(s != length){
      DBG("Warning: Wrote %lu out of %lu!", s, length);
    }
    DBG("%lu", length);

    file_sync(memdumpfile);
    file_close(memdumpfile);
  }

  up_read(&mem->mmap_sem);

  return 0;
}

static int __init init_pinfodump(void)
{
  char * data, * stateBuffer, * command, * pathBuffer;
  struct task_struct *task;
  struct file * outfile;

  data = kmalloc(255, GFP_KERNEL);
  stateBuffer = kmalloc(255, GFP_KERNEL);
  command = kmalloc(255, GFP_KERNEL);
  pathBuffer = kmalloc(255, GFP_KERNEL);

  if(!data || !stateBuffer || !command || !pathBuffer){
    DBG("ERROR, allocation");
    return -ENOMEM;
  }

  if(!path || path[strlen(path)-1] != '/') {
    //No path specified
    DBG("No valid path specified (note: ending / )");
    return -EINVAL;
  }

  strcpy(pathBuffer, path);
  strcat(pathBuffer, "dump.info");
  
  DBG("Process info dumping now starts\n");
  outfile = file_open(pathBuffer, O_WRONLY | O_CREAT, 0444);

  rcu_read_lock();                                                    
  for_each_process(task) {                                             
    task_lock(task);                                             

    //DBG("%d %d", task->pid, task->parent->pid);

    getStateDesc(task->state, stateBuffer);
    strcpy(command, task->comm);

    sprintf(data, "%i %s %i %i %s\xFF", task->pid, command, get_task_uid(task), get_task_parent(task)->pid, stateBuffer);
    file_write(outfile, NULL, data, strlen(data));

    if(task->mm && dumpmem){
      DBG("Dumping memory");
      write_mem(task->mm, outfile);;
    }else{
      write_null_mem(outfile);
    }

    //write footer
    file_write(outfile, NULL, "nojan", 5);

    task_unlock(task);     
  }                                                                    
  rcu_read_unlock();           

  file_sync(outfile);
  file_close(outfile);

  
  kfree(data);
  kfree(stateBuffer);
  kfree(command);
  kfree(pathBuffer);

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
