/*
 *  MMIO IQ Controller
 */

#include "qemu/osdep.h"
#include "hw/qdev-properties.h"
#include "qemu/error-report.h"

#include "hw/sysbus.h"
#include "hw/irq.h"
#include "qapi/error.h"
#include <regex.h>

#define IRQ_MEM_SIZE 0x100

#if defined(TARGET_ARM) || defined(TARGET_AARCH64)
#include "target/arm/cpu.h"
#include "hw/avatar/halucinator_irq_memory.h"
#elif TARGET_MIPS
#endif


#define TYPE_HALUCINATOR_IRQ "halucinator-irq"
#define HALUCINATOR_IRQ(obj) OBJECT_CHECK(HalucinatorIRQState, (obj), TYPE_HALUCINATOR_IRQ)


static void hexstr_to_buffer (const char *str, int n,unsigned char *buff);
void qmp_pmemwrite( int64_t pmem_addr,const char *data_buff, Error **errp);

static void update_irq(HalucinatorIRQState * s){
    int i;
    unsigned char val_set = 0;
    for (i=0; i < s->size ; i++){
        val_set |= s->memory[i];
        if (s->memory[i] != 0){
            printf("IRQ Active %i: %i\n", i, s->memory[i]);
        }
    }

    CPUState * cs = qemu_get_cpu(0);
    ARMCPU * cpu = ARM_CPU(cs);
    //DeviceState *dev = get_device_type(dev_name);
    //if (dev){
    printf("Updating IRQ State %i\n", val_set);
    qemu_irq irq = qdev_get_gpio_in(DEVICE(cpu), 0);
    if(irq){
  
        if (val_set){
            printf("Setting Irq\n");
            qemu_set_irq(irq, 1);
        }else{
            printf("Clearing Irq\n");
            qemu_set_irq(irq, 0);
        }
    }
}


static uint64_t halucinator_irqc_read(void *opaque, hwaddr offset,
                           unsigned size)
{
    uint64_t ret;
    int i;

    printf("Reading Halucinator-IRQ\n 0x%lx: size 0x%x\n", offset, size);
    HalucinatorIRQState *s = (HalucinatorIRQState *) opaque;
    ret = 0;
    for (i=0; i < size; ++i){
        ret |= (0xFF & s->memory[offset + i]) << (8 * i);
    }
    printf("Returning 0x%08lx\n", ret);
    update_irq(s);
    return ret;
}


static void halucinator_irqc_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
    int i;
    printf("Writing Halucinator-IRQ 0x%lx: value 0x%08lx size 0x%x\n", offset, value, size);
    HalucinatorIRQState *s = (HalucinatorIRQState *) opaque;
    for (i=0; i < size; ++i){
        s->memory[offset + i] = (0xFF & (value >> (8 * i)));
    }
    update_irq(s);
}


static int match(const char *string)
{   const char* pattern = "^[a-fA-F0-9]+$";
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0) return 0;
    int status = regexec(&re, string, 0, NULL, 0);
    regfree(&re);
    if (status != 0) return 0;
    return 1;
}

void qmp_pmemwrite( int64_t pmem_addr,const char *data_buff, Error **errp)
{   //printf("Here now\n");
    int len = (strlen (data_buff) / 2);
    unsigned char buff[len];
    
    memset( buff, 0, len*sizeof(unsigned char) );
    
    if (strlen (data_buff) % 2 != 0){
        error_setg(errp, "data_buff is not of the correct hexcode format must be an even number of character");
        return;
    }
    if(match(data_buff) == 0){
        error_setg(errp, "data_buff is not of the correct hexcode format");
        return;
    }
    
    hexstr_to_buffer(data_buff, len, buff);
    cpu_physical_memory_write(pmem_addr,&buff, sizeof(buff));

    return;
}

static void hexstr_to_buffer (const char *str, int n,unsigned char *buff)
{
  int str_size = strlen (str);
  int i;
  int part_size;

  part_size = str_size / n;
  int j = 0;
  int k = 0;
  char myString[2];
  for (i = 0; i < str_size; i++)
    {
        if (i % part_size == 0 && i !=0){
          sscanf(myString, "%02hhx", &buff[j]);
          j++;
          k = 0;
        }
        myString[k] = str[i];
        k++;
        if (i == str_size - 1){
            sscanf(myString, "%02hhx", &buff[j]);
        }
    }
    return;
}


static const MemoryRegionOps halucinator_irq_ops = {
    .read = halucinator_irqc_read,
    .write = halucinator_irqc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static Property halucinator_irq_properties[] = {
    DEFINE_PROP_UINT64("address", HalucinatorIRQState, address, 0x101f1000),
    DEFINE_PROP_UINT32("size", HalucinatorIRQState, size, IRQ_MEM_SIZE),
    DEFINE_PROP_END_OF_LIST(),
};



static void halucinator_irq_realize(DeviceState *dev, Error **errp)
{
    printf("Realizing Halucinator-IRQ\n");
    HalucinatorIRQState *s = HALUCINATOR_IRQ(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(s);
    memory_region_init_io(&s->iomem, OBJECT(s), &halucinator_irq_ops, s, "halucinator-irq", s->size);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);

    s->memory = g_new(unsigned char, s->size);
    bzero(s->memory, s->size);

}

static void halucinator_irq_class_init(ObjectClass *oc, void *data)
{
    printf("Initializing Halucinator-IRQ 2\n");
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = halucinator_irq_realize;
    dc->props = halucinator_irq_properties;
}

static const TypeInfo halucinator_irq_arm_info = {
    .name          = TYPE_HALUCINATOR_IRQ,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HalucinatorIRQState),
    .class_init    = halucinator_irq_class_init
};

static void halucinator_irq_register_types(void)
{
    printf("Halucinator-IRQ: Register types\n");
    type_register_static(&halucinator_irq_arm_info);
}

type_init(halucinator_irq_register_types)
