/* C startup file, called from startup0.s-- */

extern void clr_bss(void);
extern void init_devio(void);
extern void tiny_kernel_init(void);

void startupc1(void);

void startupc1(void)
{
  clr_bss();
  init_devio();
  (void)tiny_kernel_init();
}
