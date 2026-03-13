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

static w25q128j_status_t w25q128j_select(w25q128j_obj_t *pobj);
static w25q128j_status_t w25q128j_deselect(w25q128j_obj_t *pobj);
static w25q128j_status_t w25q128j_send_cmd(w25q128j_obj_t *pobj, uint8_t *p_data, uint32_t size);
static w25q128j_status_t w25q128j_receive_cmd(w25q128j_obj_t *pobj, uint8_t *p_cmd, uint8_t cmd_len_byte,
                                              uint8_t *p_data, uint32_t data_len_byte);

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
    else if (w25q128j_write_enable(pobj) != W25Q128J_OK)
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
  /* Initialize the read unique ID command */
  uint8_t cmd = W25Q128J_READ_UNIQUE_ID_CMD;

  /* Send the read unique ID command via SPI */
  if ((w25q128j_receive_cmd(pobj, &cmd, W25Q128J_UNIQUE_ID_CMD_SIZE, p_id, W25Q128J_UNIQUE_ID_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Resets the flash memory.
  * @param  pobj    : W25Q128J part Object Pointer
  * @retval error status
  */
w25q128j_status_t w25q128j_reset(w25q128j_obj_t *pobj)
{
  w25q128j_status_t ret = W25Q128J_OK;
  uint8_t stat_reg = 0;
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  /* Initialize the enable reset command */
  uint8_t tData = W25Q128J_RESET_ENABLE_CMD;
  /* Check the BUSY bit in the status register */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);

  /* Send the reset enable command via SPI */
  if ((w25q128j_send_cmd(pobj, &tData, W25Q128J_RESET_ENABLE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }

  /* Initialize the reset command */
  tData = W25Q128J_RESET_MEMORY_CMD;

  /* Send the reset command via SPI */
  if ((w25q128j_send_cmd(pobj, &tData, W25Q128J_RESET_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}


/**
  * @brief  Suspend erase block/sector or page program operation.
  * @param  pobj    : W25Q128J part Object Pointer
  * @retval error status
  */
w25q128j_status_t w25q128j_suspend(w25q128j_obj_t *pobj)
{
  w25q128j_status_t ret = W25Q128J_OK;
  /* Check parameters */
  if (pobj == NULL)
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
  uint8_t stat_reg;
  w25q128j_status_t ret =  W25Q128J_OK;
  /* Check parameters */
  if ((pobj == NULL) || (p_value == NULL))
  {
    return W25Q128J_ERROR;
  }
  /* make sure the memory is ready */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);
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
  uint8_t stat_reg;
  w25q128j_status_t ret =  W25Q128J_OK;
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  /* make sure the memory is ready */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);
  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed to set the write enable */
    ret = W25Q128J_ERROR;
  }
  /* Prepare the command sequence for writing the status register */
  uint8_t cmd[2] = {W25Q128J_WRITE_STATUS_REG1_CMD, value};

  /* Send the command sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_WRITE_STATUS_REG_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Writes the w25q128j status register2.
  * @param  pobj       : W25Q128J part Object Pointer
  * @param  p_value    : The 8-bits data we want to write into the status register2
  * @retval error status
  */
w25q128j_status_t w25q128j_write_status_reg2(w25q128j_obj_t *pobj, uint8_t value)
{
  uint8_t stat_reg;
  w25q128j_status_t ret =  W25Q128J_OK;
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  /* make sure the memory is ready */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);
  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed to set the write enable */
    ret = W25Q128J_ERROR;
  }
  /* Prepare the command sequence for writing the status register */
  uint8_t cmd[2] = {W25Q128J_WRITE_STATUS_REG2_CMD, value};

  /* Send the command sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_WRITE_STATUS_REG_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Writes the w25q128j status register3.
  * @param  pobj        : W25Q128J part Object Pointer
  * @param  p_value     : The 8-bits data we want to write into the status register2
  * @retval error status
  */
w25q128j_status_t w25q128j_write_status_reg3(w25q128j_obj_t *pobj, uint8_t value)
{
  uint8_t stat_reg;
  w25q128j_status_t ret =  W25Q128J_OK;
  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  /* make sure the memory is ready */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);
  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed to set the write enable */
    ret = W25Q128J_ERROR;
  }
  /* Prepare the command sequence for writing the status register */
  uint8_t cmd[2] = {W25Q128J_WRITE_STATUS_REG3_CMD, value};

  /* Send the command sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_WRITE_STATUS_REG_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    ret = W25Q128J_ERROR;
  }

  /* Return PART status */
  return ret;
}

/**
  * @brief  Erase a 64K block from the w25q128j NOR flash memory.
  * @param  pobj                  : W25Q128J part Object Pointer
  * @param  block_address         : Block address
  * @retval error status
  */
static w25q128j_status_t w25q128j_erase_64k_block_cmd(w25q128j_obj_t *pobj, uint32_t block_address)
{
  w25q128j_status_t ret =  W25Q128J_OK;
  uint8_t stat_reg;

  /* Initialize the erase block instructions sequence */
  uint8_t cmd[4] = {W25Q128J_BLOCK_ERASE_64K_CMD, (uint8_t)((block_address & W25Q128J_MEM_ADDR_MASK1) >> 16),
                    (uint8_t)((block_address & W25Q128J_MEM_ADDR_MASK2) >> 8),
                    (uint8_t)(block_address & W25Q128J_MEM_ADDR_MASK3)
                   };

  /* make sure the memory is ready */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);

  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed set the write enable */
    ret = W25Q128J_ERROR;
  }

  /* Send the erase block instructions sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_BLOCK_ERASE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    ret = W25Q128J_ERROR;
  }
  /* Return PART status */
  return ret;
}


/**
  * @brief  Erase a 32K block from the w25q128j NOR flash memory.
  * @param  pobj                  : W25Q128J part Object Pointer
  * @param  block_address         : Block address
  * @retval error status
  */
static w25q128j_status_t w25q128j_erase_32k_block_cmd(w25q128j_obj_t *pobj, uint32_t block_address)
{
  w25q128j_status_t ret =  W25Q128J_OK;
  uint8_t stat_reg;

  /* Initialize the erase block instructions sequence */
  uint8_t cmd[4] = {W25Q128J_BLOCK_ERASE_32K_CMD, (uint8_t)((block_address & W25Q128J_MEM_ADDR_MASK1) >> 16),
                    (uint8_t)((block_address & W25Q128J_MEM_ADDR_MASK2) >> 8),
                    (uint8_t)(block_address & W25Q128J_MEM_ADDR_MASK3)
                   };

  /* make sure the memory is ready */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);

  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed set the write enable */
    ret = W25Q128J_ERROR;
  }

  /* Send the erase block instructions sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_BLOCK_ERASE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    ret = W25Q128J_ERROR;
  }

  /* Erase block time */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);
  /* Return PART status */
  return ret;
}

/**
  * @brief  Erase a 64K block from the w25q128j NOR flash memory.
  * @param  pobj                  : W25Q128J part Object Pointer
  * @param  sector_address        : Sector address
  * @retval error status
  */
static w25q128j_status_t w25q128j_erase_sector_cmd(w25q128j_obj_t *pobj, uint32_t sector_address)
{
  w25q128j_status_t ret =  W25Q128J_OK;
  uint8_t stat_reg;

  /* Initialize the erase block instructions sequence */
  uint8_t cmd[4] = {W25Q128J_SECTOR_ERASE_CMD, (uint8_t)((sector_address & W25Q128J_MEM_ADDR_MASK1) >> 16),
                    (uint8_t)((sector_address & W25Q128J_MEM_ADDR_MASK2) >> 8),
                    (uint8_t)(sector_address & W25Q128J_MEM_ADDR_MASK3)
                   };

  /* make sure the memory is ready */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);

  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed set the write enable */
    ret = W25Q128J_ERROR;
  }

  /* Send the erase block instructions sequence via SPI */
  if ((w25q128j_send_cmd(pobj, cmd, W25Q128J_BLOCK_ERASE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    ret = W25Q128J_ERROR;
  }

  /* Erase block time */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);
  /* Return PART status */
  return ret;
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
  w25q128j_status_t ret =  W25Q128J_OK;
  uint8_t stat_reg;

  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }
  /* Initialize the erase block instructions sequence */
  uint8_t cmd = W25Q128J_CHIP_ERASE_CMD;
  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed set the write enable */
    ret = W25Q128J_ERROR;
  }

  /* Send the chip erase instructions sequence via SPI */
  if ((w25q128j_send_cmd(pobj, &cmd, W25Q128J_CHIP_ERASE_CMD_SIZE)) != W25Q128J_OK)
  {
    /* Failed to send the command sequence */
    ret = W25Q128J_ERROR;
  }

  /* Erase block time */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);
  /* Return PART status */
  return ret;
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
  uint8_t stat_reg;
  w25q128j_status_t ret = W25Q128J_OK;

  /* Check parameters */
  if ((pobj == NULL) || (p_data == NULL))
  {
    return W25Q128J_ERROR;
  }
  /* Check that the memory is ready */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);

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
  uint8_t stat_reg;
  w25q128j_status_t ret = W25Q128J_OK;

  /* Check parameters */
  if ((pobj == NULL) || (p_data == NULL))
  {
    return W25Q128J_ERROR;
  }
  /* Check that the memory is ready */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);

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
  w25q128j_status_t ret = W25Q128J_OK;
  uint8_t stat_reg;

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

  /* Check that the memory is ready */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);

  /* Set write enable */
  if ((w25q128j_write_enable(pobj)) != W25Q128J_OK)
  {
    /* Failed to set the write enable */
    ret = W25Q128J_ERROR;
  }

  /* Select the w25q128j flash memory  */
  if ((w25q128j_select(pobj)) != W25Q128J_OK)
  {
    /* Failed to select w25q128j flash memory  */
    ret = W25Q128J_ERROR;
  }

  /* Send the program data load instruction via SPI */
  if ((HAL_SPI_Transmit(pobj->pio.phspi, cmd, W25Q128J_PAGE_PROGRAM_CMD_SIZE, W25Q128J_SPI_POLL_TIMEOUT)) != HAL_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }
  /* Send the data we want to write via SPI */
  if ((HAL_SPI_Transmit(pobj->pio.phspi, p_data, size_byte, W25Q128J_SPI_POLL_TIMEOUT)) != HAL_OK)
  {
    /* Failed to send data buffer */
    ret = W25Q128J_ERROR;
  }

  /* De-select the w25q128j flash memory  */
  if ((w25q128j_deselect(pobj)) != W25Q128J_OK)
  {
    /* Failed to de-select w25q128j flash memory  */
    ret = W25Q128J_ERROR;
  }

  /* Wait for BUSY bit in the status register to bet cleared */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);
  /* Return PART status */
  return ret;
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
  w25q128j_status_t ret = W25Q128J_OK;
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
      ret = W25Q128J_ERROR;
    }
    else
    {
      write_index = first_page;
      addr += first_page;

      for (uint32_t ipage = 0; ipage < full_pages; ipage++)
      {
        /* Check if an error has occurred before writing to the next page */
        if (ret == W25Q128J_OK)
        {
          if (w25q128j_page_program(pobj, addr, &p_data[write_index],
                                    W25Q128J_PAGE_SIZE) != W25Q128J_OK)
          {
            ret = W25Q128J_ERROR;
          }
          write_index += W25Q128J_PAGE_SIZE;
          addr += W25Q128J_PAGE_SIZE;
        }
      }
      if (ret == W25Q128J_OK)
      {
        if (last_page != 0U)
        {
          if (w25q128j_page_program(pobj, addr, &p_data[write_index], last_page) != W25Q128J_OK)
          {
            ret = W25Q128J_ERROR;
          }
        }
      }
    }
  }
  return ret;
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
  w25q128j_status_t ret =  W25Q128J_OK;

  /* Set the CS pin LOW */
  HAL_GPIO_WritePin(pobj->pio.cs_port, pobj->pio.cs_pin, W25Q128J_CS_PIN_RESET);

  /*  Verify if the CS pin is indeed LOW */
  if (HAL_GPIO_ReadPin(pobj->pio.cs_port, pobj->pio.cs_pin) != W25Q128J_CS_PIN_RESET)
  {
    /* Failed to set CS pin LOW */
    ret =  W25Q128J_ERROR;
  }
  return ret;
}

/**
  * @brief  De-selects the W25Q128J flash memory by setting the CS pin HIGH.
  * @param  pobj            : W25Q128J part Object Pointer
  * @retval error status
  */
static w25q128j_status_t w25q128j_deselect(w25q128j_obj_t *pobj)
{
  w25q128j_status_t ret =  W25Q128J_OK;

  /* Set the CS pin HIGH */
  HAL_GPIO_WritePin(pobj->pio.cs_port, pobj->pio.cs_pin, W25Q128J_CS_PIN_SET);

  /*  Verify if the CS pin is indeed HIGH */
  if (HAL_GPIO_ReadPin(pobj->pio.cs_port, pobj->pio.cs_pin) != W25Q128J_CS_PIN_SET)
  {
    /* Failed to set CS pin HIGH */
    ret =  W25Q128J_ERROR;
  }
  /* Return PART status */
  return ret;
}

static w25q128j_status_t w25q128j_send_cmd(w25q128j_obj_t *pobj, uint8_t *p_data, uint32_t size)
{
  w25q128j_status_t ret =  W25Q128J_OK;

  /* Select the w25q128j flash memory  */
  if ((w25q128j_select(pobj)) != W25Q128J_OK)
  {
    /* Failed to select w25q128j flash memory  */
    ret = W25Q128J_ERROR;
  }

  if ((HAL_SPI_Transmit(pobj->pio.phspi, p_data, size, W25Q128J_SPI_POLL_TIMEOUT)) != HAL_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }

  /* Deselect the w25q128j flash memory  */
  if ((w25q128j_deselect(pobj)) != W25Q128J_OK)
  {
    /* Failed to deselect w25q128j flash memory  */
    ret = W25Q128J_ERROR;
  }
  /* Return PART status */
  return ret;
}

static w25q128j_status_t w25q128j_receive_cmd(w25q128j_obj_t *pobj, uint8_t *p_cmd, uint8_t cmd_len_byte,
                                              uint8_t *p_data, uint32_t data_len_byte)
{
  w25q128j_status_t ret =  W25Q128J_OK;

  /* Select the w25q128j flash memory  */
  if ((w25q128j_select(pobj)) != W25Q128J_OK)
  {
    /* Failed to select w25q128j flash memory  */
    ret = W25Q128J_ERROR;
  }

  if ((HAL_SPI_Transmit(pobj->pio.phspi, p_cmd, cmd_len_byte, W25Q128J_SPI_POLL_TIMEOUT)) != HAL_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }

  if ((HAL_SPI_Receive(pobj->pio.phspi, p_data, data_len_byte, W25Q128J_SPI_POLL_TIMEOUT)) != HAL_OK)
  {
    /* Failed to send command */
    ret = W25Q128J_ERROR;
  }
  /* Deselect the w25q128j flash memory  */
  if ((w25q128j_deselect(pobj)) != W25Q128J_OK)
  {
    /* Failed to deselect w25q128j flash memory  */
    ret = W25Q128J_ERROR;
  }
  /* Return PART status */
  return ret;
}


#if defined (USE_HAL_SPI_DMA) && (USE_HAL_SPI_DMA == 1)
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
  uint8_t stat_reg = 0;

  /* Check parameters */
  if ((pobj == NULL) || (p_data == NULL))
  {
    return W25Q128J_ERROR;
  }
#if defined (W25Q128J_CALLBACKS) && (W25Q128J_CALLBACKS == 1)
  /* Check that there are no ongoing async transfer */
  if ((pobj->wr_phase != W25Q128J_ASYNC_WRITE_IDLE) || (pobj->rd_phase != W25Q128J_ASYNC_READ_IDLE))
  {
    return W25Q128J_ERROR;
  }
#endif /* W25Q128J_CALLBACKS */
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
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);

  /* Initialize the read data instruction */
  uint8_t cmd[4] = {W25Q128J_READ_DATA_CMD, (uint8_t)((read_addr & W25Q128J_MEM_ADDR_MASK1) >> 16),
                    (uint8_t)((read_addr & W25Q128J_MEM_ADDR_MASK2) >> 8),
                    (uint8_t)(read_addr & W25Q128J_MEM_ADDR_MASK3)
                   };

#if defined (W25Q128J_CALLBACKS) && (W25Q128J_CALLBACKS == 1)
  /* update the state */
  pobj->rd_phase = W25Q128J_ASYNC_READING;
#endif /* W25Q128J_CALLBACKS */
  /* Select the NOR memory */
  if (w25q128j_select(pobj) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  /* Transmit the CMD */
  if (HAL_SPI_Transmit(pobj->pio.phspi, cmd, (uint16_t) 4U, W25Q128J_SPI_POLL_TIMEOUT) != HAL_OK)
  {
    return W25Q128J_ERROR;
  }
  /* Receive the Data */
  if (HAL_SPI_Receive_DMA(pobj->pio.phspi, p_data, size_byte) != HAL_OK)
  {
    return W25Q128J_ERROR;
  }

  /* error status */
  return W25Q128J_OK;

}
#endif /* USE_HAL_SPI_DMA */

#if defined (W25Q128J_CALLBACKS) && (W25Q128J_CALLBACKS == 1)

static w25q128j_status_t w25q128j_write_dma_cmd(w25q128j_obj_t *pobj, const uint8_t *pdata, uint16_t len)
{
  if (w25q128j_select(pobj) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  if (HAL_SPI_Transmit_DMA(pobj->pio.phspi, pdata, len) != HAL_OK)
  {
    /* Failed to start transfer */
    return W25Q128J_ERROR;
  }
  return W25Q128J_OK;
}

static w25q128j_status_t w25q128j_write_enable_async(w25q128j_obj_t *pobj)
{
  uint8_t stat_reg;
  /* Initialize the write enable command */
  uint8_t cmd = W25Q128J_WRITE_ENABLE_CMD;

  /* Check parameters */
  if (pobj == NULL)
  {
    return W25Q128J_ERROR;
  }

  /* make sure the memory is ready */
  do
  {
    if (w25q128j_read_status_reg1(pobj, &stat_reg) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  } while ((stat_reg & W25Q128J_SR1_BUSY) == W25Q128J_SR1_BUSY);

  /* Update the async phase */
  pobj->wr_phase = W25Q128J_ASYNC_WEL;

  /* Send the write enable command (async) */
  if (w25q128j_write_dma_cmd(pobj, &cmd, (uint16_t) W25Q128J_WRITE_ENABLE_CMD_SIZE) != W25Q128J_OK)
  {
    return W25Q128J_ERROR;
  }
  /* error status */
  return W25Q128J_OK;
}

/**
  * @brief  Allows one or more data bytes to be sequentially written at
            previously erased (FFh) memory locations using DMA.
  * @param  pobj       : Pointer to the part object structure
  * @param  p_data     : Pointer to data to be written.
  * @param  write_addr : Write start address.
  * @param  size_byte  : Size of data to write in bytes.
  * @retval  error status
 */
w25q128j_status_t w25q128j_write_dma(w25q128j_obj_t *pobj, const uint8_t *p_data,
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

    /* Update the obj and start the transfer */
    pobj->inhibit_callbacks = nb_transfers - 1U;
    pobj->current_address = write_addr;
    pobj->p_buff = p_data;
    pobj->current_index = 0U;
    pobj->first_page_size = first_page;
    pobj->last_page_size = last_page;
    pobj->is_first_page = 0U;

    if (w25q128j_write_enable_async(pobj) != W25Q128J_OK)
    {
      return W25Q128J_ERROR;
    }
  }

  return W25Q128J_OK;
}

/**
  * @brief  Gets the current phase of the asynchronous read state machine.
  * @retval asynchronous read phase
  */
w25q128j_async_read_phase_t w25q128j_get_async_read_phase(w25q128j_obj_t *pobj)
{
  (void)pobj;
  return pobj->rd_phase;
}

/**
  * @brief  Gets the current phase of the asynchronous write state machine.
  * @retval asynchronous write phase
  */
w25q128j_async_write_phase_t w25q128j_get_async_write_phase(w25q128j_obj_t *pobj)
{
  (void)pobj;
  return pobj->wr_phase;
}

/**
  * @brief  Internal receive complete callback function.
  * @param  phspi : pointer to spi handle
  */
static void spi_rd_cplt_cb(hal_spi_handle_t *phspi)
{
  w25q128j_obj_t *pobj = (w25q128j_obj_t *)HAL_SPI_GetUserData(phspi);

  pobj->rd_phase = W25Q128J_ASYNC_READ_IDLE;
  pobj->rd_cb_ctx.callback(pobj, pobj->rd_cb_ctx.parg);
}

/**
  * @brief  Internal transmit complete callback function.
  * @param  phspi : pointer to spi handle
  */
static void spi_wr_cplt_cb(hal_spi_handle_t *phspi)
{
  w25q128j_obj_t *pobj = (w25q128j_obj_t *)HAL_SPI_GetUserData(phspi);
  /* Initialize the page program instruction */
  uint8_t cmd[4] = {0U};
  /* Page address calculation */
  uint32_t mem_addr = 0U;
  uint16_t len_byte;

  switch (pobj->wr_phase)
  {
    case W25Q128J_ASYNC_WEL:
      /* Deselect the memory */
      (void)w25q128j_deselect(pobj);
      mem_addr = pobj->current_address;
      cmd[0] = W25Q128J_PAGE_PROG_CMD;
      cmd[1] = (uint8_t)((mem_addr & W25Q128J_MEM_ADDR_MASK1) >> 16);
      cmd[2] = (uint8_t)((mem_addr & W25Q128J_MEM_ADDR_MASK2) >> 8);
      cmd[3] = (uint8_t)(mem_addr & W25Q128J_MEM_ADDR_MASK3);
      /* Update the async phase */
      pobj->wr_phase = W25Q128J_ASYNC_WRITE_CMD;
      (void)w25q128j_write_dma_cmd(pobj, cmd, (uint16_t) W25Q128J_PAGE_PROGRAM_CMD_SIZE);
      break;
    case W25Q128J_ASYNC_WRITE_CMD:
      /* Update the async phase */
      pobj->wr_phase = W25Q128J_ASYNC_WRITE_DATA;
      /* First page */
      if (pobj->is_first_page == 0U)
      {
        len_byte = pobj->first_page_size;
      }
      /* Last page*/
      else if ((pobj->inhibit_callbacks == 0U) && (pobj->last_page_size != 0U))
      {
        len_byte = pobj->last_page_size;
      }
      else
      {
        len_byte = W25Q128J_PAGE_SIZE;
      }
      /* Load the buffer */
      (void)w25q128j_write_dma_cmd(pobj, &pobj->p_buff[pobj->current_index], len_byte);
      break;
    case W25Q128J_ASYNC_WRITE_DATA:
      /* Deselect the memory */
      (void)w25q128j_deselect(pobj);
      if (pobj->inhibit_callbacks == 0U)
      {
        pobj->wr_phase = W25Q128J_ASYNC_WRITE_IDLE;
        pobj->wr_cb_ctx.callback(pobj, pobj->wr_cb_ctx.parg);
      }
      /*First page */
      else if (pobj->is_first_page == 0U)
      {
        pobj->inhibit_callbacks--;
        pobj->current_address += pobj->first_page_size;
        pobj->current_index += pobj->first_page_size;
        pobj->is_first_page = 1U;
        (void)w25q128j_write_enable_async(pobj);
      }
      else
      {
        pobj->inhibit_callbacks--;
        pobj->current_address += W25Q128J_PAGE_SIZE;
        pobj->current_index += W25Q128J_PAGE_SIZE;
        (void)w25q128j_write_enable_async(pobj);
      }
      break;
    default:
      break;
  }
}


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

  HAL_SPI_SetUserData(pobj->pio.phspi, pobj);
  if (HAL_SPI_RegisterRxCpltCallback(pobj->pio.phspi, spi_rd_cplt_cb) != HAL_OK)
  {
    return W25Q128J_ERROR;
  }

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

  HAL_SPI_SetUserData(pobj->pio.phspi, pobj);
  if (HAL_SPI_RegisterTxCpltCallback(pobj->pio.phspi, spi_wr_cplt_cb) != HAL_OK)
  {
    return W25Q128J_ERROR;
  }

  return W25Q128J_OK;
}

#endif /* W25Q128J_CALLBACKS */
