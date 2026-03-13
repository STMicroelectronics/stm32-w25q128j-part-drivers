# __w25q128j Part Drivers__

![tag](https://img.shields.io/badge/tag-2.0.0-brightgreen.svg)
[![release note](https://img.shields.io/badge/release_note-view_html-gold.svg)](https://htmlpreview.github.io/?https://github.com/STMicroelectronics/stm32-w25q128j-part-drivers/blob/main/Release_Notes.html)

## Overview

The W25Q128J part drivers provide APIs to drive the W25Q128J NOR FLASH through SPI.


## __Description__

### Peripherals initialization:

The W25Q128J part driver assumes that the initialization of all needed peripherals (SPI, DMA, GPIO) is done by the main application.

## How to use it?

To use the W25Q128J part driver APIs, the application must define the w25q128j object and assign SPI HW resources to it.

w25q128j object struct:
```c
typedef struct w25q128j_obj_s w25q128j_obj_t;

struct w25q128j_obj_s
{
  w25q128j_init_state_t       is_initialized;    /*!< Init state                                 */
  w25q128j_io_t               pio;               /*!< IO interface                               */
#if defined (W25Q128J_CALLBACKS) && (W25Q128J_CALLBACKS == 1)
  volatile uint32_t inhibit_callbacks;               /*!< Used for multi-pages writes           */
  w25q128j_callback_ctx_t     rd_cb_ctx;             /*!< Read  Complete: Callback data         */
  w25q128j_callback_ctx_t     wr_cb_ctx;             /*!< Write Complete: Callback data         */
  w25q128j_async_read_phase_t  rd_phase;             /*!< Async write phase                     */
  w25q128j_async_write_phase_t wr_phase;             /*!< Async read phase                      */
  uint8_t                     current_page;          /*!< Current write page          - async writes */
  uint16_t                    current_block;         /*!< Current write block         - async writes */
  uint8_t                     *p_buff;               /*!< Pointer to the write buffer - async writes */
  uint32_t                    size_byte;             /*!< Total write data size       - async writes */
  uint32_t                    nb_transfer;           /*!< Number of pages to write    - async writes */
#endif /* W25Q128J_CALLBACKS */
};

```
The application must implement the following:

```c
#define MX_W25Q128J_0       0 /* instance id */

```
A redefinition of the `w25q128j_io_init()` function where the SPI handle getter function is linked to the w25q128j object:

```c
int32_t w25q128j_io_init(w25q128j_io_t *pio)
{
  switch (pio->dev_id)
  {
    case MX_W25Q128J_0:
      pio->phspi = MX_W25Q128J_0_SPI_GETHANDLE();
      pio->cs_port = MX_W25Q128J_0_CS_PORT;
      pio->cs_pin = MX_W25Q128J_0_CS_PIN;
      break;
    default:
      return -1;
  }
  return 0;
}

```

The link to the spi handle getter function is done in the `w25q128j_io_init()`, which is internally called by the `w25q128j_init()` API.
The `w25q128j_init()` function must be the first API to be called. Once this is successfully done, all operations can be called.

```c
w25q128j_obj_t NandObj;

(...)

w25q128j_init(&NandObj, MX_W25Q128J_0);
```
## Asynchronous read/write operations

The W25Q128J part driver provides full asynchronous read/write functionality, including multipage writes management through the use of an internal asynchronous state machine.

To use this feature, both `USE_HAL_SPI_REGISTER_CALLBACKS` and `USE_HAL_SPI_USER_DATA` must be enabled, and both the write complete callback and the read complete callback must be registered with the `w25q128j_register_write_cplt_callback()` & `w25q128j_register_read_cplt_callback()` APIs.

### Asynchronous read

The write state machine starts with the call to `w25q128j_read_dma()` and stops when all data has been received. At that point, the previously registered callback function will be called.

The state of the transfer can be checked with the help of `w25q128j_get_async_read_phase()` function, it can be one of the values defined in the enumeration below:

```c
typedef enum
{
  W25Q128J_ASYNC_READ_IDLE = 0,      /*!<  Idle state - No ongoing transfer */
  W25Q128J_ASYNC_READING             /*!<  Read transfer ongoing            */
} w25q128j_async_read_phase_t;
```

### Asynchronous write

The write state machine starts with the call to `w25q128j_write_dma()` and stops when all data has been transmitted. At that point, the previously registered callback function will be called.

The state of the transfer can be checked with the help of `w25q128j_get_async_write_phase()` function, it can be one of the values defined in the enumeration below:

```c
typedef enum
{
  W25Q128J_ASYNC_WRITE_IDLE = 0,      /*!< Idle state - No ongoing transfer                                 */
  W25Q128J_ASYNC_WEL,                 /*!< Send the write enable cmd                                        */
  W25Q128J_ASYNC_WRITE_CMD,           /*!< Send the write page cmd                                          */
  W25Q128J_ASYNC_WRITE_DATA           /*!< Send data to write                                               */
} w25q128j_async_write_phase_t;
```

