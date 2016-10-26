#
#                       Copyright (C) Dialogic Corporation 1998-2007.  All Rights Reserved.
#
#   File:               mtu.mnt
#
#   Makefile to build:  mtu
#   Output file:        mtu (executable for Windows NT)
#
#   -------+---------+------+------------------------------------------
#   Issue     Date      By    Description
#   -------+---------+------+------------------------------------------
#     1     13-Jan-98   SFP    - Initial makefile.
#     2     07-Jul-99   JER    - Updated to include asciibin.c and bit2byte.c
#                                (previously provided in a library). One common
#                                library, gctlib, now used.
#     3     16-Feb-00   MH     - Minor updates.
#     4     27-Feb-07   ML     - Change copyright.

# Include common definitions:
!INCLUDE ../makdefs.mak

TARGET = $(BINPATH)\mtu.exe

all:    $(TARGET)

clean:
        del /q *.obj
        del /q $(TARGET)

OBJS = mtu.obj mtu_fmt.obj mtu_main.obj

$(TARGET): $(OBJS)
    $(LINKER) -out:$(TARGET) $(OBJS) $(LIBS) $(SYSLIBS)


