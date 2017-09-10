/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_G3_CCR_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_G3_CCR_DEFS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_G3_CCR_DEFS


/* 
  PHY indirect control access, bit definitions
  ---------------------------------------------
  Control address, offset = 0x1D4:
  Bits[15:0] = write data
  Bit[16] = Capture address
  Bit[17] = capture data
  Bit[18] = CR read
  Bit[19] = CR write

  Status address, offset = 0x1D8
  Bits[15:0] = read data
  Bit[16] = ack.
*/
 
static __INLINE void write_phy_reg(uint32 reg, uint16 val, struct g3_pas_map *pas)
{
    uint32 phy_status, ct = 0;
    /* 1.Set address bus */
    /* 2.Assert capture address bit */
    writel((reg | (1<<16)), &pas->HTEXT.phy_ctl);

    /* 3.Wait (by polling appropriate bit in status register) for CR_ack assertion */
    do {
      phy_status = readl(&pas->HTEXT.phy_data); 
      ct += 1;
    } while (!(phy_status & (1<<16)) && (ct < 300));
    if (ct >= 300)
      printk("Exceeded ct on do-while-w1 (phy_status = 0x%08x)\n", phy_status);
    ct = 0;

    /* 4.De-assert capture address bit */
    writel(0x00000000, &pas->HTEXT.phy_ctl);

    /* 5.Wait for CR_ack de-assertion */
    do {
      phy_status = readl(&pas->HTEXT.phy_data);
      ct += 1;
    } while ((phy_status & (1<<16)) && (ct < 300));
    if (ct >= 300)
      printk("Exceeded ct on do-while-w2 (phy_status = 0x%08x)\n", phy_status);
    ct = 0;

    /* 6.Set write data */
    /* 7.Assert capture data bit (while keeping the write data) */
    writel(((1<<17)|val),&pas->HTEXT.phy_ctl); 
    
    /* 8.Wait for CR_ack assertion */
    do {
      phy_status = readl(&pas->HTEXT.phy_data);
      ct += 1;
    } while (!(phy_status & (1<<16)) && (ct < 300));
    if (ct >= 300)
      printk("Exceeded ct on do-while-w3 (phy_status = 0x%08x)\n", phy_status);
    ct = 0;

    /* 9.De-assert capture data bit */
    writel(0X00000000,&pas->HTEXT.phy_ctl);   
    /* 10.Wait for CR_ack de-assertion */
    do {
      phy_status = readl(&pas->HTEXT.phy_data);
      ct += 1;
    } while ((phy_status & (1<<16)) && (ct < 300));
    if (ct >= 300)
      printk("Exceeded ct on do-while-w4 (phy_status = 0x%08x)\n", phy_status);
    ct = 0;

    /* 11.Set CR write bit */
    writel((1<<19),&pas->HTEXT.phy_ctl); 
    /* 12.Wait for CR ack assertion */
    do {
      phy_status = readl(&pas->HTEXT.phy_data);
      ct += 1;
    } while (!(phy_status & (1<<16)) && (ct < 300));
    if (ct >= 300)
      printk("Exceeded ct on do-while-w5 (phy_status = 0x%08x)\n", phy_status);
    ct = 0;

    /* 13.De-assert CR write bit */
    writel(0x00000000,&pas->HTEXT.phy_ctl);
    /* 14.Wait for CR ack de-assertion */
    do {
      phy_status = readl(&pas->HTEXT.phy_data);
      ct += 1;
    } while ((phy_status & (1<<16)) && (ct < 300));
    if (ct >= 300)
      printk("Exceeded ct on do-while-w6 (phy_status = 0x%08x)\n", phy_status);
    ct = 0;
}


static __INLINE uint32 read_phy_reg(uint32 reg, struct g3_pas_map *pas) 
{
    uint32 phy_status, val, ct = 0;

    /* 1.Set address bus */
    /* 2.Assert capture address bit */
    writel((reg | (1<<16)), &pas->HTEXT.phy_ctl);

    /* 3.Wait (by polling appropriate bit in status register) for CR_ack assertion */ 
    do {
      phy_status = readl(&pas->HTEXT.phy_data); 
      ct += 1;
    } while (!(phy_status & (1<<16)) && (ct < 300));
    if (ct >= 300)
      printk("Exceeded ct on do-while-1 (phy_status = 0x%08x)\n", phy_status);
    ct = 0;
    /* 4.De-assert capture address bit */ 
    writel(0x00000000, &pas->HTEXT.phy_ctl);

    /* 5.Wait for CR_ack de-assertion */ 
    do {
      phy_status = readl(&pas->HTEXT.phy_data);
      ct += 1;
    } while ((phy_status & (1<<16)) && (ct < 300));
    if (ct >= 300)
      printk("Exceeded ct on do-while-2 (phy_status = 0x%08x)\n", phy_status);
    ct = 0;

    /* 6.Set CR-read data bit */
    writel((1<<18),&pas->HTEXT.phy_ctl); 
    /* 7.Wait for CR ack assertion */ 
    do {
      phy_status = readl(&pas->HTEXT.phy_data); 
      ct += 1;
    } while (!(phy_status & (1<<16)) && (ct < 300));
    if (ct >= 300)
      printk("Exceeded ct on do-while-3 (phy_status = 0x%08x)\n", phy_status);
    ct = 0;
    /* 8.Read the <read data> from status bus */
    val = (readl(&pas->HTEXT.phy_data) & 0x0000ffff);

    /* 9.De-assert CR-read bit */
    writel(0x00000000, &pas->HTEXT.phy_ctl);
    /* 10.Wait for CR_ack de-assertion */
    do {
      phy_status = readl(&pas->HTEXT.phy_data);
      ct += 1;
    } while ((phy_status & (1<<16)) && (ct < 300));
    if (ct >= 300)
      printk("Exceeded ct on do-while-4 (phy_status = 0x%08x)\n", phy_status);
    return val;
}


static __INLINE  void
__mtlk_g3_release_ctl_from_reset_phase1(mtlk_hw_bus_t *bus, 
                                       struct g3_pas_map *pas)
{
  MTLK_ASSERT(NULL != bus);
  MTLK_ASSERT(NULL != pas);

  /* See WLSG3-37 for detailed info */
  __ccr_writel(bus, 0x03333335,
               &pas->UPPER_SYS_IF.m4k_rams_rm);
  __ccr_writel(bus, 0x00000555,
               &pas->UPPER_SYS_IF.iram_rm);
  __ccr_writel(bus, 0x00555555,
               &pas->UPPER_SYS_IF.eram_rm);

  __ccr_writel(bus, 0x03333335,
               &pas->LOWER_SYS_IF.m4k_rams_rm);
  __ccr_writel(bus, 0x00000555,
               &pas->LOWER_SYS_IF.iram_rm);
  __ccr_writel(bus, 0x00555555,
               &pas->LOWER_SYS_IF.eram_rm);
}

static __INLINE  void
__mtlk_g3_release_ctl_from_reset_phase2(mtlk_hw_bus_t *bus, 
                                       struct g3_pas_map *pas)
{
  MTLK_ASSERT(NULL != bus);
  MTLK_ASSERT(NULL != pas);

  /* See WLSG3-37 for detailed info */
  __ccr_writel(bus, 0x00005555,
               &pas->HTEXT.shram_rm);

  __ccr_writel(bus, 0x33533353,
               &pas->TD.phy_rxtd_reg175);
}


static __INLINE  void
_mtlk_g3_put_ctl_to_reset(mtlk_hw_bus_t *bus, 
                          struct g3_pas_map *pas)
{
  MTLK_ASSERT(NULL != bus);
  MTLK_ASSERT(NULL != pas);

  /* Disable RX */
  __ccr_resetl(bus, &pas->PAC.rx_control, G3_MASK_RX_ENABLED);
}
