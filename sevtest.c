// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Prototype of SEV DEMO Kernel driver
// This driver flips C bit for user supplied address. 
// THIS IS VERY UNSAFE:
// 1. Unsupported. Kernel does not support C bit flip on user addresses with set_memory decrypted function.
// 2. Guest swapping may not save and restore C bit, kernel does not expect user pages to have c bit diabled. 
// 3. Code below cuts corners and ignores safert checks, can lead to crash.
// THIS CAN LEAD TO UNSTABLE SYSTEM

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/set_memory.h>
#include <linux/device.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgtable_types.h>
#include <asm/tlbflush.h>

static int sevtest_major;
static struct class *sevtest_class;

#define SET_C_BIT 1
#define CLEAR_C_BIT 0

#define DEBUG

#ifdef DEBUG
    #define DBG_PRINT(x,y) printk(x,y)
#else
    #define DBG_PRINT(x,y) 
#endif 


static void cpa_flush_all(unsigned long cache);
pte_t *lookup_address_in_pgd(pgd_t *pgd, unsigned long address, unsigned int *level);


// ioctl function to flip c bit of any address, even user-address. Remeber the driver cannnot directly access user memory, 
// access will result in crash due to SMAP etc. This  ioctl forces user pgd (cr3 va) instead of kernel pgd.
// The standard set_memory_encrypted and set_memory_decrypted functions work on kernel pgs only from init_mm
static long sevtest_ioctl(struct file *file, unsigned int cmd, unsigned long secret)
{
    int ret = 0;
    int level = -1;
    pte_t *pte = NULL;

    // ensure SEV is enabled in guest VM
    if (!mem_encrypt_active())
    {
        printk(KERN_ERR"SEVTEST: Looks like SEV is not enabled!!\n");
        return -ENXIO;
    }

    DBG_PRINT(KERN_ERR"SEVTEST:secret address is: %lx", secret);

    // find the PTE for the address supplied
    pte = lookup_address_in_pgd(pgd_offset(current->mm,secret), secret, &level);
    if (!pte)
    {
        printk(KERN_ERR"SEVTEST: PT could not be found");
        return -ENXIO;
    }
    DBG_PRINT(KERN_ERR"SEVTEST: pte level is: %d", level);
    DBG_PRINT(KERN_ERR"SEVTEST: pte_val = 0x%lx\n", pte_val(*pte));
    if (!(pte_val(*pte) & 0x1))
    {
        printk(KERN_ERR"SEVTEST: not mapped in pte\n");
        return -ENXIO;
    }


    switch (cmd)
    {
        case CLEAR_C_BIT:
            cpa_flush_all(true);
            pte->pte = pte->pte & (~sme_me_mask);
            cpa_flush_all(true);
            DBG_PRINT(KERN_ERR"SEVTEST: updated pte_val = 0x%lx\n", pte_val(*pte));
            printk(KERN_ERR"Done setting page as decrypted at address: %lx", secret);
            ret = 0;
            break;
        case SET_C_BIT:
            cpa_flush_all(true);
            pte->pte = pte->pte | sme_me_mask;
            cpa_flush_all(true);
            DBG_PRINT(KERN_ERR"SEVTEST: updated pte_val = 0x%lx\n", pte_val(*pte));
            printk(KERN_ERR"Done setting page as encryptedd at address: %lx", secret);
            ret = 0;
            break;
    default:
        return -EINVAL;
    }
    return ret;
}


static struct file_operations sevtest_ops = {
    .owner   = THIS_MODULE,
    .unlocked_ioctl = sevtest_ioctl,
};
 
static void sevtest_cleanup(void)
{
    if (sevtest_class) {
        device_destroy(sevtest_class, MKDEV(sevtest_major, 0));
        class_destroy(sevtest_class);
    }
    __unregister_chrdev(sevtest_major, 0, 1, "sevtest");
}
 
static int sevtest_init(void)
{
    int err = 0;
 
    sevtest_major = __register_chrdev(sevtest_major, 0, 1,
                "sevtest", &sevtest_ops);
    if (sevtest_major < 0) {
                pr_err("unable to get major %d for msr\n", sevtest_major);
                err = sevtest_major;
        goto errout;
        }
        sevtest_class = class_create(THIS_MODULE, "sevtest");
        if (IS_ERR(sevtest_class)) {
                err = PTR_ERR(sevtest_class);
                goto errout;
        }
 
    device_create(sevtest_class, NULL, MKDEV(sevtest_major, 0), NULL, "sevtest");

    printk("SEVTESTt driver initialized\n");
 
    return 0;
 
errout:
    sevtest_cleanup();
        return err;
}
    
static void sevtest_exit(void)
{
    sevtest_cleanup();
}


module_init(sevtest_init);
module_exit(sevtest_exit);

static void __cpa_flush_all(void *arg)
{
	unsigned long cache = (unsigned long)arg;

	/*
	 * Flush all to work around Errata in early athlons regarding
	 * large page flushing.
	 */
	__flush_tlb_all();

	if (cache && boot_cpu_data.x86 >= 4)
		wbinvd();
}

static void cpa_flush_all(unsigned long cache)
{
	//BUG_ON(irqs_disabled() && !early_boot_irqs_disabled);
	on_each_cpu(__cpa_flush_all, (void *) cache, 1);
}

pte_t *lookup_address_in_pgd(pgd_t *pgd, unsigned long address,
			     unsigned int *level)
{
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;

	*level = PG_LEVEL_NONE;

	if (pgd_none(*pgd))
		return NULL;

	p4d = p4d_offset(pgd, address);
	if (p4d_none(*p4d))
		return NULL;

	*level = PG_LEVEL_512G;
	if (p4d_large(*p4d) || !p4d_present(*p4d))
		return (pte_t *)p4d;

	pud = pud_offset(p4d, address);
	if (pud_none(*pud))
		return NULL;

	*level = PG_LEVEL_1G;
	if (pud_large(*pud) || !pud_present(*pud))
		return (pte_t *)pud;

	pmd = pmd_offset(pud, address);
	if (pmd_none(*pmd))
		return NULL;

	*level = PG_LEVEL_2M;
	if (pmd_large(*pmd) || !pmd_present(*pmd))
		return (pte_t *)pmd;

	*level = PG_LEVEL_4K;

	return pte_offset_kernel(pmd, address);
}

 
MODULE_LICENSE("GPL");
