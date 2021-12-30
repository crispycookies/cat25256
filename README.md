# OnSemi CAT25256-Series Driver

### What it is

This is a generic driver for the CAT25256-EEPROM. It can be used with any microcontroller, or even operating system. 

It provides an abstraction to the system-depended SPI-drivers and thus can be used on STM32s, NXP-controllers and others alike.

All you need to do is to set the ``read``, ``write``, ``cs_enable`` and  ``cs_disable`` callbacks to some to your system-depended SPI-driver-functions as seen in the next chapter. 

### How to use

* Declare a config struct of type ``cat25256_handle_t``.

  ```c
  cat25256_handle_t config;
  ```

* Create callback-functions for read, write, chip-select enable and disable. These callbacks are just wrappers to your platform-depended SPI-driver.

  An example used in one of my projects is given below:

  ```c
  memory_status_t read(void *handle, uint8_t *data, uint32_t length) {
      SPI_Init_Struct *spi = (SPI_Init_Struct *) handle; // When using STM32 Hal, this can also be SPI_HandleTypeDef
      /** 
       * When using STM32 Hal, this must be HAL_SPI_TransmitReceive, where RX is an array of 0x00. 
       * This is because the SPI must issue clock cycles and this is best done by "sending" dummy 0x00 bytes. 
       **/
      return SPI_ReadData(spi, data, length, 200) == SPI_RET_OK ? MEMORY_STATUS_OK : MEMORY_STATUS_NOK;
  }
  
  memory_status_t write(void *handle, uint8_t *data, uint32_t length) {
      SPI_Init_Struct *spi = (SPI_Init_Struct *) handle; // When using STM32 Hal, this can also be SPI_HandleTypeDef
      return SPI_WriteData(spi, data, length, 200) == SPI_RET_OK ? MEMORY_STATUS_OK : MEMORY_STATUS_NOK;
  }
  
  memory_status_t cs_enable(void *handle, size_t cs) {
      /**
       * cs simply is the chip-select pin. e.g. pin 0,1,2,3,4,16 etc.
       **/
      SPI_Init_Struct *spi = (SPI_Init_Struct *) handle; // When using STM32 Hal, this can also be SPI_HandleTypeDef
      return SPI_CS_Enable(&spi->CS[cs]) == SPI_RET_OK ? MEMORY_STATUS_OK : MEMORY_STATUS_NOK;
  }
  
  memory_status_t cs_disable(void *handle, size_t cs) {
      SPI_Init_Struct *spi = (SPI_Init_Struct *) handle; // When using STM32 Hal, this can also be SPI_HandleTypeDef
      return SPI_CS_Disable(&spi->CS[cs]) == SPI_RET_OK ? MEMORY_STATUS_OK : MEMORY_STATUS_NOK;
  }
  ```

* Add these callbacks to the `config`

  The ``low_level_handle`` is a pointer to any data you need for your platform-depended SPI-driver. In case your platform-depended SPI-driver is 	   STM32HAL based, this would be a ptr to ``SPI_HandleTypeDef``.

  ```c
  config.write = write;
  config.read = read;
  config.cs_enable = cs_enable;
  config.cs_disable = cs_disable;
  config.low_level_handle = (void *) &spi;
  ```

* Now you can use the following functions:

  ```c
  /**
   * @brief Reads data from the CAT25256 memory.
   * @param handle The cat25256_handle_t to use
   * @param address The address to read from
   * @param data The data buffer to read into
   * @param length The length of the data buffer
   * @param cs The chip select to use
   * @return MEMORY_STATUS_OK on success, MEMORY_STATUS_NOK on failure
   */
  memory_status_t cat25256_read(cat25256_handle_t *handle, uint32_t address, uint8_t *data, uint32_t length, size_t cs);
  
  /**
   * @brief Writes data to a single EEPROM-page
   * @param handle The cat25256_handle_t to use
   * @param address The address to write to
   * @param data The data buffer to write
   * @param length The length of the data buffer
   * @param cs The chip select to use
   * @return MEMORY_STATUS_OK on success, MEMORY_STATUS_NOK on failure
   */
  memory_status_t
  cat25256_write_page(cat25256_handle_t *handle, uint32_t address, const uint8_t *data, uint32_t length, size_t cs);
  
  /**
   * @brief Writes as much data to any address you want
   * @param handle The cat25256_handle_t to use
   * @param address The address to write to
   * @param data The data buffer to write
   * @param length The length of the data buffer
   * @param cs The chip select to use
   * @return MEMORY_STATUS_OK on success, MEMORY_STATUS_NOK on failure
   */
  memory_status_t
  cat25256_write(cat25256_handle_t *handle, uint32_t address, const uint8_t *data, uint32_t length, size_t cs);
  
  /**
   * @brief Reads the status register of the CAT25256 memory.
   * @param handle The handle to use
   * @param data The data to read into
   * @param cs The chip select to use
   * @return MEMORY_STATUS_OK on success, MEMORY_STATUS_NOK on failure
   */
  memory_status_t cat25256_read_register(cat25256_handle_t *handle, uint8_t *data, size_t cs);
  
  /**
   * @brief Writes into the status register of the CAT25256 memory.
   * @param handle The handle to use
   * @param data The data to write
   * @param cs The chip select to use
   * @return MEMORY_STATUS_OK on success, MEMORY_STATUS_NOK on failure
   */
  memory_status_t cat25256_write_register(cat25256_handle_t *handle, uint8_t data, size_t cs);
  ```
  
  
