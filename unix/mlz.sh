#!/bin/sh

#==============================================================================
# unix/mlz.sh: Combine loose object files and existing object libraries
#              into a new (big) object library.         Revised: 2014-05-20
#
# Copyright (c) 2012-2014 Info-ZIP.  All rights reserved.
#
# See the accompanying file LICENSE, version 2009-Jan-2 or later (the
# contents of which are also included in zip.h) for terms of use.  If,
# for some reason, all these files are missing, the Info-ZIP license may
# also be found at: ftp://ftp.info-zip.org/pub/infozip/license.html
#==============================================================================

#==============================================================================
# This script combines a set of loose object files ($2) with the object
# files in a set of object libraries ($3, $4, ...), to make a new (big)
# object library ($1).
#
# $1           Output archive.
# $2           ".o" file list.
# $3 $4 ...    ".a" file list.
#
# Default target directory is ".", but if $PROD is defined will use that
# for the location of the created archive.
#
# ${PROD}      Product file directory.
#
# Paths of .o and .a files should be relative to "." (not $PROD).  $PROD
# is not used to resolve input paths.
#
# Currently, each section of the optional code (AES_WG, LZMA, PPMd, ...)
# goes into its own, separate object library ($(LIB_AES_WG), $(LIB_LZMA),
# $(LIB_PPMD), $(LIB_BZ)).  It's easier to extract the object files from
# each of these object libraries and store them in a single archive than
# keep their component object files around and maintain lists of what's
# in them.
#==============================================================================

tmpdir="` basename $0 `_$$_tmp"

trap "sts=$? ; rm -rf $tmpdir; exit $sts" 0 1 2 3 15

# create temp dir for gathering object files
mkdir "$tmpdir"

#echo "into mlz"
#echo "1 = $1 (aro)"
#echo "2 = $2 (o_zip)"
#echo "3 = $3"
#echo "4 = $4"
#echo "5 = $5"
#echo "6 = $6"
#echo "7 = $7"

#echo "PROD = $PROD"
#echo "tmpdir = $tmpdir"

aro="$1"
o_zip="$2"
shift
shift

if test -z "${PROD}" ; then
  echo "#PROD not set - using ."
  PROD=.
fi

# Copy object files to tmpdir - this makes creating
# the archive easier
cp $o_zip $tmpdir

# move into the temp dir
cd "$tmpdir"
#pwd

# for each archive (args 3 (now 1) on) extract object files
# to tmpdir
for ar in "$@"
do
  abs_path="`echo $ar | grep '^/'`"
  if test -n "${abs_path}" ; then
    # look for absolute path archive
    ar x $ar
  else
    # look for relative path archive one level above tmpdir
    ar x ../$ar
  fi
done

#echo "-------------------"
#pwd
#echo "extracted files:"
#ls
#echo "-------------------"

# get list of object files in tmpdir, adjust path to include tmpdir
o_ext="` find . -name '*.o' -print | sed -e \"s|^.|${tmpdir}|\" `"

# change line ends to spaces
o_exta="`echo $o_ext | tr '\r\n' ' '`"
#echo "o_exta = $o_exta"

# back up to source root
cd ..
#pwd

# remove any existing old library
if test -f "$aro" ; then
  echo "removing old library $aro"
  rm -f "$aro"
fi

# build archive
#echo "ar r "$aro" $o_exta"
#echo ''
ar r "$aro" $o_exta

#ar r "$aro" $o_zip \
# ` ( cd "$tmpdir" ; find . -name '*.o' -print | sed -e "s|^.|${PROD}|" ) `

