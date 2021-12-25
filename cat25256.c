/**
 *  Copyright (C) 2021  Tobias Egger
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdbool.h>
#include "cat25256.h"

#define WREN    0b00000110
#define WRDI    0b00000100
#define RDSR    0b00000101
#define WRSR    0b00000001
#define READ    0b00000011
#define WRITE   0b00000010

#define NREADY     0x01
#define PAGE_SIZE  64

static memory_status_t cat25256_check_handle(const cat25256_handle_t *const handle) {
    if (handle == NULL) {
        return MEMORY_STATUS_INVALID_HANDLE;
    }
    if (handle->read == NULL) {
        return MEMORY_STATUS_INVALID_HANDLE;
    }
    if (handle->cs_disable == NULL) {
        return MEMORY_STATUS_INVALID_HANDLE;
    }
    if (handle->cs_enable == NULL) {
        return MEMORY_STATUS_INVALID_HANDLE;
    }
    if (handle->write == NULL) {
        return MEMORY_STATUS_INVALID_HANDLE;
    }
    return MEMORY_STATUS_OK;
}

static memory_status_t
cat25256_atomic_read(cat25256_handle_t *handle, uint32_t address, uint8_t *data, uint32_t length, size_t cs) {
    uint8_t header[3] = {0};
    header[0] = READ;
    header[1] = address >> 8;
    header[2] = address;

    handle->cs_enable(handle->low_level_handle, cs);
    if (handle->write(handle->low_level_handle, header, sizeof header) != MEMORY_STATUS_OK) {
        handle->cs_disable(handle->low_level_handle, cs);
        return MEMORY_STATUS_NOK;
    }
    uint8_t rc = handle->read(handle->low_level_handle, data, length);
    handle->cs_disable(handle->low_level_handle, cs);

    return rc;
}

memory_status_t cat25256_read(cat25256_handle_t *handle, uint32_t address, uint8_t *data, uint32_t length, size_t cs) {
    memory_status_t rc = cat25256_check_handle(handle);
    if (rc != MEMORY_STATUS_OK) {
        return rc;
    }

    return cat25256_atomic_read(handle, address, data, length, cs);
}

static memory_status_t cat25256_atomic_write_latch(cat25256_handle_t *handle, uint8_t enable, size_t cs) {
    handle->cs_enable(handle->low_level_handle, cs);
    memory_status_t rc = handle->write(handle->low_level_handle, &enable, 1);
    handle->cs_disable(handle->low_level_handle, cs);
    return rc;
}

memory_status_t cat25256_atomic_write_latch_enable(cat25256_handle_t *handle, size_t cs) {
    uint8_t enable = WREN;

    return cat25256_atomic_write_latch(handle, enable, cs);
}

memory_status_t cat25256_atomic_write_latch_disable(cat25256_handle_t *handle, size_t cs) {
    uint8_t disable = WRDI;

    return cat25256_atomic_write_latch(handle, disable, cs);
}

memory_status_t cat25256_atomic_wait_wip_completed(cat25256_handle_t *handle, size_t cs) {
    uint8_t wip = NREADY;
    while (wip & NREADY) {
        if (cat25256_read_register(handle, &wip, cs) != MEMORY_STATUS_OK) {
            return MEMORY_STATUS_NOK;
        }
    }
    return MEMORY_STATUS_OK;
}

static memory_status_t
cat25256_atomic_write(cat25256_handle_t *handle, uint32_t address, const uint8_t *data, uint32_t length, size_t cs) {
    uint8_t header[3] = {0};
    header[0] = WRITE;
    header[1] = address >> 8;
    header[2] = address;

    handle->cs_enable(handle->low_level_handle, cs);

    // Send the header
    if (handle->write(handle->low_level_handle, header, sizeof header) != MEMORY_STATUS_OK) {
        // If sending the header fails, we need to disable the latch and chip select
        handle->cs_disable(handle->low_level_handle, cs);
        return MEMORY_STATUS_NOK;
    }

    // Send the data
    uint8_t rc = handle->write(handle->low_level_handle, data, length);
    handle->cs_disable(handle->low_level_handle, cs);
    return rc;
}

memory_status_t
cat25256_write_page(cat25256_handle_t *handle, uint32_t address, const uint8_t *data, uint32_t length, size_t cs) {
    memory_status_t rc = cat25256_check_handle(handle);
    if (rc != MEMORY_STATUS_OK) {
        return rc;
    }

    if (cat25256_write_register(handle, NREADY, cs) != MEMORY_STATUS_OK) {
        return MEMORY_STATUS_NOK;
    }

    if (cat25256_atomic_write_latch_enable(handle, cs) != MEMORY_STATUS_OK) {
        return MEMORY_STATUS_NOK;
    }

    if (cat25256_atomic_write(handle, address, data, length, cs) != MEMORY_STATUS_OK) {
        cat25256_atomic_write_latch_disable(handle, cs);
        return MEMORY_STATUS_NOK;
    }

    cat25256_atomic_write_latch_disable(handle, cs);

    if (cat25256_atomic_wait_wip_completed(handle, cs) != MEMORY_STATUS_OK) {
        return MEMORY_STATUS_NOK;
    }

    return rc;
}

memory_status_t cat25256_read_register(cat25256_handle_t *handle, uint8_t *data, size_t cs) {
    memory_status_t rc = cat25256_check_handle(handle);
    if (rc != MEMORY_STATUS_OK) {
        return rc;
    }

    uint8_t read_reg = RDSR;

    handle->cs_enable(handle->low_level_handle, cs);
    if (handle->write(handle->low_level_handle, &read_reg, 1) != MEMORY_STATUS_OK) { ;
        handle->cs_disable(handle->low_level_handle, cs);
        return MEMORY_STATUS_NOK;
    }
    rc = handle->read(handle->low_level_handle, data, 1);
    handle->cs_disable(handle->low_level_handle, cs);

    return rc;
}

memory_status_t cat25256_write_register(cat25256_handle_t *handle, uint8_t data, size_t cs) {
    uint8_t write_reg[] = {WRSR, data};

    memory_status_t rc = cat25256_check_handle(handle);
    if (rc != MEMORY_STATUS_OK) {
        return rc;
    }

    handle->cs_enable(handle->low_level_handle, cs);
    rc = handle->write(handle->low_level_handle, write_reg, sizeof write_reg);
    handle->cs_disable(handle->low_level_handle, cs);

    return rc;
}

static memory_status_t
cat25256_write_address_aligned(cat25256_handle_t *handle, uint32_t address, const uint8_t *data, uint32_t length,
                               size_t cs) {
    memory_status_t rc;

    size_t page_count = length / PAGE_SIZE;
    if (length % PAGE_SIZE != 0) {
        page_count++;
    }

    for (int i = 0; i < page_count; ++i) {
        if (i == page_count - 1) {
            rc = cat25256_write_page(handle, address + i * PAGE_SIZE, &data[i * PAGE_SIZE], length % PAGE_SIZE, cs);
            if (rc != MEMORY_STATUS_OK) {
                return rc;
            }
        } else {
            rc = cat25256_write_page(handle, address + i * PAGE_SIZE, &data[i * PAGE_SIZE], PAGE_SIZE, cs);
            if (rc != MEMORY_STATUS_OK) {
                return rc;
            }
        }
    }
    return MEMORY_STATUS_OK;
}

static memory_status_t
cat25256_write_address_unaligned(cat25256_handle_t *handle, uint32_t address, const uint8_t *data, uint32_t length,
                                 size_t cs) {
    size_t length_first_transmission = PAGE_SIZE - (address % PAGE_SIZE);
    if (length_first_transmission > length) {
        length_first_transmission = length;
    }
    memory_status_t rc = cat25256_write_page(handle, address, data, length_first_transmission, cs);
    if (rc != MEMORY_STATUS_OK) {
        return rc;
    }
    int length_remainder = length - length_first_transmission;
    if (length_remainder > 0) {
        rc = cat25256_write_address_aligned(handle, address + length_first_transmission,
                                            data + length_first_transmission, length_remainder, cs);
    }
    return rc;
}

memory_status_t
cat25256_write(cat25256_handle_t *handle, uint32_t address, const uint8_t *data, uint32_t length, size_t cs) {
    if (length <= PAGE_SIZE) {
        return cat25256_write_page(handle, address, data, length, cs);
    }
    if (address % PAGE_SIZE == 0) {
        return cat25256_write_address_aligned(handle, address, data, length, cs);
    }
    return cat25256_write_address_unaligned(handle, address, data, length, cs);
}

