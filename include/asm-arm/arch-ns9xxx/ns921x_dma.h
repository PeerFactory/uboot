/*
 * include/asm-arm/arch-ns9xxx/dma-ns921x.h
 *
 * Copyright (C) 2006 by Digi International Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 *  !Revision:   $Revision$
 *  !Author:     Luis Galdos
 *  !Desc:
 *  !References:
 */


#ifndef __ASM_ARCH_NS921X_DMA_H
#define __ASM_ARCH_NS921X_DMA_H


/* 
 * The maximal number of DMA-buffer descriptors comes from the NET+OS 
 * distribution (iop_private.h) 
 */
#define IOHUB_MAX_DMA_BUFFERS                   (64)
#define IOHUB_MAX_DMA_LENGTH                    (65535)


#define IOHUB_DMA_DESC_CTRL_WRAP                0x8000
#define IOHUB_DMA_DESC_CTRL_INT                 0x4000
#define IOHUB_DMA_DESC_CTRL_LAST                0x2000
#define IOHUB_DMA_DESC_CTRL_FULL                0x1000
#define IOHUB_DMA_DESC_CTRL_ALL                 (IOHUB_DMA_DESC_CTRL_FULL | \
                                                IOHUB_DMA_DESC_CTRL_INT | \
                                                IOHUB_DMA_DESC_CTRL_LAST | \
                                                IOHUB_DMA_DESC_CTRL_WRAP)

struct iohub_dma_desc_t {
    unsigned int src;
    unsigned int length;
    unsigned int reserved;
    unsigned short status;
    unsigned short control;
}__attribute__((__packed__, aligned(4)));


#define IOHUB_DMA_DESC_LENGTH                   sizeof(struct iohub_dma_desc_t)


/* This is the FIFO used for the DMA-transfers of the IOHUB (e.g. FIMs) */
struct iohub_dma_fifo_t {
    int length;
    struct iohub_dma_desc_t **descs;
    dma_addr_t phys_descs;
    struct iohub_dma_desc_t *first;
    struct iohub_dma_desc_t *last;
    struct iohub_dma_desc_t *dma_first;
    struct iohub_dma_desc_t *dma_last;
    struct iohub_dma_desc_t *dma_next;
    struct iohub_dma_desc_t *next_free;
};


#endif /* ifndef __ASM_ARCH_NS912X_DMA_H */
