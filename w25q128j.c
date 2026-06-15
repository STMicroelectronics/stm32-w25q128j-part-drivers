/**
  ******************************************************************************
  * @file    w25q128j.c
  * @brief   This file provides the w25q128j SPI drivers.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "w25q128j.h"

/* Private Functions --------------------------------------------------------------- */
static w25q128j_status_t w25q128j_select(w25q128j_obj_t *pobj);
static w25q128j_status_t w25q128j_deselect(w25q128j_obj_t *pobj);
static w25q128j_status_t w25q128j_send_cmd(w25q128j_obj_t *pobj, uint8_t *p_data, uint32_t size);
static w25q128j_status_t w25q128j_receive_cmd(w25q128j_obj_t *pobj, uint8_t *p_cmd, uint8_t cmd_len_byte,
                                              uint8_t *p_data, uint32_t data_len_byte);
/* protection register set get functions */
static w25q128j_status_t w25q128j_set_status_reg_protection(w25q128j_obj_t *pobj);
static w25q128j_status_t w25q128j_check_status_reg_protection(w25q128j_obj_t *pobj);
#if defined (USE_HAL_SPI_DMA) && (USE_HAL_SPI_DMA == 1)
#if defined (W25Q128J_CALLBACKS) && (W25Q128J_CALLBACKS == 1)
static void w25q128j_async_wait_ready(w25q128j_obj_t *pobj);
/* Async write cmd functions */
static w25q128j_status_t w25q128j_write_it_cmd(w25q128j_obj_t *pobj, uint8_t *pdata, uint16_t len_byte);
static w25q128j_status_t w25q128j_write_dma_cmd(w25q128j_obj_t *pobj, const uint8_t *pdata, uint16_t len_byte);
static w25q128j_status_t w25q128j_write_enable_async(w25q128j_obj_t *pobj);
/* Async write internal callbacks */
static void spi_we_cplt_cb(hal_spi_handle_t *phspi);
static void spi_write_cmd_cplt_cb(hal_spi_handle_t *phspi);
static void spi_tx_head_cplt_cb(hal_spi_handle_t *phspi);
static void spi_tx_body_cplt_cb(hal_spi_handle_t *phspi);
static void spi_write_data_cplt_cb(hal_spi_handle_t *phspi);
/* Async read internal callbacks */
static void spi_read_cmd_cplt_cb(hal_spi_handle_t *phspi);
static void spi_rx_head_cplt_cb(hal_spi_handle_t *phspi);
static void spi_rx_body_cplt_cb(hal_spi_handle_t *phspi);
static void spi_rd_cplt_cb(hal_spi_handle_t *phspi);
/* Alignment management */
static void split_buffer(uint8_t *in_buf, uint16_t in_len, uint16_t *head_len,
                         uint8_t **body_ptr, uint16_t *body_len,
                         uint8_t **tail_ptr, uint16_t *tail_len);
static void w25q128j_write_nextpage(w25q128j_obj_t *pobj);
#endif /* W25Q128J_CALLBACKS */
#endif /* USE_HAL_SPI_DMA */


/**
  * @brief initialize the IO layer. This specific implementation does nothing.
  * @retval 0 (always succeeds)
  * @note This function must be overridden (using codegen or otherwise) with a proper implementation).
  */
__weak int32_t w25q128j_io_init(w25q128j_io_t *pio)
{
  (void)(pio);
  return 0;
}

/**
  * @brief  Initializes the memory.
  * @param  pobj    : W25Q128J part object pointer
  * @param  id      : Configuration ID
  * @retval error status
  */
w25q128j_status_t w25q128j_init(w25q128j_obj_t *pobj, uint32_t id)
{
  w25q128j_status_t ret = W25Q128J_OK;

  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  /* Check if the instance is already initialized */
  if (pobj->is_initialized == W25Q128J_INIT_NO)
  {
    pobj->pio.id = id;

    if (w25q128j_io_init(&pobj->pio) != 0)
    {
      ret = W25Q128J_ERROR;
    }
    else if (w25q128j_reset(pobj) != W25Q128J_OK)
    {
      ret = W25Q128J_ERROR;
    }
    else
    {
      pobj->is_initialized = W25Q128J_INIT_OK;
    }
  }

  return ret;
}

/**
  * @brief  De-Initializes the memory.
  * @param  pobj : w25q128j part object pointer
  * @retval error status
  */
w25q128j_status_t w25q128j_deinit(w25q128j_obj_t *pobj)
{
  w25q128j_status_t ret = W25Q128J_OK;

  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  /* Check if the instance is already deinitialized */
  if (pobj->is_initialized != W25Q128J_INIT_NO)
  {
    if (w25q128j_write_disable(pobj) != W25Q128J_OK)
    {
      ret = W25Q128J_ERROR;
    }
    else
    {
      pobj->is_initialized = W25Q128J_INIT_NO;
    }
  }

  return ret;
}

/**
  * @brief  Gets Flash information.
  * @param  pobj  : w25q128j part object pointer
  * @param  p_info : Pointer to information structure.
  * @retval error status.
  */
w25q128j_status_t w25q128j_get_info(const w25q128j_obj_t *pobj, w25q128j_info_t *p_info)
{
  /* Unused parameter */
  STM32_UNUSED(pobj);

  /* Configure the structure with the memory configuration */
  p_info->flash_size                 = W25Q128J_FLASH_SIZE;
  p_info->erase_block_64k_size       = W25Q128J_BLOCK_SIZE;
  p_info->erase_block_64k_number     = (W25Q128J_FLASH_SIZE / W25Q128J_BLOCK_SIZE);
  p_info->erase_block_32k_size       = W25Q128J_SUBBLOCK_SIZE;
  p_info->erase_block_32k_number     = (W25Q128J_FLASH_SIZE / W25Q128J_SUBBLOCK_SIZE);
  p_info->erase_sector_size          = W25Q128J_SECTOR_SIZE;
  p_info->erase_sector_number        = (W25Q128J_FLASH_SIZE / W25Q128J_SECTOR_SIZE);
  p_info->prog_page_size             = W25Q128J_PAGE_SIZE;
  p_info->prog_pages_number          = (W25Q128J_FLASH_SIZE / W25Q128J_PAGE_SIZE);

  return W25Q128J_OK;
}

/**
  * @brief  Reads current status of the flash memory.
  * @param  pobj    : w25q128j part object pointer
  * @param p_status : Pointer to store current memory status
  * @retval error status.
  */
w25q128j_status_t w25q128j_get_status(w25q128j_obj_t *pobj, w25q128j_status_t *p_status)
{
  w25q128j_status_t ret = W25Q128J_OK;
  uint8_t stat_reg1;
  uint8_t stat_reg2;

  /* Check param */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  if (w25q128j_read_status_reg1(pobj, &stat_reg1) != W25Q128J_OK)
  {
    ret = W25Q128J_ERROR;
  }
  else if (w25q128j_read_status_reg2(pobj, &stat_reg2) != W25Q128J_OK)
  {
    ret = W25Q128J_ERROR;
  }
  else
  {
    if ((stat_reg1 & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY)
    {
      *p_status = W25Q128J_BUSY;
    }
    else if ((stat_reg2 & W25Q128J_SR2_SUS) == W25Q128J_SR2_SUS)
    {
      *p_status = W25Q128J_SUSPENDED;
    }
    else
    {
      *p_status = W25Q128J_OK;
    }
  }

  /* Return status */
  return ret;
}

/**
  * @brief  Reads the w25q128j flash memory Manufacturer ID.
  * @param  pobj       : W25Q128J part object pointer
  * @param  p_id       : Pointer to store the part Manufacturer ID
  * @retval error status
  */
w25q128j_status_t w25q128j_read_id(w25q128j_obj_t *pobj, uint8_t *p_id)
{
  w25q128j_status_t ret = W25Q128J_OK;
  /* Check parameters */
  if ((pobj == NULL) || (p_id == NULL))
  {
    return W25Q128J_ERROR;
  }
  /* Initialize the read ID command */
  uint8_t cmd[W25Q128J_READ_ID_CMD_SIZE] = {W25Q128J_READ_ID_CMD, W25Q128J_DUMMY_BYTE,
                                            W25Q128J_DUMMY_BYTE, W25Q128J_DUMMY_BYTE
                                           };

  /* Send the read ID command via SPI */
  if ((w25q128j_receive_cmd(pobj, cmd, W25Q128J_READ_ID_CMD_SIZE, p_id, W25Q128J_ID_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Reads the w25q128j flash memory JEDEC ID.
  * @param  pobj              : W25Q128J part object pointer
  * @param  p_jedec_id        : Pointer to store the part JEDEC ID
  * @retval error status
  */
w25q128j_status_t w25q128j_read_jedec_id(w25q128j_obj_t *pobj, uint8_t *p_jedec_id)
{
  w25q128j_status_t ret = W25Q128J_OK;
  /* Check parameters */
  if ((pobj == NULL) || (p_jedec_id == NULL))
  {
    return W25Q128J_ERROR;
  }
  /* Initialize the read JEDEC ID command */
  uint8_t cmd = W25Q128J_READ_JEDEC_ID_CMD;

  /* Send the read JEDEC ID command via SPI */
  if ((w25q128j_receive_cmd(pobj, &cmd, W25Q128J_READ_JDEC_ID_CMD_SIZE, p_jedec_id,
                            W25Q128J_JDEC_ID_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Reads the w25q128j flash memory unique ID.
  *         The unique ID is a factory-set read-only 64-bit number that is unique to each W25Q128J device.
  * @param  pobj       : W25Q128J part object pointer
  * @param  p_id       : Pointer to store the part unique ID
  * @retval error status
  */
w25q128j_status_t w25q128j_read_unique_id(w25q128j_obj_t *pobj, uint8_t *p_id)
{
  w25q128j_status_t ret = W25Q128J_OK;
  /* Check parameters */
  if ((pobj == NULL) || (p_id == NULL))
  {
    return W25Q128J_ERROR;
  }
  /* Initialize the read unique ID command.
     The command is followed by 4 dummy bytes before the 64-bit unique ID payload. */
  uint8_t cmd[W25Q128J_UNIQUE_ID_CMD_SIZE] = {W25Q128J_READ_UNIQUE_ID_CMD, W25Q128J_DUMMY_BYTE,
                                              W25Q128J_DUMMY_BYTE, W25Q128J_DUMMY_BYTE,
                                              W25Q128J_DUMMY_BYTE
                                             };

  /* Send the read unique ID command via SPI */
  if ((w25q128j_receive_cmd(pobj, cmd, W25Q128J_UNIQUE_ID_CMD_SIZE, p_id, W25Q128J_UNIQUE_ID_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Sets the status-register based block protection in the memory.
  * @param  pobj   : Part object pointer.
  * @param  bp_cfg : Block protection configuration from the datasheet WPS=0 table.
  * @retval error status.
  */
w25q128j_status_t w25q128j_set_block_protection(w25q128j_obj_t *pobj, w25q128j_bp_cfg_t bp_cfg)
{
  uint8_t reg = 0U;

  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  /* Force status-register based protection mode */
  if (w25q128j_set_status_reg_protection(pobj) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Read status register 1 */
  if (w25q128j_read_status_reg1(pobj, &reg) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Keep unrelated SR1 bits unchanged and update only SEC + BP[2:0] */
  reg = (reg & (uint8_t)(~W25Q128J_SR1_PROTECTION_CFG_MASK))
        | ((uint8_t)bp_cfg & W25Q128J_SR1_PROTECTION_CFG_MASK);

  if (w25q128j_write_status_reg1(pobj, reg) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  return W25Q128J_OK;
}

/**
  * @brief  Gets the status-register based block protection from the memory.
  * @param  pobj     : Part object pointer.
  * @param  p_bp_cfg : Pointer to store the block protection configuration.
  * @retval error status.
  */
w25q128j_status_t w25q128j_get_block_protection(w25q128j_obj_t *pobj, w25q128j_bp_cfg_t *p_bp_cfg)
{
  uint8_t reg = 0U;

  if ((pobj == NULL) || (p_bp_cfg == NULL))
  {
    return W25Q128J_ERROR;
  }

  /* Check status-register based protection mode */
  if (w25q128j_check_status_reg_protection(pobj) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Read status register 1 */
  if (w25q128j_read_status_reg1(pobj, &reg) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Keep only SEC + BP[2:0] */
  reg &= W25Q128J_SR1_PROTECTION_CFG_MASK;

  /* Full array protection */
  if ((reg & W25Q128J_SR1_BP_BITS_MASK) >= (uint8_t)W25Q128J_BP_ALL)
  {
    *p_bp_cfg = W25Q128J_BP_ALL;
  }
  else
  {
    /* Map SEC + BP[2:0] to the driver enum */
    switch (reg)
    {
      case (uint8_t)W25Q128J_BP_NONE:
        *p_bp_cfg = W25Q128J_BP_NONE;
        break;
      case (uint8_t)W25Q128J_BP_4KB:
        *p_bp_cfg = W25Q128J_BP_4KB;
        break;
      case (uint8_t)W25Q128J_BP_8KB:
        *p_bp_cfg = W25Q128J_BP_8KB;
        break;
      case (uint8_t)W25Q128J_BP_16KB:
        *p_bp_cfg = W25Q128J_BP_16KB;
        break;
      case (uint8_t)W25Q128J_BP_32KB:
        *p_bp_cfg = W25Q128J_BP_32KB;
        break;
      case (uint8_t)W25Q128J_BP_256KB:
        *p_bp_cfg = W25Q128J_BP_256KB;
        break;
      case (uint8_t)W25Q128J_BP_512KB:
        *p_bp_cfg = W25Q128J_BP_512KB;
        break;
      case (uint8_t)W25Q128J_BP_1MB:
        *p_bp_cfg = W25Q128J_BP_1MB;
        break;
      case (uint8_t)W25Q128J_BP_2MB:
        *p_bp_cfg = W25Q128J_BP_2MB;
        break;
      case (uint8_t)W25Q128J_BP_4MB:
        *p_bp_cfg = W25Q128J_BP_4MB;
        break;
      case (uint8_t)W25Q128J_BP_8MB:
        *p_bp_cfg = W25Q128J_BP_8MB;
        break;
      default:
        return W25Q128J_ERROR;
        break;
    }
  }

  return W25Q128J_OK;
}

/**
  * @brief  Sets the top/bottom protection direction in the memory.
  * @param  pobj   : Part object pointer.
  * @param  tb_cfg : Top/bottom configuration to set.
  * @retval error status.
  */
w25q128j_status_t w25q128j_set_top_bottom_cfg(w25q128j_obj_t *pobj, w25q128j_bp_tb_cfg_t tb_cfg)
{
  uint8_t reg = 0U;

  if ((pobj == NULL) || ((tb_cfg != W25Q128J_BP_TB_TOP) && (tb_cfg != W25Q128J_BP_TB_BOTTOM)))
  {
    return W25Q128J_ERROR;
  }

  /* Force status-register based protection mode */
  if (w25q128j_set_status_reg_protection(pobj) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Read status register 1 */
  if (w25q128j_read_status_reg1(pobj, &reg) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Clear TB bit to default for top protection */
  reg &= (uint8_t)(~W25Q128J_SR1_TB);

  /* Set TB bit for bottom protection */
  if (tb_cfg == W25Q128J_BP_TB_BOTTOM)
  {
    reg |= W25Q128J_SR1_TB;
  }

  if (w25q128j_write_status_reg1(pobj, reg) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  return W25Q128J_OK;
}

/**
  * @brief  Reads the top/bottom protection direction from the memory.
  * @param  pobj     : Part object pointer.
  * @param  p_tb_cfg : Pointer to store the top/bottom configuration.
  * @retval error status.
  */
w25q128j_status_t w25q128j_get_top_bottom_cfg(w25q128j_obj_t *pobj, w25q128j_bp_tb_cfg_t *p_tb_cfg)
{
  uint8_t reg = 0U;

  if ((pobj == NULL) || (p_tb_cfg == NULL))
  {
    return W25Q128J_ERROR;
  }

  /* Check status-register based protection mode */
  if (w25q128j_check_status_reg_protection(pobj) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Read status register 1 */
  if (w25q128j_read_status_reg1(pobj, &reg) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Get TB direction */
  *p_tb_cfg = (((reg & W25Q128J_SR1_TB) == W25Q128J_SR1_TB) ? W25Q128J_BP_TB_BOTTOM : W25Q128J_BP_TB_TOP);

  return W25Q128J_OK;
}

/**
  * @brief  Force the device into status-register based protection mode.
  * @param  pobj : Part object pointer.
  * @retval error status.
  */
static w25q128j_status_t w25q128j_set_status_reg_protection(w25q128j_obj_t *pobj)
{
  uint8_t reg2 = 0U;
  uint8_t reg3 = 0U;

  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  /* Read status registers 2 and 3 */
  if (w25q128j_read_status_reg2(pobj, &reg2) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  if (w25q128j_read_status_reg3(pobj, &reg3) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Clear CMP bit */
  if ((reg2 & W25Q128J_SR2_CMP) == W25Q128J_SR2_CMP)
  {
    reg2 &= (uint8_t)(~W25Q128J_SR2_CMP);
    if (w25q128j_write_status_reg2(pobj, reg2) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  }

  /* Clear WPS bit */
  if ((reg3 & W25Q128J_SR3_WPS) == W25Q128J_SR3_WPS)
  {
    reg3 &= (uint8_t)(~W25Q128J_SR3_WPS);
    if (w25q128j_write_status_reg3(pobj, reg3) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  }

  return W25Q128J_OK;
}

/**
  * @brief  Check that the device is still in status-register based protection mode.
  * @param  pobj : Part object pointer.
  * @retval error status.
  */
static w25q128j_status_t w25q128j_check_status_reg_protection(w25q128j_obj_t *pobj)
{
  uint8_t reg2 = 0U;
  uint8_t reg3 = 0U;

  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  /* Read status registers 2 and 3 */
  if (w25q128j_read_status_reg2(pobj, &reg2) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  if (w25q128j_read_status_reg3(pobj, &reg3) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Check CMP and WPS bits */
  if (((reg2 & W25Q128J_SR2_CMP) == W25Q128J_SR2_CMP) || ((reg3 & W25Q128J_SR3_WPS) == W25Q128J_SR3_WPS))
  {
    return W25Q128J_ERROR;
  }

  return W25Q128J_OK;
}

/**
  * @brief  Waits until the flash reaches the requested state.
  * @param  pobj           : W25Q128J part object pointer
  * @param  wait_condition : Target state to wait for.
  *                        - W25Q128J_STATE_WAIT_UNTIL_IDLE waits for the Both BUSY and Suspend bits to clear
  *                          both read and write operations are available in this state.
  *                        - W25Q128J_STATE_WAIT_UNTIL_READABLE waits only for the BUSY bit to clear
  *                          if the memory is in suspended state only read operations are available.
  *
  * @retval error status
  */
w25q128j_status_t w25q128j_wait_for_state(w25q128j_obj_t *pobj, w25q128j_wait_condition_t wait_condition)
{
  uint32_t tstart = HAL_GetTick();
  w25q128j_status_t mem_state = W25Q128J_ERROR;
  do
  {
    if (w25q128j_get_status(pobj, &mem_state) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while (((HAL_GetTick() - tstart) < W25Q128J_WRITE_PAGE_MAX_WAIT)
           && ((wait_condition == W25Q128J_STATE_WAIT_UNTIL_IDLE) ?
               ((mem_state == W25Q128J_OK) ? 0U : 1U) : ((mem_state == W25Q128J_BUSY) ? 1U : 0U)));
  if ((wait_condition == W25Q128J_STATE_WAIT_UNTIL_IDLE) && (mem_state == W25Q128J_OK))
  {
    /* Memory is ready */
    return W25Q128J_OK;
  }
  if ((wait_condition == W25Q128J_STATE_WAIT_UNTIL_READABLE)
      && ((mem_state == W25Q128J_OK) || (mem_state == W25Q128J_SUSPENDED)))
  {
    /* Memory is readable */
    return W25Q128J_OK;
  }

  /* timeout */
  return W25Q128J_ERROR;
}

/**
  * @brief  Resets the flash memory.
  * @param  pobj    : W25Q128J part Object Pointer
  * @retval error status
  */
w25q128j_status_t w25q128j_reset(w25q128j_obj_t *pobj)
{
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  /* Initialize the enable reset command */
  uint8_t tData = W25Q128J_RESET_ENABLE_CMD;
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Send the reset enable command via SPI */
  if ((w25q128j_send_cmd(pobj, &tData, W25Q128J_RESET_ENABLE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send command */
    return W25Q128J_ERROR;
  }

  /* Initialize the reset command */
  tData = W25Q128J_RESET_MEMORY_CMD;

  /* Send the reset command via SPI */
  if ((w25q128j_send_cmd(pobj, &tData, W25Q128J_RESET_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send command */
    return W25Q128J_ERROR;
  }

  /* Return PART status */
  return W25Q128J_OK;
}

/**
  * @brief  Sets the Write Enable Latch (WEL) bit in the Status Register to a 1.
  * @param  pobj    : W25Q128J part Object Pointer
  * @retval error status
  */
w25q128j_status_t w25q128j_write_enable(w25q128j_obj_t *pobj)
{
  w25q128j_status_t ret = W25Q128J_OK ;
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Initialize the write enable command */
  uint8_t cmd = W25Q128J_WRITE_ENABLE_CMD;

  /* Send the write enable command via SPI */
  if ((w25q128j_send_cmd(pobj, &cmd, W25Q128J_WRITE_ENABLE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Resets the Write Enable Latch (WEL) bit in the Status Register to a 0.
  * @param  pobj         : W25Q128J part Object Pointer
  * @retval error status
  */
w25q128j_status_t w25q128j_write_disable(w25q128j_obj_t *pobj)
{
  w25q128j_status_t ret = W25Q128J_OK ;
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Initialize the write disable command */
  uint8_t cmd = W25Q128J_WRITE_DISABLE_CMD;

  /* Send the write disable command via SPI */
  if ((w25q128j_send_cmd(pobj, &cmd, W25Q128J_WRITE_DISABLE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Reads the w25q128j status register1.
  * @param  pobj                  : W25Q128J part Object Pointer
  * @param  p_value               : Pointer to status register1 value
  * @retval error status
  */
w25q128j_status_t w25q128j_read_status_reg1(w25q128j_obj_t *pobj, uint8_t *p_value)
{
  w25q128j_status_t ret =  W25Q128J_OK;
  /* Check parameters */
  if ((pobj == NULL) || (p_value == NULL))
  {
    return W25Q128J_ERROR;
  }
  /* Prepare the command sequence for reading the STATUS register */
  uint8_t cmd = W25Q128J_READ_STATUS_REG1_CMD;

  /* Send the read Status Reg command via SPI */
  if ((w25q128j_receive_cmd(pobj, &cmd, W25Q128J_READ_STATUS_REG_CMD_SIZE,
                            p_value, W25Q128J_STATUS_REG_DATA_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Reads the w25q128j status register2.
  * @param  pobj          : W25Q128J part Object Pointer
  * @param  p_value       : Pointer to status register2 value
  * @retval error status
  */
w25q128j_status_t w25q128j_read_status_reg2(w25q128j_obj_t *pobj, uint8_t *p_value)
{
  w25q128j_status_t ret =  W25Q128J_OK;
  /* Check parameters */
  if ((pobj == NULL) || (p_value == NULL))
  {
    return W25Q128J_ERROR;
  }
  /* Prepare the command sequence for reading the STATUS register */
  uint8_t cmd = W25Q128J_READ_STATUS_REG2_CMD;

  /* Send the read Status Reg command via SPI */
  if ((w25q128j_receive_cmd(pobj, &cmd, W25Q128J_READ_STATUS_REG_CMD_SIZE,
                            p_value, W25Q128J_STATUS_REG_DATA_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Reads the w25q128j status register3.
  * @param  pobj           : W25Q128J part Object Pointer
  * @param  p_value        : Pointer to status register3 value
  * @retval error status
  */
w25q128j_status_t w25q128j_read_status_reg3(w25q128j_obj_t *pobj, uint8_t *p_value)
{
  w25q128j_status_t ret =  W25Q128J_OK;
  /* Check parameters */
  if ((pobj == NULL) || (p_value == NULL))
  {
    return W25Q128J_ERROR;
  }
  /* Prepare the command sequence for reading the STATUS register */
  uint8_t cmd = W25Q128J_READ_STATUS_REG3_CMD;

  /* Send the read Status Reg command via SPI */
  if ((w25q128j_receive_cmd(pobj, &cmd, W25Q128J_READ_STATUS_REG_CMD_SIZE,
                            p_value, W25Q128J_STATUS_REG_DATA_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}


/**
  * @brief  Writes the w25q128j status register1.
  * @param  pobj        : W25Q128J part Object Pointer
  * @param  p_value     : The 8-bits data we want to write into the status register1
  * @retval error status
  */
w25q128j_status_t w25q128j_write_status_reg1(w25q128j_obj_t *pobj, uint8_t value)
{
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed to set the write enable */
    return W25Q128J_ERROR;
  }
  /* Prepare the command sequence for writing the status register */
  uint8_t cmd[2] = {W25Q128J_WRITE_STATUS_REG1_CMD, value};

  /* Send the command sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_WRITE_STATUS_REG_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    return W25Q128J_ERROR;
  }

  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Return PART status */
  return W25Q128J_OK;
}

/**
  * @brief  Writes the w25q128j status register2.
  * @param  pobj       : W25Q128J part Object Pointer
  * @param  p_value    : The 8-bits data we want to write into the status register2
  * @retval error status
  */
w25q128j_status_t w25q128j_write_status_reg2(w25q128j_obj_t *pobj, uint8_t value)
{
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed to set the write enable */
    return W25Q128J_ERROR;
  }
  /* Prepare the command sequence for writing the status register */
  uint8_t cmd[2] = {W25Q128J_WRITE_STATUS_REG2_CMD, value};

  /* Send the command sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_WRITE_STATUS_REG_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    return W25Q128J_ERROR;
  }

  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Return PART status */
  return W25Q128J_OK;
}

/**
  * @brief  Writes the w25q128j status register3.
  * @param  pobj        : W25Q128J part Object Pointer
  * @param  p_value     : The 8-bits data we want to write into the status register2
  * @retval error status
  */
w25q128j_status_t w25q128j_write_status_reg3(w25q128j_obj_t *pobj, uint8_t value)
{
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed to set the write enable */
    return W25Q128J_ERROR;
  }
  /* Prepare the command sequence for writing the status register */
  uint8_t cmd[2] = {W25Q128J_WRITE_STATUS_REG3_CMD, value};

  /* Send the command sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_WRITE_STATUS_REG_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    return W25Q128J_ERROR;
  }

  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Return PART status */
  return W25Q128J_OK;
}

/**
  * @brief  Erase a 64K block from the w25q128j NOR flash memory.
  * @param  pobj                  : W25Q128J part Object Pointer
  * @param  block_address         : Block address
  * @retval error status
  */
static w25q128j_status_t w25q128j_erase_64k_block_cmd(w25q128j_obj_t *pobj, uint32_t block_address)
{
  /* Initialize the erase block instructions sequence */
  uint8_t cmd[4] = {W25Q128J_BLOCK_ERASE_64K_CMD, (uint8_t)((block_address & W25Q128J_MEM_ADDR_MASK1) >> 16),
                    (uint8_t)((block_address & W25Q128J_MEM_ADDR_MASK2) >> 8),
                    (uint8_t)(block_address & W25Q128J_MEM_ADDR_MASK3)
                   };

  /* make sure the memory is ready */
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed set the write enable */
    return W25Q128J_ERROR;
  }

  /* Send the erase block instructions sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_BLOCK_ERASE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    return W25Q128J_ERROR;
  }

  /* make sure the memory is ready */
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Return PART status */
  return W25Q128J_OK;
}

/**
  * @brief  Erase a 32K block from the w25q128j NOR flash memory.
  * @param  pobj                  : W25Q128J part Object Pointer
  * @param  block_address         : Block address
  * @retval error status
  */
static w25q128j_status_t w25q128j_erase_32k_block_cmd(w25q128j_obj_t *pobj, uint32_t block_address)
{
  /* Initialize the erase block instructions sequence */
  uint8_t cmd[4] = {W25Q128J_BLOCK_ERASE_32K_CMD, (uint8_t)((block_address & W25Q128J_MEM_ADDR_MASK1) >> 16),
                    (uint8_t)((block_address & W25Q128J_MEM_ADDR_MASK2) >> 8),
                    (uint8_t)(block_address & W25Q128J_MEM_ADDR_MASK3)
                   };

  /* make sure the memory is ready */
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed set the write enable */
    return W25Q128J_ERROR;
  }

  /* Send the erase block instructions sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_BLOCK_ERASE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    return W25Q128J_ERROR;
  }

  /* Erase block time */
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Return PART status */
  return W25Q128J_OK;
}

/**
  * @brief  Erase a 64K block from the w25q128j NOR flash memory.
  * @param  pobj                  : W25Q128J part Object Pointer
  * @param  sector_address        : Sector address
  * @retval error status
  */
static w25q128j_status_t w25q128j_erase_sector_cmd(w25q128j_obj_t *pobj, uint32_t sector_address)
{
  /* Initialize the erase block instructions sequence */
  uint8_t cmd[4] = {W25Q128J_SECTOR_ERASE_CMD, (uint8_t)((sector_address & W25Q128J_MEM_ADDR_MASK1) >> 16),
                    (uint8_t)((sector_address & W25Q128J_MEM_ADDR_MASK2) >> 8),
                    (uint8_t)(sector_address & W25Q128J_MEM_ADDR_MASK3)
                   };

  /* make sure the memory is ready */
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed set the write enable */
    return W25Q128J_ERROR;
  }

  /* Send the erase block instructions sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_BLOCK_ERASE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    return W25Q128J_ERROR;
  }

  /* Erase block time */
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Return PART status */
  return W25Q128J_OK;
}

/**
  * @brief  Erases the specified block of flash memory.
  *         W25Q128J supports 4K sector or 32K block or 64K erase commands.
  * @param  pobj       : Part object pointer.
  * @param  addr       : Sector/block Address to erase
  * @param  size       : size configuration W25Q128J_ERASE_SECTOR (4K) or W25Q128J_ERASE_32K_BLOCK (32K)
  *                      or  W25Q128J_ERASE_64K_BLOCK (64K)
  * @retval error status.
  */
w25q128j_status_t w25q128j_erase(w25q128j_obj_t *pobj, uint32_t addr, w25q128j_erase_t size)
{
  /* Check param */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  /* Send the Sector/Block erase command */
  if (size == W25Q128J_ERASE_SECTOR)
  {
    if (w25q128j_erase_sector_cmd(pobj, addr) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  }
  else if (size == W25Q128J_ERASE_32K_BLOCK)
  {
    if (w25q128j_erase_32k_block_cmd(pobj, addr) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  }
  else if (size == W25Q128J_ERASE_64K_BLOCK)
  {
    if (w25q128j_erase_64k_block_cmd(pobj, addr) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  }
  else
  {
    return W25Q128J_ERROR;
  }

  return W25Q128J_OK;
}

/**
  * @brief  Erases sequentially one or multiple sectors (4K) from the W25Q128J NOR flash memory.
  * @param  pobj         : W25Q128J part Object Pointer
  * @param  erase_addr   : Start address of the data to be erased
  * @param  size_byte    : Size of data to be erased in bytes .
  * @retval error status
  */
w25q128j_status_t w25q128j_erase_sectors(w25q128j_obj_t *pobj, uint32_t erase_addr, uint32_t size_byte)
{
  uint32_t sectors_to_erase;

  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  w25q128j_status_t ret =  W25Q128J_OK;

  /* Calculate number of sectors to erase */
  sectors_to_erase = (uint32_t)(size_byte / W25Q128J_SECTOR_SIZE);
  /* Check if there is remaining data that requires an additional sector */
  if (((size_byte % W25Q128J_SECTOR_SIZE) != 0U) || ((erase_addr % W25Q128J_SECTOR_SIZE) != 0U))
  {
    sectors_to_erase += 1U;
  }
  /* Erase the total number of sectors in the w25q128j nor flash memory */
  for (uint32_t i = 0; i < sectors_to_erase; i++)
  {
    /* Calculate next address */
    uint32_t tmp_address = erase_addr + (i * W25Q128J_SECTOR_SIZE);
    if ((w25q128j_erase_sector_cmd(pobj, tmp_address) != W25Q128J_OK))
    {
      /* Failed to erase sector */
      return W25Q128J_ERROR;
    }
  }
  /* error status */
  return ret;
}

/**
  * @brief  Erase full w25q128j chip.
  * @param  pobj          : W25Q128J part Object Pointer
  * @retval error status
  */
w25q128j_status_t w25q128j_chip_erase(w25q128j_obj_t *pobj)
{
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  /* Erase block time */
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Initialize the erase block instructions sequence */
  uint8_t cmd = W25Q128J_CHIP_ERASE_CMD;
  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed set the write enable */
    return W25Q128J_ERROR;
  }

  /* Send the chip erase instructions sequence via SPI */
  if ((w25q128j_send_cmd(pobj, &cmd, W25Q128J_CHIP_ERASE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    return W25Q128J_ERROR;
  }

  /* Erase block time */
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Return PART status */
  return W25Q128J_OK;
}

/**
  * @brief  allows one or more data bytes to be sequentially read from the w25q128j flash memory.
  * @param  pobj                  : W25Q128J part Object Pointer
  * @param  p_data                : Pointer to read data
  * @param  read_addr             : Read start address
  * @param  size_byte             : Size of data to read
  * @retval  error status
  */
w25q128j_status_t w25q128j_read(w25q128j_obj_t *pobj, uint8_t *p_data,
                                uint32_t read_addr, uint32_t size_byte)
{
  w25q128j_status_t ret = W25Q128J_OK;

  /* Check parameters */
  if ((pobj == NULL) || (p_data == NULL))
  {
    return W25Q128J_ERROR;
  }
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_READABLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Initialize the read data instruction */
  uint8_t cmd[4] = {W25Q128J_READ_DATA_CMD, (uint8_t)((read_addr & W25Q128J_MEM_ADDR_MASK1) >> 16),
                    (uint8_t)((read_addr & W25Q128J_MEM_ADDR_MASK2) >> 8),
                    (uint8_t)(read_addr & W25Q128J_MEM_ADDR_MASK3)
                   };

  /* Send the read data command via SPI */
  if ((w25q128j_receive_cmd(pobj, cmd, W25Q128J_READ_DATA_CMD_SIZE, p_data, size_byte)) != W25Q128J_OK)
  {
    /* Failed to send the command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}


/**
  * @brief  allows one or more data bytes to be sequentially read from the w25q128j flash memory.
  *         The Fast Read instruction is similar to the Read Data instruction except that it can
  *         operate at the highest possible Clock frequency (133Mhz)
  * @param  pobj                  : W25Q128J part Object Pointer
  * @param  p_data                : Pointer to read data
  * @param  read_addr             : Read start address
  * @param  size_byte             : Size of data to read
  * @retval  error status
  */
w25q128j_status_t w25q128j_fast_read(w25q128j_obj_t *pobj, uint8_t *p_data,
                                     uint32_t read_addr, uint32_t size_byte)
{
  w25q128j_status_t ret = W25Q128J_OK;

  /* Check parameters */
  if ((pobj == NULL) || (p_data == NULL))
  {
    return W25Q128J_ERROR;
  }
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_READABLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Initialize the fast read data instruction */
  uint8_t cmd[5] = {W25Q128J_FAST_READ_CMD, (uint8_t)((read_addr & W25Q128J_MEM_ADDR_MASK1) >> 16),
                    (uint8_t)((read_addr & W25Q128J_MEM_ADDR_MASK2) >> 8),
                    (uint8_t)(read_addr & W25Q128J_MEM_ADDR_MASK3),
                    W25Q128J_DUMMY_BYTE
                   };

  /* Send the read data command via SPI */
  if ((w25q128j_receive_cmd(pobj, cmd, W25Q128J_FAST_DATA_CMD_SIZE, p_data, size_byte)) != W25Q128J_OK)
  {
    /* Failed to send the command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  allows from one byte to 256 bytes (a page) of data to be programmed at
  *          previously erased (FFh) memory locations.
  * @param  pobj                  : W25Q128J part Object Pointer
  * @param  write_addr            : Write start address.
  * @param  p_data                : Pointer to write data
  * @param  size_byte             : Size of data to write
  * @retval  error status
  */
static w25q128j_status_t w25q128j_page_program(w25q128j_obj_t *pobj, uint32_t write_addr,
                                               const uint8_t *p_data, uint32_t size_byte)
{
  /* Check parameters */
  if ((pobj == NULL) || (p_data == NULL))
  {
    return W25Q128J_ERROR;
  }

  /* Initialize the page program instruction */
  uint8_t cmd[4] = {W25Q128J_PAGE_PROG_CMD, (uint8_t)((write_addr & W25Q128J_MEM_ADDR_MASK1) >> 16),
                    (uint8_t)((write_addr & W25Q128J_MEM_ADDR_MASK2) >> 8),
                    (uint8_t)(write_addr & W25Q128J_MEM_ADDR_MASK3)
                   };

  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed to set the write enable */
    return W25Q128J_ERROR;
  }

  /* Select the w25q128j flash memory  */
  if ((w25q128j_select(pobj)) != W25Q128J_OK)
  {
    /* Failed to select w25q128j flash memory  */
    return W25Q128J_ERROR;
  }

  /* Send the program data load instruction via SPI */
  if ((HAL_SPI_Transmit(pobj->pio.phspi, cmd, W25Q128J_PAGE_PROGRAM_CMD_SIZE, W25Q128J_SPI_POLL_TIMEOUT)) != HAL_OK)
  {
    /* Failed to send command */
    return W25Q128J_ERROR;
  }
  /* Send the data we want to write via SPI */
  if ((HAL_SPI_Transmit(pobj->pio.phspi, p_data, size_byte, W25Q128J_SPI_POLL_TIMEOUT)) != HAL_OK)
  {
    /* Failed to send data buffer */
    return W25Q128J_ERROR;
  }

  /* De-select the w25q128j flash memory  */
  if ((w25q128j_deselect(pobj)) != W25Q128J_OK)
  {
    /* Failed to de-select w25q128j flash memory  */
    return W25Q128J_ERROR;
  }

  /* Wait for BUSY bit in the status register to bet cleared */
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  /* Return PART status */
  return W25Q128J_OK;
}

/**
  * @brief  Allows one or more data bytes to be sequentially written at previously erased (FFh) memory locations.
  * @param  pobj       : Pointer to the part object structure
  * @param  p_data     : Pointer to data to be written.
  * @param  write_addr : Write start address.
  * @param  size_byte  : Size of data to write in bytes.
  * @retval error status
  */
w25q128j_status_t w25q128j_write(w25q128j_obj_t *pobj, const uint8_t *p_data,
                                 uint32_t write_addr, uint32_t size_byte)
{
  uint32_t addr = write_addr;
  uint32_t write_index;

  if (size_byte == 0U)
  {
    /* nothing to write */
  }
  else
  {
    /* Calculate the maximum size of the first page */
    const uint32_t max_first_page = W25Q128J_PAGE_SIZE - (addr % W25Q128J_PAGE_SIZE);
    /* Amount of data in the first page */
    const uint32_t first_page = (max_first_page <= size_byte) ? max_first_page : size_byte;
    /* Number of full pages to write */
    const uint32_t full_pages = (size_byte - first_page) / W25Q128J_PAGE_SIZE;
    /* Size after the last full page */
    const uint32_t last_page = (size_byte - first_page) % W25Q128J_PAGE_SIZE;

    if (w25q128j_page_program(pobj, addr, p_data, first_page) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }

    write_index = first_page;
    addr += first_page;

    for (uint32_t ipage = 0; ipage < full_pages; ipage++)
    {
      if (w25q128j_page_program(pobj, addr, &p_data[write_index],
                                W25Q128J_PAGE_SIZE) != W25Q128J_OK)
      {
        return W25Q128J_ERROR;
      }
      write_index += W25Q128J_PAGE_SIZE;
      addr += W25Q128J_PAGE_SIZE;
    }

    if (last_page != 0U)
    {
      if (w25q128j_page_program(pobj, addr, &p_data[write_index], last_page) != W25Q128J_OK)
      {
        return W25Q128J_ERROR;
      }
    }
  }

  return W25Q128J_OK;
}

/**
  * @brief  enter power down.
  * @param  pobj            : W25Q128J part Object Pointer
  * @retval error status
  */
w25q128j_status_t w25q128j_enter_power_down(w25q128j_obj_t *pobj)
{
  w25q128j_status_t ret =  W25Q128J_OK;
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  /* Prepare the command sequence for power down */
  uint8_t cmd = W25Q128J_POWER_DOWN_CMD;

  /* Send the read Status Reg command via SPI */
  if ((w25q128j_send_cmd(pobj, &cmd, W25Q128J_ENTER_POWER_DOWN_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  exit power down.
  * @param  pobj          : W25Q128J part Object Pointer
  * @retval error status
  */
w25q128j_status_t w25q128j_exit_power_down(w25q128j_obj_t *pobj)
{
  w25q128j_status_t ret =  W25Q128J_OK;
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  /* Prepare the command sequence for release power down */
  uint8_t cmd = W25Q128J_RELEASE_POWER_DOWN_CMD;

  /* Send the read Status Reg command via SPI */
  if ((w25q128j_send_cmd(pobj, &cmd, W25Q128J_EXIT_POWER_DOWN_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Selects the W25Q128J flash memory by setting the CS line low.
  * @param  pobj          : W25Q128J part Object Pointer
  * @retval error status
  */
static w25q128j_status_t w25q128j_select(w25q128j_obj_t *pobj)
{

  /* Set the CS pin LOW */
  HAL_GPIO_WritePin(pobj->pio.cs_port, pobj->pio.cs_pin, W25Q128J_CS_PIN_RESET);

  /*  Verify if the CS pin is indeed LOW */
  for (uint8_t t = 0U; t < W25Q128J_CS_MAX_CHECK_COUNT; t++)
  {
    if (HAL_GPIO_ReadPin(pobj->pio.cs_port, pobj->pio.cs_pin) == HAL_GPIO_PIN_RESET)
    {
      return W25Q128J_OK;
    }
  }
  return W25Q128J_ERROR;
}

/**
  * @brief  De-selects the W25Q128J flash memory by setting the CS pin HIGH.
  * @param  pobj            : W25Q128J part Object Pointer
  * @retval error status
  */
static w25q128j_status_t w25q128j_deselect(w25q128j_obj_t *pobj)
{

  /* Set the CS pin HIGH */
  HAL_GPIO_WritePin(pobj->pio.cs_port, pobj->pio.cs_pin, W25Q128J_CS_PIN_SET);

  /*  Verify if the CS pin is indeed HIGH */
  for (uint8_t t = 0U; t < W25Q128J_CS_MAX_CHECK_COUNT; t++)
  {
    if (HAL_GPIO_ReadPin(pobj->pio.cs_port, pobj->pio.cs_pin) == HAL_GPIO_PIN_SET)
    {
      return W25Q128J_OK;
    }
  }
  return W25Q128J_ERROR;
}

static w25q128j_status_t w25q128j_send_cmd(w25q128j_obj_t *pobj, uint8_t *p_data, uint32_t size)
{
  /* Select the w25q128j flash memory  */
  if ((w25q128j_select(pobj)) != W25Q128J_OK)
  {
    /* Failed to select w25q128j flash memory  */
    return W25Q128J_ERROR;
  }

  if ((HAL_SPI_Transmit(pobj->pio.phspi, p_data, size, W25Q128J_SPI_POLL_TIMEOUT)) != HAL_OK)
  {
    /* Failed to send command */
    return W25Q128J_ERROR;
  }

  /* Deselect the w25q128j flash memory  */
  if ((w25q128j_deselect(pobj)) != W25Q128J_OK)
  {
    /* Failed to deselect w25q128j flash memory  */
    return W25Q128J_ERROR;
  }
  /* Return PART status */
  return W25Q128J_OK;
}

static w25q128j_status_t w25q128j_receive_cmd(w25q128j_obj_t *pobj, uint8_t *p_cmd, uint8_t cmd_len_byte,
                                              uint8_t *p_data, uint32_t data_len_byte)
{
  /* Check param before SPI access (data buffer is only required when data_len_byte > 0). */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  if (p_cmd == NULL)
  {
    return W25Q128J_ERROR;
  }

  if (cmd_len_byte == 0U)
  {
    return W25Q128J_ERROR;
  }

  if ((p_data == NULL) && (data_len_byte > 0U))
  {
    return W25Q128J_ERROR;
  }

  /* Select the w25q128j flash memory  */
  if ((w25q128j_select(pobj)) != W25Q128J_OK)
  {
    /* Failed to select w25q128j flash memory  */
    return W25Q128J_ERROR;
  }

  if ((HAL_SPI_Transmit(pobj->pio.phspi, p_cmd, cmd_len_byte, W25Q128J_SPI_POLL_TIMEOUT)) != HAL_OK)
  {
    /* Failed to send command */
    return W25Q128J_ERROR;
  }

  if ((HAL_SPI_Receive(pobj->pio.phspi, p_data, data_len_byte, W25Q128J_SPI_POLL_TIMEOUT)) != HAL_OK)
  {
    /* Failed to send command */
    return W25Q128J_ERROR;
  }
  /* Deselect the w25q128j flash memory  */
  if ((w25q128j_deselect(pobj)) != W25Q128J_OK)
  {
    /* Failed to deselect w25q128j flash memory  */
    return W25Q128J_ERROR;
  }
  /* Return PART status */
  return W25Q128J_OK;
}


#if defined (USE_HAL_SPI_DMA) && (USE_HAL_SPI_DMA == 1)
#if defined (W25Q128J_CALLBACKS) && (W25Q128J_CALLBACKS == 1)
/* Memory async read/write functions */
/**
  * @brief  Allows one or more data bytes to be sequentially written at
            previously erased (FFh) memory locations using DMA.
  * @param  pobj       : Pointer to the part object structure
  * @param  p_data     : Pointer to data to be written.
  * @param  write_addr : Write start address.
  * @param  size_byte  : Size of data to write in bytes.
  * @retval  error status
  */
w25q128j_status_t w25q128j_write_dma(w25q128j_obj_t *pobj, uint8_t *p_data,
                                     uint32_t write_addr, uint32_t size_byte)
{

  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  if (size_byte == 0U)
  {
    /* nothing to write */
  }
  else
  {
    /* Calculate the maximum size of the first page */
    const uint16_t max_first_page = (uint16_t)(W25Q128J_PAGE_SIZE - (write_addr % W25Q128J_PAGE_SIZE));
    /* Amount of data in the first page */
    const uint16_t first_page = ((uint32_t)max_first_page <= size_byte) ? max_first_page : (uint16_t)size_byte;
    /* Number of full pages to write */
    const uint32_t full_pages = (size_byte - first_page) / W25Q128J_PAGE_SIZE;
    /* Size after the last full page */
    const uint16_t last_page = (uint16_t)((size_byte - first_page) % W25Q128J_PAGE_SIZE);
    /* if this point is reached, we have data to write so 'first_page' is always not empty */
    const uint32_t nb_transfers = 1U + full_pages + ((last_page > 0U) ? 1U : 0U);

    /* check if data fits in the memory & no transfers are ongoing */
    if (((write_addr + size_byte) >= W25Q128J_FLASH_SIZE) || (pobj->async_context.wr_phase != W25Q128J_ASYNC_WRITE_IDLE)
        || (pobj->async_context.rd_phase != W25Q128J_ASYNC_READ_IDLE))
    {
      return W25Q128J_ERROR;
    }
    /* Update the obj and start the transfer */
    pobj->async_context.inhibit_callbacks = nb_transfers - 1U;
    pobj->async_context.current_addr = write_addr;
    pobj->async_context.size_byte = size_byte;
    pobj->async_context.p_buff = p_data;
    pobj->async_context.first_page_size = first_page;
    pobj->async_context.last_page_size = last_page;
    pobj->async_context.current_index = 0U;
    pobj->async_context.pending_transfer_ptr = NULL;
    pobj->async_context.pending_transfer_len = 0U;
    pobj->async_context.pending_transfer_tail_ptr = NULL;
    pobj->async_context.pending_transfer_tail_len = 0U;
    pobj->async_context.async_exec_mode = W25Q128J_ASYNC_EXEC_BLOCKING;

    HAL_SPI_SetUserData(pobj->pio.phspi, pobj);
    /* make sure the memory is ready starting the async transfer */
    if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }

    if (w25q128j_write_enable_async(pobj) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  }

  return W25Q128J_OK;
}


/**
  * @brief  Allows one or more data bytes to be sequentially written at
            previously erased (FFh) memory locations using DMA.
  * @param  pobj       : Pointer to the part object structure
  * @param  p_data     : Pointer to data to be written.
  * @param  write_addr : Write start address.
  * @param  size_byte  : Size of data to write in bytes.
  * @retval error status
  * @note The w25q128j_write_dma_async() function requires the use of w25q128j_exec_data_handler()
  *       for wait state management.
  */
w25q128j_status_t w25q128j_write_dma_async(w25q128j_obj_t *pobj, uint8_t *p_data, uint32_t write_addr,
                                           uint32_t size_byte)
{

  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  if (size_byte == 0U)
  {
    /* nothing to write */
  }
  else
  {
    /* Calculate the maximum size of the first page */
    const uint16_t max_first_page = (uint16_t)(W25Q128J_PAGE_SIZE - (write_addr % W25Q128J_PAGE_SIZE));
    /* Amount of data in the first page */
    const uint16_t first_page = ((uint32_t)max_first_page <= size_byte) ? max_first_page : (uint16_t)size_byte;
    /* Number of full pages to write */
    const uint32_t full_pages = (size_byte - first_page) / W25Q128J_PAGE_SIZE;
    /* Size after the last full page */
    const uint16_t last_page = (uint16_t)((size_byte - first_page) % W25Q128J_PAGE_SIZE);
    /* if this point is reached, we have data to write so 'first_page' is always not empty */
    const uint32_t nb_transfers = 1U + full_pages + ((last_page > 0U) ? 1U : 0U);

    /* check if data fits in the memory & no transfers are ongoing */
    if (((write_addr + size_byte) >= W25Q128J_FLASH_SIZE) || (pobj->async_context.wr_phase != W25Q128J_ASYNC_WRITE_IDLE)
        || (pobj->async_context.rd_phase != W25Q128J_ASYNC_READ_IDLE))
    {
      return W25Q128J_ERROR;
    }
    /* Update the obj and start the transfer */
    pobj->async_context.inhibit_callbacks = nb_transfers - 1U;
    pobj->async_context.current_addr = write_addr;
    pobj->async_context.size_byte = size_byte;
    pobj->async_context.p_buff = p_data;
    pobj->async_context.first_page_size = first_page;
    pobj->async_context.last_page_size = last_page;
    pobj->async_context.current_index = 0U;
    pobj->async_context.pending_transfer_ptr = NULL;
    pobj->async_context.pending_transfer_len = 0U;
    pobj->async_context.pending_transfer_tail_ptr = NULL;
    pobj->async_context.pending_transfer_tail_len = 0U;
    pobj->async_context.async_exec_mode = W25Q128J_ASYNC_EXEC_NONBLOCKING;

    HAL_SPI_SetUserData(pobj->pio.phspi, pobj);
    /* make sure the memory is ready starting the async transfer */
    if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_IDLE) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }

    if (w25q128j_write_enable_async(pobj) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  }

  return W25Q128J_OK;
}

/**
  * @brief  Writes a page (maximum) of data into the memory using DMA (async).
  * @param  pobj       : Pointer to the part object structure
  * @param  p_data     : Pointer to data to be written.
  * @param  write_addr : Write start address.
  * @param  size_byte  : size of data to write in bytes (max W25Q128J_PAGE_SIZE).
  * @retval error status
  */
w25q128j_status_t w25q128j_write_page_dma(w25q128j_obj_t *pobj, uint8_t *p_data, uint32_t write_addr,
                                          uint32_t size_byte)
{

  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  if (size_byte == 0U)
  {
    /* nothing to write */
  }
  else
  {
    w25q128j_status_t mem_state = W25Q128J_ERROR;
    /* check if data fits in the memory & no transfers are ongoing */
    if (((write_addr + size_byte) >= W25Q128J_FLASH_SIZE) || (pobj->async_context.wr_phase != W25Q128J_ASYNC_WRITE_IDLE)
        || (pobj->async_context.rd_phase != W25Q128J_ASYNC_READ_IDLE) || (size_byte > W25Q128J_PAGE_SIZE))
    {
      return W25Q128J_ERROR;
    }
    /* Update the obj and start the transfer */
    pobj->async_context.inhibit_callbacks = 0U;
    pobj->async_context.current_addr = write_addr;
    pobj->async_context.size_byte = size_byte;
    pobj->async_context.p_buff = p_data;
    pobj->async_context.first_page_size = (uint16_t)size_byte;
    pobj->async_context.last_page_size = 0U;
    pobj->async_context.current_index = 0U;
    pobj->async_context.pending_transfer_ptr = NULL;
    pobj->async_context.pending_transfer_len = 0U;
    pobj->async_context.pending_transfer_tail_ptr = NULL;
    pobj->async_context.pending_transfer_tail_len = 0U;
    pobj->async_context.async_exec_mode = W25Q128J_ASYNC_EXEC_PAGE;

    HAL_SPI_SetUserData(pobj->pio.phspi, pobj);
    /* make sure the memory is ready starting the async transfer */
    if (w25q128j_get_status(pobj, &mem_state) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
    if (mem_state != W25Q128J_OK)
    {
      /* Memory is not ready */
      return W25Q128J_ERROR;
    }

    if (w25q128j_write_enable_async(pobj) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  }

  return W25Q128J_OK;
}

/**
  * @brief  allows one or more data bytes to be sequentially read from the w25q128j physical memory using DMA.
  * @param  pobj                  : W25Q128J part Object Pointer
  * @param  p_data                : Pointer to read data
  * @param  read_addr             : Read start address
  * @param  size_byte             : Size of data to read
  * @retval error status
  */
w25q128j_status_t w25q128j_read_dma(w25q128j_obj_t *pobj, uint8_t *p_data, uint32_t read_addr,
                                    uint32_t size_byte)
{
  /* Check parameters */
  if ((pobj == NULL) || (p_data == NULL))
  {
    return W25Q128J_ERROR;
  }
  /* Check that there are no ongoing async transfer */
  if ((pobj->async_context.wr_phase != W25Q128J_ASYNC_WRITE_IDLE)
      || (pobj->async_context.rd_phase != W25Q128J_ASYNC_READ_IDLE))
  {
    return W25Q128J_ERROR;
  }
  /* calculate the number of pages to read from */
  uint32_t full_pages = (size_byte / W25Q128J_PAGE_SIZE) + (((size_byte % W25Q128J_PAGE_SIZE) == 0U) ? 0U : 1U);
  /* calculate last read address */
  uint32_t last_read_address = read_addr + (full_pages * W25Q128J_PAGE_SIZE) - 1U;
  /* Check that the data is in the memory range */
  if (last_read_address >= W25Q128J_FLASH_SIZE)
  {
    return W25Q128J_ERROR;
  }

  /* make sure the memory is ready */
  if (w25q128j_wait_for_state(pobj, W25Q128J_STATE_WAIT_UNTIL_READABLE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  HAL_SPI_SetUserData(pobj->pio.phspi, pobj);

  /* Set the required async context for the state machine */
  pobj->async_context.size_byte = size_byte;
  pobj->async_context.p_buff = p_data;
  pobj->async_context.pending_transfer_ptr = NULL;
  pobj->async_context.pending_transfer_len = 0U;
#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
  pobj->async_context.pending_rx_body_ptr = NULL;
  pobj->async_context.pending_rx_body_len = 0U;
#endif /* __DCACHE_PRESENT */
  pobj->async_context.pending_transfer_tail_ptr = NULL;
  pobj->async_context.pending_transfer_tail_len = 0U;

  /* Initialize the read data instruction */
  uint8_t cmd[4] = {W25Q128J_READ_DATA_CMD, (uint8_t)((read_addr & W25Q128J_MEM_ADDR_MASK1) >> 16),
                    (uint8_t)((read_addr & W25Q128J_MEM_ADDR_MASK2) >> 8),
                    (uint8_t)(read_addr & W25Q128J_MEM_ADDR_MASK3)
                   };
  /* update the state */
  pobj->async_context.rd_phase = W25Q128J_ASYNC_READ_CMD;
  /* register the read cmd complete function */
  (void)HAL_SPI_RegisterTxCpltCallback(pobj->pio.phspi, spi_read_cmd_cplt_cb);
  /* Select the NOR memory */
  if (w25q128j_select(pobj) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  /* Transmit the CMD */
  if (HAL_SPI_Transmit_IT(pobj->pio.phspi, cmd, (uint16_t) 4U) != HAL_OK)
  {
    return W25Q128J_ERROR;
  }

  /* error status */
  return W25Q128J_OK;
}

/**
  * @brief  Internal async read cmd cplt callback: starts head/body/tail RX chain.
  * @param  phspi : pointer to spi handle.
  */
static void spi_read_cmd_cplt_cb(hal_spi_handle_t *phspi)
{
  w25q128j_obj_t *pobj = (w25q128j_obj_t *)HAL_SPI_GetUserData(phspi);
  uint16_t head_len;
  /* context check */
  if (pobj->async_context.rd_phase == W25Q128J_ASYNC_READ_CMD)
  {
    /* Split RX buffer and compute explicit head/body/tail segments. */
    split_buffer(pobj->async_context.p_buff, (uint16_t)pobj->async_context.size_byte, &head_len,
                 &pobj->async_context.pending_transfer_ptr, &pobj->async_context.pending_transfer_len,
                 &pobj->async_context.pending_transfer_tail_ptr, &pobj->async_context.pending_transfer_tail_len);
#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    /* Keep RX body info for cache invalidate at read completion. */
    pobj->async_context.pending_rx_body_ptr = pobj->async_context.pending_transfer_ptr;
    pobj->async_context.pending_rx_body_len = pobj->async_context.pending_transfer_len;
#endif /* __DCACHE_PRESENT */

    pobj->async_context.rd_phase = W25Q128J_ASYNC_READING;
    if (head_len != 0U)
    {
      /* Consume head bytes with IT, then continue with explicit RX chain. */
      (void)HAL_SPI_RegisterRxCpltCallback(pobj->pio.phspi, spi_rx_head_cplt_cb);
      (void)HAL_SPI_Receive_IT(pobj->pio.phspi, pobj->async_context.p_buff, head_len);
    }
    else if (pobj->async_context.pending_transfer_len != 0U)
    {
      (void)HAL_SPI_RegisterRxCpltCallback(pobj->pio.phspi, spi_rx_body_cplt_cb);
#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
      SCB_CleanDCache_by_Addr(pobj->async_context.pending_transfer_ptr,
                              (int32_t)pobj->async_context.pending_transfer_len);
#endif /* __DCACHE_PRESENT */
      (void)HAL_SPI_Receive_DMA(pobj->pio.phspi, pobj->async_context.pending_transfer_ptr,
                                (uint32_t)pobj->async_context.pending_transfer_len);
    }
    else if (pobj->async_context.pending_transfer_tail_len != 0U)
    {
      (void)HAL_SPI_RegisterRxCpltCallback(pobj->pio.phspi, spi_rd_cplt_cb);
      (void)HAL_SPI_Receive_IT(pobj->pio.phspi, pobj->async_context.pending_transfer_tail_ptr,
                               pobj->async_context.pending_transfer_tail_len);
    }
    else
    {
      /* Empty transfer fallback. */
      spi_rd_cplt_cb(phspi);
    }
  }
}

/**
  * @brief  RX head completion callback: dispatches body/tail/cleanup.
  * @param  phspi : pointer to spi handle.
  */
static void spi_rx_head_cplt_cb(hal_spi_handle_t *phspi)
{
  w25q128j_obj_t *pobj = (w25q128j_obj_t *)HAL_SPI_GetUserData(phspi);
  /* context check */
  if (pobj->async_context.rd_phase == W25Q128J_ASYNC_READING)
  {
    if (pobj->async_context.pending_transfer_len != 0U)
    {
      (void)HAL_SPI_RegisterRxCpltCallback(pobj->pio.phspi, spi_rx_body_cplt_cb);
#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
      SCB_CleanDCache_by_Addr(pobj->async_context.pending_transfer_ptr,
                              (int32_t)pobj->async_context.pending_transfer_len);
#endif /* __DCACHE_PRESENT */
      (void)HAL_SPI_Receive_DMA(pobj->pio.phspi, pobj->async_context.pending_transfer_ptr,
                                (uint32_t)pobj->async_context.pending_transfer_len);
    }
    else if (pobj->async_context.pending_transfer_tail_len != 0U)
    {
      (void)HAL_SPI_RegisterRxCpltCallback(pobj->pio.phspi, spi_rd_cplt_cb);
      (void)HAL_SPI_Receive_IT(pobj->pio.phspi, pobj->async_context.pending_transfer_tail_ptr,
                               pobj->async_context.pending_transfer_tail_len);
    }
    else
    {
      /* No remaining body/tail chunk: complete the read sequence. */
      spi_rd_cplt_cb(phspi);
    }
  }
}

/**
  * @brief  RX body completion callback: starts tail IT receive when body DMA finishes.
  * @param  phspi : pointer to spi handle.
  */
static void spi_rx_body_cplt_cb(hal_spi_handle_t *phspi)
{
  w25q128j_obj_t *pobj = (w25q128j_obj_t *)HAL_SPI_GetUserData(phspi);

  if (pobj->async_context.rd_phase == W25Q128J_ASYNC_READING)
  {
    if (pobj->async_context.pending_transfer_tail_len != 0U)
    {
      (void)HAL_SPI_RegisterRxCpltCallback(pobj->pio.phspi, spi_rd_cplt_cb);
      (void)HAL_SPI_Receive_IT(pobj->pio.phspi, pobj->async_context.pending_transfer_tail_ptr,
                               pobj->async_context.pending_transfer_tail_len);
    }
    else
    {
      /* No remaining body/tail chunk: complete the read sequence. */
      spi_rd_cplt_cb(phspi);
    }
  }
}

/* Async write command functions */
/**
  * @brief  Sends a cmd to the W25Q128J memory using DMA (async).
  * @param  pobj       : W25Q128J part Object Pointer.
  * @param  pdata      : Pointer to data buffer.
  * @param  len_byte   : Size of data to read in bytes.
  * @retval error status.
  */
static w25q128j_status_t w25q128j_write_dma_cmd(w25q128j_obj_t *pobj, const uint8_t *pdata, uint16_t len_byte)
{
  if (w25q128j_select(pobj) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
  SCB_CleanDCache_by_Addr((uint32_t *)pdata, (int32_t)len_byte);
#endif /* __DCACHE_PRESENT */
  if (HAL_SPI_Transmit_DMA(pobj->pio.phspi, pdata, (uint32_t)len_byte) != HAL_OK)
  {
    /* Failed to start transfer */
    return W25Q128J_ERROR;
  }
  return W25Q128J_OK;
}

/**
  * @brief  Sends a cmd to the W25Q128J memory in non-blocking mode with Interrupt.
  * @param  pobj       : W25Q128J part Object Pointer.
  * @param  pdata      : Pointer to data buffer.
  * @param  len_byte   : Size of data to read in bytes.
  * @retval error status.
  */
static w25q128j_status_t w25q128j_write_it_cmd(w25q128j_obj_t *pobj, uint8_t *pdata, uint16_t len_byte)
{
  if (w25q128j_select(pobj) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  if (HAL_SPI_Transmit_IT(pobj->pio.phspi, pdata, len_byte) != HAL_OK)
  {
    /* Failed to start transfer */
    return W25Q128J_ERROR;
  }
  return W25Q128J_OK;
}

/* Async write internal functions and callbacks */
static w25q128j_status_t w25q128j_write_enable_async(w25q128j_obj_t *pobj)
{
  /* Initialize the write enable command */
  pobj->async_context.cmd_buf[0] = W25Q128J_WRITE_ENABLE_CMD;

  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  /* Update the async phase */
  pobj->async_context.wr_phase = W25Q128J_ASYNC_WEL;
  /* Register page program cmd completion callback. */
  (void)HAL_SPI_RegisterTxCpltCallback(pobj->pio.phspi, spi_we_cplt_cb);

  /* Send the write enable command (async) */
  if (w25q128j_write_it_cmd(pobj, pobj->async_context.cmd_buf,
                            (uint16_t) W25Q128J_WRITE_ENABLE_CMD_SIZE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  /* error status */
  return W25Q128J_OK;
}

/**
  * @brief  Internal async write enable callback function.
  * @param  phspi : pointer to spi handle.
  */
static void spi_we_cplt_cb(hal_spi_handle_t *phspi)
{
  w25q128j_obj_t *pobj = (w25q128j_obj_t *)HAL_SPI_GetUserData(phspi);
  /* context check */
  if (pobj->async_context.wr_phase == W25Q128J_ASYNC_WEL)
  {
    /* Deselect the memory */
    (void)w25q128j_deselect(pobj);
    uint32_t mem_addr = pobj->async_context.current_addr;
    pobj->async_context.cmd_buf[0] = W25Q128J_PAGE_PROG_CMD;
    pobj->async_context.cmd_buf[1] = (uint8_t)((mem_addr & W25Q128J_MEM_ADDR_MASK1) >> 16);
    pobj->async_context.cmd_buf[2] = (uint8_t)((mem_addr & W25Q128J_MEM_ADDR_MASK2) >> 8);
    pobj->async_context.cmd_buf[3] = (uint8_t)(mem_addr & W25Q128J_MEM_ADDR_MASK3);
    /* Update the async phase */
    pobj->async_context.wr_phase = W25Q128J_ASYNC_WRITE_CMD;
    /* Register page program cmd completion callback. */
    (void)HAL_SPI_RegisterTxCpltCallback(pobj->pio.phspi, spi_write_cmd_cplt_cb);
    (void)w25q128j_write_it_cmd(pobj, pobj->async_context.cmd_buf, (uint16_t) W25Q128J_PAGE_PROGRAM_CMD_SIZE);
  }
}

/**
  * @brief  Internal page program cmd complete callback function.
  * @param  phspi : pointer to spi handle.
  */
static void spi_write_cmd_cplt_cb(hal_spi_handle_t *phspi)
{
  w25q128j_obj_t *pobj = (w25q128j_obj_t *)HAL_SPI_GetUserData(phspi);
  uint16_t len_byte;
  uint16_t head_len;
  uint8_t *p_tx;

  /* context check */
  if (pobj->async_context.wr_phase == W25Q128J_ASYNC_WRITE_CMD)
  {
    if (pobj->async_context.current_index == 0U)
    {
      /* First transfer - set the size to page boundary */
      len_byte = pobj->async_context.first_page_size;
      p_tx = pobj->async_context.p_buff;
    }
    else
    {
      if (pobj->async_context.inhibit_callbacks == 0U)
      {
        /* Last transfer */
        if (pobj->async_context.last_page_size != 0U)
        {
          len_byte = pobj->async_context.last_page_size;
        }
        else
        {
          len_byte = W25Q128J_PAGE_SIZE;
        }
      }
      else
      {
        len_byte = W25Q128J_PAGE_SIZE;
      }
      p_tx = (pobj->async_context.p_buff + pobj->async_context.current_index);
    }

    /* Split TX buffer and prepare head/body/tail chunks. */
    split_buffer(p_tx, len_byte, &head_len, &pobj->async_context.pending_transfer_ptr,
                 &pobj->async_context.pending_transfer_len,
                 &pobj->async_context.pending_transfer_tail_ptr, &pobj->async_context.pending_transfer_tail_len);
    /* Update the async phase */
    pobj->async_context.wr_phase = W25Q128J_ASYNC_WRITE_DATA;
    if (head_len != 0U)
    {
      (void)HAL_SPI_RegisterTxCpltCallback(pobj->pio.phspi, spi_tx_head_cplt_cb);
      (void)w25q128j_write_it_cmd(pobj, p_tx, head_len);
    }
    else if (pobj->async_context.pending_transfer_len != 0U)
    {
      (void)HAL_SPI_RegisterTxCpltCallback(pobj->pio.phspi, spi_tx_body_cplt_cb);
      (void)w25q128j_write_dma_cmd(pobj, pobj->async_context.pending_transfer_ptr,
                                   pobj->async_context.pending_transfer_len);
    }
    else if (pobj->async_context.pending_transfer_tail_len != 0U)
    {
      (void)HAL_SPI_RegisterTxCpltCallback(pobj->pio.phspi, spi_write_data_cplt_cb);
      (void)w25q128j_write_it_cmd(pobj, pobj->async_context.pending_transfer_tail_ptr,
                                  pobj->async_context.pending_transfer_tail_len);
    }
    else
    {
      /* nothing to left transfer */
      spi_write_data_cplt_cb(phspi);
    }
  }
}

/**
  * @brief  TX head callback: dispatches next body/tail/cleanup step after head completion.
  * @param  phspi : pointer to spi handle.
  */
static void spi_tx_head_cplt_cb(hal_spi_handle_t *phspi)
{
  w25q128j_obj_t *pobj = (w25q128j_obj_t *)HAL_SPI_GetUserData(phspi);

  /* context check */
  if (pobj->async_context.wr_phase == W25Q128J_ASYNC_WRITE_DATA)
  {
    if (pobj->async_context.pending_transfer_len != 0U)
    {
      (void)HAL_SPI_RegisterTxCpltCallback(pobj->pio.phspi, spi_tx_body_cplt_cb);
      (void)w25q128j_write_dma_cmd(pobj, pobj->async_context.pending_transfer_ptr,
                                   pobj->async_context.pending_transfer_len);
    }
    else if (pobj->async_context.pending_transfer_tail_len != 0U)
    {
      (void)HAL_SPI_RegisterTxCpltCallback(pobj->pio.phspi, spi_write_data_cplt_cb);
      (void)w25q128j_write_it_cmd(pobj, pobj->async_context.pending_transfer_tail_ptr,
                                  pobj->async_context.pending_transfer_tail_len);
    }
    else
    {
      /* nothing to left transfer */
      spi_write_data_cplt_cb(phspi);
    }
  }
}

/**
  * @brief  TX body callback: dispatches next tail/cleanup step after body completion.
  * @param  phspi : pointer to spi handle.
  */
static void spi_tx_body_cplt_cb(hal_spi_handle_t *phspi)
{
  w25q128j_obj_t *pobj = (w25q128j_obj_t *)HAL_SPI_GetUserData(phspi);

  /* context check */
  if (pobj->async_context.wr_phase == W25Q128J_ASYNC_WRITE_DATA)
  {
    if (pobj->async_context.pending_transfer_tail_len != 0U)
    {
      (void)HAL_SPI_RegisterTxCpltCallback(pobj->pio.phspi, spi_write_data_cplt_cb);
      (void)w25q128j_write_it_cmd(pobj, pobj->async_context.pending_transfer_tail_ptr,
                                  pobj->async_context.pending_transfer_tail_len);
    }
    else
    {
      /* nothing to left transfer */
      spi_write_data_cplt_cb(phspi);
    }
  }
}

/**
  * @brief  TX body callback: dispatches next tail/cleanup step after body completion.
  * @param  phspi : pointer to spi handle.
  */
static void spi_write_data_cplt_cb(hal_spi_handle_t *phspi)
{
  w25q128j_obj_t *pobj = (w25q128j_obj_t *)HAL_SPI_GetUserData(phspi);

  /* context check */
  if (pobj->async_context.wr_phase == W25Q128J_ASYNC_WRITE_DATA)
  {
    /* Deselect the memory */
    (void)w25q128j_deselect(pobj);
    /* Make sure the memory is in a ready state */
    w25q128j_async_wait_ready(pobj);
  }
}

/* Async read internal functions and callbacks */
/**
  * @brief  Internal receive complete callback function.
  * @param  phspi : pointer to spi handle
  */
static void spi_rd_cplt_cb(hal_spi_handle_t *phspi)
{
  w25q128j_obj_t *pobj = (w25q128j_obj_t *)HAL_SPI_GetUserData(phspi);
  if (pobj->async_context.rd_phase == W25Q128J_ASYNC_READING)
  {

    (void)w25q128j_deselect(pobj);
#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ((pobj->async_context.pending_rx_body_ptr != NULL) && (pobj->async_context.pending_rx_body_len != 0U))
    {
      SCB_CleanInvalidateDCache_by_Addr(pobj->async_context.pending_rx_body_ptr,
                                        (int32_t)pobj->async_context.pending_rx_body_len);
    }
    pobj->async_context.pending_rx_body_ptr = NULL;
    pobj->async_context.pending_rx_body_len = 0U;
#endif /* __DCACHE_PRESENT */
    pobj->async_context.rd_phase = W25Q128J_ASYNC_READ_IDLE;
    if (pobj->rd_cb_ctx.callback != NULL)
    {
      pobj->rd_cb_ctx.callback(pobj, pobj->rd_cb_ctx.parg);
    }
  }
}

/* Alignment management */
/**
  * @brief  Splits a transfer buffer into head IT, aligned body and tail IT segments.
  * @param  in_buf      : Pointer to input buffer.
  * @param  in_len      : Total input length in bytes.
  * @param  head_len    : Misaligned head length to transfer with IT.
  * @param  body_ptr    : Pointer to aligned body buffer part.
  * @param  body_len    : Aligned body length in bytes.
  * @param  tail_ptr    : Pointer to misaligned tail buffer part.
  * @param  tail_len    : Misaligned tail length in bytes.
  */
static void split_buffer(uint8_t *in_buf, uint16_t in_len, uint16_t *head_len,
                         uint8_t **body_ptr, uint16_t *body_len,
                         uint8_t **tail_ptr, uint16_t *tail_len)
{
  uint32_t addr_mod = ((uint32_t)in_buf % W25Q128J_ALIGNMENT_SIZE);
  uint16_t remaining_len;

  *head_len = 0U;
  *body_ptr = NULL;
  *body_len = 0U;
  *tail_ptr = NULL;
  *tail_len = 0U;

  if ((in_buf == NULL) || (in_len == 0U))
  {
    /* Empty input: keep output segments empty. */
    return;
  }

  /* Head (misaligned): bytes before the first aligned address. */
  if (addr_mod != 0U)
  {
    *head_len = (uint16_t)(W25Q128J_ALIGNMENT_SIZE - addr_mod);
    if (*head_len > in_len)
    {
      *head_len = in_len;
    }
  }

  remaining_len = in_len - *head_len;
  /* Body starts right after the optional head segment. */
  *body_ptr = &in_buf[*head_len];

  /* Body (aligned): largest aligned chunk transferable with DMA. */
  *body_len = (uint16_t)((remaining_len / W25Q128J_ALIGNMENT_SIZE) * W25Q128J_ALIGNMENT_SIZE);

  /* Tail (misaligned): leftover bytes after the aligned body. */
  *tail_len = remaining_len - *body_len;
  if (*tail_len != 0U)
  {
    *tail_ptr = *body_ptr + *body_len;
  }
  else
  {
    *tail_ptr = NULL;
  }
  if (*body_len == 0U)
  {
    *body_ptr = NULL;
  }
}

/* Async phase getter functions */
/**
  * @brief  Gets the current phase of the asynchronous read state machine.
  * @retval asynchronous read phase
  */
w25q128j_async_read_phase_t w25q128j_get_async_read_phase(w25q128j_obj_t *pobj)
{

  return pobj->async_context.rd_phase;
}

/**
  * @brief  Gets the current phase of the asynchronous write state machine.
  * @retval asynchronous write phase
  */
w25q128j_async_write_phase_t w25q128j_get_async_write_phase(w25q128j_obj_t *pobj)
{

  return pobj->async_context.wr_phase;
}

/* Suspend/resume functions */
/**
  * @brief  Suspend erase block/sector or page program operation.
  * @param  pobj    : W25Q128J part Object Pointer
  * @retval error status
  * @note This function can only be used along with the w25q128j_write_page_dma(),
  *       as it is the only operation that does not handle wait state management.
  */
w25q128j_status_t w25q128j_suspend(w25q128j_obj_t *pobj)
{
  w25q128j_status_t ret = W25Q128J_OK;
  uint8_t stat_reg1 = 0U;
  uint8_t stat_reg2 = 0U;

  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  /* Read both status registers to confirm suspend is currently allowed. */
  if (w25q128j_read_status_reg1(pobj, &stat_reg1) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  if (w25q128j_read_status_reg2(pobj, &stat_reg2) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }

  /* Suspend is valid only while an erase or program operation is still running. */
  if ((stat_reg1 & W25Q128J_SR1_BUSY) != W25Q128J_SR1_BUSY)
  {
    return W25Q128J_ERROR;
  }

  /* Reject the request if the device is already in suspended state. */
  if ((stat_reg2 & W25Q128J_SR2_SUS) == W25Q128J_SR2_SUS)
  {
    return W25Q128J_ERROR;
  }

  /* Initialize the suspend command */
  uint8_t tData = W25Q128J_ERASE_PROG_SUSPEND_CMD;

  /* Send the reset enable command via SPI */
  if ((w25q128j_send_cmd(pobj, &tData, W25Q128J_SUSPEND_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }
  /* Return PART status */
  return ret;
}

/**
  * @brief  Resume erase block/sector or page program operation.
  * @param  pobj    : W25Q128J part Object Pointer
  * @retval error status
  */
w25q128j_status_t w25q128j_resume(w25q128j_obj_t *pobj)
{
  w25q128j_status_t ret = W25Q128J_OK;
  uint8_t stat_reg = 0;
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  if (w25q128j_read_status_reg2(pobj, &stat_reg) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  else if ((stat_reg & W25Q128J_SR2_SUS) != W25Q128J_SR2_SUS)
  {
    return W25Q128J_ERROR;
  }
  else
  {
    /* Initialize the suspend command */
    uint8_t tData = W25Q128J_ERASE_PROG_RESUME_CMD;

    /* Send the reset enable command via SPI */
    if ((w25q128j_send_cmd(pobj, &tData, W25Q128J_RESUME_CMD_SIZE)) != W25Q128J_OK)
    {
      /* Failed to send command */
      ret = W25Q128J_ERROR;
    }
  }
  /* Return PART status */
  return ret;
}

/* Async write utility functions */
/**
  * @brief  Handles wait state management for the async write state machine.
  * @param  pobj : W25Q128J part Object Pointer.
  * @retval error status.
  * @note The w25q128j_exec_data_handler() function must be used after a call to w25q128j_write_dma_async()
  *       to manage wait states.
  */
w25q128j_status_t w25q128j_exec_data_handler(w25q128j_obj_t *pobj)
{
  switch (pobj->async_context.wr_phase)
  {
    case W25Q128J_ASYNC_WAIT_READY:
    {
      uint32_t tstart = HAL_GetTick();
      w25q128j_status_t mem_state = W25Q128J_ERROR;
      do
      {
        if (w25q128j_get_status(pobj, &mem_state) != W25Q128J_OK)
        {
          return W25Q128J_ERROR;
        }
      } while (((HAL_GetTick() - tstart) < W25Q128J_WRITE_PAGE_MAX_WAIT)
               && (mem_state != W25Q128J_OK));

      if (mem_state == W25Q128J_OK)
      {
        /* Memory is ready - update the wr_phase */
        pobj->async_context.wr_phase = W25Q128J_ASYNC_READY;
        w25q128j_write_nextpage(pobj);
        return W25Q128J_OK;
      }
      else
      {
        /* timeout */
        return W25Q128J_ERROR;
      }
      break;
    }
    default: /* all other states: no operation to do */
      break;
  }

  return W25Q128J_OK;
}

/**
  * @brief  Starts the next page write in the async state machine.
  * @param  pobj : W25Q128J part Object Pointer.
  */
static void w25q128j_write_nextpage(w25q128j_obj_t *pobj)
{
  /* context check */
  if (pobj->async_context.wr_phase == W25Q128J_ASYNC_READY)
  {
    if (pobj->async_context.inhibit_callbacks == 0U)
    {
      pobj->async_context.wr_phase = W25Q128J_ASYNC_WRITE_IDLE;
      if (pobj->wr_cb_ctx.callback != NULL)
      {
        pobj->wr_cb_ctx.callback(pobj, pobj->wr_cb_ctx.parg);
      }
    }
    else
    {
      if (pobj->async_context.current_index == 0U)
      {
        /*First page */
        pobj->async_context.inhibit_callbacks--;
        pobj->async_context.current_addr  += pobj->async_context.first_page_size;
        pobj->async_context.current_index += pobj->async_context.first_page_size;
        (void)w25q128j_write_enable_async(pobj);
      }
      else
      {
        pobj->async_context.inhibit_callbacks--;
        pobj->async_context.current_index += W25Q128J_PAGE_SIZE;
        pobj->async_context.current_addr  += W25Q128J_PAGE_SIZE;
        (void)w25q128j_write_enable_async(pobj);
      }
    }
  }
}

/**
  * @brief  Waits until the memory is no longer busy/enters the wait state.
  * @param  pobj : W25Q128J part Object Pointer.
  */
static void w25q128j_async_wait_ready(w25q128j_obj_t *pobj)
{
  if (pobj->async_context.async_exec_mode == W25Q128J_ASYNC_EXEC_BLOCKING)
  {
    w25q128j_status_t mem_state = W25Q128J_ERROR;
    do
    {
      (void)w25q128j_get_status(pobj, &mem_state);
    } while (mem_state != W25Q128J_OK);
    pobj->async_context.wr_phase = W25Q128J_ASYNC_READY;
    w25q128j_write_nextpage(pobj);
  }
  else if (pobj->async_context.async_exec_mode == W25Q128J_ASYNC_EXEC_PAGE)
  {
    /* readiness check must be handled by the user application */
    pobj->async_context.wr_phase = W25Q128J_ASYNC_READY;
    w25q128j_write_nextpage(pobj);
  }
  else
  {
    pobj->async_context.wr_phase = W25Q128J_ASYNC_WAIT_READY;
  }
}

/* Register async read/write complete callback functions */
/**
  * @brief  Registers the read complete callback function.
  * @param  pobj : Pointer to part object
  * @param  cb   : Pointer to the callback function
  * @param  arg  : Pointer to the user data.
  * @retval error status
  */
w25q128j_status_t w25q128j_register_read_cplt_callback(w25q128j_obj_t *pobj, w25q128j_callback_t cb, void *arg)
{
  pobj->rd_cb_ctx.callback = cb;
  pobj->rd_cb_ctx.parg = arg;

  return W25Q128J_OK;
}

/**
  * @brief  Registers the write complete callback function.
  * @param  pobj : Pointer to part object
  * @param  cb   : Pointer to the callback function
  * @param  arg  : Pointer to the user data
  * @retval error status
  */
w25q128j_status_t w25q128j_register_write_cplt_callback(w25q128j_obj_t *pobj, w25q128j_callback_t cb, void *arg)
{
  pobj->wr_cb_ctx.callback = cb;
  pobj->wr_cb_ctx.parg = arg;

  return W25Q128J_OK;
}
#endif /* USE_HAL_SPI_DMA */
#endif /* W25Q128J_CALLBACKS */
