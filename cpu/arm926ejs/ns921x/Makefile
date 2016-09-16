#
# (C) Copyright 2000-2005
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# Copyright (C) 2007 by Digi International Inc.
# All rights reserved.
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

include $(TOPDIR)/config.mk

LIB	:= lib$(SOC).a

COBJS	:= timer.o serial_ns921x.o ns921x_edt.o ns921x_eth.o \
	   fim_serial.o fim.o fim_sdio.o

SOBJS	:= reset.o
OBJS	:= $(addprefix $(obj),$(COBJS) $(SOBJS))
SRCS	:= $(SOBJS:.o=.S) $(COBJS:.o=.c)

DIGI_DIR := $(TOPDIR)/common/digi
DIGI_BSP := -I $(DIGI_DIR) -I $(DIGI_DIR)/cmd_nvram/ -I $(DIGI_DIR)/cmd_nvram/lib/include -DUBOOT
CFLAGS   += $(DIGI_BSP)

all:	$(LIB)

$(LIB):	$(obj).depend $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
