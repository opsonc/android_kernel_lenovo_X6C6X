#
# Makefile - WantJoin information tech co.ltd driver.
# Copyright(c) 2015, WantJoin information tech co.ltd.
#

#ccflags-y += -I$(srctree) -D__BTREE2__ -D__DROP_ONDNSREQ__ -D__DROP_ONSEND__ -D__DEBUG__
#ccflags-y += -I$(srctree) -D__BTREE2__ -D__DROP_ONSEND__ -D__DEBUG__
#ccflags-y += -I$(srctree) -D__BTREE2__ -D__DEBUG__
ccflags-y += -I$(srctree) -D__BTREE2__ -D__MY_THREAD__ -D__BLOCK_ALL__
obj-$(CONFIG_WANTJOIN_FIREWALL)	+= wjfirewall.o
wjfirewall-y += wjfw.o
wjfirewall-y += btree2.o
