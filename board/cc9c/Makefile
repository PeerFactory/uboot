#
# (C) Copyright 2000, 2001, 2002
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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

LIB	= lib$(BOARD).a
DIGI_DIR = $(TOPDIR)/common/digi
DIGI_BSP = -I $(DIGI_DIR) -I $(DIGI_DIR)/cmd_nvram/ -I $(DIGI_DIR)/cmd_nvram/lib/include -DUBOOT
CFLAGS   += $(DIGI_BSP)
CPPFLAGS += $(DIGI_BSP)

OBJS	:= cc9c.o
SOBJS	:= platform.o nandboot.o

$(LIB):	$(OBJS) $(SOBJS) $(LIB_BIG)
	$(AR) crv $@ $(SOBJS) $(OBJS)
$(LIB_BIG):	$(BIGOBJS)
	$(AR) crv $@ $^

clean:
	rm -f $(SOBJS) $(OBJS)  $(BIGOBJS)

distclean:	clean
	rm -f $(LIB) $(LIB_BIG) core *.bak .depend

#########################################################################

.depend:	Makefile $(SOBJS:.o=.S) $(OBJS:.o=.c)
		$(CC) -M $(CPPFLAGS) $(SOBJS:.o=.S) $(OBJS:.o=.c) > $@

-include .depend

#########################################################################
