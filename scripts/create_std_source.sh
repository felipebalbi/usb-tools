#!/bin/sh

##
# create_std_source.sh - creates standard C source with GPL Header
#
# Copyright (C) 2009-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
#
# This file is part of the USB Verification Tools Project
#
# USB Tools is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public Liicense as published by
# the Free Software Foundation, either version 3 of the license, or
# (at your option) any later version.
#
# USB Tools is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with USB Tools. If not, see <http://www.gnu.org/licenses/>.
##

user=`git config user.name`
email=`git config user.email`
year=`date +%Y`

EDIT=0

usage ()
{
       echo "Usage: $0 [-h] [-e] [-o output_file.vhd]"
       exit 0
}

create_file ()
{
       local filename=$OUTPUT

       touch $filename
       echo "/**
 * $filename - **** ADD DESCRIPTION HERE ****
 *
 * Copyright (C) $year $user <$email>
 *
 * This file is part of the USB Verification Tools Project
 *
 * USB Tools is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public Liicense as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * USB Tools is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with USB Tools. If not, see <http://www.gnu.org/licenses/>.
 */
 " > $filename

       if [ $EDIT = 1 ]; then
               /usr/bin/editor $filename
       fi

       exit 0;
}

while getopts "eo:h" args
do
       case $args in
       e)
               EDIT=1;;
       o)
               OUTPUT=$OPTARG;;
       h | [?])
               usage;
               exit 1;;
       esac
done

if [ "x$OUTPUT" = "x" ]; then
       usage;
       exit 1
fi

create_file $OUTPUT

